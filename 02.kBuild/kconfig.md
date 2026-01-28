# Kconfig语法介绍

(引用博客：https://blog.csdn.net/qq_23274715/article/details/104880443)



# 基本说明

1. 语法文档位置:linux源码目录Documentation/kbuild/kconfig-language.txt。

2. 语法例程：linux源码下的所有Kconfig文件。



在项目开发中我们通常需要对一个工程根据不同的需求进行配置、裁剪。通常做法是专门定义一个config_xxx.h的文件，然后再文件中使用#define CONFIG_USING_XX等宏进行配置和裁剪。但是这种配置不太直观化，而且当一个工程由很多模块组成时，人为的去维护这个文件效率也较低。为此linux使用了KConfig来组织并生成配置文件。

KConfig文件一般使用menuconfig命令可视化配置，配置完成后被保存为.config文件。然后又将.config文件转化成xxx.h文件。这样c语言就可以识别使用了。



1. 在内核路径下打开 menuconfig 之后按"h"或"?"可以查看帮助，这里会写类型、提示信息、在哪个Kconfig中定义等
2. 在生成的 .config 文件中通常有一个前缀，这个前缀可以从linux根目录中的makefile中看到
3. 针对不同架构，Kconfig 的入口在 arch 下，例如arm架构的入口Kconfig在 ./arch/arm/Kconfig





# Kconfig语法

1. KConfig语法使用菜单项作为基本组成单位。每个菜单项都有自己的相关属性(菜单项属性)。菜单项之间可以嵌套。
2. 使用#作为注释符.



# KConfig的菜单项

KConfig中按菜单项的性质可以分为下面几种菜单项:

**config菜单项**: 定义一个配置选项。可以接受所有的菜单项属性。**是语法中最常用的一个菜单项**。语法格式为

```
//格式
"config" <symbol>
    <config options>
//eg:
config CONFIG_USING_XXX     # 这里的 CONFIG_USING_XXX 会显示给用户看
    bool "this is use xxx"  # bool 是 CONFIG_USING_XXX d的类型，"this is use xxx"是提示信息，在条目菜单上按"?"可以显示提示信息(prompt)
    default n
    help
    	this is help        # 帮助信息
```



**menuconfig菜单项**:定义一个配置选项，类似config菜单项。它常常配合if块使用。只有menuconfig菜单项选中时，才会展现if中的配置选项。

```
//格式
"menuconfig" <symbol>
    <config options>
//eg:
```



**mainmenu菜单项**:这将设置配置程序的标题栏，如果配置程序选择使用它。它应该放在配置的顶部，在任何其他语句之前。

```
//格式
"mainmenu" info
    <config options>
//eg:
mainmenu "KConfig test"

config CONFIG_USING_XXX
    bool "this is use xxx"
    default n
```



**choice/endchoice菜单项**:选择器，内部包含多个选项，只能单选。

    //格式
    "choice" [symbol]
        <choice options>
        <choice block>
    "endchoice"
    //eg:
    choice
        prompt "Version"
        default PKG_USING_LITTLED_LATEST_VERSION
        help
            Select the package version
    config PKG_USING_LITTLED_V020
        bool "v0.2.0"
    
    config PKG_USING_LITTLED_LATEST_VERSION
        bool "latest"
    endchoice


**menu/endmenu菜单项:**定义一个菜单块。

```
//格式
    "menu" <prompt>

    <menu options>
    <menu block>
    "endmenu"
//eg:
menu "menu test"
    config CONFIG_USING_XXX
        bool "this is use xxx"
        default n

	config CONFIG_USING_XXXy
    	bool "this is use xxx"
   		default y
endmenu
```



**comment菜单项**: 添加注释,这个注释在配置文件转化成.h文件时也会转换成注释。

```
//格式
"comment" <prompt>
<comment options>
//eg:
comment "this is set period"
config CONFIG_USING_XXX
	int "default pwm period (ms)"
	range 1 1000
	default 1000
```



**if/endif菜单项:**定义一个if条件判断块

```
//格式
"if" <expr>
    <if block>
"endif"
//eg:
config RT_USING_USER_MAIN      # 该条被选中下边 if 中的语句才能显示出来
   	bool "this is use xxx"
	default y

if RT_USING_USER_MAIN
    config RT_MAIN_THREAD_STACK_SIZE
        int "Set main thread stack size"
        default 2048
    config RT_MAIN_THREAD_PRIORITY
        int "Set main thread priority" 
        default 4  if RT_THREAD_PRIORITY_8
endif
```



**source菜单项:** 将其他的Kconfig文件加入到此位置,并解析显示。

```
//格式
"source" <prompt>
//eg:
source "./xxx/Kconfig"
```



# 菜单项属性

1. 每个菜单项必须选择一种类型定义

   1) bool:二值量。取值为y或者n。

   2. int:十进制整型数

   3. hex:十六进制整型数

   4. tristate:三态量。取值为

      1. y:选中
      2. n:不选中
      3. m: 编译成模块

   5. string:字符串

2. 输入提示(input prompt):每个菜单项最多只有一个输入提示。可以使用“if”添加仅针对此提示的可选依赖项。
3. default(默认值):在用户没有对其设置时它使用默认值。配置选项可以有任意数量的默认值。如果多个默认值是可见的，则只有第一个定义的值是活动的。默认值不局限于定义它们的菜单项。这意味着默认值可以在其他地方定义，也可以由以前的定义覆盖。
4. 类型定义+默认值:是上面两个的简写。eg:bool "this is test"。
5. depends on(菜单项依赖):这个菜单项所依赖的菜单项,只有所依赖的菜单项被选中,当前的菜单项才会出现。如果有多个依赖，使用&&连接。这个属性一般多用if语句代替。
6. select(反向依赖),当前配置被选中,则此依赖也会被选中。他只能用在bool、tristate类型的菜单项中。
7. visible if <expr>(限制菜单显示): 此属性仅适用于菜单块，如果条件为false，则菜单块不会显示给用户(但包含在其中的符号仍然可以由其他符号选择)。“可见”的默认值为真。
8. range min max(数值范围):限制int、hex类型的范围。大于等于min并且小于等于max
9. 帮助信息:help或者"---help---"它的结束由缩进决定。可以使用?查看.
10. 其他属性:
    1. defconfig_list
    2. modules
    3. env=<value>:将环境变量导入Kconfig.



下面是一个菜单项属性使用的简单展示：

KConfig文件内容：

```
config ARM
	bool
	default y
	select ARCH_HAS__POSITIVE
	select ARCH_HAVE_CUSTOM_GPIO
	help
		The ARM series is a line of low-power-consumption RISC chip designslicensed by ARM Ltd and targeted at... 

config ARM_DMA_IOMMU_ALIGNMENT
	int "Maximum PAGE_SIZE"
	range 4 9
	default 8

config ARCH_MMAP_RND_BITS_MAX
	default 14 if PAGE_OFFSET=0x40000000
	default 15 if PAGE_OFFSET=0x80000000
	default 16

config ARCH_MULTIPLATFORM
	bool "Allow multiple platforms to be selected"
	depends on MMU

```

