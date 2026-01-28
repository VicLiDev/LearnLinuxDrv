<参考资料>
[Kernel Build System](https://docs.kernel.org/kbuild/index.html)
[Linux Kernel Makefiles](https://docs.kernel.org/kbuild/makefiles.html)

# 配置

# Kconfig

# Linux Kernel Makefiles

## Overview
Linux 的makefile由五部分组成
```
Makefile                    the top Makefile.
.config                     the kernel configuration file.
arch/$(SRCARCH)/Makefile    the arch Makefile.
scripts/Makefile.*          common rules etc. for all kbuild Makefiles.
kbuild Makefiles            exist in every subdirectory
```

scripts 中重点关注以下文件：  
**scripts/KBuild.include**  
在整个Kbuild系统中，scripts/Makefile.include 提供了大量通用函数以及变量的定义，
这些定义将被 Makefile.build 、Makefile.lib 和 top Makefile频繁调用，以实现相应的
功能,scripts/Makefile.include 参与整个内核编译的过程，是编译的核心脚本之一。  
在研究整个Kbuild系统前，有必要先了解这些变量及函数的使用，才能更好地理解整个内核编译的过程。


**scripts/Makefile.lib**  
在linux内核的整个Kbuild系统中，Makefile.lib 对于目标编译起着决定性的作用，如果
说 Makefile.build 负责执行 make 的编译过程，而 Makefile.lib 则决定了哪些文件需要
编译，哪些目录需要递归进入。


**scripts/Makefile.build**  
在Kbuild系统中，Makefile.build文件算是最重要的文件之一了，它控制着整个内核的
核心编译部分。哪些文件需要递归进入由 Makefile.lib 决定，但是进入的实际操作是由
Makefile.build 处理


# scripts/KBuild.include文件解析

## kbuild-dir 和 kbuild-file

在 Kbuild.include 中的定义如下：
```makefile
# The path to Kbuild or Makefile. Kbuild has precedence over Makefile.
kbuild-dir = $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
kbuild-file = $(or $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Makefile)
```

功能实现：  
使用包含 KBuild.include 的文件中的src变量寻找src下的 Kbuild 或 Makefile，Kbuild
有更高的优先级，后续可以直接使用 `kbuild-file` 表示 src 目录下的Kbuild/Makefile

## $(build) 变量

### 定义：
```makefile
###
# Shorthand for $(Q)$(MAKE) -f scripts/Makefile.build obj=
# Usage:
# $(Q)$(MAKE) $(build)=dir
build := -f $(srctree)/scripts/Makefile.build obj
```

### 使用示例：

```makefile
%config: outputmakefile scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
````
`$(Q)`是一个打印控制参数，决定在执行该指令时是否将其打印到终端。
`$(MAKE)`，为理解方便，我们可以直接理解为 make。
所以，当我们执行make menuconfig时，就是对应这么一条指令：
```makefile
menuconfig: outputmakefile scripts_basic FORCE
    make -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig menuconfig
```

**make -f $(srctree)/scripts/Makefile.build 部分**

在Makefile的规则中，make -f file，这个指令会将 file 作为Makefile 读入并解析，
相当于执行另一个指定的文件，这个文件不一定要以 makfile 命名，在这里就是执行
`$(srctree)/scripts/Makefile.build` 文件。

**obj=scripts/kconfig menuconfig部分**

给目标 makfile 中 obj 变量赋值为scripts/kconfig，具体的处理在scripts/Makefile.build中。

在 Mkefile.build 中，首先会将传入的 obj 参数赋值给 src ，然后包含了 Kbuild.include
```makefile
src := $(obj)

include $(srctree)/scripts/Kbuild.include
include $(srctree)/scripts/Makefile.compiler
include $(kbuild-file)
include $(srctree)/scripts/Makefile.lib
```
这时 kbuild-file 变量就指向了 src 下的Kbuild/Makefile文件，接下来使用 `include $(kbuild-file)`
包含该文件，在本例中即包含了 scripts/kconfig/Makefile，因此在执行目标 menuconfig 时，
实际上是执行的 scripts/kconfig/Makefile 中的 menuconfig

总结：
Makefile.build 会包含 obj 变量指向目录下的 Kbuild/Makefile 文件，所以在使用 `$(build)`
变量执行 Makefile.build 时，指定的目标可能未必在 Makefile.build 中，有可能在 obj
只想目录下的 Kbuild/Makefile 中，如果这里没有，才会重新回到 Makefile.build 中寻找
目标或者执行默认目标

`$(build)` 的功能就是将编译的流程转交到 obj 指定的文件夹下，鉴于Kbuild系统的兼容性
和复杂性，`$(build)`起到一个承上启下的作用，从上接收编译指令，往下将指令分发给对应
的功能部分，包括配置、源码编译、dtb编译等等。


### 类似指令

与 `$(build)` 相类似的，还有以下的编译指令：

**$(modbuiltin)**
```makefile
modbuiltin := -f $(srctree)/scripts/Makefile.modbuiltin obj
```
**$($(dtbinst)**
```makefile
dtbinst := -f $(srctree)/scripts/Makefile.dtbinst obj
```
**$(clean)**
```makefile
clean := -f $(srctree)/scripts/Makefile.clean obj
```
**$(hdr-inst)**
```makefile
hdr-inst := -f $(srctree)/scripts/Makefile.headersinst obj
```

这对应的四条指令，分别对应 內建模块编译、dtb文件安装、目标清除和头文件安装。
和\$(build)用法一致，都遵循这样的调用形式：
```makefile
$(Q)$(MAKE) $(build)=$(dir) $(target)
```
至于具体的实现，可以自行研究。

## $(if_changed)

if_changed 指令也是核心指令，它的功能就是检查文件的变动

在 Makefile 中实际的用法是这样的,
```makefile
foo:
    $(call if_changed,$@)
```
一般情况下 if_changed 函数被调用时，后面都接一个参数，那么这个语句是什么作用呢？
我们来看具体定义：
```makefile
# if_changed      - execute command if any prerequisite is newer than
#                   target, or command line has changed
# if_changed_dep  - as if_changed, but uses fixdep to reveal dependencies
#                   including used config symbols
# if_changed_rule - as if_changed but execute rule instead
# See Documentation/kbuild/makefiles.rst for more info

# print and execute commands
cmd = @$(if $(cmd_$(1)),set -e; $($(quiet)log_print) $(delete-on-interrupt) $(cmd_$(1)),:)

if-changed-cond = $(newer-prereqs)$(cmd-check)$(check-FORCE)

# Execute command if command has changed or prerequisite(s) are updated.
if_changed = $(if $(if-changed-cond),$(cmd_and_savecmd),@:)

# Execute command if command has changed or prerequisite(s) are updated.
cmd_and_savecmd =                                                            \
	$(cmd);                                                              \
	printf '%s\n' 'savedcmd_$@ := $(make-cmd)' > $(dot-target).cmd
```

**`$(if_changed)使用`**
简单来说，调用 if_changed 时会传入一个参数`$var`，当目标的依赖文件有更新时，就执行
`cmd_$var` 指令。比如下面 vmlinux 编译的示例：
```makefile
cmd_link-vmlinux =                                                 \
    $(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
    $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
    $(call if_changed,link-vmlinux)
```
可以看到，vmlinux 的依赖文件列表为 scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE。

调用 if_changed 函数时传入的参数为 link-vmlinux，当依赖文件有更新时，将执行 cmd_link-vmlinux

### 同类型函数

与`$(if_changed)`同类型的，还有`$(if_changed_dep),$(if_changed_rule)`。
```makefile
if_changed_dep = $(if $(strip $(any-prereq) $(arg-check)),$(cmd_and_fixdep),@:)
if_changed_rule = $(if $(strip $(any-prereq) $(arg-check)),$(rule_$(1)),@:)
```
if_changed_dep 与 if_changed 同样执行cmd_$1,不同的是它会检查目标的依赖文件列表是否有更新。

需要注意的是，这里的依赖文件并非指的是规则中的依赖，而是指fixdep程序生成的 .*.o.cmd 文件。

而 if_changed_rule 与 if_changed 不同的是，if_changed 执行 `cmd_$1`,而 if_changed_rule
执行 `rule_$1` 指令。

## $(filechk)

同样，通过名称就可以看出，它的功能是检查文件，严格来说是先操作文件，再检查文件的更新。

它的定义是这样的：
```makefile
###
# filechk is used to check if the content of a generated file is updated.
# Sample usage:
#
# filechk_sample = echo $(KERNELRELEASE)
# version.h: FORCE
#	$(call filechk,sample)
#
# The rule defined shall write to stdout the content of the new file.
# The existing file will be compared with the new one.
# - If no file exist it is created
# - If the content differ the new file is used
# - If they are equal no change, and no timestamp update
define filechk
	$(check-FORCE)
	$(Q)set -e;						\
	mkdir -p $(dir $@);					\
	trap "rm -f $(tmp-target)" EXIT;			\
	{ $(filechk_$(1)); } > $(tmp-target);			\
	if [ ! -r $@ ] || ! cmp -s $@ $(tmp-target); then	\
		$(kecho) '  UPD     $@';			\
		mv -f $(tmp-target) $@;				\
	fi
endef
```
它主要实现的操作是这样的：
`mkdir -p $(dir $@)`：如果`$@`目录不存在，就创建目录，`$@`是编译规则中的目标部分。
(`$@` 在 Makefile 的规则中表示需要生成的目标文件)
执行 `filechk_$(1)` ,然后将执行结果保存到 `$@.tmp`中
对比 `$@.tmp` 和 `$@` 是否有更新，有更新就使用 `$@.tmp`，否则删除 `$@.tmp`。
说白了，`$(filechk)` 的作用就是执行 `filechk_$@` 然后取最优的结果。

举一个 top Makefile 中的例子：
```makefile
filechk_kernel.release = \
    echo "$(KERNELVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"  

include/config/kernel.release: FORCE
    $(call filechk,kernel.release)
```
这部分命令的作用就是输出 kernel.release 到 kernel.release.tmp 文件，最后对比
kernel.release 文件是否更新，kernelrelease 对应内核版本。

同时，如果include/config/不存在，就创建该目录。



# scripts/Makefile.lib文件解析

Makefile.lib 通常也类似 Kbuild.include ,是被 include 到其他文件中使用
```makefile
include $(srctree)/scripts/Makefile.lib
```


## 编译标志位

Makefile.lib开头的部分是这样的：
```makefile
# SPDX-License-Identifier: GPL-2.0
# Backward compatibility
asflags-y  += $(EXTRA_AFLAGS)
ccflags-y  += $(EXTRA_CFLAGS)
cppflags-y += $(EXTRA_CPPFLAGS)
ldflags-y  += $(EXTRA_LDFLAGS)

# flags that take effect in current and sub directories
KBUILD_AFLAGS += $(subdir-asflags-y)
KBUILD_CFLAGS += $(subdir-ccflags-y)
KBUILD_RUSTFLAGS += $(subdir-rustflags-y)
```
这些大多是一些标志位的设置，细节部分暂时不关注


## 目录及文件处理部分

### 去除重复部分

```makefile
# When an object is listed to be built compiled-in and modular,
# only build the compiled-in version
obj-m := $(filter-out $(obj-y),$(obj-m))

# Libraries are always collected in one lib file.
# Filter out objects already built-in
lib-y := $(filter-out $(obj-y), $(sort $(lib-y) $(lib-m)))
```
这一部分主要是去重，如果某个模块已经被定义在obj-y中，证明已经或者将要被编译进内核，
就没有必要在其他地方进行编译了。

### 目录的处理

在查看各级子目录下的 Makefile 时，发现 obj-y/m 的值并非只有目标文件，还有一些目标
文件夹，但是文件夹并不能被直接编译，处理方法如下：
```makefile
# Subdirectories we need to descend into
subdir-ym := $(sort $(subdir-y) $(subdir-m) \
			$(patsubst %/,%, $(filter %/, $(obj-y) $(obj-m))))


ifdef need-modorder
obj-m := $(patsubst %/,%/modules.order, $(filter %/, $(obj-y)) $(obj-m))
else
obj-m := $(filter-out %/, $(obj-m))
endif

ifdef need-builtin
obj-y		:= $(patsubst %/, %/built-in.a, $(obj-y))
else
obj-y		:= $(filter-out %/, $(obj-y))
endif
```
将 obj-y/m 中以"/"结尾的纯目录部分提取出来，并赋值给 subdir-ym，同时去掉尾部的 `/`，
然后，处理 obj-y/m 中的文件夹。

总结：
subdir-ym 是从 obj-y 和 obj-m 中提取到的文件夹
obj-y 和 obj-m 会去掉文件夹或者将文件夹改为`<dir>/built-in.a(modules.order)`。这与
参数 `need-modorder` 和 `need-builtin` 有关

### 多文件依赖的处理

```makefile
# Expand $(foo-objs) $(foo-y) etc. by replacing their individuals
# 将变量 $1 的后缀为 $2 的变量后缀替换为变量 $3，这里后缀替换的方法可以参考makefile
# 笔记中的变量高级用法
# 注意这里替换之后，外部还包了一层 $()，即表示这是替换之后的值，例如替换之后是 foo-objs，
# 这里取的是 foo-objs 的值而不是 foo-objs
suffix-search = $(strip $(foreach s, $3, $($(1:%$(strip $2)=%$s))))
# List composite targets that are constructed by combining other targets
# 将 $1 中，后缀为 $2 的变量替换为后缀为 $3 和 - 组成的字符串，并取替换后变量的值，
# 如果该值非空，则记录原始的 .o 目标
multi-search = $(sort $(foreach m, $1, $(if $(call suffix-search, $m, $2, $3 -), $m)))

# If $(foo-objs), $(foo-y), $(foo-m), or $(foo-) exists, foo.o is a composite object
# 如果 obj-y 中包含了多文件目标 foo.o，调用 multi-search 函数则会返回 foo.o
# 如果 obj-m 中包含了多文件目标 foo.o，调用 multi-search 函数则会返回 foo.o
# 如果包含的目标不是多目标文件，则会被过滤掉，不返回
multi-obj-y := $(call multi-search, $(obj-y), .o, -objs -y)
multi-obj-m := $(call multi-search, $(obj-m), .o, -objs -y -m)
multi-obj-ym := $(multi-obj-y) $(multi-obj-m)
```
这里处理的是一个目标文件是否依赖于多个目标文件的问题。

这一部分的理解是比较难的，需要有一定的Makefile功底，具体实现如下：

对于obj-y/m中每一个后缀为 .o 的元素，查找对应的xxx-objs、xxx-y、xxx-（、xxx-m）
变量值。如果 foo.o 目标依赖于 a.c,b.c,c.c,那么它在 Makefile 中的写法是这样的：
```makefile
obj-y += foo.o
foo-objs = a.o b.o c.o
```

总结：
kbuild 会将obj-y/m中的每个后缀为 .o 的元素拿出来，然后将后缀替换为 -objs、 -y、 - (、-m)
取替换之后变量的值，如果这些值不为空，则将原本的.o 后缀保存到 multi-obj-y 和 multi-obj-m
中，否则不保存当前 .o 文件
因此，muti-obj-y 和 multi-obj-m 中都是多文件依赖的目标，而没有单文件依赖的目标

### 编译的目标文件处理

接下来的部分，就属于真正的目标文件部分了。
```makefile
# List primitive targets that are compiled from source files
real-search = $(foreach m, $1, $(if $(call suffix-search, $m, $2, $3 -), $(call suffix-search, $m, $2, $3), $m))

# Replace multi-part objects by their individual parts,
# including built-in.a from subdirectories
# 将 obj-y/m 中后缀为 .o 的多依赖目标文件替换为后缀 -objs -y (-m) 变量的值，即将多依赖目标展开
real-obj-y := $(call real-search, $(obj-y), .o, -objs -y)
real-obj-m := $(call real-search, $(obj-m), .o, -objs -y -m)
```
总结：
这里 real-obj-y/m 中保存的就是多文件依赖的情况下真正需要编译的目标，关于 obj-y/m
中可能存在的目录目标，可以看后续的Makefile.build 文件介绍

### 设备树相关

```makefile
# DTB
# If CONFIG_OF_ALL_DTBS is enabled, all DT blobs are built
dtb-$(CONFIG_OF_ALL_DTBS)       += $(dtb-)

# Composite DTB (i.e. DTB constructed by overlay)
multi-dtb-y := $(call multi-search, $(dtb-y), .dtb, -dtbs)
# Primitive DTB compiled from *.dts
real-dtb-y := $(call real-search, $(dtb-y), .dtb, -dtbs)
# Base DTB that overlay is applied onto (each first word of $(*-dtbs) expansion)
base-dtb-y := $(foreach m, $(multi-dtb-y), $(firstword $(call suffix-search, $m, .dtb, -dtbs)))

always-y			+= $(dtb-y)
```
Kbuild系统同时支持设备树的编译，除了编译器使用的是dtc，其他编译操作几乎是一致的。

### 添加路径

```makefile
# Add subdir path

extra-y		:= $(addprefix $(obj)/,$(extra-y))
always-y	:= $(addprefix $(obj)/,$(always-y))
targets		:= $(addprefix $(obj)/,$(targets))
obj-m		:= $(addprefix $(obj)/,$(obj-m))
lib-y		:= $(addprefix $(obj)/,$(lib-y))
real-obj-y	:= $(addprefix $(obj)/,$(real-obj-y))
real-obj-m	:= $(addprefix $(obj)/,$(real-obj-m))
multi-obj-m	:= $(addprefix $(obj)/, $(multi-obj-m))
multi-dtb-y	:= $(addprefix $(obj)/, $(multi-dtb-y))
real-dtb-y	:= $(addprefix $(obj)/, $(real-dtb-y))
subdir-ym	:= $(addprefix $(obj)/,$(subdir-ym))
```
文件的处理最后，给所有的变量加上相应的路径，以便编译的时候进行索引。
Makefile中的 addprefix 函数指添加前缀.

关于上述添加路径部分的 `$(obj)`:
Makefile.lib 通常都被包含在于 Makefile.build中，这个变量继承了 Makefile.build 的
obj 变量。而 Makefile.build 的 obj 变量则是通过调用 `$(build)` 时进行赋值的。


# scripts/Makefile.build文件详解

在Kbuild系统中，Makefile.build文件是最重要的文件之一了，它控制着整个内核的核心编译部分。
关于 Makefile.build 的使用方法参考 scripts/Kbuind.include 一章中，对 build 变量的介绍

## 初始化变量

```makefile
// 编译进内核的文件(夹)列表
obj-y :=
// 编译成外部可加载模块的文件(夹)列表
obj-m :=
// 编译成库文件
lib-y :=
lib-m :=
// 总是需要被编译的模块
always-y :=
always-m :=
//编译目标
targets :=
// 下两项表示需要递归进入的子目录
subdir-y :=
subdir-m :=
//下列表示编译选项
EXTRA_AFLAGS   :=
EXTRA_CFLAGS   :=
EXTRA_CPPFLAGS :=
EXTRA_LDFLAGS  :=
asflags-y  :=
ccflags-y  :=
rustflags-y :=
cppflags-y :=
ldflags-y  :=
//子目录中的编译选项
subdir-asflags-y :=
subdir-ccflags-y :=
```
上述变量就是当前内核编译需要处理的变量，在此处进行初始化，通常最主要的就是 obj-y
和 obj-m 这两项，分别代表需要被编译进内核的模块和外部可加载模块。
lib-y 并不常见，通常地，它只会重新在 lib/目录下，其他部分我们在后文继续解析。

## 包含文件

初始化完成基本变量之后，我们接着往下看：
```makefile
# 将 obj 变量赋值给 src。一般情况下，`$(build)`的使用方式为：
# `$Q$MAKE $(build)=scripts/basic,$(build)`展开之后 obj 就被赋值为 scripts/basic。
src := $(obj)

# Read auto.conf if it exists, otherwise ignore
# 尝试包含 include/config/auto.conf
# 命令前面加了一个小减号的意思就是，也许某些文件出现问题，但不要管，继续做后面的事
# include/config/auto.conf 这个文件并非内核源码自带，而是在运行 make menuconfig
# 之后产生的，可以参考框架解析中对 .config 文件加载的分析。
-include include/config/auto.conf

# 该文件中包含大量通用函数和变量的实现
include $(srctree)/scripts/Kbuild.include
include $(srctree)/scripts/Makefile.compiler
include $(kbuild-file)
# Makefile.lib 则决定了哪些文件需要编译，哪些目录需要递归进入
include $(srctree)/scripts/Makefile.lib
```

## $(build)对目标文件夹的处理

处理通过 `$(build)=scripts/basic` 传入的 obj = scripts/basic目录，获取该目录的
相对位置.如果该目录下存在Kbuild文件，则包含该 Kbuild (在Kbuild系统中，make优先
处理 Kbuild 而不是 Makefile )文件，否则查看是否存在 Makefile文件，如果存在Makefile，
就包含Makefile文件，如果两者都没有，就是一条空包含指令。实现如下：
```makefile
include $(kbuild-file)
```
这一部分指令的意义在于：
`$(build)= scripts/basic` 相当于 `make -f $(srctree)/scripts/Makefile.build obj=scripts/basic`,
如果 obj=scripts/basic 下存在 Makefile 或者 Kbuild 文件，就默认以该文件的第一个
有效目标作为编译目标。如果指定目标 Makfile 下不存在有效目标(很可能对应 Makefile
中只是定义了需要编译的文件)，就默认以`$(srctree)/scripts/Makefile.build` 下的第一个
有效目标作为编译目标。

对于Kbuild和Makefile的选择见如下代码：
```Makefile
kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
kbuild-file := $(if $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Kbuild,$(kbuild-dir)/Makefile)
```

同样的，遵循make的规则，用户也可以指定编译目标，比如：
```makefile
scripts_basic:
    $(Q)$(MAKE) $(build)=scripts/basic $@
```
这样，该规则展开就变成了：
```makefile
scripts_basic:
    $(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/basic scripts_basic
```
表示指定编译 scripts/basic/Kbuild 或 scripts/basic/Makefile 文件下的scripts_basic
目标(这里仅为讲解方便，实际上scripts/basic/Makefile中并没有scripts_basic目标)。



## $(subdir-ym)(递归)

在 Makefile.lib 中，这个变量记录了所有在 obj-y/m 中指定的目录部分，同样在本文件中
可以找到它的定义：
```makefile
# Descending
# ---------------------------------------------------------------------------

PHONY += $(subdir-ym)
$(subdir-ym):
	$(Q)$(MAKE) $(build)=$@ \
	need-builtin=$(if $(filter $@/built-in.a, $(subdir-builtin)),1) \
	need-modorder=$(if $(filter $@/modules.order, $(subdir-modorder)),1) \
	$(filter $@/%, $(single-subdir-goals))
```
<font color=red>
这是整个Makefile.build中最重要的一部分，这决定Kbuild系统到底是以怎样的策略递归
进入每个子目录中
</font>

对于每个需要递归进入编译的目录，都对其调用：
```makefile
$(Q)$(MAKE) $(build)=$@ \
need-builtin=$(if $(filter $@/built-in.a, $(subdir-builtin)),1) \
need-modorder=$(if $(filter $@/modules.order, $(subdir-modorder)),1) \
$(filter $@/%, $(single-subdir-goals))
```
也就是递归地进入并编译该目录下的文件，基本上，大多数子目录下的 Makefile 并非起编译
作用，而只是添加文件。

所以，大多数情况下，都是执行了Makefile.build 中的 build 默认目标进行编译。所以，
当我们编译某个目录下的源代码，例如 drivers/ 时，主要的步骤大概是这样的：
1. 由top Makefile 调用 \$(build) 指令，此时，obj = drivers/
2. 在 Makefile.build 中包含 drivers/Makefile，drivers/Makefile 中的内容为：添加
   需要编译的文件和需要递归进入的目录，赋值给obj-y/m，并没有定义编译目标，所以默认
   以 Makefile.build 中 build 作为编译目标。
3. Makefile.build 包含 Makefile.lib ,Makefile.lib 对 drivers/Makefile 中赋值的
   obj-y/m 进行处理，确定 real-obj-y/m subdir-ym 的值。
4. 回到 Makefile.build ， 默认目标，编译 real-obj-y/m 等一众目标文件
5. 执行 $(subdir-ym) 的编译，递归地进入 subdir-y/m 进行编译。
6. 最后，直到没有目标可以递归进入，在递归返回的时候生成 built-in.a 文件，每一个
   目录下的 built-in.a 静态库都包含了该目录下所有子目录中的 built-in.a 和`*.o`文件，
   所以，在最后统一链接的时候，vmlinux.o 只需要链接源码根目录下的几个主要目录下的
   built-in.a 即可，比如 drivers, init.

## $(always)

相对来说，`$(always)`的出场率并不高，而且不会被统一编译，在寥寥无几的几处定义中，
一般伴随着它的编译规则，这个变量中记录了所有在编译时每次都需要被编译的目标。




# make config 文件处理

## .config 文件的生成

kernel的配置文件(.config)在 scripts/kconfig/Makefile 中生成，通过以下命令查看帮助
```shell
make -f /scripts/kconfig/Makefile help
```

在顶层Makefile 中执行以下操作，使得 kconfig 得以启动
```makefile
config: outputmakefile scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

%config: outputmakefile scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
```

在 scripts/kconfig/Makefile 中可以看到启动过程
```makefile
define config_rule
PHONY += $(1)
$(1): $(obj)/$($(1)-prog)
	$(Q)$$< $(silent) $(Kconfig)

PHONY += build_$(1)
build_$(1): $(obj)/$($(1)-prog)
endef

$(foreach c, config menuconfig nconfig gconfig xconfig, $(eval $(call config_rule,$(c))))
```

## .config 文件的加载

config/auto.conf 是 .config 的镜像，如果 .config 更新则更新它
```makefile
KCONFIG_CONFIG	?= .config
export KCONFIG_CONFIG

# The actual configuration files used during the build are stored in
# include/generated/ and include/config/. Update them if .config is newer than
# include/config/auto.conf (which mirrors .config).
#
# This exploits the 'multi-target pattern rule' trick.
# The syncconfig should be executed only once to make all the targets.
# (Note: use the grouped target '&:' when we bump to GNU Make 4.3)
#
# Do not use $(call cmd,...) here. That would suppress prompts from syncconfig,
# so you cannot notice that Kconfig is waiting for the user input.
%/config/auto.conf %/config/auto.conf.cmd %/generated/autoconf.h %/generated/rustc_cfg: $(KCONFIG_CONFIG)
	$(Q)$(kecho) "  SYNC    $@"
	$(Q)$(MAKE) -f $(srctree)/Makefile syncconfig
```

在顶层 Makefile 中 加载 auto.conf
```makefile
ifdef need-config
include include/config/auto.conf
endif
```

在scripts/Makefile.build 中也有加载
```makefile
-include include/config/auto.conf
```

## 加载子目录中的Makefile(关于递归)

```makefile
PHONY += $(subdir-ym)
$(subdir-ym):
	$(Q)$(MAKE) $(build)=$@ \
	$(if $(filter $@/, $(KBUILD_SINGLE_TARGETS)),single-build=) \
	need-builtin=$(if $(filter $@/built-in.a, $(subdir-builtin)),1) \
	need-modorder=$(if $(filter $@/modules.order, $(subdir-modorder)),1)
```

以上这段代码是Makefile.build 中的，在这里执行 `$(Q)$(MAKE) $(build)=$@` 相当于执行
```makefile
$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=$@
```
其中 `$@` 表示目标文件中的每一个值，也就是说，该语句会对每一个目标文件执行一次这条
语句，实现递归在Kbuild.include 中可以看到 `build` 的定义
```makefile
build := -f $(srctree)/scripts/Makefile.build obj
```

# 编译 vmlinux （待完善）

在内核根目录执行 make，默认的目标是__all，但是根据不同的条件，__all 被赋予不同的值
```makefile
# That's our default target when none is given on the command line
PHONY := __all
__all:
```

这里针对 vmlinux 进行分析
```makefile
# The all: target is the default when no target is given on the
# command line.
# This allow a user to issue only 'make' to build a kernel including modules
# Defaults to vmlinux, but the arch makefile usually adds further targets
all: vmlinux
```

对于 vmlinux，在Top Makefile 中有如下处理
```makefile
PHONY += vmlinux
# LDFLAGS_vmlinux in the top Makefile defines linker flags for the top vmlinux,
# not for decompressors. LDFLAGS_vmlinux in arch/*/boot/compressed/Makefile is
# unrelated; the decompressors just happen to have the same base name,
# arch/*/boot/compressed/vmlinux.
# Export LDFLAGS_vmlinux only to scripts/Makefile.vmlinux.
#
# _LDFLAGS_vmlinux is a workaround for the 'private export' bug:
#   https://savannah.gnu.org/bugs/?61463
# For Make > 4.4, the following simple code will work:
#  vmlinux: private export LDFLAGS_vmlinux := $(LDFLAGS_vmlinux)
vmlinux: private _LDFLAGS_vmlinux := $(LDFLAGS_vmlinux)
vmlinux: export LDFLAGS_vmlinux = $(_LDFLAGS_vmlinux)
vmlinux: vmlinux.o $(KBUILD_LDS) modpost
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.vmlinux
```

## 内核编译文件说明

在 Linux 内核编译过程中，会生成很多中间文件用于辅助构建和记录构建信息。以下是一些
常见的内核编译中间文件及其含义说明：

### 常见编译生成文件说明

| 文件名           | 含义                                                                                      |
| ---------------- | ----------------------------------------------------------------------------------------- |
| `*.o`            | 编译后的目标文件（object file），由源代码 `.c` 或 `.S` 文件生成。                         |
| `*.ko`           | 内核模块（kernel object），可以使用 `insmod` 加载的模块。                                 |
| `*.cmd`          | 保存了构建该目标的完整命令行（由 Makefile 自动生成），Make 会根据它判断是否需要重新编译。 |
| `*.mod.c`        | 为内核模块自动生成的源码文件，包含模块初始化、许可证信息等。                              |
| `*.mod.o`        | `.mod.c` 编译生成的目标文件，是模块构建的一部分。                                         |
| `*.mod`          | 模块注册信息，列出一个模块依赖的其他模块名，主要供 `depmod` 使用。                        |
| `.tmp_versions/` | 暂存每个模块的 `.mod` 文件的目录，用于构建依赖检查。                                      |
| `*.symversions`  | 包含模块导出的符号及其校验信息，支持模块间版本匹配检查。                                  |
| `*.order`        | 描述模块的加载顺序，`modules.order` 是所有模块按顺序排列的列表（用于生成最终模块包）。    |
| `Module.symvers` | 汇总所有模块导出的符号（用于 symbol versioning），在模块之间共享接口时非常重要。          |
| `built-in.o`     | 内核内建模块(CONFIG_XXX=y)才有，当前目录下所有编译进内核的对象文件合并后的目标文件        |
| `built-in.a`     | 内核内建模块(CONFIG_XXX=y)才有，所有模块以静态库方式打包（供链接 vmlinux）                |

built-in.x 实际上作用的是当前目录以及当前目录一下的内容，例如：
```text
dir1 +-- built-in.x
     |
     +-- dir2 +-- foo.c
     |        |
     |        +-- foo2.c
     |
     +-- dir3 +-- foo3.c
              |
              +-- foo4.c

这里 built-in.x 将 dir2、dir3 中的内容进行统一汇总
```

常见隐藏文件：

| 文件名               | 含义                                             |
| -------------------- | ------------------------------------------------ |
| `.hello.o.cmd`       | hello.o 的构建命令及依赖列表                     |
| `.depend` / `.tmp_*` | 依赖临时文件，用于判断是否需要重新构建           |
| `.version`           | 内核版本号，每次 `make` 增加（用于区分不同编译） |
| `.config`            | 内核配置文件（用于决定哪些模块内建/模块化）      |
| `.tmp_versions/`     | 存放所有模块的 `.mod` 文件（模块状态信息）       |

`.tmp_versions/` 详细说明：
* 每个模块编译后，会生成 `<module>.mod` 放入这里
* 内容是类似于 `modinfo` 结构的数据
* 最终会被用于生成 `modules.dep`, `modules.symbols` 等

`.version` 文件举例：
* 内容是一个数字（如 `5`）
* 每次运行 `make`，版本号 +1
* 用于给 `vmlinux` 等加版本标识，例如 `vmlinux-5`


### 内核模块构建全过程解析（基于 kbuild 系统）

以一个简单模块 `hello.c` 为例，搭配如下的 `Makefile`：

```makefile
obj-m := hello.o
```

执行：

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

#### 1. 预处理/依赖处理阶段

`hello.c` → `hello.o`

kbuild 首先会为 `hello.c` 生成 `.o` 文件：

```bash
gcc -D__KERNEL__ -DMODULE ... -c hello.c -o hello.o
```

生成文件：

* `hello.o`：目标文件
* `hello.cmd`：保存这条构建命令，供下一次 make 判断是否重新编译

> `hello.cmd` 的作用是记录 **上次构建的详细命令和参数**。下次 `make` 会读取 `.cmd`
> 与当前参数比对，决定是否重新编译。


#### 2. 生成模块元信息

生成 `hello.mod.c`

kbuild 自动生成 `hello.mod.c`，它包含模块结构描述：

```c
struct module __this_module = {
    .name = "hello",
    .init = init_module,
    .exit = cleanup_module,
};
```

然后编译它：

```bash
gcc -c hello.mod.c -o hello.mod.o
```

生成文件：

* `hello.mod.o`
* `hello.mod.c`
* `hello.mod.o.cmd`


#### 3. 链接阶段

链接 `hello.o` 和 `hello.mod.o`：

```bash
ld -r hello.o hello.mod.o -o hello.ko
```

生成：

* `hello.ko`：最终的内核模块


#### 4. 生成依赖与版本信息

* **`hello.symversions`**：包含本模块导出的符号及版本信息（供符号版本检查使用）
* **`.tmp_versions/hello.mod`**：中间记录模块状态的文件


#### 5. 模块顺序信息

* **`hello.order`**：模块在构建时的顺序（最终会被追加到 `modules.order`）
* **`modules.order`**：构建目录下所有模块的有序列表


#### 6. 导出符号汇总

* **`Module.symvers`**：汇总本次编译过程中所有导出的符号（提供给其他模块使用）

如果模块使用了 `EXPORT_SYMBOL()`，则该符号和其校验信息将写入此文件。


#### 构建过程总结流程图

```text
hello.c ─────┐
             │     (GCC 编译)
             └──> hello.o ─────┐
                               │
hello.mod.c ──> hello.mod.o ───┼───(LD 链接)───> hello.ko
                               │
        (符号导出/依赖分析)    ↓
        ┌──────────────┐
        │ hello.symversions
        │ .tmp_versions/
        │ hello.order
        └──────────────┘
```

#### 额外说明：Makefile 指令解释

* `obj-m := hello.o`
  告诉 kbuild：`hello.o` 是一个模块（模块会被构建为 `hello.ko`）

* `M=$(pwd)`
  指定编译当前目录中的模块

* `-C /lib/modules/$(uname -r)/build`
  切换到当前系统内核的源码构建目录（通常是 `/usr/src/linux-*` 的符号链接）


### 符号版本机制：`EXPORT_SYMBOL` 与 `Module.symvers`

#### 背景

模块之间可能会互相调用函数/变量（例如网络协议栈使用驱动层函数）。为了确保模块与
内核或其他模块之间的 **接口兼容性**，内核使用符号版本机制进行符号校验。

#### 常见文件与宏

| 内容                      | 作用                               |
| ------------------------- | ---------------------------------- |
| `EXPORT_SYMBOL(name)`     | 向其他模块导出符号（函数或变量）   |
| `EXPORT_SYMBOL_GPL(name)` | 仅对 GPL 模块导出符号              |
| `Module.symvers`          | 记录导出的符号名及其校验值（CRC）  |


#### 示例：

```c
// foo.c
int foo_add(int x, int y) {
    return x + y;
}
EXPORT_SYMBOL(foo_add);
```

生成的 `Module.symvers` 中将包含：

```
0xA1B2C3D4  foo_add  /path/to/foo.o
```

#### 使用意义：

* 编译 bar.ko 使用 `foo_add()` 时，编译器检查符号版本一致性。
* 如果符号不匹配（例如接口变了），编译或加载时会报错，避免运行时崩溃。


### 内核内建模块 vs 外部模块的编译差异

| 比较项         | 内建模块                                     | 外部模块（Out-of-tree）          |
| -------------- | -------------------------------------------- | -------------------------------- |
| 编译方式       | `make menuconfig` + `make`                   | `make -C /lib/modules/... M=...` |
| 配置方式       | 配置进内核 `.config` 中（如 `CONFIG_FOO=y`） | 使用 `obj-m += xxx.o` 指定       |
| 是否进 vmlinux | 是（被链接进内核映像）                       | 否（独立 `*.ko`）                |
| 动态加载       | 否（内核启动时即加载）                       | 是（运行时可 insmod/modprobe）   |


#### 示例：外部模块编译

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

编译出：

* `hello.ko`（可插拔模块）
* `Module.symvers`
* `.mod.o`、`.cmd`、`.order` 等中间文件


### `make modules_install` 的作用

#### 命令：

```bash
sudo make modules_install
```

#### 作用：

* 将所有编译出的 `.ko` 模块安装到 `/lib/modules/$(uname -r)/kernel/` 下合适位置。
* 更新模块索引文件（供 `modprobe` 使用）：

```bash
sudo depmod -a
```

#### 安装后的文件结构：

```
/lib/modules/$(uname -r)/
│
├── kernel/
│   ├── drivers/
│   │   └── mydriver.ko
│   └── ...
├── modules.dep
├── modules.symbols
├── modules.order
└── ...
```

这些文件被 `modprobe`、`insmod`、`depmod` 用来自动查找和管理模块。


### 模块加载、卸载与运行时管理

#### 加载模块方式：

| 工具              | 功能                         |
| ----------------- | ---------------------------- |
| `insmod <mod>.ko` | 直接加载模块（不会处理依赖） |
| `modprobe <mod>`  | 自动加载模块和依赖（推荐）   |


#### 卸载模块：

```bash
rmmod <mod>
```

#### 运行时查看模块：

```bash
cat /proc/modules
```

输出示例：

```
hello  16384  0 - Live 0x0000000000000000
```

含义：

* 模块名：`hello`
* 大小：占用内存大小
* 使用计数（`0` 表示无人依赖）
* 状态（Live/Loading等）
* 地址（加载到内核的虚拟地址）


#### 其他工具：

* `lsmod`：列出所有已加载模块（解析自 `/proc/modules`）
* `modinfo hello.ko`：查看模块元信息（作者、版本、license 等）
* `dmesg`：查看模块加载日志输出（如 `printk()` 打印）


### 总结图：各种文件的角色归类

```text
               +-------------------+
               |  源代码 (.c/.h)   |
               +--------+----------+
                        |
                        ▼
           +------------+------------+
           |      编译生成的产物      |
           +------------+------------+
                        |
        +---------------+--------------+
        |                              |
        ▼                              ▼
+---------------+           +-----------------------+
|  build-in.o   |           |       xxx.ko          |
| (内建模块)    |           |     (可加载模块)      |
+---------------+           +-----------------------+
| xxx.cmd       |           | xxx.cmd               |
+---------------+           +-----------------------+
| .mod.c        |           | .mod.c                |
| xxx.mod.o     |           | xxx.mod.o             |
| xxx.mod       |           | xxx.mod               |
+---------------+           +-----------------------+
| Module.symvers|           | Module.symvers        |
+---------------+           +-----------------------+
| xxx.order     |           | xxx.order             |
+---------------+           +-----------------------+
| built-in.a    |
| built-in.cmd  |
+---------------+

依赖追踪文件：.cmd, .depend, .version, Module.symvers ...
```


# Tips

## quiet prn
经常会看见这样的写法：
`$(Q)$(MAKE)`
这里的变量 Q 与是否 quite 打印有关，可以看顶层makefile前边的定义

## FORCE
经常在Makefile中能看到这样的依赖:
```makefile
foo : FORCE
    ...
```
这算是Makefile中的一个使用技巧，FORCE的定义是这样的：
```makefile
FORCE:
```
是的，它是一个目标，没有依赖文件且没有命令部分，由于它没有命令生成FORCE，所以每次
都会被更新。所以它的作用就是：当FORCE作为依赖时，就导致依赖列表中每次都有FORCE依赖
被更新，导致目标每次被重新编译生成。
