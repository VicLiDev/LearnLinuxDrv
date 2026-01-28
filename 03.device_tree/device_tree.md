# 设备树规范

[devicetree.org](https://www.devicetree.org/specifications/)

[devicetree-specification](https://github.com/devicetree-org/devicetree-specification)

参考设备树规范版本：0.4

## 简介

DTSpec 支持具有 32 位和 64 位寻址功能的 CPU。

**AMP**

非对称多处理。 计算机可用的 CPU 被划分为多个组，每个组运行不同的操作系统映像。
CPU 可能相同也可能不同。

**boot CPU**

引导程序引导至客户端程序入口点的第一个 CPU。

**Book III-E**

嵌入式环境。 Power ISA 的一部分，定义了嵌入式 Power 处理器实现中使用的管理程序
指令和相关设施。

**boot program**

用于一般指初始化系统状态并执行称为客户端程序的另一个软件组件的软件组件。 引导
程序的示例包括：固件、引导加载程序和管理程序。

**client program**

通常包含应用程序或操作系统软件的程序。 客户端程序的示例包括：引导加载程序、管理
程序、操作系统和专用程序。

**cell**

由 32 位组成的信息单元。

**DMA**

直接内存访问

**DTB**

设备树编译得到的二进制文件

**DTC**

设备树编译器，将DTS文件编译成DTB文件，是一个开源工具。在Linux内核的scripts/dtc
目录下可以找到，内核可以单独编译dtb文件`make dtbs`

关于哪些文件将被编译为dtb，可以查看设备树目录下的Makefile文件，被加到配置中的
设备树文件都将被编译

DTC也可以在ubuntu上单独安装：
```shell
sudo apt-get install device-tree-compiler
```

**DTS**

设备树源码文件。.dts文件可以引用C语言中的.h文件，甚至也可以引用.dts文件。因此，
.dts文件可以通过`#include`来引用.h、.dtsi和.dts文件，只是我们在编写设备树头文件
的时候最好选择.dtsi作为扩展名。一般.dtsi文件用于描述SOC的内部外设信息。

**DTSI**

一个芯片（SOC）可以做出很多不同的板子，这些不同的板子肯定是有共同的信息的，将这些
共同的信息提取出来作为一个通用的文件，其他的.dts文件直接引用这个通用文件即可，这
个通用文件就是.dtsi文件，类似于C语言中的头文件。一般.dts文件描述板级信息（也就是
开发板上有哪些I2C设备、SPI设备等），dtsi描述SOC级信息（也就是SOC有几个CPU、主频
是杜少、各个外设控制器信息、SOC内部IP等）。

**effective address**

由处理器存储访问或分支指令计算的内存地址。

**physical address**

处理器用来访问外部设备（通常是内存控制器）的地址。

**Power ISA**

电源指令集架构

**interrupt specifier**

描述中断的属性值。 通常包括指定中断号和灵敏度以及触发机制的信息。

**secondary CPU**

除启动 CPU 之外的属于客户端程序的 CPU 被视为辅助 CPU。

**SMP**

对称多处理。 一种计算机架构，其中两个或多个相同的 CPU 可以共享内存和 IO，并在
单个操作系统下运行。

**SoC**

片上系统。 集成一个或多个 CPU 核心以及许多其他外围设备的单个计算机芯片。

**unit address**

节点名称的一部分，指定节点在父节点地址空间中的地址。

**quiescent CPU**

静止CPU处于一种状态，它不能干扰其他CPU的正常操作，其状态也不能受到其他正在运行的
CPU的正常操作的影响，除非通过显式方法启用或重新启用静止CPU。



## 设备树

### 概述

DTSpec 指定了一种称为设备树的结构来描述系统硬件。 引导程序将设备树加载到客户端
程序(这里指kernel)的内存中，并将指向设备树的指针传递给客户端。

* 本章描述了设备树的逻辑结构，并给出了用于描述设备节点的一组基本属性。
* 第 3 章描述了符合 DTSpec 的设备树必要的某些设备节点。
* 第 4 章描述了 DTSpec 定义的设备绑定——表示某些设备类型或设备类别的要求。
* 第 5 章描述了设备树的内存中编码。

设备树是一种树形数据结构，其节点描述系统中的设备。 每个节点都有描述当前设备的
特征的属性/值对。 每个节点只有一个父节点，但根节点除外，根节点没有父节点。

<font color='red'>可以理解设备树节点用于描述设备，而设备树的主干表示系统总线</font>

设备树中的设备可以是
1. 实际的`硬件设备`，例如UART。
2. 可能是`硬件设备的一部分`，例如 TPM 中的随机数生成器
3. 可以是`通过虚拟化提供的设备`，例如提供对连接到远程CPU的I2C设备的访问的协议。

设备可以包括由在更高权限级别或远程处理器中运行的固件实现的功能。
设备树中的节点不要求是物理硬件设备，但通常它们与物理硬件设备有一定的相关性。
节点不应设计用于特定于操作系统或项目的目的。它们应该描述可以由任何操作系统或项目实现的东西。

设备树通常用于描述客户端程序不一定能够动态检测到的设备。 例如，PCI 的体系结构使
客户端能够探测和检测连接的设备，因此可能不需要描述 PCI 设备的设备树节点。 然而，
设备节点通常用来描述系统中的 PCI 主桥设备。 如果无法通过探测检测到网桥，则此节点
是必需的，但在其他情况下是可选的。 此外，引导加载程序可以进行 PCI 探测并生成包含
其扫描结果的设备树，以传递给操作系统。

下图显示了一个简单设备树的示例，该设备树几乎足够完整，可以启动一个简单的操作系统，
并描述了平台类型、CPU、内存和单个 UART。 设备节点显示每个节点内的属性和值。

![](./device_tree.assets/1.jpg)

### Devicetree 结构和约定

#### 节点名称

设备树中的每个节点根据以下约定命名：
```
node-name@unit-address
```
节点名称长度应为1-31个字符，并且仅由下表中的字符组成

| Character | Description |
|--|--|
| 0-9 | 数字 |
| a-z | 小写字母 |
| A-Z | 大写字母 |
| , | 逗号 |
| . | 句号 |
| _ | 下划线 |
| + | 加号|
| - | 短划线 |

节点名称应以小写或大写字符开头，并应描述设备的一般类别。

名称的单元地址部分特定于节点所在的总线类型。 它由上表中的字符集中的一个或多个
ASCII 字符组成。 **单元地址必须与节点的 reg 属性中指定的第一个地址匹配，
即unit-address必须是首地址。 如果节点没有 reg 属性，则必须省略 @unit-address**，
并且仅使用节点名称即可将该节点与树中同一级别的其他节点区分开来。 特定总线的绑定
可以为 reg 和单元地址的格式指定附加的、更具体的要求。

对于没有@unit-address 的节点名称，节点名称对于树中同一级别的任何属性名称来说应是
唯一的。

根节点没有节点名称或单元地址。 它由正斜杠 (/) 标识。

![](./device_tree.assets/2.jpg)

上例中：
* 名称为 cpu 的节点通过其单元地址值 0 和 1 进行区分。
* 名称为ethernet 的节点通过其单元地址值fe002000 和fe003000 进行区分。

也存在如下形式的格式：
```
label: node-name@unit-address
```
引入label的目的就是便于访问节点，可以直接通过`&label`来访问这个节点。

例如：
```dts
//dts-v1
/{
  uart0: uart@fe001000{
    compatible="ns16550";
    reg=<0xfe001000 0x100>;
  };
};

// 可以使用如下两种方法修改uart@fe001000这个node:
// 在根结点之外使用 label 引用 node：
&uart0{
  status="disabled";
};

// 或在根节点之外使用全路径：
&{/uart@fe001000}{
  status="disabled";
}
// 这里的含义为修改 uart0 的成员: status
```

还可以借label简化数据追加或者修改，如果节点存在于desi中，板子需要对其进行修改，
肯定是不能直接在dtsi中修改的，因为这样的话所有引用dtsi的dts文件都会受到影响，
因此比较好的方法是在dts中根据自己板子的需求追加或者修改节点内容，例如：
```dts
&i2c1 {
    /* 要 追加/修改 的内容 */
}
```


#### 通用名推荐

节点的名称应该有点通用，反映设备的功能而不是其精确的编程模型。 如果合适，名称应为以下选择之一：

|||||
|--|--|--|--|
|  adc                  |  efuse                 |  lora               |  rng                 |
|  accelerometer        |  endpoint              |  magnetometer       |  rtc                 |
|  air-pollution-sensor |  ethernet              |  mailbox            |  sata                |
|  atm                  |  ethernet-phy          |  mdio               |  scsi                |
|  audio-codec          |  fdc                   |  memory             |  serial              |
|  audio-controller     |  flash                 |  memory-controller  |  sound               |
|  backlight            |  gnss                  |  mmc                |  spi                 |
|  bluetooth            |  gpio                  |  mmc-slot           |  spmi                |
|  bus                  |  gpu                   |  mouse              |  sram-controller     |
|  cache-controller     |  gyrometer             |  nand-controller    |  ssi-controller      |
|  camera               |  hdmi                  |  nvram              |  syscon              |
|  can                  |  hwlock                |  oscillator         |  temperature-sensor  |
|  charger              |  i2c                   |  parallel           |  timer               |
|  clock                |  i2c-mux               |  pc-card            |  touchscreen         |
|  clock-controller     |  ide                   |  pci                |  tpm                 |
|  co2-sensor           |  interrupt-controller  |  pcie               |  usb                 |
|  compact-flash        |  iommu                 |  phy                |  usb-hub             |
|  cpu                  |  isa                   |  pinctrl            |  usb-phy             |
|  cpus                 |  keyboard              |  pmic               |  vibrator            |
|  crypto               |  key                   |  pmu                |  video-codec         |
|  disk                 |  keys                  |  port               |  vme                 |
|  display              |  lcd-controller        |  ports              |  watchdog            |
|  dma-controller       |  led                   |  power-monitor      |  wifi                |
|  dsi                  |  leds                  |  pwm                |                      |
|  dsp                  |  led-controller        |  regulator          |                      |
|  eeprom               |  light-sensor          |  reset-controller   |                      |

#### 路径名字

设备树中的节点可以通过指定从根节点经过所有子节点到目前节点的完整路径来唯一标识。

指定设备路径的约定是：
```
/node-name-1/node-name-2/node-name-N
```

以图2.2中CPU #1 的设备路径为例：
```
/cpus/cpu@1
```
根节点的路径是`/`，如果存在多个根节点，例如dtsi和dts文件中都存在根节点，这样的
情况下多个根节点会被合并为一个根节点

如果到节点的完整路径明确，则可以省略单元地址。

如果客户端程序遇到不明确的路径，则其行为是未定义的。

#### 属性

设备树中的每个节点都具有描述该节点特征的属性。 属性由名称和值组成。

**属性名**

属性名称是下表中显示的 1 到 31 个字符的字符串

| Character | Description |
|--|--|
| 0-9 | 数字 |
| a-z | 小写字母 |
| A-Z | 大写字母 |
| , | 逗号 |
| . | 句号 |
| _ | 下划线 |
| + | 加号|
| ? | 问号|
| # | 井号|
| - | 短划线 |

非标准属性名称应指定唯一的字符串前缀，例如股票代码，标识定义该属性的公司或组织的
名称。 例子：
```
fsl,channel-fifo-len
ibm,ppc-interrupt-server#s
linux,network-index
```

**属性值**

属性值是零个或多个字节的数组，其中包含与属性关联的信息。

如果传达真假信息，属性可能具有空值。 在这种情况下，属性的存在或不存在就足以描述性的。

下表描述了 DTSpec 定义的基本值类型集

| Value | Description |
|--|--|
| `<empty>` | 值为空。 当属性本身的存在或不存在具有足够的描述性时，用于传达真假信息。 |
| `<u32>` | 大端格式的 32 位整数。 |
| `<u64>` | 表示大端格式的 64 位整数。 由两个 `<u32>` 值组成，其中第一个值包含整数的最高有效位，第二个值包含最低有效位。示例：64 位值 0x1122334455667788 将表示为两个单元格：`<0x11223344 0x55667788>`。 |
| `<string>` | 字符串是可打印的并且以 null 结尾。 |
| `<prop-encoded-array>` | 格式特定于属性。 请参阅属性定义。 |
| `<phandle>` | phandle 值是引用设备树中另一个节点的一种方式。 任何可以引用的节点都会定义具有唯一 `<u32>` 值的 phandle 属性。 该数字用于具有 phandle 值类型的属性值。 |
| `<stringlist>` | 连接在一起的 `<string>` 值的列表。 |

示例：

一个32位的数据，用尖括号包围起来，如
```c
interrupts = <17 0xc>;
```

一个64位数据（使用2个32位数据表示），用尖括号包围起来，如：
```c
clock-frequency = <0x00000001 0x00000000>;
```

有结束符的字符串，用双引号包围起来，如：
```c
compatible = "simple-bus";
```

字节序列，用中括号包围起来，如：
```c
local-mac-address = [00 00 12 34 56 78]; // 每个byte使用2个16进制数来表示
local-mac-address = [000012345678];      // 每个byte使用2个16进制数来表示
```

可以是各种值的组合，用逗号隔开，如：
```c
compatible = "ns16550", "ns8250";
example = <0xf00f0000 19>, "a strange property format";
```

### 标准属性

DTSpec 指定了设备节点的一组标准属性。 DTSpec（参见第 3 章）定义的设备节点可以指定
有关标准属性使用的附加要求或约束。 第 4 章描述了特定设备的表示，也可能指定附加要求。

驱动架构通常会直接依赖标准属性，但各个驱动内部也可以定义自己使用的属性，由于这部分
属性不是设备树特性所定义的，因此驱动架构也不会使用。

#### compatible

**属性名**：compatible

**值类型**：`<stringlist>`

**描述**：

兼容属性值由一个或多个与设备特性强相关的字符串组成。 客户端程序应使用此字符串列表
来匹配选择设备驱动程序。 属性值由以 null 结尾的字符串的串联列表组成，从最具体到最
一般。 它们允许设备表达其与一系列类似设备的兼容性，从而可能允许单个设备驱动程序与
多个设备进行匹配。

推荐的格式是“制造商,型号”，其中制造商是描述制造商名称的字符串（例如股票代码），
型号指定型号。

兼容字符串应仅包含小写字母、数字和破折号，并且应以字母开头。 单个逗号通常仅在
供应商前缀之后使用。 不应使用下划线。

一般驱动程序文件都会有一个OF匹配表（属于驱动），此OF匹配表保存着一些compatible值，
如果设备节点的compatible属性值和OF匹配表中的任何一个值相等，那么就表示设备可以使用
这个驱动。

设备节点的compatible属性值是为了匹配Linux内核中的驱动程序，而根节点的compatible
属性可以知道我们所使用的设备，一般第一个值描述了所使用的硬件设备名字，第二个值
描述了设备所使用的soc，Linux内核会通过根节点的compatible属性查看是否支持此设备，
如果支持，则设备会启动Linux内核，例如：`compatible = "fsl,imx6ull-14x14-evk","fsl,imx6ull";`

**举例**：
```dts
compatible = "fsl,mpc8641", "ns16550";
```
在此示例中，操作系统将首先尝试查找支持 fsl、mpc8641 的设备驱动程序。 如果未找到
驱动程序，它将尝试查找支持更通用的 ns16550 设备类型的驱动程序。

**举例**：
```c
led {
    compatible = “A”, “B”, “C”;
};
```
内核启动时，就会为这个LED按这样的优先顺序为它找到驱动程序：A、B、C。

#### model

**属性名**：model

**值类型**：`<string>`

**描述**：

model 属性值是一个 `<string>`，指定设备制造商的型号。

推荐的格式是：“制造商，型号”，其中制造商是描述制造商名称的字符串（例如股票代码），
型号指定型号。

model属性与compatible属性有些类似，但是有差别。
* compatible属性是一个字符串列表，表示可以你的硬件兼容A、B、C等驱动
* model用来准确地定义这个硬件是什么

**举例**：

```dts
model = "fsl,MPC8349EMITX";
```

**举例**：
```dts
/ {
    compatible = "samsung,smdk2440", "samsung,mini2440";
    model = "jz2440_v3";
};
```
它表示这个单板，可以兼容内核中的“smdk2440”，也兼容“mini2440”。从compatible属性中
可以知道它兼容哪些板，但是它到底是什么板？用model属性来明确。



#### phandle

**属性名**：phandle

**值类型**：`<u32>`

**描述**：

phandle 属性指定设备树中唯一的节点的数字标识符。 phandle 属性值由需要引用与该
属性关联的节点的其他节点使用。

**举例**：

请参阅以下设备树摘录：
```dts
pic@10000000 {
    phandle = <1>;
    interrupt-controller;
    reg = <0x10000000 0x100>;
};
```
phandle 值被定义为 1。 另一个设备节点可以引用 phandle 值为 1 的 pic 节点：
```dts
another-device-node {
    interrupt-parent = <1>;
};
```

```
注意：可能会遇到旧版本的设备树，其中包含此属性的已弃用形式，称为 linux、phandle。
为了兼容性，如果 phandle 属性不存在，客户端程序可能希望支持 linux,phandle。 这
两个属性的含义和用途是相同的。
```
```
注意：DTS 中的大多数设备树（参见附录 A）将不包含显式的 phandle 属性。 当 DTS 编译
为二进制 DTB 格式时，DTC 工具会自动插入 phandle 属性。
```


#### status

**属性名**：status

**值类型**：`<string>`

**描述**：

status 属性指示设备的操作状态。 缺少状态属性应被视为该属性存在且具有“okay”值。
下表列出并定义了有效值。

| value | description |
|--|--|
| "okay" | 表明设备正在运行，是可操作的 |
| "disabled" | 表示设备当前无法运行，是不可操作的，但将来可能会运行（例如，某些设备未插入或关闭）,有关给定设备的禁用含义的详细信息，请参阅设备绑定 |
| "reserved" | 表示设备可以运行，但不应使用。 通常，这用于由另一个软件组件（例如平台固件）控制的设备 |
| "fail" | 表明设备无法运行。 设备中检测到严重错误，如果不修复则不太可能运行 |
| "fail-sss" | 表明设备无法运行。 设备中检测到严重错误，如果不进行修复，则不太可能运行。 该值的 sss 部分特定于设备并指示检测到的错误情况。 |


#### `#address-cells and #size-cells`

**属性名**：`#address-cells and #size-cells`

**值类型**：`<u32>`

**描述**：

`#address-cells` 和 `#size-cells` 属性可用于在设备树层次结构中具有子设备的任何
设备节点，并描述应如何对子设备节点进行寻址。 `#address-cells` 属性定义用于对
子节点的 reg 属性中的地址字段的 `<u32>` 单元格的数量（这里应该是为了兼容需要64位
地址的系统准备的，如果地址是64位的，这里值应该为2，32位的地址，这里的值为1）。
`#size-cells` 属性定义用于对子节点的 reg 属性中的寄存器容量/大小字段的 `<u32>`
单元格的数量。

简言之：
* address-cells：address要用多少个32位数来表示； 
* size-cells：size要用多少个32位数来表示。 

`#address-cells` 和 `#size-cells` 属性不是从设备树中的祖先继承的。

符合 DTSpec 的引导程序应在所有具有子节点的节点上提供 `#address-cells` 和 `#size-cells`。
如果丢失，客户端程序应假定 `#address-cells` 的默认值为 2，`#sizecells` 的默认值为 1。

**举例**：

请参阅以下设备树摘录：
```dts
soc {
    #address-cells = <1>;
    #size-cells = <1>;
    serial@4600 {
        compatible = "ns16550";
        reg = <0x4600 0x100>;
        clock-frequency = <0>;
        interrupts = <0xA 0x8>;
        interrupt-parent = <&ipic>;
    };
};
```
在本例中，soc 节点的 `#address-cells` 和 `#size-cells` 属性均设置为 1。
该节点的子节点需要一个单元格来表示地址、一个单元格来表示节点的大小。

串行设备 reg 属性必须遵循父 (soc) 节点中设置的此规范 — 地址由单个单元格(0x4600)
表示，大小由单个单元格 (0x100) 表示。


**举例**：
```dts
/ {
address-cells = <1>;
size-cells = <1>;
    memory {
        reg = <0x80000000 0x20000000>;
    };
};
```
上例中，address-cells为1，所以reg中用1个数来表示地址，即用0x80000000来表示地址；
size-cells为1，所以reg中用1个数来表示大小，即用0x20000000表示大小：


#### reg

**属性名**：reg

**值类型**：`<prop-encoded-array>` 编码为任意数量的（address,length）对。

**描述**：

reg的本意是register，用来表示寄存器地址。但是在设备树里，它可以用来描述一段空间。

reg 属性描述了设备资源在其父总线定义的地址空间内的地址。 最常见的是，这意味着
内存映射 IO 寄存器块的偏移量和长度，但在某些总线类型上可能有不同的含义。 根节点
定义的地址空间中的地址是CPU真实地址。

该值是一个`<prop-encoded-array>`，由任意数量的地址和长度对组成，`<address length>`。
指定地址和长度所需的`<u32>`单元数是特定于总线的，并由设备节点父节点中的`#address-cells`
和`#size-cells`属性指定。如果父节点为`#size-cells`指定值0，则reg值中的长度字段将被省略。

用多少个32位的数来表示address和size，由其父节点的`#address-cells、#size-cells`决定。


**举例**：
```dts
/dts-v1/;
/ {
    # address-cells = <1>;
    # size-cells = <1>;
    memory {
        reg = <0x80000000 0x20000000>;
    };
};
```

**举例**：

假设片上系统内的设备有两个寄存器块，一个是 SOC 中偏移量 0x3000 处的 32 字节块，
另一个是偏移量 0xFE00 处的 256 字节块。 reg 属性将编码如下（假设`#address-cells`
和`#size-cells`值为 1）：
```dts
reg = <0x3000 0x20 0xFE00 0x100>;
```


#### virtual-reg

**属性名**：virtual-reg

**值类型**：`<u32>`

**描述**：

virtual-reg 属性指定映射到设备节点的 reg 属性中指定的第一个物理地址的有效地址。
此属性使引导程序能够向客户端程序提供已映射到物理地址的虚拟地址。


#### ranges

**属性名**：ranges

**值类型**：`<empty>` or `<prop-encoded-array>` 编码为任意数量的(child-bus-address,
parent-bus-address,length)三元组。

**描述**：

range 属性提供了一种定义总线地址空间（子地址空间）和总线节点父节点地址空间
（父地址空间）之间映射或转换的方法。

range 属性值的格式是任意数量的三元组(child-bus-address,parent-bus-address,length)

* child-bus-address是子总线地址空间内的物理地址。表示地址的单元数量取决于总线，并且
由父节点的`#address-cells`确定此物理地址所占用的子长。
* parent-bus-address是父总线地址空间内的物理地址。表示父地址的单元数量取决于总线，
同样由父节点的`#address-cells`确定此物理地址所占用的子长。
* 长度指定子地址空间中范围的大小。 由父节点的`#size-cells`确定此地址长度所占用的
字长。

如果使用`<empty>`值定义该属性，则它指定父地址空间和子地址空间相同，并且不需要地址转换。

如果总线节点中不存在该属性，则假定该节点的子节点与父地址空间之间不存在映射。

**地址转换示例**：

```dts
soc {
    compatible = "simple-bus";
    #address-cells = <1>;
    #size-cells = <1>;
    ranges = <0x0 0xe0000000 0x00100000>;

    serial@4600 {
        device_type = "serial";
        compatible = "ns16550";
        reg = <0x4600 0x100>;
        clock-frequency = <0>;
        interrupts = <0xA 0x8>;
        interrupt-parent = <&ipic>;
    };
};
```

soc 节点指定了 range 属性
```dts
<0x0 0xe0000000 0x00100000>;
```
此属性值指定对于 1024 KB 的地址空间范围，位于物理 0x0 的子节点映射到物理 0xe0000000
的父地址。 通过此映射，串行设备节点可以通过地址 0xe0004600 处的加载或存储、0x4600
的偏移量（在 reg 中指定）加上范围中指定的 0xe0000000 映射来寻址。


#### dma-ranges

**属性名**：dma-ranges

**值类型**：`<empty>` or `<prop-encoded-array>` 编码为任意数量的（子总线地址、
父总线地址、长度）三元组。

**描述**：

dma-ranges 属性用于描述内存映射总线的直接内存访问 (DMA) 结构，可以通过源自总线
的 DMA 操作来访问其设备树父级。 它提供了一种定义总线的物理地址空间和总线的父级
的物理地址空间之间的映射或转换的方法。

dma-ranges 属性值的格式是任意数量的三元组（子总线地址、父总线地址、长度）。指定的
每个三元组描述一个连续的 DMA 地址范围。

* 子总线地址是子总线地址空间内的物理地址。 表示地址的单元数取决于总线，并且可以
根据该节点（出现 dma-ranges 属性的节点）的#address-cells 确定。
* 父总线地址是父总线地址空间内的物理地址。 表示父地址的单元数量取决于总线，并且
可以根据定义父地址空间的节点的 #address-cells 属性来确定。
* 长度指定子地址空间中范围的大小。 表示大小的单元格数量可以根据该节点（出现
dma-ranges 属性的节点）的#size-cells 确定。


#### dma-noncoherent

**属性名**：dma-noncoherent

**值类型**：`<empty>`

**描述**：

对于默认 I/O 一致的架构，dma-noncoherent 属性用于指示设备不能够进行一致 DMA 操作。
某些架构默认具有非相干 DMA，并且此属性不适用。


#### name (deprecated)

**属性名**：name

**值类型**：`<string>`

**描述**：

name 属性是一个指定节点名称的字符串。 该属性已被弃用，并且不建议使用。 但是，
它可能用于较旧的不符合 DTSpec 的设备树。 操作系统应根据节点名称的节点名称部分
确定节点名称（请参见第 2.2.1 节）。

在跟platform_driver匹配时，name优先级最低，compatible优先级最高。

#### device_type (deprecated)

**属性名**：device_type

**值类型**：`<string>`

**描述**：

device_type 属性在 IEEE 1275 中用于描述设备的 FCode 编程模型。 由于 DTSpec 没有
FCode，因此不推荐使用该属性，并且应仅将其包含在 cpu 和内存节点上，以便与 IEEE 1275
派生的设备树兼容。

用来表示节点的类型。在跟platform_driver匹配时，优先级为中。compatible属性在匹配
过程中，优先级最高。过时了，建议不用。

### 中断和中断映射

DTSpec 采用开放固件推荐实践：中断映射，版本 0.9 [b7] 中规定的中断树模型来表示中断。
在设备树中存在一个逻辑中断树，它表示平台硬件中中断的层次结构和路由。 虽然通常称为
中断树，但从技术上讲，它是有向无环图。

中断源到中断控制器的物理接线在设备树中用中断父属性表示。 表示中断生成设备的节点
包含一个中断父属性，该属性具有一个指向设备中断路由到的设备的 phandle 值，通常是
中断控制器。 如果中断生成设备没有中断父属性，则假定其中断父设备是其设备树父设备。

每个中断生成设备都包含一个中断属性，该属性具有描述该设备的一个或多个中断源的值。
每个源都用称为中断说明符的信息表示。 <font color='red'>中断说明符的格式和含义是
中断域特定的，即，它取决于其中断域根节点的属性。 中断域的根使用`#interrupt-cells`
属性来定义对中断说明符进行编码所需的`<u32>`值的数量</font>。 例如，对于 Open PIC
中断控制器，中断说明符采用两个 32 位值，并由中断号和中断的级别/感知信息组成。

中断域是解释中断说明符的上下文。 域的根是 (1) 中断控制器或 (2) 中断关系。

1. 中断控制器是一个物理设备，需要一个驱动程序来处理通过它路由的中断。 它还可能
级联到另一个中断域。 中断控制器由设备树中该节点上是否存在中断控制器属性来指定。

2. 中断关系定义一个中断域与另一个中断域之间的转换。 该转换基于特定于域和特定于
总线的信息。 域之间的转换是通过中断映射属性来执行的。 例如，PCI 控制器设备节点
可以是一个中断连接，它定义从 PCI 中断命名空间（INTA、INTB 等）到具有中断请求
(IRQ) 编号的中断控制器的转换。

当中断树的遍历到达没有中断属性且因此没有显式中断父的中断控制器节点时，确定中断树
的根。

请参阅下图，了解带有中断父关系的设备树的图形表示示例。 它显示了设备树的自然结构
以及每个节点在逻辑中断树中的位置。

![](./device_tree.assets/3.jpg)

在上图所示的例子中：
* open-pic 中断控制器是中断树的根。
* 中断树根有三个子设备——将其中断直接路由到开放图片的设备
```
- 设备1
- PCI主机控制器
- GPIO 控制器
```
* 存在三个中断域； 一种植根于 open-pic 节点，一种植根于 PCI 主桥节点，一种植根于
GPIO 控制器节点。
* 有两个连接节点； 一个位于 PCI 主桥，一个位于 GPIO 控制器。


#### 中断生成设备的属性

**interrupts**

**属性名**：interrupts

**值类型**：`<prop-encoded-array>` 编码为任意数量的中断说明符

**描述**：

设备节点的中断属性定义了设备生成的一个或多个中断。 中断属性的值由任意数量的中断
说明符组成。 中断说明符的格式由中断域根的绑定定义。

中断被中断扩展属性覆盖，通常只应使用其中之一。

**举例**：

开放式 PIC 兼容中断域中中断说明符的常见定义由两个单元组成： 中断号和电平/感应信息。
请参见以下示例，该示例定义了单个中断说明符，中断号为 0xA，级别/感知编码为 8。

```dts
interrupts = <0xA 8>;
```

**interrupt-parent**

**属性名**：interrupt-parent

**值类型**：`<phandle>`

**描述**：

由于中断树中节点的层次结构可能与设备树不匹配，因此可以使用中断父属性来明确中断
父级的定义。 该值是中断父级的 phandle。 如果设备缺少此属性，则假定其中断父级是
其设备树父级。


**interrupts-extended**

**属性名**：interrupts-extended

**值类型**：`<phandle> <prop-encoded-array>`

**描述**：

中断扩展属性列出了设备生成的中断。 当设备连接到多个中断控制器时，应使用中断扩展
而不是中断，因为它使用每个中断说明符对父级 phandle 进行编码。

**举例**：

此示例显示具有两个中断输出并连接到两个单独的中断控制器的设备如何使用中断扩展
属性来描述连接。 pic 是 #interrupt-cells 说明符为 2 的中断控制器，而 gic 是
`#interrupts-cells`说明符为 1 的中断控制器。
```dts
interrupts-extended = <&pic 0xA 8>, <&gic 0xda>;
```
中断和中断扩展属性是互斥的。 设备节点应该使用其中之一，但不能同时使用两者。仅当
需要与不理解中断扩展的软件兼容时才允许使用两者。 如果中断扩展和中断都存在，则
中断扩展优先。


#### 中断控制器属性

##### **`#interrupt-cells`**

**属性名**：`#interrupt-cells`

**值类型**：`<u32>`

**描述**：

`#interrupt-cells`属性定义对中断域的中断说明符进行编码所需的单元数。

interrupt-cells 的具体含义由中断控制器的驱动程序或文档定义。不同的控制器具有不同
的解释。例如：

**ARM GIC 中断控制器**

GIC 是 ARM 平台中常用的中断控制器，它的 #interrupt-cells 通常为 3。举例：
```
interrupt-controller {
    compatible = "arm,cortex-a15-gic";
    #interrupt-cells = <3>;
};
```

设备节点示例：
```
device@0 {
    compatible = "vendor,device";
    interrupts = <1 17 4>;
};
```
解释：
1. 第一单元 (1)：中断类型
    * 0 表示 SPI（共享中断）
    * 1 表示 PPI（专用中断）
2. 第二单元 (17)：中断号
    * SPI 中断号偏移为 32，即实际中断号是 32 + 17 = 49，设备树里写的是相对偏移。
3. 第三单元 (4)：触发类型
    * 1 表示上升沿触发
    * 4 表示高电平触发

**GPIO 中断控制器**

GPIO 控制器的 #interrupt-cells 通常为 2。示例：
```
gpio-controller {
    compatible = "vendor,gpio-controller";
    #interrupt-cells = <2>;
};
```
设备节点示例：
```
device@1 {
    compatible = "vendor,device";
    interrupts = <5 8>;
};
```
解释：
1. 第一个单元 (5)：GPIO 引脚号。
2. 第二个单元 (8)：触发类型（边沿或电平）。



##### **interrupt-controller**

**属性名**：interrupt-controller

**值类型**：`<empty>`

**描述**：

中断控制器属性的存在将节点定义为中断控制器节点。


#### 中断 Nexus 属性

中断连接节点应具有`#interrupt-cells`属性。

**interrupt-map**

**属性名**：interrupt-map

**值类型**：`<prop-encoded-array>`编码为任意数量的中断映射条目。

**描述**：

中断映射是连接节点上的一个属性，它将一个中断域与一组父中断域桥接起来，并指定子域
中的中断说明符如何映射到其各自的父域。

中断映射是一个表，其中每行是一个映射条目，由五个部分组成：子单元地址、子中断说明
符、父中断、父单元地址、父中断说明符。

**子单位地址**

被映射的子节点的单元地址。 指定该值所需的 32 位单元数由子节点所在总线节点的 #address-cells 属性描述。

**子中断说明符**

被映射的子节点的中断说明符。 指定该组件所需的 32 位单元数由该节点（包含中断映射属性的连接节点）的 #interrupt-cells 属性描述。

**中断父级**

指向子域映射到的中断父级的单个 <phandle> 值。

**父单位地址**

中断父域中的单元地址。 指定该地址所需的 32 位单元数由中断父字段指向的节点的 #address-cells 属性描述。

**父中断说明符**

父域中的中断说明符。 指定该组件所需的 32 位单元数由中断父字段指向的节点的 #interrupt-cells 属性描述。

通过将单元地址/中断说明符对与中断映射中的子组件进行匹配，在中断映射表上执行查找。 由于单元中断说明符中的某些字段可能不相关，因此在完成查找之前应用掩码。 该掩码在中断映射掩码属性中定义（参见第 2.4.3 节）。

```
注意：子节点和中断父节点都需要定义#address-cells 和#interruptcells 属性。 如果
不需要单元地址组件，则#address-cells 应明确定义为零。
```

**interrupt-map-mask**

**属性名**：interrupt-map-mask

**值类型**：`<prop-encoded-array>` 编码为位掩码

**描述**：

为中断树中的关系节点指定中断映射掩码属性。 该属性指定一个掩码，该掩码与在中断映射
属性指定的表中查找的传入单元中断说明符进行“与”运算。


**`#interrupt-cells`**

**属性名**：`#interrupt-cells`

**值类型**：`<u32>`

**描述**：

`#interrupt-cells`属性定义对中断域的中断说明符进行编码所需的单元数。


#### 中断映射示例

下面显示了带有 PCI 总线控制器的设备树片段的表示以及用于描述两个 PCI 插槽
(IDSEL 0x11,0x12) 的中断路由的示例中断映射。 插槽 1 和 2 的 INTA、INTB、
INTC 和 INTD 引脚连接到 Open PIC 中断控制器。
```dts
soc {
    compatible = "simple-bus";
    #address-cells = <1>;
    #size-cells = <1>;

    open-pic {
        clock-frequency = <0>; interrupt-controller;
        #address-cells = <0>;
        #interrupt-cells = <2>;
    };

    pci {
        #interrupt-cells = <1>;
        #size-cells = <2>;
        #address-cells = <3>;
        interrupt-map-mask = <0xf800 0 0 7>;
        interrupt-map = <
            /* IDSEL 0x11 - PCI slot 1 */
            0x8800 0 0 1 &open-pic 2 1 /* INTA */
            0x8800 0 0 2 &open-pic 3 1 /* INTB */
            0x8800 0 0 3 &open-pic 4 1 /* INTC */
            0x8800 0 0 4 &open-pic 1 1 /* INTD */
            /* IDSEL 0x12 - PCI slot 2 */
            0x9000 0 0 1 &open-pic 3 1 /* INTA */
            0x9000 0 0 2 &open-pic 4 1 /* INTB */
            0x9000 0 0 3 &open-pic 1 1 /* INTC */
            0x9000 0 0 4 &open-pic 2 1 /* INTD */
        >;
    };
};
```
一个开放式 PIC 中断控制器被表示并被识别为具有中断控制器属性的中断控制器。

中断映射表中的每一行由五部分组成：子单元地址和中断说明符，它映射到具有指定父单元
地址和中断说明符的中断父节点。

例如，中断映射表的第一行指定插槽 1 的 INTA 的映射。该行的组件如下所示

```
子单元地址：0x8800 0 0
子中断说明符：1
父中断说明符：&open-pic
父单元地址：（因为 open-pic 节点中的#address-cells = <0>，所以为空）
父中断说明符：2 1

- 子单元地址为<0x8800 0 0>。 该值由三个 32 位单元编码，由 PCI 控制器的#address-cells
  属性值（值为 3）确定。 这三个单元代表 PCI 地址，如 PCI 总线绑定所描述。
  * 编码包括总线号（0x0 << 16）、设备号（0x11 << 11）和功能号（0x0 << 8）。

- 子中断说明符为<1>，它指定 PCI 绑定所描述的 INTA。 这需要 PCI 控制器的
  #interrupt-cells 属性（值为 1）指定的一个 32 位单元，它是子中断域。

- 中断父级由 phandle 指定，该 phandle 指向插槽的中断父级（Open PIC 中断控制器）。

- 父级没有单元地址，因为父级中断域（open-pic 节点）的 #address-cells 值为 <0>。

- 父中断说明符为<2 1>。 表示中断说明符的单元格数量（两个单元格）由中断父级
  （open-pic 节点）上的 #interrupt-cells 属性确定。
  * 值<2 1>是由Open PIC中断控制器的设备绑定指定的值（参见第4.5节）。 值<2>指定
    INTA所连接的中断控制器上的物理中断源编号。 值<1>指定级别/意义编码。
```

在此示例中，interrupt-map-mask 属性的值为`<0xf800 0 0 7>`。在中断映射表中执行查找
之前，将该掩码应用于子单元中断说明符。

要查找 IDSEL 0x12（插槽 2）、功能 0x3 的 INTB 的 open-pic 中断源编号，将执行
以下步骤：
* 子单元地址和中断说明符形成值`<0x9300 0 0 2>`。
  * 地址的编码包括总线号`(0x0 << 16)`、设备号`(0x12 << 11)`和功能号`(0x3 << 8)`。
  * 中断说明符为 2，这是根据 PCI 绑定的 INTB 编码。
* 应用中断映射掩码值`<0xf800 0 0 7>`，给出结果`<0x9000 0 0 2>`。
* 在中断映射表中查找该结果，该表映射到父中断说明符`<4 1>`。


### Nexus 节点和说明符映射

#### Nexus 节点属性

Nexus 节点应具有`#<specifier>-cells`属性，其中`<specifier>`是一些说明符空间，
例如“gpio”、“clock”、“reset”等。

**`<specifier>-map`**

**属性名**：`<specifier>-map`

**值类型**：`<prop-encoded-array>`编码为任意数量的说明符映射条目。

**描述**：

`<specifier>-map`是连接节点中的一个属性，它将一个说明符域与一组父说明符域桥接
起来，并描述子域中的说明符如何映射到其各自的父域。

映射是一个表，其中每行都是一个映射条目，由三个组件组成：子说明符、父说明符和父说明符。

**子指定符**
被映射的子节点的说明符。指定此组件所需的32位单元数由此节点的`#<specifier>-cells`
属性描述 - 包含`<specifier>-map`属性的连接节点。

**说明符父级**
指向子域映射到的说明符父级的单个`<phandle>`值。

**父说明符**
父域中的说明符。 指定该组件所需的 32 位单元数由说明符父节点的`#<specifier>-cells`
属性描述。

通过将说明符与映射中的子说明符进行匹配来对映射表执行查找。 由于说明符中的某些字段
可能不相关或需要修改，因此在完成查找之前应用掩码。 该掩码在`<specifier>-map-mask`
属性中定义（参见第 2.5.1 节）。

类似地，当映射说明符时，单元说明符中的某些字段可能需要保持不变并从子节点传递到父
节点。 在这种情况下，可以指定`<specifier>-map-pass-thru`属性（请参阅第 2.5.1 节）
以将掩码应用于子说明符并复制与父单位说明符匹配的任何位。


**`<specifier>-map-mask`**

**属性名**：`<specifier>-map-mask`

**值类型**：`<<prop-encoded-array>`编码为位掩码

**描述**：

可以为关系节点指定`<specifier>-map-mask`属性。 此属性指定一个掩码，该掩码与在
`<specifier>-map`属性指定的表中查找的子单元说明符进行 AND 运算。 如果未指定此
属性，则假定掩码是所有位均已设置的掩码。


**`<specifier>-map-pass-thru`**

**属性名**：`<specifier>-map-pass-thru`

**值类型**：`<prop-encoded-array>` 编码为位掩码

**描述**：

可以为关系节点指定`<specifier>-map-pass-thru`属性。 此属性指定一个掩码，该掩码
应用于在`<specifier>-map`属性指定的表中查找的子单元说明符。 子单元说明符中的任何
匹配位都会复制到父单元说明符。 如果未指定此属性，则假定掩码是未设置位的掩码。


**`#<specifier>-cells`**

**属性名**：`#<specifier>-cells`

**值类型**：`<u32>`

**描述**：

`#<specifier>-cells`属性定义对域的说明符进行编码所需的单元格数量。


#### 说明符映射示例

下面显示了带有两个 GPIO 控制器的设备树片段的表示以及一个示例说明符映射，用于描述
两个控制器上的一些 GPIO 通过板上连接器到设备的 GPIO 路由。 扩展设备节点位于连接器
节点的一侧，带有两个 GPIO 控制器的 SoC 位于连接器的另一侧。

```dts
soc {
    soc_gpio1: gpio-controller1 {
        #gpio-cells = <2>;
    };

    soc_gpio2: gpio-controller2 {
        #gpio-cells = <2>;
    };
};

connector: connector {
    #gpio-cells = <2>;
    gpio-map = <0 0 &soc_gpio1 1 0>,
               <1 0 &soc_gpio2 4 0>,
               <2 0 &soc_gpio1 3 0>,
               <3 0 &soc_gpio2 2 0>;
    gpio-map-mask = <0xf 0x0>;
    gpio-map-pass-thru = <0x0 0x1>;
};

expansion_device {
    reset-gpios = <&connector 2 GPIO_ACTIVE_LOW>;
};
```
gpio-map 表中的每一行由三部分组成：子单元说明符，它通过父说明符映射到
`gpio-controller`节点。

例如，说明符映射表的第一行指定连接器的 GPIO 0 的映射。 该行的组件如下所示
```
子说明符：0 0 说明符父说明符：&soc_gpio1 父说明符：1 0

- 子说明符为 <0 0>，它在标志字段为 0 的连接器中指定 GPIO 0。这需要由连接器节点的
  #gpio-cells 属性指定的两个 32 位单元，该节点是子说明符 领域。

- 说明符父级由 phandle 指定，该 phandle 指向连接器（SoC 中的第一个 GPIO 控制器）
  的说明符父级。

- 父说明符是<1 0>。 表示 gpio 说明符的单元格数量（两个单元格）由说明符父级
  soc_gpio1 节点上的 #gpio-cells 属性确定。
  * 值<1 0>是由GPIO控制器的设备绑定指定的值。 值 <1> 指定连接器上的 GPIO 0 连接到
    的 GPIO 控制器上的 GPIO 引脚编号。 值<0>指定标志（低电平有效、高电平有效等）。
```

在此示例中，gpio-map-mask 属性的值为`<0xf 0>`。在 GPIO 映射表中执行查找之前，
将此掩码应用于子单元说明符。 同样，gpio-map-pass-thru 属性的值为`<0x0 0x1>`。
当将子单位说明符映射到父单位说明符时，此掩码将应用于子单位说明符。 此掩码中
设置的任何位都会从父单元说明符中清除，并从子单元说明符复制到父单元说明符。

要从扩展设备的 Reset-gpios 属性查找 GPIO 2 的连接器说明符源编号，需要执行以下步骤：
```
• 子说明符形成值<2 GPIO_ACTIVE_LOW>。
  – 说明符根据 GPIO 绑定使用低电平有效标志对 GPIO 2 进行编码。
• gpio-map-mask 值<0xf 0x0> 与子说明符进行AND 运算，得到结果<0x2 0>。
• 在gpio-map 表中查找结果，该表映射到父说明符<3 0> 和&soc_gpio1 phandle。
• gpio-map-pass-thru 值<0x0 0x1> 被反转并与gpiomap 表中找到的父说明符进行AND 运算，
  结果为<3 0>。 子说明符与 gpio-map-pass-thru 掩码进行 AND 运算，形成 <0 GPIO_ACTIVE_LOW>，
  然后与清除的父说明符 <3 0> 进行 OR 运算，得到 <3 GPIO_ACTIVE_LOW>。
• 说明符<3 GPIO_ACTIVE_LOW> 附加到映射的phandle &soc_gpio1，结果是<&soc_gpio1 3 GPIO_ACTIVE_LOW>。
```

## 设备节点要求

### 基本设备节点类型

以下部分指定了符合 DTSpec 的设备树中所需的基本设备节点集的要求。

所有设备树应有一个根节点，并且以下节点应出现在所有设备树的根处：
* 1 个/cpus 节点
* 至少一个/内存节点

### 根节点

设备树有一个根节点，所有其他设备节点都是该根节点的后代。 根节点的完整路径是`/`。

| 属性名 | 使用 | 值类型 | 描述 |
|--|--|--|--|
| `#address-cells` | R  | `<u32>`        | 指定`<u32>`单元格的数量来表示 root 子级中 reg 属性中的地址。 |
| `#size-cells`    | R  | `<u32>`        | 指定`<u32>`单元格的数量，以表示 root 子级中 reg 属性中的大小。 |
| `model`          | R  | `<string>`     | 指定唯一标识系统板型号的字符串。 推荐的格式是“制造商，型号”。 |
| `compatible`     | R  | `<stringlist>` | 指定与该平台兼容的平台体系结构列表。 操作系统可以使用此属性来选择特定于平台的代码。 属性值的推荐形式为：“制造商,型号” 例如：`compatible = "fsl,mpc8572ds"` |
| `serial-number`  | O  | `<string>`     | 指定表示设备序列号的字符串。 |
| `chassis-type`   | OR | `<string>`     | 指定标识系统外形规格的字符串。 属性值可以是以下之一："desktop"、"laptop"、"convertible"、"server"、"tablet"、"handset"、"watch"、"embedded" |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义
```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```


### `/aliases` 节点

设备树可能有一个定义一个或多个别名属性的别名节点 (/aliases)。 别名节点应位于设备树
的根部，并具有节点名称`/aliases`。

`/aliases`节点的每个属性都定义一个别名。 属性名称指定别名。 该属性值指定设备树中
节点的完整路径。
例如，属性`serial0 =“/simple-bus@fe000000/serial@llc500”`定义别名serial0。

别名应为以下字符集中 1 到 31 个字符的小写文本字符串。

别名的有效字符如下表所示：
| 特点 | 描述 |
|--|--|
| 0-9 | 数字 |
| a-z | 小写字母 |
| - | 短划线 |

别名值是设备路径并被编码为字符串。 该值表示节点的完整路径，但该路径不需要引用叶节点。

客户端程序可以使用别名属性名称来引用完整设备路径作为其字符串值的全部或部分。
客户端程序在将字符串视为设备路径时，应检测并使用别名。

举例
```
aliases {
    serial0 = "/simple-bus@fe000000/serial@llc500";
    ethernet0 = "/simple-bus@fe000000/ethernet@31c000";
};
```

给定别名serial0，客户端程序可以查看`/aliases`节点并确定别名引用设备路径
`/simple-bus@fe000000/serial@llc500`。


### `/memory` 节点

所有设备树都需要一个内存设备节点，它描述了系统的物理内存布局。 如果系统有多个内存
范围，则可以创建多个内存节点，或者可以在单个内存节点的 reg 属性中指定范围。

节点名称的单元名称部分（参见第 2.2.1 节）应为内存。

客户端程序可以使用它选择的任何存储属性来访问任何内存预留（参见第 5.3 节）未涵盖
的内存。 然而，在更改用于访问真实页面的存储属性之前，客户端程序负责执行架构和
实现所需的操作，可能包括从缓存中刷新真实页面。 引导程序负责确保，在不采取任何
与存储属性更改相关的操作的情况下，客户端程序可以安全地访问所有内存（包括内存预留
覆盖的内存），如 WIMG = 0b001x。 那是：
* 不直写 必需
* 不禁止缓存
* 记忆一致性
* 需要不受保护或受保护

如果支持 VLE 存储属性，则 VLE=0。

`/memory` 节点属性如下：

| 属性名 | 使用 | 值类型 | 描述 |
|--|--|--|--|
| `device_type`         | R | `<string>`             | 值应为“memory” |
| `reg`                 | R | `<prop-encoded-array>` | 由任意数量的地址和大小对组成，这些地址和大小对指定内存范围的物理地址和大小。 |
| `initial-mapped-area` | O | `<prop-encoded-array>` | 指定初始映射区域的地址和大小是一个由三元组（有效地址、物理地址、大小）组成的属性编码数组。 有效地址和物理地址均应为 64 位（`<u64>` 值），大小应为 32 位（`<u32>`值）。 |
| `hotpluggable`        | O | `<empty>`              | 向操作系统指定一个显式提示，表明此内存稍后可能会被删除。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

芯片厂家不可能事先确定你的板子使用多大的内存，所以 memory 节点需要板厂设置，比如：

```c
memory {
    reg = <0x80000000 0x20000000>;
};
```

#### `/memory` 节点 和 UEFI

通过 [UEFI] 引导时，系统内存映射通过 [UEFI] § 7.2 中定义的 GetMemoryMap() UEFI 
引导时间服务获取，如果存在，操作系统必须忽略任何 /memory 节点。

#### `/memory` 举例

给定一个具有以下物理内存布局的 64 位 Power 系统：
* RAM：起始地址0x0，长度0x80000000（2 GB）
* RAM：起始地址0x100000000，长度0x100000000（4 GB）

内存节点可以定义如下，假设`#address-cells = <2>`和`#size-cells = <2>`。

例1：
```dts
memory@0 {
    device_type = "memory";
    reg = <0x000000000 0x00000000 0x00000000 0x80000000
           0x000000001 0x00000000 0x00000001 0x00000000>;
};
```

例2：
```dts
memory@0 {
    device_type = "memory";
    reg = <0x000000000 0x00000000 0x00000000 0x80000000>;
};
memory@100000000 {
    device_type = "memory";
    reg = <0x000000001 0x00000000 0x00000001 0x00000000>;
};
```

reg 属性用于定义两个内存范围的地址和大小。 2 GB I/O 区域被跳过。 请注意，根节点
的`#address-cells`和`#size-cells`属性指定值2，这意味着需要两个32 位单元来定义内存
节点的reg 属性的地址和长度。


### `/reserved-memory` 节点

保留内存被指定为 /reserved-memory 节点下的节点。 操作系统应将保留内存排除在正常
使用之外。 人们可以创建描述特定保留（从正常使用中排除）内存区域的子节点。 这些
内存区域通常是为各种设备驱动程序的特殊用途而设计的。

每个内存区域的参数可以通过以下节点编码到设备树中：

#### `/reserved-memory` 父节点

`/reserved-memory` 父节点属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|--|
| `#address-cells` | R | `<u32>`                | 指定`<u32>`单元格的数量来表示 root 子级中 reg 属性中的地址。 |
| `#size-cells`    | R | `<u32>`                | 指定`<u32>`单元格的数量，以表示 root 子级中 reg 属性中的大小。 |
| `ranges`         | R | `<prop encoded array>` | 该属性表示父地址到子地址空间之间的映射（请参见第 2.3.8 节，范围）。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

`#address-cells`和`#size-cells`应使用与根节点相同的值，并且范围应为空，以便地址
转换逻辑正常工作。

#### `/reserved-memory/` 子节点

保留内存节点的每个子节点指定一个或多个保留内存区域。 每个子节点可以使用 reg 属性
来指定保留内存的特定范围，或者使用带有可选约束的 size 属性来请求动态分配的内存块。

按照通用名称推荐的做法，节点名称应反映节点的用途（即“framebuffer”或“dma-pool”）。
如果节点是静态分配，则应将单元地址 (@<地址>) 附加到名称后。

保留的内存节点需要静态分配的 reg 属性或动态分配的大小属性。 动态分配可以使用对齐
和分配范围属性来限制内存的分配位置。 如果 reg 和 size 都存在，则该区域将被视为静态
分配，其中 reg 属性优先，而 size 被忽略。

`/reserved-memory/` 子节点属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| reg          | O | `<prop-encoded-array>` | 由任意数量的地址和大小对组成，这些地址和大小对指定内存范围的物理地址和大小。 |
| size         | O | `<prop-encoded-array>` | 为动态分配区域保留的内存大小（以字节为单位）。 该属性的大小基于父节点的`#size-cells`属性。 |
| alignment    | O | `<prop-encoded-array>` | 分配对齐的地址边界。 该属性的大小基于父节点的`#size-cells`属性。 |
| alloc-ranges | O | `<prop-encoded-array>` | 指定可接受分配的内存区域。 格式是（地址，长度对）元组，格式与 reg 属性相同。 |
| compatible   | O | `<stringlist>`         | 可能包含以下字符串：1. 共享 DMA 池：这表示一个内存区域，旨在用作一组设备的 DMA 缓冲区共享池。 如有必要，操作系统可以使用它来实例化必要的池管理子系统。2. 供应商特定字符串，格式为 `<vendor>,[<device>-]<usage>` |
| no-map       | O | `<empty>`              | 如果存在，则表示操作系统不得创建该区域的虚拟映射作为其系统内存标准映射的一部分，也不允许在除使用该区域的设备驱动程序控制之外的任何情况下对其进行推测访问。 |
| reusable     | O | `<empty>`              | 操作系统可以使用该区域中的内存，但拥有该区域的设备驱动程序需要能够将其回收。 通常，这意味着操作系统可以使用该区域来存储易失性或缓存的数据，这些数据可以在其他地方重新生成或迁移。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

no-map 和可重用属性是互斥的，并且不能在同一节点中同时使用两者。

Linux实现注意事项：
* 如果存在linux,cma-default 属性，则Linux 将使用该区域作为连续内存分配器的默认池。
* 如果存在linux,dma-default 属性，则Linux 将使用该区域作为一致DMA 分配器的默认池。


#### 设备节点对保留内存的引用

通过向设备节点添加内存区域属性，/reserved-memory 节点中的区域可以被其他设备节点引用。

用于引用保留内存区域的属性:

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| memory-region       | O | `<prop-encoded-array>` | phandle，与 / 保留内存的子代的说明符对 |
| memory-region-names | O | `<stringlist>`         | 名称列表，一个名称对应于内存区域属性中的每个对应条目 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义


#### `/reserved-memory` 和 uefi

通过 [UEFI] 引导时，静态/保留内存区域还必须列在通过 GetMemoryMap() UEFI 引导时间
服务获取的系统内存映射中，如 [UEFI] § 7.2 中定义。 保留的内存区域需要包含在UEFI
内存映射中，以防止 UEFI 应用程序进行分配。

具有 no-map 属性的保留区域必须在类型为 EfiReservedMemoryType 的内存映射中列出。
所有其他保留区域必须以 EfiBootServicesData 类型列出。

动态保留内存区域不得在 [UEFI] 内存映射中列出，因为它们是在退出固件引导服务后由
操作系统分配的。


#### `/reserved-memory`举例

此示例定义了 3 个为 Linux 内核定义的连续区域：一个是所有设备驱动程序的默认区域
（名为 linux、cma，大小为 64MiB），一个专用于帧缓冲设备（名为framebuffer@78000000，
8MiB），一个用于多媒体处理（ 命名为多媒体@77000000，64MiB）。

```dts
/ {
    #address-cells = <1>;
    #size-cells = <1>;

    memory {
        reg = <0x40000000 0x40000000>;
    };

    reserved-memory {
        #address-cells = <1>;
        #size-cells = <1>;
        ranges;

        /* global autoconfigured region for contiguous allocations */
        linux,cma {
            compatible = "shared-dma-pool";
            reusable;
            size = <0x4000000>;
            alignment = <0x2000>;
            linux,cma-default;
        };

        display_reserved: framebuffer@78000000 {
            reg = <0x78000000 0x800000>;
        };

        multimedia_reserved: multimedia@77000000 {
            compatible = "acme,multimedia-memory";
            reg = <0x77000000 0x4000000>;
        };
    };

    /* ... */

    fb0: video@12300000 {
        memory-region = <&display_reserved>;
        /* ... */
    };

    scaler: scaler@12500000 {
        memory-region = <&multimedia_reserved>;
        /* ... */
    };

    codec: codec@12600000 {
        memory-region = <&multimedia_reserved>;
        /* ... */
    };
};
```

### `/chosen` Node

/chosen 节点并不代表系统中的真实设备，而是描述系统固件在运行时选择或指定的参数。
它应该是根节点的子节点。

chosen节点主要是为了Uboot向Linux内核传递数据，重点是bootargs参数。一般.dts文件中
的chosen节点通常为空或者内容很少。

`/chosen`节点属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| bootargs    | O | `<string>` | 指定客户端程序启动参数的字符串。 如果不需要启动参数，该值可能是空字符串。 |
| stdout-path | O | `<string>` | 一个字符串，指定代表要用于启动控制台输出的设备的节点的完整路径。 如果值中存在字符“:”，则会终止路径。 该值可以是别名。 如果未指定 stdin-path 属性，则应假定 stdout-path 定义输入设备。 |
| stdin-path  | O | `<string>` | 一个字符串，指定代表要用于启动控制台输入的设备的节点的完整路径。 如果值中存在字符“:”，则会终止路径。 该值可以是别名。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

举例:
```dts
chosen {
    bootargs = "root=/dev/nfs rw nfsroot=192.168.1.1 console=ttyS0,115200";
};
```
这些参数将作为启动参数传递给内核

可能会遇到旧版本的设备树，其中包含已弃用的stdout-path属性形式，称为linux,stdout-path。
为了兼容性，如果 stdout-path 属性不存在，客户端程序可能希望支持 linux,stdout-path。
这两个属性的含义和用途是相同的。


uboot对设备树chosen节点的处理
1. uboot会调用fdt_find_or_add_subnode()从设备树中找chosen节点，如果没有找到就自己创建
2. 读取uboot中bootargs环境变量的内容
3. 调用函数fdt_setprop向chosen节点添加bootargs属性，并且bootargs属性的值就是环境变量
bootargs的内容

uboot处理流程
```c
bootz命令
|
do_bootz()
    --> do_bootm_states()
        --> boot_selected_os()
```


### `/cpus` 节点属性

所有设备树都需要 /cpus 节点。 它并不代表系统中的真实设备，而是充当代表系统CPU
的子 cpu 节点的容器。

`/cpus` 节点属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| `#address-cells` | R | `<u32>` | 该值指定 reg 属性数组的每个元素在此节点的子节点中占据多少个单元格。 |
| `#size-cells`    | R | `<u32>` | 值应为 0。指定此节点的子节点中的 reg 属性不需要大小。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

/cpus 节点可能包含跨 cpu 节点通用的属性。 详细信息请参见第 3.8 节。

有关示例，请参见第 3.8.4 节。


### `/cpus/cpu*` 节点属性

cpu 节点代表一个足够独立的硬件执行块，能够运行操作系统，而不会干扰可能运行其他
操作系统的其他 CPU。

共享 MMU 的硬件线程通常在一个 cpu 节点下表示。 如果设计了其他更复杂的 CPU 拓扑，
则 CPU 的绑定必须描述该拓扑（例如，不共享 MMU 的线程）。

CPU 和线程通过统一的数字空间进行编号，该数字空间应尽可能匹配中断控制器的 CPU/
线程编号。

跨 cpu 节点具有相同值的属性可以放置在 /cpus 节点中。 客户端程序必须首先检查特定的
cpu 节点，但如果未找到预期的属性，则它应该查看父 /cpus 节点。 这会导致所有 CPU 中
相同的属性的更简洁的表示。

每个 CPU 节点的节点名称应为 cpu。

**举例：**
```dts
cpus {
    # address-cells = <1>;
    # size-cells = <0>;
    cpu0: cpu@0 {
        .......
    }
};
```


#### `/cpus/cpu*` 节点的常规属性

下表描述了 cpu 节点的一般属性。 表 3.9 中描述的一些属性是具有特定适用细节的精选
标准属性。

`/cpus/cpu*` 节点常规属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| device_type        | R | `<string>` | 值应为“cpu”。 |
| reg                | R | `array` | reg 的值是一个 `<prop-encoded-array>`，它为 CPU 节点表示的 CPU/线程定义唯一的 CPU/线程 id。如果 CPU 支持多个线程（即多个执行流），则 reg 属性是一个数组，每个线程有 1 个元素。 /cpus 节点上的#address-cells 指定数组的每个元素占用多少个单元。 软件可以通过将 reg 的大小除以父节点的 #address-cells 来确定线程数。如果 CPU/线程可以是外部中断的目标，则 reg 属性值必须是中断控制器可寻址的唯一 CPU/线程 ID。如果CPU/线程不能成为外部中断的目标，则reg必须是唯一的并且超出中断控制器寻址范围的范围如果CPU/线程的PIR（挂起中断寄存器）是可修改的，则客户端程序应该修改 PIR 以匹配 reg 属性值。 如果无法修改 PIR 并且 PIR 值与中断控制器编号空间不同，则 CPU 绑定可以根据需要定义 PIR 值的绑定特定表示。 |
| clock-frequency    | O | `array` | 指定 CPU 的时钟速度（以赫兹为单位）（如果该时钟速度恒定）。 该值是以下两种形式之一的`<prop-encoded-array>`：1. 一个32 位整数，由一个指定频率的`<u32>`组成。2. 表示为`<u64>`的64 位整数，指定频率。 |
| timebase-frequency | O | `array` | 指定更新时基和减量寄存器的当前频率（以赫兹为单位）。 该值是以下两种形式之一的`<propencoded-array>`：1. 一个32 位整数，由一个指定频率的`<u32>`组成。2. 表示为`<u64>`的 64 位整数 |
| status             | SD | `<string>` | 描述 CPU 状态的标准属性。 对于代表对称多处理 (SMP) 配置中的 CPU 的节点，应存在此属性。 对于 CPU 节点，“okay”、“disabled”和“fail”值的含义如下：“okay” ：CPU 正在运行。“disabled”：CPU 处于静止状态。“fail” ：CPU 无法运行或不存在。  静态CPU处于一种状态，它不能干扰其他CPU的正常运行，也不能受到其他正在运行的CPU的正常运行的影响，除非通过显式方法启用或重新启用静态CPU（请参阅enable方法） 财产）。特别是，正在运行的 CPU 应能够发出广播 TLB 无效，而不影响静止的 CPU。示例：静态 CPU 可能处于旋转循环、保持复位状态、与系统总线电隔离或处于其他依赖于实现的状态。处于“故障”状态的 CPU 不会以任何方式影响系统。 该状态分配给不存在相应 CPU 的节点。 |
| enable-method      | SD | `<stringlist>` | 描述启用处于禁用状态的 CPU 的方法。 对于状态属性值为“已禁用”的 CPU，此属性是必需的。 该值由一个或多个字符串组成，这些字符串定义释放此 CPU 的方法。 如果客户端程序识别其中任何方法，则它可以使用它。 该值应为以下之一：**"spin-table"**: CPU 通过 DTSpec 中定义的旋转表方法启用。 **"[vendor],[method]"**: 与实现相关的字符串，描述 CPU 从“禁用”状态释放的方法。所需的格式为：“[供应商]，[方法]”，其中供应商是描述制造商名称的字符串，方法是描述供应商特定机制的字符串。例如：`fsl,MPC8572DS` 注意：其他方法可能会添加到 DTSpec 规范的后续修订版中。 |
| cpu-release-addr   | SD | `<64>` | 对于启用方法属性值为“spin-table”的 cpu 节点，需要 cpu-release-addr 属性。 该值指定将辅助 CPU 从其自旋循环中释放的自旋表条目的物理地址。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```



`/cpus/cpu*` 节点 Power ISA 属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| power-isa-version        | O  | `<string>` | 指定 Power ISA 版本字符串的数字部分的字符串。 例如，对于符合 Power ISA 版本 2.06 的实现，该属性的值将为“2.06”。 |
| power-isa-*              | O  | `<empty>`  | 如果 power-isa-version 属性存在，则对于指示的 Power ISA 版本的 Book I 的类别部分中的每个类别，存在名为 power-isa-[CAT] 的属性，其中 [CAT] 是缩写类别 名称中所有大写字母都转换为小写，表示该类别受实现支持。例如，如果 power-isa-version 属性存在且其值为“2.06”，并且 power-isa-e.hv 属性存在，则该实现支持 Power ISA 版本 2.06 中定义的 [Category:Embedded.Hypervisor]。 |
| cache-op-block-size      | SD | `<u32>`    | 指定高速缓存块指令操作的块大小（以字节为单位）（例如，dcbz）。 如果与 L1 缓存块大小不同，则为必需。 |
| reservation-granule-size | SD | `<u32>`    | 指定该处理器支持的保留粒度大小（以字节为单位）。 |
| mmu-type                 | O  | `<string>` | 指定CPU的MMU类型。有效的值如下所示："mpc8xx"、"ppc40x"、"ppc440"、"ppc476"、"power-embedded"、"powerpc-classic"、"power-server-stab"、"power-server-slb"、"none" |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

可能会遇到旧版本的 devicetree，其中包含 CPU 节点上的总线频率属性。 为了兼容性，
客户端程序可能希望支持总线频率。 该值的格式与时钟频率的格式相同。 推荐的做法是
使用时钟频率属性来表示总线节点上总线的频率。

#### TLB 属性

cpu 节点的以下属性描述了处理器 MMU 中的转换后备缓冲区。

`/cpu/cpu*` 节点电源 ISA TLB 属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| tlb-split  | SD | `<empty>` | 如果存在，则指定 TLB 具有分割配置，具有用于指令和数据的单独 TLB。 如果不存在，则指定TLB具有统一配置。 对于具有拆分配置中的 TLB 的 CPU 是必需的。 |
| tlb-size   | SD | `<u32>`   | 指定 TLB 中的条目数。 对于具有统一的指令和数据地址 TLB 的 CPU 来说是必需的。 |
| tlb-sets   | SD | `<u32>`   | 指定 TLB 中关联集的数量。 对于具有统一的指令和数据地址 TLB 的 CPU 来说是必需的。 |
| d-tlb-size | SD | `<u32>`   | 指定数据 TLB 中的条目数。 对于具有拆分 TLB 配置的 CPU 是必需的。 |
| d-tlb-sets | SD | `<u32>`   | 指定数据 TLB 中关联集的数量。 对于具有拆分 TLB 配置的 CPU 是必需的。 |
| i-tlb-size | SD | `<u32>`   | 指定指令 TLB 中的条目数。 对于具有拆分 TLB 配置的 CPU 是必需的。 |
| i-tlb-sets | SD | `<u32>`   | 指定指令 TLB 中关联集的数量。 对于具有拆分 TLB 配置的 CPU 是必需的。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

#### 内部 (L1) 缓存属性

cpu 节点的以下属性描述了处理器的内部 (L1) 缓存。

`/cpu/cpu*` 节点电源 ISA 缓存属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| cache-unified      | SD | `<empty>`   | 如果存在，则指定缓存具有统一的组织。 如果不存在，则指定缓存具有哈佛架构，具有单独的指令和数据缓存。 |
| cache-size         | SD | `<u32>`     | 指定统一缓存的大小（以字节为单位）。 如果缓存是统一的（组合指令和数据），则需要。 |
| cache-sets         | SD | `<u32>`     | 指定统一缓存中关联集的数量。 如果缓存是统一的（指令和数据组合），则需要 |
| cache-block-size   | SD | `<u32>`     | 指定统一缓存的块大小（以字节为单位）。 如果处理器具有统一缓存（组合指令和数据），则需要 |
| cache-line-size    | SD | `<u32>`     | 指定统一高速缓存的行大小（以字节为单位）（如果与高速缓存块大小不同） 如果处理器具有统一高速缓存（组合指令和数据），则需要。 |
| i-cache-size       | SD | `<u32>`     | 指定指令高速缓存的大小（以字节为单位）。 如果 cpu 有单独的指令缓存，则需要。 |
| i-cache-sets       | SD | `<u32>`     | 指定指令高速缓存中关联性集的数量。 如果 cpu 有单独的指令缓存，则需要。 |
| i-cache-block-size | SD | `<u32>`     | 指定指令高速缓存的块大小（以字节为单位）。 如果 cpu 有单独的指令缓存，则需要。 |
| i-cache-line-size  | SD | `<u32>`     | 如果与高速缓存块大小不同，则指定指令高速缓存的行大小（以字节为单位）。 如果 cpu 有单独的指令缓存，则需要。 |
| d-cache-size       | SD | `<u32>`     | 指定数据缓存的大小（以字节为单位）。 如果 cpu 有单独的数据缓存，则需要。 |
| d-cache-sets       | SD | `<u32>`     | 指定数据缓存中关联性集的数量。 如果 cpu 有单独的数据缓存，则需要。 |
| d-cache-block-size | SD | `<u32>`     | 指定数据缓存的块大小（以字节为单位）。 如果 cpu 有单独的数据缓存，则需要。 |
| d-cache-line-size  | SD | `<u32>`     | 如果与缓存块大小不同，则指定数据缓存的行大小（以字节为单位）。 如果 cpu 有单独的数据缓存，则需要。 |
| next-level-cache   | SD | `<phandle>` | 如果存在，则表明存在另一级缓存。 该值是下一级缓存的phandle。 phandle 值类型在第 2.3.3 节中有完整描述。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

可能会遇到旧版本的设备树，其中包含已弃用的下一级缓存属性（称为 l2-cache）。
为了兼容性，如果不存在下一级缓存属性，客户端程序可能希望支持二级缓存。

这两个属性的含义和用途是相同的。

#### 举例

以下是具有一个子 cpu 节点的 /cpus 节点的示例：

```dts
cpus {
    #address-cells = <1>;
    #size-cells = <0>;
    cpu@0 {
        device_type = "cpu";
        reg = <0>;
        d-cache-block-size = <32>; // L1 - 32 bytes i-cache-block-size = <32>; // L1 - 32 bytes
        d-cache-size = <0x8000>; // L1, 32K
        i-cache-size = <0x8000>; // L1, 32K
        timebase-frequency = <82500000>; // 82.5 MHz
        clock-frequency = <825000000>; // 825 MHz
    };
};
```

### 多级共享缓存节点（/cpus/cpu*/l?-cache）

处理器和系统可以实现附加级别的高速缓存层次结构。 例如，二级(L2)或三级(L3)缓存。
这些缓存可以紧密集成到 CPU 中，也可以在多个 CPU 之间共享。

具有兼容值“cache”的设备节点描述了这些类型的缓存。

缓存节点应定义 phandle 属性，并且与缓存关联或共享缓存的所有 cpu 节点或缓存节点均
应包含下一级缓存属性，该属性指定缓存节点的 phandle。

缓存节点可以在CPU节点或设备树中的任何其他适当位置下表示。

多级和共享缓存的属性如表 3-9 所示。 L1 缓存属性如表 3-8 所示。

/cpu/cpu*/l?-cache 节点电源 ISA 多级和共享缓存属性

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| compatible  | R | `<string>` | 标准属性。 该值应包含字符串“cache”。 |
| cache-level | R | `<u32>`    | 指定缓存层次结构中的级别。 例如，2 级缓存的值为 2。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

#### 举例

请参阅以下两个 CPU 的设备树表示示例，每个 CPU 都有自己的片上 L2 和共享 L3。

```dts
cpus {
    #address-cells = <1>;
    #size-cells = <0>;
    cpu@0 {
        device_type = "cpu"; cache-unified;
        reg = <0>;
        cache-size = <0x8000>; // L1, 32 KB
        cache-block-size = <32>;
        timebase-frequency = <82500000>; // 82.5 MHz
        next-level-cache = <&L2_0>; // phandle to L2

        L2_0:l2-cache {
            compatible = "cache";
            cache-unified;
            cache-size = <0x40000>; // 256 KB

            cache-sets = <1024>;
            cache-block-size = <32>;
            cache-level = <2>;
            next-level-cache = <&L3>; // phandle to L3

            L3:l3-cache {
                compatible = "cache";
                cache-unified;
                cache-size = <0x40000>; // 256 KB
                cache-sets = <0x400>; // 1024
                cache-block-size = <32>;
                cache-level = <3>;
            };
        };
    };

    cpu@1 {
        device_type = "cpu"; cache-unified;
        reg = <1>;
        cache-block-size = <32>;
        cache-size = <0x8000>; // L1, 32 KB
        timebase-frequency = <82500000>; // 82.5 MHz
        clock-frequency = <825000000>; // 825 MHz
        next-level-cache = <&L2_1>; // phandle to L2
        L2_1:l2-cache {
            compatible = "cache";
            cache-unified;
            cache-level = <2>;
            cache-size = <0x40000>; // 256 KB
            cache-sets = <0x400>; // 1024
            cache-line-size = <32>; // 32 bytes next-level-cache = <&L3>; // phandle to L3
        };
    };
};
```

本章包含有关如何在设备树中表示特定类型和设备类别的要求（称为绑定）。 设备节点的
兼容属性描述了该节点所遵守的特定绑定（或多个绑定）。

绑定可以被定义为彼此的扩展。 例如，可以将新的总线类型定义为简单总线绑定的扩展。
在这种情况下，兼容属性将包含几个标识每个绑定的字符串——从最具体到最一般（参见第 
2.3.1 节，compatible）。


## 设备绑定

### 约束准则

#### 一般原则

为设备创建新的设备树表示时，应创建一个完整描述设备所需属性和值的绑定。 这组属性
应具有足够的描述性，以便为设备驱动程序提供设备所需的属性。

一些推荐的做法包括：
1. 使用第 2.3.1 节中描述的约定定义兼容字符串。
2. 使用适用于新设备的标准属性（第 2.3 节和第 2.4 节中定义）。 这种用法通常至少
包括寄存器和中断属性。
3. 如果新设备适合 DTSpec 定义的设备类之一，则使用第 4 节（设备绑定）中指定的约定。
4. 如果适用，请使用第 4.1.2 节中指定的其他属性约定。
5. 如果绑定需要新属性，则推荐的属性名称格式为：`<company>, <property-name>`，
其中`<company>`是 OUI 或简短的唯一字符串（例如标识创建者的股票行情代码） 的绑定。
示例：`"ibm,ppc-interrupt-server#s"`


#### 其他属性

本节定义了可能适用于多种类型的设备和设备类别的有用属性列表。 在这里定义它们是为了
促进名称和用法的标准化。


**clock-frequency Property**

| 属性 | 时钟频率 |
|--|--|
| 值类型 | `<prop-encoded-array>` |
| 描述 | 指定时钟频率（以 Hz 为单位）。 该值是以下两种形式之一的`<prop-encoded-array>`：由指定频率的`<u32>`组成的 32 位整数 表示为指定频率的`<u64>`的 64 位整数  |


**reg-shift Property**

| 属性 | 寄存器移位 |
|--|--|
| 值类型 | `<u32>` |
| 描述 | reg-shift 属性提供了一种机制来表示除了寄存器之间的字节数之外在大多数方面都相同的设备。 reg-shift 属性以字节为单位指定离散设备寄存器彼此分开的距离。 各个寄存器位置使用以下公式计算：`“寄存器地址”<< reg-shift`。 如果未指定，则默认值为 0。例如，在 16540 个 UART 寄存器位于地址 0x0、0x4、0x8、0xC、0x10、0x14、0x18 和 0x1C 的系统中，`reg-shift = <2>` 属性将 用于指定寄存器位置。 |


**label Property**

| 属性 | 标签 |
|--|--|
| 值类型 | `<string>` |
| 描述 | label 属性定义了描述设备的人类可读字符串。 给定设备的绑定指定该设备的属性的确切含义。 |


### 串口设备

#### 串行类绑定

串行设备类别由各种类型的点对点串行线路设备组成。 串行线路设备的示例包括 8250 UART、
16550 UART、HDLC 设备和 BISYNC 设备。 在大多数情况下，与 RS-232 标准兼容的硬件适合
串行设备类别。

I2C 和 SPI（串行外设接口）设备不应表示为串行端口设备，因为它们有自己的特定表示。

**clock-frequency Property**

| 属性 | 时钟频率 |
|--|--|
| 值类型 | `<u32>` |
| 描述 | 指定波特率发生器输入时钟的频率（以赫兹为单位）。 |
| 举例 | `clock-frequency = <100000000>;` |


**current-speed Property**

| 属性 | 当前速度 |
|--|--|
| 值类型 | `<u32>` |
| 描述 | 指定串行设备的当前速度（以每秒位数为单位）。 如果引导程序已初始化串行设备，则应设置此属性。 |
| 举例 | `115,200 Baud: current-speed = <115200>;` |


#### National Semiconductor 16450/16550 兼容 UART 要求

与 National Semiconductor 16450/16550 UART（通用异步接收器发送器）兼容的串行设备
应使用以下属性在设备树中表示。

ns16550 UART 属性


| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| compatible      | R  | `<string list>`        | 值应包括“ns16550”。 |
| clock-frequency | R  | `<u32>`                | 指定波特率发生器输入时钟的频率（以 Hz 为单位） |
| current-speed   | OR | `<u32>`                | 指定当前串行设备速度（以每秒位数为单位） |
| reg             | R  | `<prop encoded array>` | 指定父总线地址空间内寄存器设备的物理地址 |
| interrupts      | OR | `<prop encoded array>` | 指定该设备生成的中断。 中断属性的值由一个或多个中断说明符组成。 中断说明符的格式由描述节点中断父级的绑定文档定义。 |
| reg-shift       | O  | `<u32>`                | 以字节为单位指定离散设备寄存器彼此分开的距离。 各个寄存器位置使用以下公式计算：`"registers address"<< reg-shift`。 如果未指定，则默认值为 0。 |
| virtual-reg     | SD | `<u32>` or `<u64>`     | 参见第 2.3.7 节。 指定映射到 reg 属性中指定的第一个物理地址的有效地址。 如果该设备节点是系统的控制台，则需要此属性。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

### 网络设备

网络设备是面向分组的通信设备。 假定此类设备实现七层 OSI 模型的数据链路层（第2层）
并使用媒体访问控制 (MAC) 地址。 网络设备的示例包括以太网、FDDI、802.11 和令牌环。

#### 网络类绑定

**address-bits Property**

| 属性 | 地址bits |
|--|--|
| 值类型 | `<u32>` |
| 描述 | 指定寻址该节点所描述的设备所需的地址位数。 该属性指定 MAC 地址中的位数。 如果未指定，则默认值为 48。 |
| 举例 | `address-bits = <48>;` |


**local-mac-address Property**

| 属性 | 本地mac地址 |
|--|--|
| 值类型 | `<prop-encoded-array>`编码为十六进制数字数组 |
| 描述 | 指定分配给包含此属性的节点所描述的网络设备的 MAC 地址 |
| 举例 | `local-mac-address = [ 00 00 12 34 56 78 ];` |



**mac-address Property**

| 属性 | mac地址 |
|--|--|
| 值类型 | `<prop-encoded-array>`编码为十六进制数字数组 |
| 描述 | 指定引导程序最后使用的 MAC 地址。 当引导程序分配给设备的 MAC 地址与 local-mac-address 属性不同时，应使用此属性。 仅当该值与 local-mac-address 属性值不同时才应使用此属性 |
| 举例 | `mac-address = [ 02 03 04 05 06 07 ];` |


**max-frame-size Property**

| 属性 | 最大帧大小 |
|--|--|
| 值类型 | `<u32>` |
| 描述 | 指定物理接口可以发送和接收的最大数据包长度（以字节为单位）。 |
| 举例 | `max-frame-size = <1518>;` |


#### 以太网特定注意事项

除了网络设备类指定的属性之外，基于 IEEE 802.3 LAN 标准集合（统称为以太网）的网络
设备还可以使用以下属性在设备树中表示。

本节中列出的属性补充了网络设备类中列出的属性。

**max-speed Property**

| 属性 | 最大速度 |
|--|--|
| 值类型 | `<u32>` |
| 描述 | 指定设备支持的最大速度（以每秒兆位为单位指定） |
| 举例 | `max-speed = <1000>;` |


**phy-connection-type Property**

| 属性 | 物理连接类型 |
|--|--|
| 值类型 | `<string>` |
| 描述 | 指定以太网设备和物理层 (PHY) 设备之间的接口类型。 该属性的值特定于实现。 推荐值如下表所示。 |
| 举例 | `phy-connection-type = "mii";` |

phy-connection-type 属性的定义值

| 连接类型 | 值 |
|--|--|
| Media Independent Interface                       | mii        |
| Reduced Media Independent Interface               | rmii       |
| Gigabit Media Independent Interface               | gmii       |
| Reduced Gigabit Media Independent                 | rgmii      |
| rgmii with internal delay                         | rgmii-id   |
| rgmii with internal delay on TX only              | rgmii-txid |
| rgmii with internal delay on RX only              | rgmii-rxid |
| Ten Bit Interface                                 | tbi        |
| Reduced Ten Bit Interface                         | rtbi       |
| Serial Media Independent Interface                | smii       |
| Serial Gigabit Media Independent Interface        | sgmii      |
| Reverse Media Independent Interface               | rev-mii    |
| 10 Gigabits Media Independent Interface           | xgmii      |
| Multimedia over Coaxial                           | moca       |
| Quad Serial Gigabit Media Independent Interface   | qsgmii     |
| Turbo Reduced Gigabit Media Independent Interface | trgmii     |

**phy-handle Property**

| 属性 | 物理句柄 |
|--|--|
| 值类型 | `<phandle>` |
| 描述 | 指定对表示连接到该以太网设备的物理层 (PHY) 设备的节点的引用。 如果以太网设备连接到物理层设备，则需要此属性。 |
| 举例 | `phy-handle = <&PHY0>;` |


### Power ISA 开放式 PIC 中断控制器

本节规定了表示 Open PIC 兼容中断控制器的要求。 开放式 PIC 中断控制器实现开放式
PIC 架构（由 AMD 和 Cyrix 联合开发），并在开放式可编程中断控制器 (PIC) 寄存器
接口规范修订版 1.2 [b18] 中指定。

Open PIC 中断域中的中断说明符用两个单元进行编码。 第一个单元格定义中断号。
第二个单元定义了意义和电平信息。

中断说明符中的感知和电平信息应按如下方式编码：
```
0 = low to high edge sensitive type enabled
1 = active low level sensitive type enabled
2 = active high level sensitive type enabled
3 = high to low edge sensitive type enabled
```

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| compatible           | R | `<string>`             | 值应包括“open-pic” |
| reg                  | R | `<prop encoded array>` | 指定父总线地址空间内寄存器设备的物理地址 |
| interrupt-controller | R | `<empty>`              | 指定该节点是中断控制器 |
| `#interrupt-cells`   | R | `<u32>`                | 应为2 |
| `#address-cells`     | R | `<u32>`                | 应为0 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义

```
注意：所有其他标准属性（第 2.3 节）都是允许的，但都是可选的。
```

### 简单总线兼容值

片上系统处理器可能具有无法探测设备的内部 I/O 总线。 总线上的设备可以直接访问，
无需额外配置。 这种类型的总线被表示为具有“simple-bus”兼容值的节点。

| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
| compatible     | R | `<string>`       | 值应包括“simple-bus”。 |
| ranges         | R | `<prop encoded>` | 该属性表示父地址到子地址空间之间的映射（请参见第 2.3.8 节，范围）。 |
| nonposted-mmio | O | `<empty>`        | 指定该总线的直接子级应该对 MMIO 范围使用非发布内存访问（即非发布映射模式）。 |

用法：R=必需，O=可选，OR=可选但推荐，SD=参见定义



## 扁平化设备树 (DTB) 格式

Devicetree Blob (DTB) 格式是设备树数据的平面二进制编码。 它用于在软件程序之间
交换设备树数据。 例如，当启动操作系统时，固件会将 DTB 传递给操作系统内核。

```
注意：IEEE1275 开放固件 [IEEE1275] 未定义 DTB 格式。 在大多数兼容开放固件的平台上，
通过调用固件方法遍历树结构来提取设备树。
```

DTB 格式将设备树数据编码在单个、线性、无指针数据结构中。 它由一个小标头（参见第
5.2 节）组成，后面跟着三个可变大小的部分：内存保留块（参见第 5.3 节）、结构块
（参见第 5.4 节）和字符串块（参见第 5.5 节）。 这些应该按顺序出现在展平的设备树中。
因此，当加载到内存地址处时，整个设备树结构将类似于图 5.1 中的图（较低的地址位于
图的顶部）。

| Devicetree .dtb Structure |
|--|
| struct fdt_header |
| (free space) |
| memory reservation block |
| (free space) |
| structure block |
| (free space) |
| strings block |
| (free space) |

（free space）部分可能不存在，但在某些情况下可能需要它们来满足各个块的对齐约束（参见第 5.6 节）。


### 版本控制

自该格式最初定义以来，已经定义了多个版本的扁平化设备树结构。 标头中的字段给出版本，
以便客户端程序可以确定设备树是否以兼容的格式编码。

本文档仅描述该格式的版本 17。 符合 DTSpec 的引导程序应提供版本 17 或更高版本的
设备树，并且应提供向后兼容版本 16 的版本的设备树。符合 DTSpec 的客户端程序应接受
向后兼容版本 17 的任何版本的设备树，并且可以接受其他版本 以及。

```
注意：版本是相对于设备树的二进制结构，而不是其内容。
```

### Header

devicetree 的标头布局由以下 C 结构定义。 所有标头字段都是 32 位整数，以大端格式存储。

扁平化设备树标头字段
(Flattened Devicetree Header Fields)

```C
struct fdt_header {
    // 该字段应包含值 0xd00dfeed（大端）。
    uint32_t magic;
    // 该字段应包含设备树数据结构的总大小（以字节为单位）。
    // 该大小应包含结构的所有部分：标头、内存保留块、结构块和字符串块，以及块之间
    // 或最终块之后的任何可用空间间隙。
    uint32_t totalsize;
    // 该字段应包含结构块（参见第 5.4 节）距标头开头的字节偏移量。
    uint32_t off_dt_struct;
    // 该字段应包含字符串块（参见第 5.5 节）距标头开头的字节偏移量。
    uint32_t off_dt_strings;
    // 该字段应包含内存预留块（参见第 5.3 节）距标头开头的字节偏移量。
    uint32_t off_mem_rsvmap;
    // 该字段应包含设备树数据结构的版本。 如果使用本文档中定义的结构，则版本为 17。 
    // DTSpec 引导程序可以提供更高版本的设备树，在这种情况下，该字段应包含在提供
    // 该版本详细信息的后续文档中定义的版本号。
    uint32_t version;
    // 该字段应包含设备树数据结构的最低版本，所使用的版本向后兼容。 因此，对于
    // 本文档（版本 17）中定义的结构，该字段应包含 16，因为版本 17 向后兼容版本16，
    // 但不向后兼容早期版本。 根据第 5.1 节，DTSpec 引导程序应提供与版本 16 向后
    // 兼容的格式的设备树，因此该字段应始终包含 16。
    uint32_t last_comp_version;
    // 该字段应包含系统启动 CPU 的物理 ID。 它应与设备树中该 CPU 节点的 reg 属性
    // 中给出的物理 ID 相同。
    uint32_t boot_cpuid_phys;
    // 该字段应包含设备树 blob 的字符串块部分的长度（以字节为单位）。
    uint32_t size_dt_strings;
    // 该字段应包含设备树 blob 的结构块部分的长度（以字节为单位）。
    uint32_t size_dt_struct;
};
```


### 内存预留块

#### 目的

内存预留块向客户端程序提供物理内存中预留区域的列表； 也就是说，不应将其用于一般
内存分配。 它用于保护重要的数据结构不被客户端程序覆盖。 例如，在某些具有 IOMMU 
的系统上，由 DTSpec 引导程序初始化的 TCE（转换控制条目）表需要以这种方式进行保护。
同样，在客户端程序运行时使用的任何引导程序代码或数据都需要保留（例如，开放固件
平台上的 RTAS）。 DTSpec 不要求引导程序提供任何此类运行时组件，但它并不禁止实现
作为扩展这样做。

更具体地说，客户端程序不应访问保留区域中的内存，除非引导程序提供的其他信息明确
指示它应这样做。 然后，客户端程序可以以指示的方式访问保留存储器的指示的部分。
引导程序可以向客户端程序指示保留内存的特定用途的方法可能会出现在本文档、其可选
扩展中或特定于平台的文档中。

引导程序提供的保留区域可以但不要求包含设备树 blob 本身。 客户端程序应确保在使用
该数据结构之前不会覆盖该数据结构，无论它是否在保留区域中。

必须保留在存储器节点中声明并由引导程序访问或在客户端进入之后由引导程序访问的任何
存储器。 这种类型的访问的示例包括（例如，通过非保护虚拟页进行推测内存读取）。

这个要求是必要的，因为任何未保留的内存都可以被具有任意存储属性的客户端程序访问。

任何由引导程序引起的对保留内存的访问都必须按照不禁止缓存和要求内存一致性的方式
进行（即 WIMG = 0bx01x），此外，对于 Book III-S 实现，也必须按照不要求直写的方式
进行（即 WIMG = 0b001x） ）。 此外，如果支持 VLE 存储属性，则所有对保留内存的
访问都必须在 VLE=0 时完成。

此要求是必要的，因为允许客户端程序映射具有指定为不要求直写、不禁止缓存和要求内存
一致性（即 WIMG = 0b001x）和 VLE=0（如果支持）的存储属性的内存。 客户端程序可以
使用包含保留内存的大虚拟页。 然而，客户端程序可能不会修改保留的存储器，因此引导
程序可以按照需要写入的方式执行对保留的存储器的访问，其中该存储属性的冲突值在架构
上是允许的。

#### 格式

内存预留块由一系列 64 位大端整数对组成，每对由以下 C 结构表示。

```C
struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};
```

每对给出保留内存区域的物理地址和大小（以字节为单位）。 这些给定区域不应相互重叠。
保留块列表应以地址和大小都等于 0 的条目结束。请注意，地址和大小值始终为 64 位。
在 32 位 CPU 上，该值的高 32 位将被忽略。

内存预留块中的每个 uint64_t 以及整个内存预留块应位于距 devicetree blob 开头8字节
对齐的偏移处（请参见第 5.6 节）。


#### 内存预留块和 UEFI

与 /reserved-memory 节点（第 3.5.4 节）一样，当通过 [UEFI] 引导时，内存预留块中的
条目也必须列在通过 GetMemoryMap() 获取的系统内存映射中，以防止 UEFI 应用程序进行
分配。 内存预留块条目应以 EfiReservedMemoryType 类型列出。


### 结构块

结构块描述了设备树本身的结构和内容。 它由一系列带有数据的标记组成，如下所述。 
它们被组织成线性树结构，如下所述。

结构块中的每个令牌以及结构块本身应位于距 devicetree blob 开头 4 字节对齐的偏移处
（请参见第 5.6 节）。


#### 词汇结构

结构块由一系列片段组成，每个片段以一个标记开始，即一个大端序的 32 位整数。 一些
令牌后面跟着额外的数据，其格式由令牌值决定。 所有令牌应在 32 位边界上对齐，这可能
需要在前一个令牌数据之后插入填充字节（值为 0x0）。

五种代币类型如下：

**FDT_BEGIN_NODE（0x00000001）**

FDT_BEGIN_NODE 标记标记节点表示的开始。 其后应跟随节点的单元名称作为额外数据。
该名称存储为以 null 结尾的字符串，并且应包括单元地址（请参阅第 2.2.1 节）（如果有）。
节点名称后跟零填充字节（如果需要对齐），然后是下一个标记，该标记可以是除 FDT_END
之外的任何标记。

**FDT_END_NODE（0x00000002）**

FDT_END_NODE 标记标记节点表示的结束。 该令牌没有额外的数据； 因此它后面紧跟着下
一个标记，该标记可以是除 FDT_PROP 之外的任何标记。

**FDT_PROP（0x00000003）**

FDT_PROP 标记标志着设备树中一个属性表示的开始。 其后应是描述该属性的额外数据。
该数据首先由属性的长度和名称组成，表示为以下 C 结构：
```C
struct {
    uint32_t len;
    uint32_t nameoff;
}
```

该结构中的两个字段都是 32 位大端整数。
* len 给出属性值的长度（以字节为单位）（可能为零，表示属性为空，请参见第 2.2.4 节）。
* nameoff 给出字符串块中的偏移量（参见第 5.5 节），在该偏移量中属性的名称存储为
以 null 结尾的字符串。

在此结构之后，属性的值以长度为 len 的字节字符串形式给出。 该值后面跟着清零的填充
字节（如果需要），以与下一个 32 位边界对齐，然后是下一个令牌，该令牌可以是除
FDT_END 之外的任何令牌。

**FDT_NOP (0x00000004)**

FDT_NOP 标记将被任何解析设备树的程序忽略。 该令牌没有额外的数据； 因此它后面紧跟
着下一个标记，该标记可以是任何有效的标记。 树中的属性或节点定义可以用 FDT_NOP
标记覆盖，以将其从树中删除，而无需在 devicetree blob 中移动树表示的其他部分。

**FDT_END（0x00000009）**

FDT_END 标记标记结构块的结束。 只能有一个 FDT_END 令牌，并且它应该是结构块中的
最后一个令牌。 它没有额外的数据； 因此，紧随 FDT_END 令牌之后的字节距结构块开头的
偏移量等于设备树 blob 标头中 size_dt_struct 字段的值。


#### 树结构

devicetree 结构表示为线性树：每个节点的表示以 FDT_BEGIN_NODE 标记开始，以
FDT_END_NODE 标记结束。 节点的属性和子节点（如果有）在 FDT_END_NODE 之前表示，
以便这些子节点的 FDT_BEGIN_NODE 和 FDT_END_NODE 标记嵌套在父节点的标记中。

结构块作为一个整体由根节点的表示（包含所有其他节点的表示）组成，后面跟着一个
FDT_END 标记来标记整个结构块的结尾。

更准确地说，每个节点的表示由以下部分组成：
* （可选）任意数量的FDT_NOP令牌
* FDT_BEGIN_NODE 标记
    * 节点名称为空终止字符串
    * [将填充字节归零以与 4 字节边界对齐]
* 对于节点的每个属性：
    * （可选）任意数量的 FDT_NOP 令牌
    * FDT_PROP 代币
        * 第 5.4.1 节中给出的属性信息
        * [将填充字节置零以与 4 字节边界对齐]
* 此格式的所有子节点的表示
* （可选）任意数量的FDT_NOP令牌
* FDT_END_NODE 令牌

请注意，此过程要求特定节点的所有属性定义先于该节点的任何子节点定义。 尽管如果
属性和子节点混合，结构不会含糊不清，但处理平面树所需的代码通过此要求得到简化。


### 字符串块

字符串块包含表示树中使用的所有属性名称的字符串。 这些以空结尾的字符串
在本节中简单地连接在一起，并通过字符串块中的偏移量从结构块中引用。

字符串块没有对齐约束，并且可以出现在距 devicetree blob 开头的任意偏移处。


### 对齐

devicetree blob 应位于 8 字节对齐的地址。 为了保持 32 位机器的向后兼容性，某些
软件支持 4 字节对齐，但这不符合 DTSpec。

对于要在没有未对齐的内存访问的情况下使用的内存保留和结构块中的数据，它们应位于
适当对齐的内存地址处。 具体来说，内存预留块应与 8 字节边界对齐，结构块应与 4 字
节边界对齐。

此外，设备树块作为一个整体可以重新定位，而不会破坏子块的对齐方式。

如前面部分所述，结构体和字符串块应具有与设备树 blob 开头对齐的偏移量。 为了确
保块在内存中的对齐，只需确保将设备树作为一个整体加载到与任何子块的最大对齐方式
（即 8 字节边界）对齐的地址处。 符合 DTSpec 的引导程序应在将设备树 blob 传递给
客户端程序之前将其加载到此类对齐的地址。 如果 DTSpec 客户端程序在内存中重新定位
devicetree blob，则它只能将其迁移到另一个 8 字节对齐的地址。







| 属性名 | 使用 | 值类型 | 定义 |
|--|--|--|--|
|  |  | `<>` |  |
|  |  | `<>` |  |




****

**属性名**：

**值类型**：`<>`

**描述**：

**举例**：

```dts
```

## DEVICETREE 源 (DTS) 格式（版本 1）

Devicetree Source (DTS) 格式是设备树的文本表示形式，其形式可以由 dtc 处理为内核
期望形式的二进制设备树。 以下描述不是 DTS 的正式语法定义，而是描述用于表示设备树
的基本构造。

DTS 文件的名称应以“.dts”结尾。

### 编译器指令

其他源文件可以包含在 DTS 文件中。 包含文件的名称应以“.dtsi”结尾。 包含的文件
又可以包含其他文件。

```
/include/ "FILE"
```

### 标签

源格式允许将标签附加到设备树中的任何节点或属性值。 可以通过引用标签来自动生成
Phandle 和路径引用，而不是显式指定 phandle 值或节点的完整路径。 标签仅在
devicetree 源格式中使用，不会编码到 DTB 二进制文件中。

标签长度应在 1 到 31 个字符之间，只能由表 6.1 中的字符组成，并且不能以数字开头。

标签是通过在标签名称后附加冒号（‘:’）来创建的。 引用是通过在标签名称前添加与号
（“&”）作为前缀来创建的。

DTS 标签的有效字符：

| 字符 | 描述 |
|--|--|
| 0-9 | 数字 |
| a-z | 小写字母 |
| A-Z | 大写字母 |
| - | 短划线 |


### 节点和属性定义

Devicetree 节点使用节点名称和单元地址进行定义，并用大括号标记节点定义的开始和
结束。 它们之前可能有一个标签。

```
[label:] node-name[@unit-address] {
    [properties definitions]
    [child nodes]
};
```

节点可以包含属性定义和/或子节点定义。 如果两者都存在，则属性应位于子节点之前。

先前定义的节点可以被删除。
```
/delete-node/ node-name;
/delete-node/ &label;
```

属性定义是以下形式的名称值对：
```
[label:] property-name = value;
```

具有空（零长度）值的属性除外，其形式为：
```
[label:] property-name;
```

先前定义的属性可能会被删除。
```
/delete-property/ property-name;
```

属性值可以定义为 32 位整数单元的数组、空终止字符串、字节串或这些的组合。
* 单元格数组由尖括号表示，周围是空格分隔的 C 样式整数列表。 例子：
```
interrupts = <17 0xc>;
```

* 值可以表示为括号内的算术、按位或逻辑表达式。
```
Arithmetic operators

+ add
- subtract
* multiply
/ divide
% modulo
```
```
Bitwise operators

& and
| or
^ exclusive or
~ not
<< left shift
>> right shift
```
```
Logical operators

&& and
|| or
!  not
```
```
Relational operators

< less than
> greater than
<= less than or equal
>= greater than or equal
== equal
!= not equal
```
```
Ternary operators

?: (condition ? value_if_true : value_if_false)
```
* 64 位值由两个 32 位单元表示。 例子：
```
clock-frequency = <0x00000001 0x00000000>;
```
* 以 NULL 结尾的字符串值使用双引号表示（属性值被视为包含终止 NULL 字符）。 例子：
```
compatible = "simple-bus";
```
* 字节串括在方括号 [ ] 中，每个字节由两个十六进制数字表示。 每个字节之间的空格是
可选的。 例子：
```
local-mac-address = [00 00 12 34 56 78];
```
或等价：
```
local-mac-address = [000012345678];
```
* 值可能有多个以逗号分隔的组件，这些组件连接在一起。 例子：
```
compatible = "ns16550", "ns8250";
example = <0xf00f0000 19>, "a strange property format";
```
* 在元胞数组中，对另一个节点的引用将扩展到该节点的 phandle。 引用可以是 & 后
跟节点的标签。 例子：
```
interrupt-parent = < &mpic >;
```
或者它们可能是 & 后面是大括号中的节点的完整路径。 例子：
```
interrupt-parent = < &{/soc/interrupt-controller@40000} >;
```
* 在元胞数组之外，对另一个节点的引用将扩展到该节点的完整路径。 例子：
```
ethernet0 = &EMAC0;
```
* 标签还可以出现在属性值的任何组件之前或之后，或者出现在元胞数组的单元之间，或者
出现在字节串的字节之间。 例子：
```
reg = reglabel: <0 sizelabel: 0x1000000>;
prop = [ab cd ef byte4: 00 ff fe];
str = start: "string value" end: ;
```

### 文件布局

版本 1 DTS 文件的总体布局：
```
/dts-v1/;
[memory reservations]
    / {
        [property definitions]
        [child nodes]
    };
```
/dts-v1/; 应存在以将文件标识为版本 1 DTS（没有此标记的 dts 文件将被 dtc 视为过时
的版本 0，除了其他小但不兼容的更改之外，版本 0 使用不同的整数格式）。

内存预留（参见第 5.3 节）由以下形式的行表示：
```
/memreserve/ <address> <length>;
```
其中`<address>`和`<length>`是 64 位 C 风格整数，例如，
```
/* Reserve memory region 0x10000000..0x10003fff */
/memreserve/ 0x10000000 0x4000;
```
这 / { ... }; 节定义了设备树的根节点，所有设备树数据都包含在其中。

支持 C 风格 (/* ... \*/) 和 C++ 风格 (//) 注释。
