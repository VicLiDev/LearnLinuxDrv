# Kconfig 语法介绍

> 参考资料:
>
> - Linux 官方文档: `Documentation/kbuild/kconfig-language.rst`
> - 博客: <https://blog.csdn.net/qq_23274715/article/details/104880443>

## 目录

- [基本说明](#基本说明)
- [配置工作流](#配置工作流)
- [Kconfig 语法总览](#kconfig-语法总览)
- [菜单项类型](#菜单项类型)
  - [config](#config)
  - [menuconfig](#menuconfig)
  - [mainmenu](#mainmenu)
  - [choice / endchoice](#choice--endchoice)
  - [menu / endmenu](#menu--endmenu)
  - [comment](#comment)
  - [if / endif](#if--endif)
  - [source](#source)
- [菜单项属性](#菜单项属性)
  - [类型定义](#类型定义)
  - [输入提示 (prompt)](#输入提示-prompt)
  - [默认值 (default)](#默认值-default)
  - [依赖关系 (depends on)](#依赖关系-depends-on)
  - [反向依赖 (select)](#反向依赖-select)
  - [弱反向依赖 (imply)](#弱反向依赖-imply)
  - [数值范围 (range)](#数值范围-range)
  - [帮助信息 (help)](#帮助信息-help)
  - [选项修饰符 (option)](#选项修饰符-option)
  - [其他属性](#其他属性)
- [表达式语法](#表达式语法)
- [配置符号命名惯例](#配置符号命名惯例)
- [select / depends on / imply 对比](#select--depends-on--imply-对比)
- [完整示例](#完整示例)

---

## 基本说明

在项目开发中，通常需要根据不同需求对工程进行配置和裁剪。传统做法是专门定义一个
`config_xxx.h` 文件，使用 `#define CONFIG_USING_XX` 等宏进行管理。但这种方式的
问题是：**不直观、难以维护、模块多时效率低**。

Linux 内核使用 Kconfig 来组织并管理配置：
- Kconfig 文件定义**配置项的菜单结构和依赖关系**
- 通过 `make menuconfig` 等**可视化工具**生成 `.config` 文件
- `.config` 文件再被转换为 `autoconf.h`（或 `config.h`），供 C 代码通过
  `#ifdef CONFIG_XXX` 使用

要点：
1. 语法文档: Linux 源码 `Documentation/kbuild/kconfig-language.rst`
2. 参考实例: Linux 源码树中的所有 `Kconfig` 文件
3. 在 `menuconfig` 界面中按 `h` 或 `?` 可查看当前选项的帮助信息
4. `.config` 中的前缀 `CONFIG_` 由顶层 Makefile 定义，Kconfig 符号本身**不带**此前缀
5. 不同架构的入口 Kconfig 在 `arch/<arch>/Kconfig`，例如 ARM 为 `arch/arm/Kconfig`

## 配置工作流
```
                    Kconfig 文件
                         │
                    make menuconfig
                    (或其他配置工具)
                         │
                         ▼
                    .config 文件
                   (用户配置结果)
                         │
                    scripts/kconfig/
                         │
                         ▼
                 include/generated/autoconf.h
                  (C 语言可用的宏定义)
                         │
                         ▼
                  #ifdef CONFIG_XXX
                  (C 代码中使用)
```

常用配置命令：

| 命令                 | 说明                               |
|----------------------|------------------------------------|
| `make menuconfig`    | 基于 ncurses 的文本菜单（最常用）  |
| `make nconfig`       | 基于 ncurses 的增强版菜单          |
| `make xconfig`       | 基于 Qt 的图形界面                 |
| `make gconfig`       | 基于 GTK 的图形界面                |
| `make oldconfig`     | 基于现有 `.config`，只对新选项提问 |
| `make defconfig`     | 使用架构默认配置                   |
| `make savedefconfig` | 保存最小化的 defconfig             |

---

## Kconfig 语法总览

1. Kconfig 以**菜单项 (menu entry)** 为基本组成单位，每个菜单项有自己的属性
2. 菜单项之间可以**嵌套**
3. 使用 `#` 作为注释符
4. Kconfig 符号（配置项名称）使用大写字母、数字和下划线，**不使用** `CONFIG_` 前缀
   （前缀由构建系统自动添加）

---

## 菜单项类型

Kconfig 以**菜单项 (menu entry)** 为基本组成单位，不同的关键字定义了不同类型的菜单项。
按功能可以划分为以下几类：
- **定义配置符号** — `config`、`menuconfig`：最核心的类型，用于定义具体的配置选项
- **结构组织** — `mainmenu`、`menu`、`choice`、`comment`、`if`、`source`：用于组织菜单结构、
  条件控制、文件包含，本身（除 `choice` 可选绑定符号外）不产生配置符号

下面逐一介绍每种类型及其用法。

### config

定义一个配置选项，可以接受所有菜单项属性，**是语法中最常用的菜单项**。
```kconfig
# 基本格式
config <symbol>
    <config options>

# 示例
config USING_DMA
    bool "Enable DMA support"
    default n
    depends on HAS_HW_DMA
    help
      Enable Direct Memory Access support for this driver.
      When enabled, data transfers will use DMA instead of CPU PIO.
```

> **注意**: 符号名不要带 `CONFIG_` 前缀，构建系统会自动添加。即写
> `config USING_DMA`，最终生成的宏是 `CONFIG_USING_DMA`。

### menuconfig

与 `config` 类似，定义一个配置选项，但它常配合 `if` 块使用。
只有 `menuconfig` 被选中时，`if` 块中的子选项才会展现。

```kconfig
# 格式
menuconfig <symbol>
    <config options>

# 示例
menuconfig NETWORK_SUPPORT
    bool "Network support"

if NETWORK_SUPPORT
config NET_TCP
    bool "TCP protocol support"
    default y

config NET_UDP
    bool "UDP protocol support"
    default y
endif
```

### mainmenu

设置配置程序的标题栏，应放在配置文件的**最顶部**。

```kconfig
# 格式
mainmenu <title>

# 示例
mainmenu "My Embedded Project Configuration"
```

### choice / endchoice

定义一个**选择器**，内部包含多个选项，**只能单选**。用户必须选择其中一个
（如果有 `optional` 则可以都不选）。

```kconfig
# 格式
choice [symbol]
    <choice options>
    <choice block>
endchoice

# 示例
choice
    prompt "Kernel compression format"
    default KERNEL_GZIP
    help
      Select the compression format for the kernel image.

config KERNEL_GZIP
    bool "Gzip"

config KERNEL_BZIP2
    bool "Bzip2"

config KERNEL_LZMA
    bool "LZMA"

config KERNEL_XZ
    bool "XZ"
endchoice
```

`choice` 的选项值可以被子菜单项通过 `select` 来设定默认值。

### menu / endmenu

定义一个**纯菜单块**，用于将多个配置项归组显示。与 `menuconfig` 不同，
`menu` 本身不是一个配置符号。

```kconfig
# 格式
menu <prompt>
    <menu block>
endmenu

# 示例
menu "Memory Management"

config MEM_SIZE
    int "Total memory size (MB)"
    range 16 1024
    default 256

config MEM_DEBUG
    bool "Enable memory debug"
    default n

endmenu
```

### comment

在菜单中添加一行注释文字（不产生配置符号），可用于分组或提示。

```kconfig
# 格式
comment <prompt>
    <comment options>

# 示例
comment "PWM Configuration"

config PWM_PERIOD_MS
    int "Default PWM period (ms)"
    range 1 1000
    default 1000
```

### if / endif

定义一个**条件判断块**，只有条件为真时，块内的菜单项才会显示。

```kconfig
# 格式
if <expr>
    <if block>
endif

# 示例
config RT_USING_USER_MAIN
    bool "Use user main thread"
    default y

if RT_USING_USER_MAIN
config RT_MAIN_THREAD_STACK_SIZE
    int "Main thread stack size"
    default 2048

config RT_MAIN_THREAD_PRIORITY
    int "Main thread priority"
    default 4 if RT_THREAD_PRIORITY_8
    default 16
endif
```

### source

将其他 Kconfig 文件**包含**到当前位置并解析显示，类似 C 语言的 `#include`。

```kconfig
# 格式
source <pattern>

# 示例
source "drivers/Kconfig"
source "arch/$ARCH/Kconfig"
```

`source` 支持通配符（glob pattern），可以包含多个文件。

---

## 菜单项属性

### 类型定义

每个 `config` 菜单项**必须**选择一种类型（或通过 prompt 隐式指定）：

| 类型       | 说明           | 取值范围                |
|------------|----------------|-------------------------|
| `bool`     | 布尔型（二值） | `y` / `n`               |
| `tristate` | 三态型         | `y` / `n` / `m`（模块） |
| `int`      | 十进制整数     | 整数                    |
| `hex`      | 十六进制整数   | `0x` 前缀的十六进制     |
| `string`   | 字符串         | 任意字符串              |

**`m`（模块）的含义**: 值为 `m` 时表示该功能编译为可加载模块（`.ko`），
不直接链接进内核，运行时通过 `insmod` / `modprobe` 加载。

**类型的简写形式** — 类型定义和输入提示可以合写在一行：

```kconfig
# 完整写法
config USING_UART
    bool
    prompt "Enable UART"

# 简写（最常见）
config USING_UART
    bool "Enable UART"
```

**默认类型简写** — `def_bool`、`def_tristate` 等是 "类型 + 默认值" 的简写：

```kconfig
# 以下两种写法等价

# 写法一
config ARM
    bool
    default y

# 写法二（简写）
config ARM
    def_bool y
```

类似的还有 `def_int`、`def_hex`、`def_string`、`def_tristate`。

### 输入提示 (prompt)

每个菜单项最多一个输入提示，显示在菜单界面上。可用 `if` 为提示添加条件：

```kconfig
config DEBUG_INFO
    bool "Compile the kernel with debug info"
    depends on !COMPILE_TEST
```

### 默认值 (default)

当用户没有手动设置时，使用默认值。可以有**多个** default，只有第一个可见的生效。

```kconfig
config ARCH_MMAP_RND_BITS_MAX
    default 14 if PAGE_OFFSET_4G
    default 15 if PAGE_OFFSET_2G
    default 16
```

上面的含义是：
- 如果 `PAGE_OFFSET_4G` 为 `y`，则默认值为 `14`
- 否则如果 `PAGE_OFFSET_2G` 为 `y`，则默认值为 `15`
- 否则默认值为 `16`

### 依赖关系 (depends on)

当前配置项依赖的其他**配置符号**（即其他 `config` 语句定义的符号名）。
只有所有依赖条件求值为真时，当前项才会在菜单中显示。

```kconfig
# 以下 MMU、ARCH_FOO 都是其他地方通过 config 定义的配置符号
config ARCH_MULTIPLATFORM
    bool "Allow multiple platforms"
    depends on MMU            # MMU 必须为 y，当前项才可见

# 多个依赖用 &&、||、! 组合
config SMP
    bool "Symmetric Multi-Processing"
    depends on MMU && !ARCH_FOO   # MMU 为 y 且 ARCH_FOO 未选中时才可见
```

`depends on` 也可以用行内 `if` 简写：

```kconfig
# 以下两种写法等价
config FOO
    bool "Foo support"
    depends on BAR

config FOO
    bool "Foo support" if BAR
```

以下是一个完整的自包含示例，展示了 `depends on` 如何引用其他 `config` 符号：

```kconfig
# ---- 先定义被依赖的符号 ----

config HAS_HW_DMA
    bool "Platform has DMA hardware"

config HAS_MMU
    bool "Platform has MMU"

config ARCH_FOO
    bool "Enable FOO architecture"

# ---- 再定义依赖它们的符号 ----

# 只有 HAS_HW_DMA 为 y 时，USING_DMA 才会出现在菜单中
config USING_DMA
    bool "Enable DMA support"
    depends on HAS_HW_DMA

# 需要 HAS_MMU 为 y，且 ARCH_FOO 未被选中时，SMP 才可见
config SMP
    bool "Symmetric Multi-Processing"
    depends on HAS_MMU && !ARCH_FOO
```

> **注意**：被引用的符号（如 `HAS_HW_DMA`）必须在某个 Kconfig 文件中有对应的
> `config HAS_HW_DMA` 定义，`depends on` 只是引用它，不会自动创建。

### 反向依赖 (select)

**当前项被选中时，自动选中被 select 的项**。与 `depends on` 方向相反。
只能用于 `bool` 和 `tristate` 类型。

```kconfig
config ARM
    bool
    default y
    select ARCH_HAS_POSIX_CPU_TIMERS
    select ARCH_HAVE_CUSTOM_GPIO_H
```

> **警告**: `select` 会**强制**启用被选中的符号，即使该符号有自己的 `depends on`
> 不满足。这在某些场景下会导致问题。例如：
>
> ```kconfig
> config A
>     bool "Feature A"
>     depends on B        # A 依赖 B
>
> config C
>     bool "Feature C"
>     select A            # C 强制选中 A，但此时 B 可能不满足！
> ```
>
> 这被称为 **"select 泄漏"**，应尽量避免。替代方案是使用 `imply`。

### 弱反向依赖 (imply)

类似于 `select`，但只是**建议**（弱反向依赖）。如果被 imply 的符号有未满足的
`depends on`，则不会强制启用。

```kconfig
config USB
    bool "USB support"
    imply USB_STORAGE     # 建议启用 USB_STORAGE，但不强制

config USB_STORAGE
    bool "USB mass storage support"
    depends on SCSI       # 只有 SCSI 可用时才会启用
```

### 数值范围 (range)

限制 `int` 和 `hex` 类型数值的取值范围。

```kconfig
config DMA_ALIGNMENT
    int "DMA alignment (log2)"
    range 4 9
    default 8
```

### 帮助信息 (help)

为用户提供详细的帮助文本，在 `menuconfig` 中按 `?` 查看。
帮助文本以 `help` 或 `---help---` 开始，**由缩进决定结束位置**：

```kconfig
config USING_DMA
    bool "Enable DMA support"
    help
      This option enables DMA (Direct Memory Access) support.

      When enabled, the driver will use DMA for data transfer,
      which reduces CPU utilization and improves throughput.

      Say N if unsure.
```

帮助文本中的常用约定：
- `Say Y if ...` / `Say N if unsure` — 引导用户选择

### 选项修饰符 (option)

`option` 用于设置特殊行为，不产生配置符号：

```kconfig
config MODULES
    bool
    option modules           # 启用模块支持

config DEFCONFIG_LIST
    string
    option defconfig_list    # 声明默认配置文件列表
    default "arch/$ARCH/defconfig"

config ARC
    string
    option env="ARCH"        # 从环境变量 ARCH 获取值
```

常用 option：

| option           | 说明                              |
|------------------|-----------------------------------|
| `defconfig_list` | 声明默认配置文件路径列表          |
| `modules`        | 标记为模块支持入口                |
| `env="<var>"`    | 从环境变量读取值                  |
| `allnoconfig_y`  | `make allnoconfig` 时仍然设为 `y` |

### 其他属性

- **`defconfig_list`**: 定义默认配置文件搜索列表，`make defconfig` 时使用
- **`modules`**: 标识模块支持功能，通常只定义一次
- **`env=<value>`**: 将环境变量导入 Kconfig 符号

---

## 表达式语法

Kconfig 中的表达式用于 `depends on`、`if`、`default` 等条件中：

### 布尔/三态运算符

| 运算符     | 说明          |
|------------|---------------|
| `=` / `!=` | 等于 / 不等于 |
| `&&`       | 逻辑与        |
| `\|\|`     | 逻辑或        |
| `!`        | 逻辑非        |

### 字符串和数值比较

| 运算符               | 说明                      |
|----------------------|---------------------------|
| `=` / `!=`           | 字符串/数值 等于 / 不等于 |
| `<`, `>`, `<=`, `>=` | 数值比较（仅 int/hex）    |

### 示例

```kconfig
# 布尔组合
depends on ARCH_ARM && !ARCH_FOO && (SMP || PREEMPT)

# 数值比较
config PAGE_OFFSET
    hex
    default 0x80000000 if ARCH_2G
    default 0x40000000 if ARCH_1G

# 在 default 中使用表达式
config DEFAULT_HOSTNAME
    string
    default "(none)" if !DEFAULT_HOSTNAME
```

### 引用符号

- 直接使用符号名: `depends on MMU`
- 引用字符串值: `default "value"`
- 引用数值: `default 1024` 或 `default 0x400`

---

## 配置符号命名惯例

1. **符号名不使用 `CONFIG_` 前缀**: Kconfig 中定义 `config FOO`，构建系统自动
   在 `.config` 和头文件中添加 `CONFIG_` 前缀生成 `CONFIG_FOO`
2. **使用大写 + 下划线**: 如 `GPIO_MXC`, `SERIAL_8250`, `NETFILTER`
3. **按模块/子系统前缀**: 如 `I2C_`, `SPI_`, `USB_`, `NET_`
4. **bool / tristate 符号通常使用积极语义**: `ENABLE_FOO` 或 `FOO_SUPPORT`，
   而非 `DISABLE_FOO`

---

## select / depends on / imply 对比

| 特性     | `depends on`             | `select`                             | `imply`                  |
|----------|--------------------------|--------------------------------------|--------------------------|
| 方向     | 当前项依赖目标项         | 当前项选中时强制选中目标             | 当前项选中时建议选中目标 |
| 强制性   | 当前项显示需要目标项为 y | 强制设为 y，**无视**目标项的 depends | 尊重目标项的 depends on  |
| 风险     | 无                       | 可能破坏依赖关系（select 泄漏）      | 安全，推荐替代 select    |
| 使用场景 | 条件显示                 | 必要的隐式依赖                       | 可选的推荐依赖           |

---

## 完整示例

下面是一个综合运用各属性的 Kconfig 示例：

```kconfig
# Kconfig 顶层入口
mainmenu "Demo Project Configuration"

comment "Project Settings"

config PROJECT_NAME
    string "Project name"
    default "my_project"

config DEBUG_LEVEL
    int "Debug level (0-3)"
    range 0 3
    default 0

choice
    prompt "Target Architecture"
    default ARCH_ARM

config ARCH_ARM
    bool "ARM (32-bit)"
    select HAS_MMU
    imply ARM_SMP

config ARCH_X86
    bool "x86 (64-bit)"
    select HAS_MMU

config ARCH_RISCV
    bool "RISC-V"
endchoice

menu "Device Drivers"

config DRIVER_UART
    bool "UART driver support"
    default y if ARCH_ARM

config DRIVER_SPI
    bool "SPI driver support"

config DRIVER_I2C
    bool "I2C driver support"
    depends on ARCH_ARM || ARCH_X86
    help
      Enable I2C bus driver. This is only available on ARM and x86
      architectures.

      Say Y if you have I2C devices.

endmenu

menu "Networking"

config NET_SUPPORT
    bool "Networking support"
    default y

if NET_SUPPORT
config NET_TCP
    bool "TCP protocol"

config NET_UDP
    bool "UDP protocol"
    default y

config NET_DEBUG
    bool "Network debug logging"
    depends on DEBUG_LEVEL > 0
endif

endmenu

# 引入子目录的 Kconfig
source "board/Kconfig"
```

最终生成的 `.config` 可能如下：

```ini
# .config - Demo Project Configuration
CONFIG_PROJECT_NAME="my_project"
CONFIG_DEBUG_LEVEL=0
CONFIG_ARCH_ARM=y
# CONFIG_ARCH_X86 is not set
# CONFIG_ARCH_RISCV is not set
CONFIG_HAS_MMU=y
CONFIG_DRIVER_UART=y
# CONFIG_DRIVER_SPI is not set
# CONFIG_DRIVER_I2C is not set
CONFIG_NET_SUPPORT=y
# CONFIG_NET_TCP is not set
CONFIG_NET_UDP=y
# CONFIG_NET_DEBUG is not set
```
