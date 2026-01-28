<参考资料>
[Kernel Build System](https://docs.kernel.org/kbuild/index.html)
[Linux Kernel Makefiles](https://docs.kernel.org/kbuild/makefiles.html)
[博客索引页 重要](https://zhuanlan.zhihu.com/p/362640343)


# Kbuild系统框架概览

## make 和 Makefile
linux内核源代码的编译使用make工具和Makefile，但是它在普通的C程序编译的基础上对配置
和编译选项进行了扩展，这就是kbuild系统，专门针对linux的内核编译，使得linux内核的
编译更加简洁而高效。

## linux的内核镜像文件

首先我们需要认识一下linux内核镜像的各种形式，毕竟编译内核最主要的目的就是生成内核
镜像，它有几种形式：vmlinux、vmlinux.bin、vmlinuz、zImage、bzImage。

* vmlinux：这是编译linux内核直接生成的原始镜像文件，它是由静态链接之后生成的可执行
  文件，但是它一般不作为最终的镜像使用，不能直接boot启动，用于生成vmlinuz，可以
  debug的时候使用。
* vmlinux.bin：与vmlinux相同，但采用可启动的原始二进制文件格式。丢弃所有符号和
  重定位信息。通过 objcopy -O binary vmlinux vmlinux.bin 从vmlinux生成。
* vmlinuz：由 vmlinux 经过 gzip(也可以是bzip)压缩而来，同时在vmlinux的基础上进一步
  添加了启动和解压缩代码,是可以引导boot启动内核的最终镜像。vmlinuz通常被放置在/boot
  目录，/boot目录下存放的是系统引导需要的文件，同时vmlinuz文件解压出的vmlinux不带
  符号表的目标文件，所以一般 /boot 目录下会带一个符号表 System.map 文件。
* zImage：这是小内核的旧格式，有指令make zImage生成，仅适用于只有640K以下内存的linux kernel文件。
* bzImage: big zImage,需要注意的是这个 bz 与 bzip 没有任何关系，适用于更大的 linux
  kernel 文件。现代处理器的 linux 镜像有一部分是生成 bzImage 文件，同时，vmlinuz
  和bzImage 是同一类型的文件，一般情况下这个和 vmlinuz 是同一个东西。

对于这一系列的生成文件可以参考[官方文档](http://www.linfo.org/vmlinuz.html)


## kbuild系统

### 各种各样的makeifle文件

在linux中，由于内核代码的分层模型，以及兼容很多平台的特性，Makefile文件分布在各个
目录中，对每个模块进行分离编译，降低耦合性，使编译方式更加灵活。

Makefile主要是以下五个部分：

* 顶层Makefile : 在源代码的根目录有个顶层Makefile，顶层Makefile的作用就是负责生成
  两个最重要的部分：编译生成vmlinux和各种模块。
* .config文件 : 这个config文件主要是由用户对内核模块的配置产生，有三种配置方式：
    * 编译进内核
    * 编译成可加载模块
    * 不进行编译。
* arch/\$(ARCH)/Makefile : 从目录可以看出，这个 Makefile 主要是根据指定的平台对
  内核镜像进行相应的配置,提供平台信息给顶层 Makefile。
* scirpts/Makefile.* : 这些 Makefile 配置文件包含了构建内核的规则。
* kbuild Makefiles : 每一个模块都是单独被编译然后再链接的，所以这种 kbiuld Makefile
  几乎在每个模块中都存在.在这些模块文件(子目录)中，也可以使用 Kbuild 文件代替 Makefile，
  当两者同时存在时，优先选择 Kbuild 文件进行编译工作，只是用户习惯性地使用 Makefile 来命名。

### kbuild Makefile

#### 编译进内核的模块

如果需要将一个linux源码中自带的模块配置进内核，需要在Makefile中进行配置：
```makefile
obj-y += foo.o
```
将foo.o编译进内核，根据make的自动推导原则，make将会自动将foo.c编译成foo.o。

上述方式基本上用于开发时的模块单独编译，当需要一次编译整个内核时，通常是在Makefile
中这样写：
```makefile
obj-$(CONFIG_FOO) += foo.o
```
在 .config 文件中将 CONFIG_FOO 变量配置成y，当需要修改模块的编译行为时，就可以
统一在配置文件中修改，而不用到 Makefile 中去找。

kbuild 编译所有的 obj-y 的文件，然后调用 `$(AR) rcSTP` 将所有被编译的目标文件进行
打包，打包成 built-in.a 文件，需要注意的是这仅仅是一份压缩版的存档，这个目标文件
里面并不包含符号表，既然没有符号表，它就不能被链接。

紧接着调用scripts/link-vmlinux.sh，将上面产生的不带符号表的目标文件添加符号表和索引，
作为生成vmlinux镜像的输入文件，链接生成vmlinux。

对于这些被编译进内核的模块，模块排列的顺序是有意义的，允许一个模块被重复配置，系统
将会取用第一个出现的配置项，而忽略随后出现的配置项，并不会出现后项覆盖前项的现象。

链接的顺序同时也是有意义的，因为编译进内核的模块通常由xxx_initcall()来描述，内核对
这些模块分了相应的初始化优先级，相同优先级的模块初始化函数将会被依次放置在同一个段
中，而这些模块执行的顺序就取决于放置的先后顺序，由链接顺序所决定。


#### 编译可加载的模块

所有在配置文件中标记为-m的模块将被编译成可加载模块.ko文件。

如果需要将一个模块配置为可加载模块，需要在Makefile中进行配置：
```makefile
obj-m += foo.o
```
同样的，通常可以写成这样的形式：
```makefile
obj-$(CONFIG_FOO) += foo.o
```
在.config文件中将 CONFIG_FOO 变量配置成m,在配置文件中统一控制，编译完成时将会在当前
文件夹中生成foo.ko文件，在内核运行时使用insmod或者是modprobe指令加载到内核。


#### 模块编译依赖多个文件

通常的，驱动开发者也会将单独编译自己开发的驱动模块，当一个驱动模块依赖多个源文件时，
需要通过以下方式来指定依赖的文件：
```makefile
obj-m += foo.o
foo-y := a.o b.o c.o
```
foo.o 由a.o,b.o,c.o生成，然后调用$(LD) -r 将a.o,b.o,c.o链接成foo.o文件。

同样地，Makefile支持以变量的形式来指定是否生成foo.o,我们可以这样:
```makefile
obj-$(CONFIG_FOO) += foo.o
foo-$(CONFIG_FOO_XATTR) += a.o b.o c.o
```
根据CONFIG_FOO_XATTR(.config文件中)的配置属性来决定是否生成foo.o,然后根据CONFIG_FOO
属性来决定将foo.o模块编入内核还是作为模块。


#### Makefile目录层次关系的处理
需要理解的一个原则就是：一个Makefile只负责处理本目录中的编译关系，自然地，其他目录中
的文件编译由其他目录的Makefile负责，整个linux内核的Makefile组成一个树状结构，对于上层
Makefile的子目录而言，只需要让kbuild知道它应该怎样进行递归地进入目录即可。

kbuild利用目录指定的方式来进行目录指定操作，举个例子：
```makefile
obj-$(CONFIG_FOO) += foo/
```
当CONFIG_FOO被配置成y或者m时，kbuild就会进入到foo/目录中，但是需要注意的是，
这个信息仅仅是告诉kbuild应该进入到哪个目录，而不对其目录中的编译做任何指导。

#### 编译选项
**需要注意的是，在之前的版本中，编译的选项由EXTRA_CFLAGS, EXTRA_AFLAGS和 EXTRA_LDFLAGS
修改成了ccflags-y asflags-y和ldflags-y.**

**ccflags-y asflags-y和ldflags-y**
ccflags-y asflags-y和ldflags-y这三个变量的值分别对应编译、汇编、链接时的参数。

同时，所有的ccflags-y asflags-y和ldflags-y这三个变量只对有定义的Makefile中使用，
简而言之，这些flag在Makefile树中不会有继承效果，Makefile之间相互独立。

**subdir-ccflags-y, subdir-asflags-y**
这两个编译选项与ccflags-y和asflags-y效果是一致的，只是添加了subdir-前缀，意味着
这两个编译选项对本目录和所有的子目录都有效。

**CFLAGS_\$@, AFLAGS_\$@**
使用CFLAGS_或者AFLAGS_前缀描述的模块可以为模块的编译单独提供参数，举个例子:

CFLAGS_foo.o = -DAUTOCONF
在编译foo.o时，添加了-DAUTOCONF编译选项。


#### kbuild中的变量

顶层Makefile中定义了以下变量：

**KERNELRELEASE**
这是一个字符串，用于构建安装目录的名字(一般使用版本号来区分)或者显示当前的版本号。

**ARCH**
定义当前的目标架构平台，比如:"X86"，"ARM",默认情况下，ARCH的值为当前编译的主机架构,
但是在交叉编译环境中，需要在顶层Makefile或者是命令行中指定架构：
```makefile
make ARCH=arm ...
```
**INSTALL_PATH**
指定安装目录，安装目录主要是为了放置需要安装的镜像和map(符号表)文件，系统的启动
需要这些文件的参与。

**INSTALL_MOD_PATH, MODLIB**
INSTALL_MOD_PATH：为模块指定安装的前缀目录，这个变量在顶层Makefile中并没有被定义，
用户可以使用，MODLIB为模块指定安装目录.

默认情况下，模块会被安装到`$(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)`中，
默认INSTALL_MOD_PATH不会被指定，所以会被安装到/lib/modules/$(KERNELRELEASE)中。

**INSTALL_MOD_STRIP**
如果这个变量被指定，模块就会将一些额外的、运行时非必要的信息剥离出来以缩减模块的
大小，当INSTALL_MOD_STRIP为1时，--strip-debug选项就会被使用，模块的调试信息将被删除，
否则就执行默认的参数，模块编译时会添加一些辅助信息。

这些全局变量一旦在顶层Makefile中被定义就全局有效，但是有一点需要注意，在驱动开发时，
一般编译单一的模块，执行make调用的是当前目录下的Makefile.

在这种情况下这些变量是没有被定义的，只有先调用了顶层Makefile之后，这些变量在子目录中
的Makefile才被赋值。

#### 生成header文件

vmlinux中打包了所有模块编译生成的目标文件，在驱动开发者眼中，在内核启动完成之后，
它的作用相当于一个动态库，既然是一个库，如果其他开发者需要使用里面的接口，就需要
相应的头文件。

自然地，build也会生成相应的header文件供开发者使用，一个最简单的方式就是用下面这个
指令：
```makefile
make headers_install ARCH=arm INSTALL_HDR_PATH=/DIR
```
ARCH：指定CPU的体系架构，默认是当前主机的架构，可以使用以下命令查看当前源码支持
哪些架构：
```shell
ls -d include/asm-* | sed 's/.*-//'
```
INSTALL_HDR_PATH：指定头文件的放置目录，默认是./usr。

至此，build工具将在指定的DIR目录生成基于arm架构的头文件，开发者在开发时就可以引用
这些头文件。


## 小结

为了清晰地了解kbuild的执行，有必要对kbuild的执行过程做一下梳理：

* 根据用户(内核)的配置生成相应的.config文件
* 将内核的版本号存入include/linux/version.h
* 建立指向 include/asm-$(ARCH) 的符号链接，选定平台
* 更新所有编译所需的文件。
* 从顶层Makefile开始，递归地访问各个子目录，对相应的模块编译生成目标文件
* 链接过程，在源代码的顶层目录链接生成vmlinux
* 根据具体架构提供的信息添加相应符号，生成最终的启动镜像，往往不同架构之间的启动方式不一致。
    * 这一部分包含启动指令
    * 准备initrd镜像等平台相关的部分。

# kbuild 框架概览

在上一个章节中根据内核 Kbuild 文档对 Kbuild 框架进行了一个基本的介绍，由此对 Kbuild
有了一个基本的了解，这一章，将要从源码文件的角度上，梳理整个 Kbuild 系统的框架实现。

## Kbuild 系统做了什么

首先需要了解的一个事就是： Kbuild系统做了什么？

Kbuild 系统并非仅仅是完成一个常规 Makefile 的工作，除了我们以为的内核编译，它还同时
完成以下的工作：
* 输出内核与模块符号
* 输出内核头文件，用户头文件
* 编译 dts 设备树文件
* 安装编译生成的文件到系统
* ....

可以说 Kbuild 系统几乎覆盖了整个内核的配置、编译、安装、系统裁剪等等。

## 参与编译的文件总览

这里列出所有参与编译的文件与编译生成的文件，然后对他们的作用做一个概括性地解释，
在后续的章节中我们将一步步地对源码细节部分进行详细解析。


### 控制文件

#### Top Makefile

从语法和使用上 top Makefile 和其他的子目录下 Makefile 并无二致，为什么要单独列出来
呢，因为它是一切的开始，控制着整个内核编译过程。

通常情况下，对于内核编译，我们用得最多的，也几乎只用到的几条指令：
```shell
make *config
make
make install
....
```
按照博主的学习思路，学习一个较复杂框架的开始其实最好先不要进入其中，我们可以先把
Kbuild 系统看成一个黑匣子，看它到底提供了什么功能，当我们熟悉它的接口之后，自然就
会对它背后的机制和原理充满疑惑，这个时候我们就可以真正打开这个黑匣子，带着自己的
问题去研究。

按照上面的思路，我们可以先执行下面的命令：
```shell
make help
```
这样我们可以很快地了解内核编译所支持的各种参数，通常情况下，终端将输出一大堆help
选项,具体的输出结果都有较为详细的解释，这里不进行具体分析了。

#### scripts目录下控制文件

仅仅是靠 Makefile 的功能是很难完成整个内核的配置编译以及其他功能的，scripts/ 目录
下有相当多的脚本对整个内核编译进行控制，其中列出几个非常重要的文件：
* scripts/Kbuild.include : 定义了常用的一系列通用变量与函数，在 top Makefile 开始时
  就被 include 包含，作用于整个内核的编译过程。
* scripts/Makefile.build : 根据用户传入的参数完成真正核心的编译工作，包括编译目标的
  确定、递归进入子目录的编译工作等等，作用与整个内核的编译过程。
* scripts/Makefile.lib ：负责根据用户配置或者 top Makefile 传入的参数，对各类待编译
  文件进行分类处理，以确定最后需要被编译的文件、需要递归编译的子目录，将结果赋值给
  相应的变量以供真正的编译程序使用。
* scripts/link-vmlinux.sh : 对于每一个递归进入的编译目录，编译完成之后，都将在该
  目录下生成一个 build-in.a 文件，这个 build-in.a 文件就是由该目录下或子目录下需要
  编译进内核的模块打包而成，link-vmlinux.sh 将这些文件统一链接起来，生成对应的镜像。
* scripts/Makefile.host : 这个文件主要控制生成主机程序，严格来说，主机程序并不主导
  编译过程，它只是作为一种辅助软件，比如 menuconfig 在编译主机上的界面实现，fixdep
  检查工具(检查头文件依赖)等等。


#### 各级子目录下的Kconfig和Makefile

linux 下的 Kbuild 系统是一个分布式的编译系统，每个模块负责自己的编译和配置选项的
提供，这种模块化的分布可以支持非常方便的移植、裁剪和维护。

所以几乎每个目录下都存在 Makefile 和 Kconfig 文件， Kconfig 负责该模块下的配置工作，
Makefile 负责该模块下的编译工作。

**<font color=red>通常情况下，子目录下的 Makefile 并不负责编译工作，只是提供当前
目录下需要编译的目标文件或者需要递归进入的目录(arch目录除外)，交由 scripts/Makefile.build
和 scripts/Makefile.lib 统一处理。</font>**

#### 生成文件

在Makefile的编译过程中，将生成各类中间文件，通常情况下，大部分生成的中间文件是可以
不用关心的，只需要关注最后生成的 vmlinux，System.map, 系统dtb以及各类外部模块等启动
常用文件即可。

但是，如果要真正了解内核编译背后的原理，了解这些文件的作用是非常有必要的。下面列出
一些值得关注的生成文件：
* System.map : 该文件相当于镜像文件的符号表，记录了内核镜像中所有的符号地址，文件中
  对应的函数地址对应了程序运行时函数真实的地址，在调试的时候是非常有用的。
* */built-in.a : Kbuild系统会根据配置递归地进入到子目录下进行编译工作，最后将所有
  目标文件链接生成一个总的 vmlinux.o ,背后的实现机制是这样的： 对于某一个需要进入
  编译的目录，将在该目录下生成一个 built-in.a 文件，该 built-in.a 由本目录中所有
  目标文件以及一级子目录下的 built-in.a 文件使用 ar 指令打包而成。
其二级子目录下的所有目标文件以及该目录下的 built-in.a 将被打包进一级子目录的 built-in.a
文件中，依次递归。
到根目录下时，只需要将所有一级子目录中的 built-in.a 文件链接到一起即可。
* `.*.o.d` 和 `.*.o.cmd` : 不知道你有没有疑惑过内核 Kbuild 系统是如何处理头文件依赖
  问题的，它处理头文件依赖就是通过这两种文件来记录所有的依赖头文件，对于依赖的目标
  文件，自然是在编译规则中指定。
* modules.order 、 modules.build 和 modules.builtin.modinfo : 这两个文件主要负责
  记录编译的模块，modules.builtin.modinfo记录模块信息，以供 modprobe 使用。
* arch/$(ARCH)/boot : 通常在嵌入式的开发中，这个目录下的文件就是开发板对应的启动文件，
  会因平台的不同而有差异，一般包含这几部分：镜像、内核符号表、系统dtb。
* .config ：记录用户对内核模块的配置。
* include/generate/* : 内核编译过程将会生成一些头文件，其中比较重要的是 autoconf.h，
  这是 .config 的头文件版本，以及uapi/目录下的文件，这个目录下的保存着用户头文件。
* include/config/* ： 为了解决 autoconf.h 牵一发而动全身的问题(即修改一个配置导致
  所有依赖 autoconf.h 的文件需要重新编译)，将 autoconf.h 分散为多个头文件放在
  include/config/ 下，以解决 autoconf.h 的依赖问题。

# scripts/KBuild.include文件详解

在整个Kbuild系统中，scripts/Makefile.include 提供了大量通用函数以及变量的定义，
这些定义将被 Makefile.build 、Makefile.lib 和 top Makefile频繁调用，以实现相应的
功能,scripts/Makefile.include 参与整个内核编译的过程，是编译的核心脚本之一。

在研究整个Kbuild系统前，有必要先了解这些变量及函数的使用，才能更好地理解整个内核
编译的过程。

## $(build)

如果说重要性，这个函数的定义是当之无愧的榜首，我们先来看看它的定义：
```makefile
build := -f $(srctree)/scripts/Makefile.build obj
```
乍一看好像也是云里雾里，我们需要将它代入到具体的调用中才能详细分析，下面举几个
调用的示例：
```makefile
%config: scripts_basic outputmakefile FORCE
    $(Q)$(MAKE) $(build)=scripts/kconfig $@

$(vmlinux-dirs): prepare
    $(Q)$(MAKE) $(build)=$@ need-builtin=1
```
其中，第一个是 make menuconfig 的规则，执行配置，生成.config。引用自 top Makefile
第二个是直接执行 make，编译vmlinux的依赖vmlinux-dirs时对应的规则。

**示例讲解**
以第一个示例为例，将 `$(build)` 展开就是这样的：
```makefile
%config: scripts_basic outputmakefile FORCE
    $(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig $@
```
`$(Q)`是一个打印控制参数，决定在执行该指令时是否将其打印到终端。
`$(MAKE)`，为理解方便，我们可以直接理解为 make。

所以，当我们执行make menuconfig时，就是对应这么一条指令：
```makefile
menuconfig: scripts_basic outputmakefile FORCE
    make -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig menuconfig
```

**make -f $(srctree)/scripts/Makefile.build 部分**

在Makefile的规则中，make -f file，这个指令会将 file 作为Makefile 读入并解析，相当于
执行另一个指定的文件，这个文件不一定要以 makfile 命名，在这里就是执行
`$(srctree)/scripts/Makefile.build` 文件。

**obj=scripts/kconfig部分**

给目标makfile中 obj 变量赋值为scripts/kconfig，具体的处理在scripts/Makefile.build中。

在 scripts/Makefile.build 中, 因为包含的语句在第一个有效目标之前，所以当不指定
make 的目标时，将执行被包含的 Kbuild 或者 Makefile 中的默认目标，如果没有 Kbuild
或者 Makefile 文件或者文件中不存在目标，就会执行 scripts/Makefile.build 中的默认
目标`__build`。

在上述 menuconfig：的示例中，就是先执行scripts/kconfig/Makefile，然后执行该Makefile
中指定的目标：menuconfig。

**menuconfig部分**
指定编译的目标，这个目标不一定存在，如果不指定目标，就执行默认目标，在 Makefile
的规则中是 Makefile 中第一个有效目标。

**<font color=red>\$(build)功能小结</font>**
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
至于具体的实现，各位朋友可以自行研究。

## $(if_changed)

if_changed 指令也是当仁不让的核心指令，顾名思义，它的功能就是检查文件的变动。

在 Makefile 中实际的用法是这样的,
```makefile
foo:
    $(call if_changed,$@)
```
一般情况下 if_changed 函数被调用时，后面都接一个参数，那么这个语句是什么作用呢？
我们来看具体定义：
```makefile
if_changed = $(if $(strip $(any-prereq) $(arg-check)),                   \
    $(cmd);                                                              \
    printf '%s\n' 'cmd_$@ := $(make-cmd)' > $(dot-target).cmd, @:)


# 新版本的kennel会有些差异，但功能应该是差不多的
# Execute command if command has changed or prerequisite(s) are updated.
if_changed = $(if $(if-changed-cond),                                        \
	$(cmd);                                                              \
	printf '%s\n' 'cmd_$@ := $(make-cmd)' > $(dot-target).cmd, @:)
```

首先，是一个 if 函数判断，判断 `$(any-prereq)` `$(arg-check)` 是否都为空，如果两者
有一者不为空，则执行 $(cmd),否则打印相关信息。

其中，any-prereq 表示依赖文件中被修改的文件。

arg-check，顾名思义，就是检查传入的参数，检查是否存在 `cmd_$@`,至于为什么是`cmd_$@`，
我们接着看。


**any-prereq || arg-check 不为空**
当any-prereq || arg-check不为空时，就执行`$(cmd)`，那么 `$(cmd)` 是什么意思呢？
```makefile
echo-cmd = $(if $($(quiet)cmd_$(1)),\
    echo '  $(call escsq,$($(quiet)cmd_$(1)))$(echo-why)';)
cmd = @set -e; $(echo-cmd) $(cmd_$(1))
```
可以看到，cmd第一步执行echo-cmd，这个指令就是根据用户传入的控制参数(V=1|2)，决定是
否打印当前命令。

然后，就是执行`cmd_$(1)`，`$(1)` 是 if_changed 执行时传入的参数，所以在这里相当于
执行 cmd_foo。

另一个分支就是`echo ' $(call escsq,$($(quiet)cmd_$(1)))$(echo-why)'; )`，这是打印
结果到文件中，并不自行具体的编译动作。


**\$(if_changed)使用小结**
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

调用 if_changed 函数时传入的参数为 link-vmlinux，当依赖文件有更新时，将执行
cmd_link-vmlinux(该指令的详解可以在继续参考后续博客) 。

### 同类型函数

与`$(if_changed)`同类型的，还有`$(if_changed_dep),$(if_changed_rule)`。
```makefile
if_changed_dep = $(if $(strip $(any-prereq) $(arg-check)),$(cmd_and_fixdep),@:)
if_changed_rule = $(if $(strip $(any-prereq) $(arg-check)),$(rule_$(1)),@:)
```
if_changed_dep 与 if_changed 同样执行cmd_$1,不同的是它会检查目标的依赖文件列表是否
有更新。

需要注意的是，这里的依赖文件并非指的是规则中的依赖，而是指fixdep程序生成的 .*.o.cmd 文件。

而 if_changed_rule 与 if_changed 不同的是，if_changed 执行 `cmd_$1`,而 if_changed_rule
执行 `rule_$1` 指令。

## $(filechk)

同样，通过名称就可以看出，它的功能是检查文件，严格来说是先操作文件，再检查文件的更新。

它的定义是这样的：
```makefile
define filechk
    $(Q)set -e;             \
    mkdir -p $(dir $@);         \
    { $(filechk_$(1)); } > $@.tmp;      \
    if [ -r $@ ] && cmp -s $@ $@.tmp; then  \
        rm -f $@.tmp;           \
    else                    \
        $(kecho) '  UPD     $@';    \
        mv -f $@.tmp $@;        \
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

## 小结

如果需要深入 Kbuild 源码实现，我们必须先对这几个函数有相应的了解，因为在 Kbuild 的
Makefile 中存在着大量的这种函数调用。

# scripts/Makefile.lib 文件详解

在linux内核的整个Kbuild系统中，Makefile.lib 对于目标编译起着决定性的作用，如果说
Makefile.build 负责执行 make 的编译过程，而 Makefile.lib 则决定了哪些文件需要编译，
哪些目录需要递归进入。

接下来我们就来看看他们的具体实现。

## 源文件从哪里来

首先，既然是对编译文件进行处理，那么，第一个要处理的问题是：源文件从哪里来？

进入到各级子目录下,可以发现 Makefile 内容，我们以最熟悉的 drvier/i2c/Makefile 为例：
```makefile
....
obj-$(CONFIG_I2C_CHARDEV)   += i2c-dev.o
obj-$(CONFIG_I2C_MUX)       += i2c-mux.o
obj-y               += algos/ busses/ muxes/
obj-$(CONFIG_I2C_STUB)      += i2c-stub.o
obj-$(CONFIG_I2C_SLAVE_EEPROM)  += i2c-slave-eeprom.o
....
```
可以发现，一般的形式都是
```makefile
obj-$(CONFIG_VAR) += var.o
```
这个 CONFIG_VAR 在 make menuconfig 的配置阶段被设置，被保存在.config 中，对应的值
根据配置分别为：y , m 或 空。

对于某些系统必须支持的模块，可以直接写死在 obj-y 处。

还有一个发现就是：obj-y 或 obj-m 对应的并不一定是目标文件，也可以是目标目录，这些
目录同样会根据配置选项选择是否递归进入。

对于分散在各个目录下的Makefile，基本上都是这么个形式(除了arch/下的较特殊)。

## Makefile.lib内容解析

接下来我们直接打开 scripts/Makefile.lib 文件，逐句地对其进行分析。

### 编译标志位

Makefile.lib开头的部分是这样的：
```makefile
asflags-y  += $(EXTRA_AFLAGS)
ccflags-y  += $(EXTRA_CFLAGS)
cppflags-y += $(EXTRA_CPPFLAGS)
ldflags-y  += $(EXTRA_LDFLAGS)

KBUILD_AFLAGS += $(subdir-asflags-y)
KBUILD_CFLAGS += $(subdir-ccflags-y)
```
这些大多是一些标志位的设置，细节部分我们就不关注了，我们只关注框架流程部分。


## 目录及文件处理部分

### 去除重复部分

```makefile
// 去除obj-m中已经定义在obj-y中的部分
obj-m := $(filter-out $(obj-y),$(obj-m))
// 去除lib-y中已经定义在obj-y中的部分
lib-y := $(filter-out $(obj-y), $(sort $(lib-y) $(lib-m)))
```
这一部分主要是去重，如果某个模块已经被定义在obj-y中，证明已经或者将要被编译进内核，
就没有必要在其他地方进行编译了。

### modorder处理

```makefile
//将obj-y中的目录 dir 修改为 dir/modules.order赋值给modorder，
//将obj-m中的.o修改为.ko赋值给modorder。
modorder    := $(patsubst %/,%/modules.order,\
                 $(filter %/, $(obj-y)) $(obj-m:.o=.ko))
```
从 modorder 的源码定义来看，将 obj-y/m 的 %/ 提取出来并修改为 %/modules.order,比如
obj-y 中的 driver/ 变量，则将其修改为 driver/modules.order 并赋值给 modorder 变量。

同时，将所有的 obj-m 中的 .o 替换成 .ko 文件并赋值给 modorder。

那么，各子目录下的 modules.order 文件有什么作用呢？

官方文档解析为：This file records the order in which modules appear in Makefiles.
This is used by modprobe to deterministically resolve aliases that matchmultiple modules。

内核将编译的外部模块全部记录在 modules.order 文件中，以便 modprobe 命令在加载卸载
时查询使用。


### 目录的处理

在查看各级子目录下的 Makefile 时，发现 obj-y/m 的值并非只有目标文件，还有一些目标
文件夹，但是文件夹并不能被直接编译，那么它是被怎么处理的呢？带着这个疑问接着看：
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
在上文中提到，obj-y 和 obj-m 的定义中同时夹杂着目标文件和目标文件夹，文件夹当然是
不能直接参与编译的，所以需要将文件夹提取出来。

具体来说，就是将 obj-y/m 中以"/"结尾的纯目录部分提取出来，并赋值给 subdir-ym。

最后，需要处理 obj-y/m 中的文件夹。

总结：
subdir-ym 是从 obj-y 和 obj-m 中提取到的文件夹
obj-y 和 obj-m 会去掉文件夹或者将文件夹改为\<dir\>/built-in.a

### 多文件依赖的处理

```makefile
# Expand $(foo-objs) $(foo-y) etc. by replacing their individuals
# 将变量 $1 的后缀为 $2 的变量后缀替换为变量 $3，这里后缀替换的方法可以参考makefile
# 笔记中的变量高级用法
# 注意这里替换之后，外部还包了一层 $()，即表示这是替换之后的值，例如替换之后是
# foo-objs，这里取的是 foo-objs 的值而不是 foo-objs
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

对于obj-y/m中每一个后缀为 .o 的元素，查找对应的xxx-objs、xxx-y、xxx-（、xxx-m）变量
值。不知道你是否还记得前面章节所说的，如果 foo.o 目标依赖于 a.c,b.c,c.c,那么它在
Makefile 中的写法是这样的：
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
这里 real-obj-y/m 中保存的就是真正需要编译的目标，关于 obj-y/m 中可能存在的目录目标，
可以看后续的Makefile.build 文件介绍

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

至于上述添加路径部分的 $(obj) 是从哪里来的呢？
事实上，Makefile.lib 通常都被包含在于 Makefile.build中，这个变量继承了 Makefile.build
的 obj 变量。

而 Makefile.build 的 obj 变量则是通过调用 `$(build)` 时进行赋值的。

## 其余部分

基本上，对于文件和目录的处理到这里就已经完成了，Makefile.lib中剩余的部分都是一些
变量及标志位的设置，本文旨在梳理框架，对于那些细节部分有兴趣的朋友可以研究研究，
这里就不再继续深入了。


# scripts/Makefile.build文件详解

在Kbuild系统中，Makefile.build文件算是最重要的文件之一了，它控制着整个内核的核心
编译部分。

有了前面两个章节对 Kbuild.include 和 Makefile.lib的铺垫，接下来我们来揭开 Makefile.build
的面纱。


## Makefile.build入口

既然使用到Makefile.build,那不免产生一个疑问，它是怎么被用到的呢？
答案是：在Top Makefile中，通常可以看到这样的编译结构：
```makefile
scripts_basic:
    $(Q)$(MAKE) $(build)=scripts/basic
```
我们重点关注命令部分：
```makefile
$(Q)$(MAKE) $(build)=scripts/basic
```

build 变量(Makefile中变量和函数的定义是相同的)被定义在 scripts/Kbuild.include 中。

在 scripts/Makefile.build 中包含了 scripts/Kbuild.include 文件，紧接着在
scripts/Kbuild.include 文件下找到 build 的相应的定义：
```makefile
build := -f $(srctree)/scripts/Makefile.build obj
```
所以，上述的命令部分 `$(Q)$(MAKE) $(build)=scripts/basic` 展开过程就是这样：
```makefile
$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/basic
```
一目了然，它的作用就是进入到 Makefile.build 中进行编译，并将 obj赋值为scripts/basic。

注：对于\$(build)的定义，在 Kbuild.include 章节部分也有相应解析

## Makefile.build的执行

在上一小节中提到了，Makefile.build 的调用基本上都是由 `$(build)`的使用而开始的，
我们就打开 scripts/Makefile.build这一文件，逐行地分析 Makefile.build 具体做了什么。

### 初始化变量

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
src := $(obj)

# 尝试包含 include/config/auto.conf
# 命令前面加了一个小减号的意思就是，也许某些文件出现问题，但不要管，继续做后面的事
-include include/config/auto.conf

# 该文件中包含大量通用函数和变量的实现
include scripts/Kbuild.include

# 如果obj是以/目录开始的目录，kbuild-dir=src，否则kbuild-dir = srctree/src，这里
# 的src从obj赋值得到
kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
# 如果kbuild-dir存在Kbuild，则kbuild-file为Kbuild，否则为Makefile
kbuild-file := $(if $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Kbuild,$(kbuild-dir)/Makefile)
include $(kbuild-file)

# Makefile.lib 则决定了哪些文件需要编译，哪些目录需要递归进入
include scripts/Makefile.lib
```
为清晰起见，博主直接在贴出的源码中做了相应注释，下面是他们对应的详细解读：

## obj 的赋值

将 obj 变量赋值给 src。那么，obj的值是哪儿来的呢？一般情况下，`$(build)`的使用方式
为：`$Q$MAKE $(build)=scripts/basic,$(build)`展开之后 obj 就被赋值为 scripts/basic。

## 包含 include/config/auto.conf

尝试包含include/config/auto.conf文件，这个文件的内容是这样的：
```makefile
CONFIG_RING_BUFFER=y
CONFIG_HAVE_ARCH_SECCOMP_FILTER=y
CONFIG_SND_PROC_FS=y
CONFIG_SCSI_DMA=y
CONFIG_TCP_MD5SIG=y
CONFIG_KERNEL_GZIP=y
...
```
可以看出，这是对于所有内核模块的配置信息，include前添加"-"表示它可能include失败，
Makefile对于错误的默认处理就是终止并退出，"-"表示该语句失败时不退出并继续执行，
因为include/config/auto.conf 这个文件并非内核源码自带，而是在运行 make menuconfig
之后产生的。

## 包含 scripts/Kbuild.include

包含 scripts/Kbuild.include，相当于导入一些通用的处理函数

## $(build)对目标文件夹的处理

处理通过 `$(build)=scripts/basic` 传入的 obj = scripts/basic目录，获取该目录的相对
位置.如果该目录下存在Kbuild文件，则包含该 Kbuild (在Kbuild系统中，make优先处理
Kbuild 而不是 Makefile )文件，否则查看是否存在 Makefile文件，如果存在Makefile，
就包含Makefile文件，如果两者都没有，就是一条空包含指令。

这一部分指令的意义在于：
`$(build)= scripts/basic` 相当于 `make -f $(srctree)/scripts/Makefile.build obj=scripts/basic`,
如果 obj=scripts/basic 下存在 Makefile 或者 Kbuild 文件，就默认以该文件的第一个
有效目标作为编译目标。
如果指定目标 Makfile 下不存在有效目标(很可能对应 Makefile 中只是定义了需要编译的文件)，
就默认以`$(srctree)/scripts/Makefile.build` 下的第一个有效目标作为编译目标，即__build 目标。
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
表示指定编译scripts/basic/Makefile 或 scripts/basic/Kbuild文件下的scripts_basic目标
(这里仅为讲解方便，实际上scripts/basic/Makefile中并没有scripts_basic目标)。

接下来的内容就是包含include scripts/Makefile.lib，lib-y，lib-m等决定编译文件对象的
变量就是在该文件中进行处理。


## 编译目标文件的处理

接下来就是代表编译目标文件的处理部分:
```makefile
//主机程序，在前期的准备过程中可能需要用到，比如make menuconfig时需要准备命令行的图形配置。
# Do not include hostprogs rules unless needed.
# $(sort ...) is used here to remove duplicated words and excessive spaces.
hostprogs := $(sort $(hostprogs))
ifneq ($(hostprogs),)
include $(srctree)/scripts/Makefile.host
endif

//判断obj，如果obj没有指定则给出警告
5 ifndef obj
6 $(warning kbuild: Makefile.build is included improperly)
7 endif

//如果有编译库的需求，则给lib-target赋值，并将 $(obj)/lib-ksyms.o 追加到 real-obj-y 中。
9 ifneq ($(strip $(lib-y) $(lib-m) $(lib-)),)
10 lib-target := $(obj)/lib.a
11 real-obj-y += $(obj)/lib-ksyms.o
12 endif

//如果需要编译 将要编译进内核(也就是obj-y指定的文件) 的模块，则赋值 builtin-target
14 ifneq ($(strip $(real-obj-y) $(need-builtin)),)
15 builtin-target := $(obj)/built-in.a
16 endif

// 如果定义了 CONFIG_MODULES，则赋值 modorder-target。
17 ifdef CONFIG_MODULES
18 modorder-target := $(obj)/modules.order
19 endif
```
同样的，对于每一部分博主都添加了一部分注释，需要特别解释的有两点：
1. 14-16行部分，事实上，如果我们进入到每个子目录下查看Makefile文件，就会发现：obj-m
   和 obj-y的赋值对象并不一定是xxx.o这种目标文件， 也可能是目录，**real-obj-y可以理解为
   解析完目录的真正需要编译的目标文件**。 need-builtin变量则是在需要编译内核模块时被赋值
   为1，根据15行的 builtin-target := $(obj)/built-in.a可以看出，对于目录(值得注意的是，
   并非所有目录)的编译，都是先将该目录以及子目录下的所有编译进内核的目标文件打包成
   built-in.a，在父目录下将该build-in.a打包进父目录的build-in.a，一层一层地往上传递。
2. 17-19行部分，CONFIG_MODULES是在.config中被定义的，这个变量被定义的条件是在
   make menuconfig 时使能了 "Enable loadable module support" 选项，这个选项表示
   内核是否支持外部模块的加载，一般情况下，这个选项都会被使能。所以，modorder-target
   将被赋值为`$(obj)/modules.order，modules.order`文件内容如下：
   kernel/fs/efivarfs/efivarfs.ko kernel/drivers/thermal/intel/x86_pkg_temp_thermal.ko kernel/net/netfilter/nf_log_common.ko kernel/net/netfilter/xt_mark.ko kernel/net/netfilter/xt_nat.ko kernel/net/netfilter/xt_LOG.ko .... module.order
   这个文件记录了可加载模块在Makefile中出现的顺序，主要是提供给modprobe程序在匹配时使用。
   与之对应的还有module.builtin文件和modules.builtin.modinfo。
   顾名思义，module.builtin记录了被编译进内核的模块。而modules.builtin.modinfo则记录了
   所有被编译进内核的模块信息，作用跟modinfo命令相仿，每个信息部分都以模块名开头，
   毕竟所有模块写在一起是需要做区分的。这三个文件都是提供给modprobe命令使用，modporbe
   根据这些信息，完成程序的加载卸载以及其他操作。


## 默认编译目标

在上文中有提到，当命令为\$(build) = $(xxx_dir),且xxx_dir下没有对应的 Makefile 或
Kbuild 文件时或者该文件下不存在有效目标时，就编译 Makefile.build 下的默认目标，
即`__build`目标，事实上，这种情况是比较多的，其定义如下：
```makefile
__build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
     $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
     $(subdir-ym) $(always)
    @:
```
可以看到，这个默认目标没有对应的命令，只有一大堆依赖目标。而根据make的规则，当依赖
目标不存在时，又会递归地生成依赖目标。

以以往的经验来看，对于这些依赖文件，基本上都是通过一些规则生成的，我们可以继续在
当前文件下寻找它们的生成过程。

接下来我们逐个将其拆开来看：

### builtin-target

builtin-target ： 在上文中可以看到，这个变量的值为：`$(obj)/built-in.a`，也就是
当前目录下的 `built-in.a` 文件，这个文件在源文件中自然是没有的，需要编译生成，
我们在后文中寻找它的生成规则：

可以找到这样一条：
```makefile
$(builtin-target): $(real-obj-y) FORCE
    $(call if_changed,ar_builtin)
```
在前面的博客对于 Kbuild.include 详解中有提到，`$(call if_changed,ar_builtin)` 这条
指令结果将是执行 cmd_ar_builtin,然后在Makefile.build中找到对应的命令：
```makefile
real-prereqs = $(filter-out $(PHONY), $^)
cmd_ar_builtin = rm -f $@; $(AR) rcSTP$(KBUILD_ARFLAGS) $@ $(real-prereqs)
```
不难从源码看出，cmd_ar_builtin 命令的作用就是将所有当前模块下的编译目标(即依赖文件)
全部使用 ar 指令打包成 `$(builtin-target)`,也就是 `$(obj)/built-in.a`，它的依赖文件
被保存在 `$(real-obj-y)` 中。

而 `$(real-obj-y)` 变量，则是在 scritps/Makefile.lib 被处理出来的变量，它对应目录下
所有真正需要编译的目标文件，不包含文件夹。

### lib-target

lib-target : 同样，这个变量是在本文件中定义，它的值为：`$(obj)/lib.a`。

通常，只有在/lib 目录下编译静态库时才会存在这个目标，同时，我们可以在本文件中找到它的
定义：
```makefile
$(lib-target): $(lib-y) FORCE
    $(call if_changed,ar)
```
同样的，`$(call if_changed,ar)` 最终将调用 cmd_ar 命令，找到 cmd_ar 指令的定义是这样的：
```makefile
real-prereqs = $(filter-out $(PHONY), $^)
cmd_ar = rm -f $@; $(AR) rcsTP$(KBUILD_ARFLAGS) $@ $(real-prereqs)
```
看来这个命令的作用也是将本模块中的目标全部打包成`$(lib-target)`,也就是 `$(obj)/lib.a`，
只不过它的依赖是 `$(lib-y)` 。

### $(extra-y)

`$(extra-y)` 在 Makefile.lib 中被确定，主要负责 dtb 相关的编译，定义部分是这样的：
```makefile
extra-y             += $(dtb-y)
extra-$(CONFIG_OF_ALL_DTBS) += $(dtb-)
```
在 Makefile.lib 中可以找到对应的规则实现：
```makefile
$(obj)/%.dtb: $(src)/%.dts $(DTC) FORCE
    $(call if_changed_dep,dtc,dtb)
```
该命令调用了 cmd_dtc :
```makefile
cmd_dtc = mkdir -p $(dir ${dtc-tmp}) ; \
    $(HOSTCC) -E $(dtc_cpp_flags) -x assembler-with-cpp -o $(dtc-tmp) $< ; \

    $(DTC) -O $(2) -o $@ -b 0 \
        $(addprefix -i,$(dir $<) $(DTC_INCLUDE)) $(DTC_FLAGS) \
        -d $(depfile).dtc.tmp $(dtc-tmp) ; \

    cat $(depfile).pre.tmp $(depfile).dtc.tmp > $(depfile)
```
其中，以 `$(DTC)` 开头的第二部分为编译 dts 文件的核心，其中:
`$(DTC)` 表示 dtc 编译器

`$(2)` 为 dtb，-O dtb 表示输出文件格式为 dtb 。 `-o $@, $@` 为目标文件，表示输出目标
文件，输入文件则是对应的 `$<`。

### $(obj-m)

`$(obj-m)` 是所有需要编译的外部模块列表，一番搜索后并没有发现直接针对 `$(obj-m)` 的
编译规则，由于它是一系列的 .o 文件，所以它的编译是通过模式规则的匹配完成的：
```makefile
$(obj)/%.o: $(src)/%.c $(recordmcount_source) $(objtool_dep) FORCE
    $(call cmd,force_checksrc)
    $(call if_changed_rule,cc_o_c)
```
与上述不同的是，这里使用的是 if_changed_rule 函数，根据 scripts/Kbuild.include 下
对 if_changed_rule 的定义，继续找到 rule_cc_o_c 指令：
```makefile
define rule_cc_o_c
    $(call cmd,checksrc)
    $(call cmd_and_fixdep,cc_o_c)
    $(call cmd,gen_ksymdeps)
    $(call cmd,checkdoc)
    $(call cmd,objtool)
    $(call cmd,modversions_c)
    $(call cmd,record_mcount)
endef
```
这里又涉及到了 cmd 函数，简单来说，cmd 函数其实就是执行 `cmd_$1`,也就是上述命令中
分别执行 cmd_checksrc,cmd_gen_ksymdeps等等。

其中最重要的指令就是 ： `$(call cmd_and_fixdep,cc_o_c)`，它对目标文件执行了 fixdep，
生成依赖文件，然后执行了 cmd_cc_o_c, 这个命令就是真正的编译指令:
```makefile
cmd_cc_o_c = $(CC) $(c_flags) -c -o $@ $<
```
不难看出，这条指令就是将所有依赖文件编译成目标文件。

### $(modorder-target)

modorder-target同样是在本文件中定义的，它的值为 `$(obj)/modules.order`，也就是
大多数目录下都存在这么一个 modules.orders 文件，来提供一个该目录下编译模块的列表。
先找到 `$(modorder-target)` 的编译规则。
```makefile
$(modorder-target): $(subdir-ym) FORCE
    $(Q)(cat /dev/null; $(modorder-cmds)) > $@
```
然后找到 `$(modorder-cmds)` 的定义
```makefile
modorder-cmds =                     \
    $(foreach m, $(modorder),           \
        $(if $(filter %/modules.order, $m), \
            cat $m;, echo kernel/$m;))
```
从源码可以看出，该操作的目的就是将需要编译的 .ko 的模块以 kernel/$(dir)/*.ko 为
名记录到 obj-y/m 指定的目录下。

### $(subdir-ym)(主要部分)

在 Makefile.lib 中，这个变量记录了所有在 obj-y/m 中指定的目录部分，同样在本文件中
可以找到它的定义：
```makefile
$(subdir-ym):
    $(Q)$(MAKE) $(build)=$@ need-builtin=$(if $(findstring $@,$(subdir-obj-y)),1)
```
我觉得这是整个Makefile.build中最重要的一部分了，因为它解决了我一直以来的一个疑惑，
Kbuild系统到底是以怎样的策略递归进入每个子目录中的？

对于每个需要递归进入编译的目录，都对其调用：
```makefile
$(Q)$(MAKE) $(build)=$@ need-builtin=$(if $(findstring $@,$(subdir-obj-y)),1)
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
4. 回到 Makefile.build ，执行`__build`目标，编译 real-obj-y/m 等一众目标文件
5. 执行 $(subdir-ym) 的编译，递归地进入 subdir-y/m 进行编译。
6. 最后，直到没有目标可以递归进入，在递归返回的时候生成 built-in.a 文件，每一个
   目录下的 built-in.a 静态库都包含了该目录下所有子目录中的 built-in.a 和`*.o`文件，
   所以，在最后统一链接的时候，vmlinux.o 只需要链接源码根目录下的几个主要目录下的
   built-in.a 即可，比如 drivers, init.

### $(always)

相对来说，`$(always)`的出场率并不高，而且不会被统一编译，在寥寥无几的几处定义中，
一般伴随着它的编译规则，这个变量中记录了所有在编译时每次都需要被编译的目标。


# Kbuild中其他通用函数与变量

研究Kbuild系统，了解完它的大致框架以及脚本文件，接下来就非常有必要去了解它的一些
通用的变量、规则及函数。

这样，在一头扎进去时，才不至于在各种逻辑中迷路，这一篇博客，就专门聊一聊整个Kbuid
系统中常用的一些"套路";

## top Makefile中的"套路"

说到常用，首当其冲的自然是top Makefile，它是一切的起点，我们来讲讲 top Makefile
中的那些常用的部分。

## 打印信息中的奥秘

一个软件也好，一份代码也好，如果要研究它，一个最常用的技巧就是尽量获取更多关于它
的打印信息。

幸运的是，top Makefile 提供这么一个参数，我们可以执行下面的命令获取更详细的打印信息：
```makefile
make V=1
```
或
```makefile
make V=2
```
V 的全拼为 verbose，表示详细的，即打印更多的信息，在编译时不指定V时，默认 V=0 ，
表示不输出编译。

值得注意的是，V=1 和 V=2 并不是递进的关系，V=1时，它会打印更多更详细的信息，通常是
打印出所有执行的指令，当V=2时，它将给出重新编译一个目标的理由。而不是我们自以为的
V=2 比 V=1 打印更多信息。

同时，我们经常能在 top Makefile 中发现这样的指令：
```makefile
$(Q)$(MAKE) ...
```
这个 `$(Q)` 就是根据 V 的值来确定的，当 V=0 时，`$(Q)`为空，当 V=1 时，`$(Q)`为@。

在学习Makefile语法时可以知道，在命令部分前添加@，表示执行命令的同时不打印该命令到终端。

## CONFIG_SHELL

在执行命令时，CONFIG_SHELL 的出场率也非常高，它的原型是这样的：
```makefile
CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
      else if [ -x /bin/bash ]; then echo /bin/bash; \
      else echo sh; fi ; fi)
```
其实它的作用就是确定当前Kbuild使用机器上的哪个shell解释器。

在阅读Makefile的时候，为了理解方便，我们可以直接将 CONFIG_SHELL 看成是 /bin/bash。

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
都会被更新。

所以它的作用就是：当FORCE作为依赖时，就导致依赖列表中每次都有FORCE依赖被更新，导致
目标每次被重新编译生成。

## 提高编译效率

在Makefile的规则中，默认是支持隐式规则的推导和内建变量的，通常情况下，使用内建的
规则和变量使得Makefile的编写变得非常方便，但是，随之而来缺点就是：第一个降低编译
效率，第二个就是在复杂的环境下可能因此引入更多的复杂性。

降低编译效率是因为在目标的生成规则中，make工具将会在一些情况下尝试使用隐式规则进行
解析，扫描文件列表，禁用隐式规则，就免去了这部分操作。

在top Makefile的开头处，有这么一项定义：
```makefile
MAKEFLAGS += -rR
```
MAKEFLAGS 将在后续的 make 执行中被当做参数传递，而 -r 和 -R 参数不难从 make 的参数
列表中找到：
```makefile
-r, --no-builtin-rules      Disable the built-in implicit rules.
-R, --no-builtin-variables  Disable the built-in variable settings.
```
如上所示，-r 取消隐式规则，而 -R 取消内建变量。

需要注意的是，这个 -rR 参数是定义在 MAKEFLAGS 变量中，而非一个通用设置，只有在使用
了 MAKEFLAGS 参数的地方才使能了这个参数配置。

但是，由于 GNU make 规则的兼容性，在3.x 版本中，make -rR 参数并不会在定义时立即生效，
所以需要使用另一种方式来取消隐式规则，下面是一个示例：
```makefile
$(sort $(vmlinux-deps)): $(vmlinux-dirs) ;
```
就像我在 Makefile详解系列 博客中介绍的一样，取消隐式规则的方式就是重新定义规则，
上述做法就是这样，用新的规则覆盖隐式规则，而新的规则中并没有定义命令部分，所以看
起来就是取消了。

## 为生成文件建立独立的目录

内核编译的过程中，默认情况下，编译生成的文件与源代码和是混合在一起的，这样看来，
难免少了一些美感，程序员们总喜欢层次分明且干净整洁的东西。

top Makefile 支持 O= 命令参数

make O=DIR
表示所有编译生成的中间文件及目标文件放置在以 DIR 为根目录的目录树中，以此保持源
代码的纯净。

## 变量导出

在top Makefile中，经常使用 export 导出某些变量，比如：

export abs_srctree abs_objtree
这个 export 是 Makefile的关键字，它表示将变量导出到所有由本Makefile调用的子Makefile中，
子Makefile可以直接使用这个变量。

比如，Makefile1 调用 Makefile2，Makefile2 调用 Makefile3，Makefile4。 所以，Makefile2
就可以使用 Makefile1 中导出的变量，Makefile3、Makefile4可以使用Makefile1、Makefile2
中导出的变量。 但是Makefile3不能使用Makefile4中导出的，只有在继承关系中使用。

其实这就是 Makefile 中的一个特性，了解makfile的都知道，只是这个特性在 top Makefile
中使用特别频繁，啰嗦一点也不为过。


## host 软件

### fixdep

除了make本身，Kbuild 系统还需要借助一些编译主机上的软件，才能完成整个内核编译工作，
比如 make menuconfig 时，主机需要生成配置界面，又比如生成或者处理一些文件、依赖时，
可以直接使用一些软件，这些软件是平台无关的，所以可以直接由编译主机提供。

fixdep就是这样的一个软件，它的作用解决目标文件的依赖问题，那么到底是什么问题呢？

当linux编译完成时，我们会发现在很多子目录下都存在 .*.o.cmd 和 .*.o.d文件，比如编译完
fdt.c 文件，就会同时生成.fdt.o.cmd 和 .fdt.o.d 文件 (需要注意的是，这个以 . 开头的
文件都是隐藏文件)，这两个文件的作用是什么。

打开一看就知道了，.fdt.o.d文件中包含了 fdt.c 文件的依赖文件，这个文件的由来可以查看
gcc 的选项表，发现有这么一个选项：

--MD FILE   write dependency information in FILE (default none)
而在编译过程中发现确实是使用了 -MD 这个选项，所以编译过程就生成了对应的`*.d`依赖文件。

而.fdt.o.cmd 就是调用 fixdep 以 .fdt.o.d 为参数生成的依赖文件，fixdep 专门处理文件
编译的依赖问题。

.fdt.o.cmd 与 .fdt.o.d 不同的地方是 .fdt.o.cmd 多了很多的以 include/config/* 开头的
头文件。

为什么会明明.fdt.o.d就可以描述目标头文件依赖问题，还需要使用fixdep来进一步生成.fdt.o.cmd呢？

这是因为，我们在使用make menuconfig进行配置的时候，生成.config的同时，生成了
include/generated/autoconf.h 文件，这个文件算是.config的头文件版本(形式上有一些差别，
各位朋友可以自己比对一下)，然后在大部分的文件中都将包含这个文件。

但是问题来了，如果我仅仅是改了.config 中一个配置项，然后重新配置重新生成了
include/generated/autoconf.h 文件，那么依赖 autoconf.h 的所有文件都要重新编译，几乎
是相当于重新编译整个内核，这并不合理。

一番权衡下，想出了一个办法，将 autoconf.h 中的条目全部拆开，并生成一个对应的
include/config/ 目录，目录中包含linux内核中对应的空头文件，当我们修改了配置，比如
将 CONFIG_HIS_DRIVER 由 n 改为 y 时，我们就假定依赖于 include/config/his/driver.h
头文件的目标文件才需要重新编译，而不是所有文件。在一定的标准限制下，这种假定是合理的。

总的来说，我们可以把 fixdep 看做是一个处理文件依赖的主机工具。

# Top_Makefile的执行框架

经过前面5个章节一系列的铺垫，我们终于启动了对 top Makefile 的研究。

由于各内核版本的 Makefile 实际上有一些位置的调整、变量名以及功能的小区别，这里就
不逐行地对Makefile进行分析(因为随着版本的变化，Makefile的内容排布肯定会有所变化)，
而是以功能为单位，各位可以对照自己研究的Makefile版本找到对应的部分，框架部分是完全
通用的。

## top Makefile 整体框架

当我们执行 make 编译的时候，我们将整个 top Makefile的执行分成以下的逻辑部分：
1. 配置(make *config)：对内核中所有模块进行配置，以确定哪些模块需要编译进内核，
   哪些编译成外部模块，哪些不进行处理，同时生成一些配置相关的文件供内核真正编译
   文件时调用。
2. 内核编译(make)：根据配置部分，真正的编译整个内核源码，其中包含递归地进入各级
   子目录进行编译。
3. 通过命令行传入 O=DIR 进行编译 ： 所有编译过程生成的中间文件以及目标文件放置在
   指定的目录，保持源代码的干净。
4. 通过命令行传入多个编译目标，比如：make clean all，make config all，等等，一次
   指定多个目标
5. 通过命令行传入 M=DIR 进行编译：指定编译某个目录下的模块，并将其编译成外部模块
6. 指定一些完全独立的目标，与内核真实的编译过程并不强相关，比如：make clean(清除
   编译结果)，make kernelrelease(获取编译后的发型版本号)，`make headers_`(内核
   头文件相关)


## Makefile 整体框架解释

为什么要将内核编译过程分为这六个过程呢？

一方面，这六个过程对 top Makefile 的处理各不相同，且都具有代表性,在这里需要建立的
一个概念就是：top Makefile被调用的情况并非仅仅是用户使用 make 或者 make *config
指令直接调用，它可能被其它部分调用，所以如果要研究它，就得知道它是怎么被执行的，
这些六种情况对应了几种对 top Makefile 不同的调用。

另一方面，top Makefile的整个结构布局就是针对这几种情况进行处理。

如果我们不先了解它的框架，其中无数的 ifdef 条件语句只会让我们痛不欲生。根据上一
小节中列出的框架，我们对这些部分的编译过程进行详细解析：
* 第1、2 点最常用到，通常就是对应指令 make menuconfig make
* 第3点，命令行传入 O=DIR 进行编译,它的编译过程是这样的：
    * 先执行 make menuconfig O=DIR，在指定的 DIR 目录下生成 .config 、Makefile
      (top Makefile的副本)等一系列配置文件
    * 调用 make O=DIR ，进入 top Makefile，在 top Makefile 中进行一些常规变量的
      处理并 export。
    * 使用 make -C 指令进入到 DIR 中进行编译工作，并指定执行 top Makefile，所以
      相当于 top Makefile 又被调用了一次。
* 第4点，同时指定多个目标，从用户角度来说，用户并不关心 top Makefile 怎么实现，
  只知道多个目标将依次被执行，等待结果就可以了。实际上，top Makefile 对多个目标的
  处理就是对每一个目标都重新调用解析一次 top Makefile，直到所有目标都执行完成
* 第5点，通过命令行传入 M=DIR 进行编译，这种情况下在编译内核可加载模块是最常见的，
  不知道你还记不记得我们在编译可加载模块时 Makefile 中的这一条语句：
  `make -C /lib/modules/\$(shell uname -r)/build/ M=\$(PWD) modules make -C /lib/modules/\$(shell uname -r)/build/`
  表示进入到指定目录 /lib/modules/\$(shell uname -r)/build/(也就是内核源码目录)
  进行编译，指定 M=\$(PWD)，表示进入当前目录进行编译，modules为目标表示编译外部
  模块。
  所以，在这种情况下，top Makefile事实上是由外部模块中的 Makefile 调用的。
* 第6点： 这一部分与真正的内核源码编译没有太大关系，仅仅是处理一些编译之外的事件，
  这一部分较为循规蹈矩，没有其他额外的操作。

## 进入 Top Makefile

摸清了整个 top Makefile 大体的脉络，我们就结合上述框架进入到top Makefile 中，
一探庐山真面目。

在整个 top Makefile的研究中，我们将剪去一些细枝末节，将重要的部分挑取出来进行详细讲解，
同时并没有严格按照上述的框架顺序进行讲解。

### 确定打印等级

top Makefile 支持打印等级的设置，参数为命令行传入 V=1|2，默认为0,1表示打印make
执行时步骤，2表示打印重新编译的原因。下面是源码部分：
```makefile
ifeq ("$(origin V)", "command line")
  KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

ifneq ($(findstring s,$(filter-out --%,$(MAKEFLAGS))),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE
```
这一部分很好理解，主要是以下步骤：
* 判断是否传入 V 参数，如果有，判断是否为 1 。
* 如果传入 V=1 , quiet 变量设置空，Q 设置为@，在后文中经常看到 `$(Q)$(MAKE)` 的指令，
  如果 Q 为 @，将相当于执行 `@$(MAKE)`，在 Makefile 的规则中，命令部分前添加 @ 表示
  执行的同时打印该命令，以此起到打印执行的效果
* 判断是否传入 -s 选项，如果有，设置 quiet=silent_，表示静默编译。这个 -s 选项等级是
  最高的，一旦指定这个选项，不仅 V 选项失效，连默认应该打印的log都不输出。
* 将确定的 quiet Q KBUILD_VERBOSE 导出将要到调用到的 Makefile 中。


### 确定是否将生成文件存放到指定目录

这一部分也就是上述所说的，命令行传入 O=DIR 参数，看看源码实现：
```makefile
//第一部分
ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(O)
endif

ifneq ($(KBUILD_OUTPUT),)

abs_objtree := $(shell mkdir -p $(KBUILD_OUTPUT) && cd $(KBUILD_OUTPUT) && pwd)
$(if $(abs_objtree),, \
     $(error failed to create output directory "$(KBUILD_OUTPUT)"))

# $(realpath ...) resolves symlinks
abs_objtree := $(realpath $(abs_objtree))
else
abs_objtree := $(CURDIR)
endif # ifneq ($(KBUILD_OUTPUT),)

//第二部分
abs_srctree := $(realpath $(dir $(lastword $(Makefile_LIST))))

//第三部分
ifeq ($(abs_objtree),$(CURDIR))
# Suppress "Entering directory ..." unless we are changing the work directory.
MAKEFLAGS += --no-print-directory
else
need-sub-make := 1
endif

ifneq ($(abs_srctree),$(abs_objtree))
MAKEFLAGS += --include-dir=$(abs_srctree)
need-sub-make := 1
endif
```
其中分为三部分进行解析：
* 第一部分，主要是判断是否指定 O=DIR ，如果指定，赋值给 abs_objtree,并创建abs_objtree
  目录，获取`abs_objtree`的绝对路径，即指定目录的绝对路径。
* 第二部分：确定 abs_srctree，即源码根目录的绝对地址。并导出到在本文件中调用的
  Makefile 中。
* 第三部分：判断 abs_objtree 与 \$(CURDIR)是否相等，如果不等，need-sub-make := 1，
  表示指定目标目录与当前目录不一致，同样的，判断`$(abs_srctree)`和`$(abs_objtree)`是否
  相等，如果不等 need-sub-make := 1，表示源码目录和指定目标目录不一致。 如果是默认
  编译情况，目标生成目录和源码目录是相等的，当指定了 O=DIR 时，`(abs_srctree)`和
  `$(abs_objtree)`自然是不等的。need-sub-make 被赋值为1.

need-sub-make 这个变量是控制了什么逻辑呢？

接着往下看：
```makefile
ifeq ($(need-sub-make),1)

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out _all sub-make $(lastword $(Makefile_LIST)), $(MAKECMDGOALS)) _all: sub-make
    @:

sub-make:
    $(Q)$(MAKE) -C $(abs_objtree) -f $(abs_srctree)/Makefile $(MAKECMDGOALS)

endif # need-sub-make
```
显然，与我们上文中的解析一致，当 need-sub-make 变量值为 1 时，执行:
```makefile
make -C DIR -f top_Makefile $(MAKECMDGOALS)
```
即跳到指定 DIR 下再次执行 top Makefile，在这种情况下，top Makefile将被进行第二次解析。


### 确保命令行变量不被重复解析

在上文的介绍中，博主是按照 top Makefile 中的文本顺序来逐一讲解的，到这里我们了解
了<font color=red> top Makefile 事实上并非仅仅是被用户直接调用，而是可能被重复调用。</font>

但是在重复调用的情况下，截止 top Makefile 到 上文，都是一些命令行解析，这部分解析
是没必要被重复执行的，所以，在 top Makefile开头，我们可以看到这样的一个条件控制语句：
```makefile
ifneq ($(sub_make_done),1)
```
在处理完 `ifneq ($(abs_srctree),$(abs_objtree))` 这个逻辑分支之后(不管有没有执行
sub-make)，修改了该控制参数并结束逻辑控制：
```makefile
export sub_make_done := 1
#endif(correspponding to  sub_make_done)
```
所以在下一次 top Makefile 被调用时，这一部分命令行处理将不再执行。

### M=DIR 编译

当没有指定 O=DIR 参数时，接着往下看 top Makefile 的解析：
```makefile
ifeq ("$(origin M)", "command line")
  KBUILD_EXTMOD := $(M)
endif
```
将 M 参数赋值给变量 KBUILD_EXTMOD，如果没有定义，KBUILD_EXTMOD就为空，我们接着看
对 KBUILD_EXTMOD 的处理：
```makefile
ifeq ($(KBUILD_EXTMOD),)
_all: all
else
_all: modules
endif
```
不难看出，如果 KBUILD_EXTMOD 为空，则正常编译默认目标 all，否则，编译目标 modules。
接着找到 modules 的定义：
```makefile
modules: $(vmlinux-dirs) $(if $(KBUILD_BUILTIN),vmlinux) modules.builtin
    $(Q)$(AWK) '!x[$$0]++' $(vmlinux-dirs:%=$(objtree)/%/modules.order) > $(objtree)/modules.order
    @$(kecho) '  Building modules, stage 2.';
    $(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost
    $(Q)$(CONFIG_SHELL) $(srctree)/scripts/modules-check.sh
```
事实上，如果指定了 M=DIR，KBUILD_EXTMOD的值为对应目录 DIR，并为KBUILD_EXTMOD创建
独立于内核的文件夹MODVERDIR，然后导出 KBUILD_EXTMOD 和 MODVERDIR，就表示此次内核的
编译只需要将指定目录下的目标文件编译成外部模块即可，上述的指令中起主要作用的就是：
```makefile
$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost
```
将编译的控制权交给了 scripts/Makefile.modpost，该文件将被作为 Makefile 读入，根据
top Makefile导出的KBUILD_EXTMOD 和 MODVERDIR两个变量进行相应的编译工作。

scripts/Makefile.modpost实现细节部分这里就不再深究，至少到这里，我们确认了命令行
中 M=DIR 的用法。

### 多个目标的编译

在 Makefile 的规则中，当执行一个 Makefile 时，默认会将目标存在在 MAKECMDGOALS 这个
内置变量中，比如执行make clean all，MAKECMDGOALS 的值就是clean all，我们就可以通过
查看该变量确定用户输入的编译目标。

top Makefile 支持各种各样的目标组合，自然也要先扫描用户输入的目标，它的过程是这样
的(为理解方便，将部分顺序打乱)：
```makefile
no-dot-config-targets := $(clean-targets) \
             cscope gtags TAGS tags help% %docs check% coccicheck \
             $(version_h) headers_% archheaders archscripts \
             %asm-generic kernelversion %src-pkg

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
    ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
        dot-config := 0
    endif
endif
```
其中，no-dot-config-targets表示一些配置目标，但并不是 *config。
`ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)`和
`ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)` 这两条语句用于
判断编译目标中是否有且仅有 no-dot-config-targets 中的目标，如果是，dot-config
变量设置为 0 。

接着往下看：
```makefile
no-sync-config-targets := $(no-dot-config-targets) install %install \
               kernelrelease
ifneq ($(filter $(no-sync-config-targets), $(MAKECMDGOALS)),)
    ifeq ($(filter-out $(no-sync-config-targets), $(MAKECMDGOALS)),)
        may-sync-config := 0
    endif
endif
```
no-sync-config-targets 表示那些不需要进行任何编译工作(包括主机上的编译)的目标。

同样的，上述指令的目的是用于判断目标中是否有且仅有 no-sync-config-targets ，如果
是 may-sync-config := 0。

然后：
```makefile
ifneq ($(KBUILD_EXTMOD),)
    may-sync-config := 0
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(words $(MAKECMDGOALS)),1)
                        mixed-targets := 1
                endif
        endif
endif
```
如果是指定外部模块编译(KBUILD_EXTMOD是用户传入 M=DIR)，may-sync-config := 0

如果未指定 M=DIR 参数，且用户传入了 config 目标，config-targets := 1，表示需要处理
config 目标，比如 make menuconfig。

同时，如果用户传入的目标数量不为 1 ，表示除了 *config 之外还有其他目标，mixed-targets := 1。
```makefile
clean-targets := %clean mrproper cleandocs
ifneq ($(filter $(clean-targets),$(MAKECMDGOALS)),)
        ifneq ($(filter-out $(clean-targets),$(MAKECMDGOALS)),)
                mixed-targets := 1
        endif
endif
```
检查目标中是否有clean-targets，如果有，且指定目标中还有其他 target，则mixed-targets := 1
```makefile
ifneq ($(filter install,$(MAKECMDGOALS)),)
        ifneq ($(filter modules_install,$(MAKECMDGOALS)),)
            mixed-targets := 1
        endif
endif
````
这一部分是检查用户指定目标中是否有 install，如果有且指定目标中还有其他目标，则
mixed-targets := 1 目标的解析已经完成，接下来就是相关的处理：
```makefile
ifeq ($(mixed-targets),1)

PHONY += $(MAKECMDGOALS) __build_one_by_one

$(filter-out __build_one_by_one, $(MAKECMDGOALS)): __build_one_by_one
    @:

__build_one_by_one:
    $(Q)set -e; \
    for i in $(MAKECMDGOALS); do \
        $(MAKE) -f $(srctree)/Makefile $$i; \
    done
else
```
如上所示，如果 mixed-targets 的值为 1，表示要执行多个目标，上述的处理就是以每个
目标为目标重复调用 top Makefile。
```makefile
ifeq ($(config-targets),1)
    include arch/$(SRCARCH)/Makefile
    export KBUILD_DEFCONFIG KBUILD_KCONFIG CC_VERSION_TEXT

    config: scripts_basic outputMakefile FORCE
        $(Q)$(MAKE) $(build)=scripts/kconfig $@

    %config: scripts_basic outputMakefile FORCE
        $(Q)$(MAKE) $(build)=scripts/kconfig $@
```
如果 config-targets 的值为1，则执行对应的 config指令，关于 make *config 的处理，
在后续的博客中详解。

上述列出的都是一些对于特殊情况的处理，事实上，对于其他普通目标而言，直接找到
top Makefile 中对应的目标生成指令即可，比如：
```makefile
make kernelrelease
```
就可以找到对应的
```makefile
kernelrelease:
    @echo "$(KERNELVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"
```

### top Makefile其他部分

相信你如果打开了 top Makefile 查看，发现其实里面还包含非常多的内容，相信我，那些东西都是一些纸老虎。

除了上述提到的内容，top Makefile 当然还有的东西：
* 各个目标的定义实现，编译vmlinux依赖的大量目标，top Makefile 中包含这些目标的编译
  规则，这一部分将占据 top Makefile 的大量篇幅，事实上我们只需要顺着 vmlinux 的编译
  规则各个击破就可以了，并没有太多理解上的难点。当然，在下一篇文章我们我们就是要
  解决这个问题。
* 各类FLAG，GCC、LD、Makefile_FLAG 等等，这些flag作为执行参数，这一部分我们暂不讨论，
  不影响我们对 Kbuild 的整体把握
* help 信息，执行make help 返回的帮助信息。

## 小结

在这里，我们再回头看一遍 top Makefile的6大部分：
1. 配置(make *config)：对内核中所有模块进行配置，以确定哪些模块需要编译进内核，
   哪些编译成外部模块，哪些不进行处理，同时生成一些配置相关的文件供内核真正编译
   文件时调用。
2. 内核编译(make)：根据配置部分，真正的编译整个内核源码，其中包含递归地进入各级
   子目录进行编译。
3. 通过命令行传入 O=DIR 进行编译 ： 所有编译过程生成的中间文件以及目标文件放置在
   指定的目录，保持源代码的干净。
4. 通过命令行传入多个编译目标，比如：make clean all，make config all，等等，一次
   指定多个目标
5. 通过命令行传入 M=DIR 进行编译：指定编译某个目录下的模块，并将其编译成外部模块
6. 指定一些完全独立的目标，与内核真实的编译过程并不强相关，比如：make clean(清除
   编译结果)，make kernelrelease(获取编译后的发型版本号)，`make headers_`(内核头
   文件相关)

在这一章节中，我们讲解了第 3、4、5点，关于第6点大家可以直接在 top Makefile 中找到
相应的实现，大多并不复杂。

Kbuild 中的配置和内核源码的真正编译部分留到后续的章节中进行解析。


# Kbuild中make_config的实现

在前面我们总共使用了多个章节的篇幅，为最后的 make *config 解析和 vmlinux 的生成做铺垫。

之后的三篇博客，就是从 top Makefile 开始，根据源码，讲解我们常用的 make menuconfig、
make 指令执行背后的原理，彻底弄清楚内核镜像到底是怎么被编译出来的。

对于`make *config`对应的内核配置，通常情况下，我们都是使用 make menuconfig，我们
就以 make menuconfig 为例来讲配置的执行过程。


## top Makefile的处理

当我们执行 make menuconfig 时，对应 top Makefile 中的规则是这样的：
```makefile
%config: scripts_basic outputMakefile FORCE
    $(Q)$(MAKE) $(build)=scripts/kconfig $@
```
目标有两个依赖： scripts_basic 和 outputMakefile，接着我们找到这两个目标的生成规则：
outputMakefile：
```makefile
outputMakefile:
    ifneq ($(srctree),.)
        $(Q)ln -fsn $(srctree) source
        $(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkMakefile $(srctree)
        $(Q)test -e .gitignore || \
        { echo "# this is build directory, ignore it"; echo "*"; } > .gitignore
    endif
````
scripts_basic:
```makefile
scripts_basic:
    $(Q)$(MAKE) $(build)=scripts/basic
    $(Q)rm -f .tmp_quiet_recordmcount
```
看起来整个 make menuconfig 的过程并不复杂，只要弄清楚 %config、outputMakefile、
scripts_basic 这三个目标的生成规则即可，接下来就是逐个研究这三个目标。

### outputMakefile

outputMakefile的定义是这样的：
```makefile
outputMakefile:
    ifneq ($(srctree),.)
        $(Q)ln -fsn $(srctree) source
        $(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkMakefile $(srctree)
        //版本管理软件的处理
        $(Q)test -e .gitignore || \
        { echo "# this is build directory, ignore it"; echo "*"; } > .gitignore
    endif
```
这部分并不难理解：先判断源码目录`($(srctree))`与当前目录是否相等，如果相等，则
不执行任何指令，如果不等，则执行以下两条指令：
1. 在当前目录下创建源码根目录的软链接
2. 执行 `$(srctree)/scripts/mkMakefile $(srctree)` 这条 shell 指令。
```makefile
cat << EOF > Makefile
include $(realpath $1/Makefile)
EOF
```
很明显，这条语句就是将 `$1/Makefile` 也就是 `$(srctree)/Makefile` 包含(include)
到当前目录的 Makefile 下，效果上相当于将源码根目录下的 Makefile 复制到当前目录下。

那么这些是什么意思呢？

答案是：当我们没有通过 O=DIR 指定源码与生成文件目录分开存放时，ifneq ($(srctree),.)
这条语句是不成立的，也就是什么都不执行。

显然，这条指令是为用户指定了 O=DIR 服务的，此时，在指定目录下创建源码的软链接，
并且将 top Makefile 复制到该目录的 Makefile 中，然后就跳转到指定目录下执行对应的
Makefile.

### scripts_basic

scripts_basic的定义部分是这样的：
```makefile
scripts_basic:
    $(Q)$(MAKE) $(build)=scripts/basic
    $(Q)rm -f .tmp_quiet_recordmcount
```
由我之前的博客 Makefile.build文件详解 可知,`$(Q)$(MAKE) $(build)=scripts/basic`
这条指令可以简化成：
```makefile
make -f $(srctree)/scripts/Makefile.build obj=scripts/basic
```
将 `$(srctree)/scripts/Makefile.build` 作为 Makefile 执行，并传入参数 obj=scripts/basic。

我们直接查看 scripts/basic/Makefile,因为在 `$(srctree)/scripts/Makefile.build` 中
包含了它：
```makefile
hostprogs-y := fixdep
always      := $(hostprogs-y)

$(addprefix $(obj)/,$(filter-out fixdep,$(always))): $(obj)/fixdep
```
因为 `$(srctree)/scripts/Makefile` 并没有包含目录，整个过程没有出现目录的递归，
所以 `$(always)` 的值就是 fixdep。

因为 `$(srctree)/scripts/Makefile` 没有指定目标，所以将执行 Makefile.build 中的
默认目标：
```makefile
__build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
     $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
     $(subdir-ym) $(always)
    @:
```
因为 `$(KBUILD_BUILTIN)` 和 `$(KBUILD_MODULES)` 都为空，`$(subdir-ym)` 也没有在
Makefile 中设置，所以，在 make menuconfig 时，仅编译 `$(always)`，也就是 fixdep。

所以，在生成 scripts_basic 的阶段就干了一件事：生成 fixdep 。

注：这一部分的理解需要借助前面章节博客中对 Makefile.build 和 fixdep 的讲解


### %config

两个依赖文件的生成搞定了，那么就来看主要的目标，也就是 menuconfig。
```makefile
%config: scripts_basic outputMakefile FORCE
    $(Q)$(MAKE) $(build)=scripts/kconfig $@
```
menuconfig也好、oldconfig也好，都将匹配 %config 目标，以 menuconfig 为例，命令部分可以简化为：
```makefile
make -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig menuconfig
```
如果你有看了前面关于 Makefile.build 的解析博客，就知道，对于上述指令，编译流程是：
1. make -f 指令将 Makefile.build 作为目标 Makefile 解析
2. 在 Makefile.build 中解析并包含 scripts/kconfig/Makefile，同时指定编译目标 menuconfig
3. 如果指定了 hostprogs-y ，同时进入 Makefile.host 对 hostprogs-y 部分进行处理

根据 $(build) 的规则，我们进入到 scripts/kconfig/ 下查看 Makefile 文件，由于指定了
编译目标为：menuconfig，我们找到 scripts/kconfig/Makefile 中 menuconfig 部分的定义：
```makefile
ifdef KBUILD_KCONFIG
Kconfig := $(KBUILD_KCONFIG)
else
Kconfig := Kconfig
endif

menuconfig: $(obj)/mconf
    $< $(silent) $(Kconfig)
```
该部分命令可以简化成(忽略 silent 部分，这是打印相关参数,obj=scripts/kconfig/)：
```makefile
menuconfig: $(obj)/mconf
    $(obj)mconf  Kconfig
```

### (obj)/mconf 的生成

依赖 scripts/kconfig/mconf，我们先看依赖文件是怎么生成的：
```makefile
hostprogs-y += mconf
// lxdialog 依赖于checklist.c inputbox.c menubox.c textbox.c util.c yesno.c
lxdialog    := checklist.o inputbox.o menubox.o textbox.o util.o yesno.o

// mconf 依赖于 $(lxdialog) mconf.o
mconf-objs  := mconf.o $(addprefix lxdialog/, $(lxdialog)) $(common-objs)

HOSTLDLIBS_mconf = $(shell . $(obj)/mconf-cfg && echo $$libs)
$(foreach f, mconf.o $(lxdialog), \
  $(eval HOSTCFLAGS_$f = $$(shell . $(obj)/mconf-cfg && echo $$$$cflags)))

$(obj)/mconf.o: $(obj)/mconf-cfg
$(addprefix $(obj)/lxdialog/, $(lxdialog)): $(obj)/mconf-cfg
```
相对而言，mconf 的生成过程还是比较复杂的：
```shell
        |-- mconf.o -|-- mconf-cfg
        |            |-- mconf.c
        |
        |             |-- checklist.o
        |             |-- inputbox.o
mconf --|-- lxdialog -|-- menubox.o
        |             |-- textbox.o
        |             |-- util.o
        |             |-- yesno.o
        |
        |-- hostprogs-y += mconf
```
根据图中的流程可以看出：mconf 的生成分成3个步骤：

**mconf.o :**
mconf.o 的生成，依赖于 mconf.c 和 mconf-cfg,mconf.c不用说，mconf-cfg 主要是调用
mconf-cfg.sh 检查主机上的 ncurse 库，整个图形界面就是在 ncurse 图形库上生成的。

**lxdialog ：**
这一部分就是依赖于 scripts/kconfig/ 目录下的各个 .c 编译而成，主要负责图形以及
用户操作的支持。

**mconf ：hostprogs-y += mconf**
对于 mconf 的生成，稍微有点复杂，在这里将mconf 添加到 hostprogs-y 中，但是我们
并没有在当前文件中找到 hostprogs-y 的生成规则，答案要从 scripts/Makefile.build 说起。

在 scripts/Makefile.build 中，先是通过 include \$(kbuild-file) 包含了 scripts/kconfig/Makefile，
因为在该 Makefile 中指定了 hostprogs-y，所以也将使下面的语句成立：
```makefile
ifneq ($(hostprogs-y)$(hostprogs-m)$(hostlibs-y)$(hostlibs-m)$(hostcxxlibs-y)$(hostcxxlibs-m),)
include scripts/Makefile.host
endif
```
从而包含了 include scripts/Makefile.host，事实上，hostprogs-y 变量中的目标就是在
scripts/Makefile.host 文件中被编译生成的(编译过程并不难，自己去看看吧)。

**回到 menuconfig**
解决了依赖文件的生成问题，我们再回到 menuconfig 的生成：
```makefile
menuconfig: $(obj)/mconf
    $(obj)mconf  Kconfig
```
生成指令非常简单，就是 mconf Kconfig，使用 conf 程序来解析 Kconfig，我们继续来看看
这两个到底是何方神圣：
* Kconfig ： Kconfig 就是我们经常碰到的内核配置文件，以源码根目录下的配置文件为参数，
  传递给 conf 程序， 源码根目录下的 Kconfig 大致是这样的：
```makefile
mainmenu "Linux/$(ARCH) $(KERNELVERSION) Kernel Configuration"
comment "Compiler: $(CC_VERSION_TEXT)"
source "scripts/Kconfig.include"
source "init/Kconfig"
source "kernel/Kconfig.freezer"
source "fs/Kconfig.binfmt"
source "mm/Kconfig"
....
```
* 最上级的 Kconfig 文件中全是子目录，不难想到，这又是一颗树，所有子目录下的 Kconfig
  文件层层关联，生成一颗 Kconfig 树，为内核配置提供参考
* 了解了上面的知识点，就很容易猜到 conf 命令是干什么的了：它负责层层递归地解析这
  棵 Kconfig 树，同时，解析完这棵树肯定得留下这棵树相关的信息，这些信息就是以 .config
  文件为首的一系列配置文件、头文件。主要包括：include/generated/ 下的头文件、
  include/config/ 下的头文件 .config 等等。

如果你对 conf 程序有兴趣，也可以自己尝试阅读它的源代码实现。

## 其他的配置方式

鉴于本博客是建立在 make menuconfig 的基础上进行解析，对于其他的解析方式，其实也是
大同小异，只需要在 scripts/kconfig/Makefile 中找到对应的目标生成规则即可：
```makefile
xconfig: $(obj)/qconf
    $< $(silent) $(Kconfig)

gconfig: $(obj)/gconf
    $< $(silent) $(Kconfig)

menuconfig: $(obj)/mconf
    $< $(silent) $(Kconfig)

config: $(obj)/conf
    $< $(silent) --oldaskconfig $(Kconfig)

nconfig: $(obj)/nconf
    $< $(silent) $(Kconfig)
```
关于其他的配置方式，原理是类似的，这里就不再啰嗦了。

## 小结

整个 make menuconfig 的流程还是比较简单的，主要是以下步骤：
1. 匹配 top Makefile 中的 %config 目标，其中分两种情况：
    * 指定了 O=DIR 命令参数，需要将所有生成目标转移到指定的 DIR 中
    * 未指定 O=DIR，一切相安无事，按照正常流程编译。
2. 由 %config 的编译指令进入 scripts/kconfig/Makefile 中编译，找到 menuconfig 目标
   的生成规则
    * 生成 mconf 程序
    * 使用 mconf 读取并解析 Kconfig，mconf主要就是生成图形界面，与用户进行交互，
      然后把用户的选择记录下来，同时根据各文件夹下 Kbuild 配置 ，生成一系列配置文件，
      供后续的编译使用。

# Kbuild中vmlinux以及镜像的生成(0)

## 编译的开始

### 模块的编译

当我们执行直接键入 make 时，根据 Makefile 的规则，默认执行的目标就是 top Makefile
第一个有效目标，即 all，随后，all的定义如下：
```makefile
all: vmlinux
```
all 依赖于 vmlinux ，它就是整个内核编译的核心所在，事实上除了默认的 make 不仅编译
了vmlinux ，同时也将所有在配置阶段选为 "m" 的模块进行了编译，将其编译成 .ko 目标
文件，并将其记录到各子目录下的 modules.order 文件中，在 top Makefile中是这样的处理的：
```makefile
ifneq ($(filter all _all modules,$(MAKECMDGOALS)),)
  KBUILD_MODULES := 1
endif

ifeq ($(MAKECMDGOALS),)
  KBUILD_MODULES := 1
endif
export KBUILD_MODULES
```

当执行 make all 、`make _all`、make modules 或者不指定目标时，`KBUILD_MODULES`值被
置为1.

这个变量在 scripts/Makefile.build的默认目标`__build`中被使用到：
```makefile
__build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
     $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
     $(subdir-ym) $(always)
    @:
```
当 `$(KBUILD_MODULES)` 为1时，将编译所有由 obj-m 指定的对象和 `$(modorder-target)`
指定的对象。

### vmlinux的编译

鉴于整个 vmlinux 编译流程较为复杂，我们先梳理它的框架，以便对其编译过程有一个基本的
认识：

同时，查看 vmlinux 的定义：
```makefile
// $(vmlinux-dirs) 依赖于 prepare
$(vmlinux-dirs): prepare
    $(Q)$(MAKE) $(build)=$@ need-builtin=1

// vmlinux-deps 的内容
vmlinux-deps := $(KBUILD_LDS) $(KBUILD_VMLINUX_OBJS) $(KBUILD_VMLINUX_LIBS)

//vmlinux deps 依赖于 $(vmlinux-dirs)
$(sort $(vmlinux-deps)): $(vmlinux-dirs) ;

// vmlinux 目标的命令部分
cmd_link-vmlinux =                                                 \
    $(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
    $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

// vmlinux 的编译规则
vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
    +$(call if_changed,link-vmlinux)
```
根据 vmlinux 的定义，博主将整个 vmlinux 的编译一共分成三部分
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分
* vmlinux的依赖：vmlinux-deps 的内容，`$(KBUILD_VMLINUX_OBJS)` `$(KBUILD_VMLINUX_LIBS)`
  分别对应需要编译的内核模块和内核库。
* 把视线放到vmlinux 编译规则的命令部分，所有目标文件编译完成后的链接工作，调用脚本
  文件对生成的各文件进行链接工作，最后生成 vmlinux。  
了解了这三个步骤，就可以开始对每一个步骤进行层层解析了。

#### 准备工作 prepare

准备部分的调用流程为：

all -> vmlinux -> vmlinux-deps -> vmlinux-dirs -> prepare
我们就从 prepare开始，先来看一下整个 prepare 的框架：
```
                                        |-- archheaders
                                        |-- archscripts
                                        |
                                        |              |-- prepare3  include/config/kernel_rebase
          |-- prepare0 -- archprepare --|              |-- outputmakefile
          |                             |              |-- asm-generic
          |                             |-- prepare1 --|-- $(version_h)
          |                             |              |-- $(autoksyms_h)
          |                             |              |-- include/generated/utsrelease.h
prepare --|                             |
          |                             |             |-- scripts_basic
          |                             |-- scripts --|
          |                                           |-- scripts_dtc
          |
          |-- prepare-objtool
```

而 prepare 被调用的地方和定义为：
```makefile
prepare: prepare0 prepare-objtool
```
整个 prepare 没有命令部分，并没有做什么时候的事，转而查看它的两个依赖目标的生成。

##### prepare0

prepare0 的定义如下：
```makefile
prepare0: archprepare
    $(Q)$(MAKE) $(build)=scripts/mod
    $(Q)$(MAKE) $(build)=.
```
查看它的命令部分，发现执行了两个命令部分：
```makefile
$(Q)$(MAKE) $(build)=scripts/mod
```
该指令将进入到 scripts/Makefile.build ,然后包含 scripts/mod/ 下的 Makefile 文件，
执行 scripts/mod/Makefile 下的默认目标规则：
```makefile
hostprogs-y := modpost mk_elfconfig
always      := $(hostprogs-y) empty.o

devicetable-offsets-file := devicetable-offsets.h

$(obj)/$(devicetable-offsets-file): $(obj)/devicetable-offsets.s FORCE
    $(call filechk,offsets,__DEVICETABLE_OFFSETS_H__)
```
这部分命令将生成 scripts/mod/devicetable-offsets.h 这个全局偏移文件。

同时，modpost 和 mk_elfconfig 这两个目标(这是两个主机程序)被赋值给 always 变量，
根据 scripts/Makefile.build 中的规则，将在 Makefile.host 中被生成。

modpost 和 mk_elfconfig 是两个主机程序，负责处理模块与编译符号相关的内容。

我们再来看 prepare0 命令的另一部分：
```makefile
$(Q)$(MAKE) $(build)=.
```
以当前目录也就是源码根目录为 obj 参数执行 `$(build)`，`$(build)`将在当前目录下
搜索 Kbuild 文件或者 Makefile 文件并包含它(Kbuild优先)。

发现根目录下同时有一个 Kbuild 文件 和 Makefile 文件(也就是top Makefile)，查看
Kbuild 文件的源码发现，该文件中定义了一系列的目标文件，包括：include/generated/bounds.h、
include/generated/timeconst.h、include/generated/asm-offsets.h、missing-syscalls
等等目标文件，用于之后的编译工作。

(这里倒是解决了我长时间以来存在的一个疑问：源码根目录下已经存在了一个 top Makefile 了，
为什么还会有一个 Kbuild 文件，因为在Kbuild系统中，Kbuild 和 Makefile 都是可以当成
Makfile 进行解析的，且 Kbuild 的优先级比 Makefile 高，原来源码根目录下的 Kbuild是在
这里被调用。) 同时注意，Kbuild 文件只是在 Kbuild 系统中与 Makefile作用类似，但是，
执行make的默认目标依旧是 Makefile 。

然后，prepare0 依赖了 archprepare 目标，接下来我们继续看 archprepare 部分的实现。

##### archprepare

对于 archprepare，看到 arch 前缀就知道这是架构相关的，这部分的定义与 `arch/$(ARCH)/Makefile`
有非常大的关系。
```makefile
archprepare: archheaders archscripts prepare1 scripts
```
archprepare 并没有命令部分，仅仅是依赖四个目标文件：archheaders、archscripts、
prepare1、scripts，继续往下跟：

##### archheaders

一番寻找发现，在 top Makefile 中并不能找到 archheaders 的定义部分，事实上，这个目标的
定义在 arch/$(ARCH)/Makfile 下定义：
```makefile
archheaders:
    $(Q)$(MAKE) $(build)=arch/arm/tools uapi
```
以 arm 为例，这里再次使用了 `$(build)` 命令，表示进入到 scripts/Makefile.build 中，
并包含 arch/arm/tools/Makefile,编译目标为 uapi：
```makefile
uapi-hdrs-y := $(uapi)/unistd-common.h
uapi-hdrs-y += $(uapi)/unistd-oabi.h
uapi-hdrs-y += $(uapi)/unistd-eabi.h
uapi:   $(uapi-hdrs-y)
```
uapi 为用户 API 的头文件，包含unistd-common.h、unistd-oabi.h、unistd-eabi.h等通用
头文件。

##### archscripts

事实上，archscripts 表示平台相关的支持脚本，这一部分完全根据平台的需求而定，而且很多
平台似乎都没有这一部分的需求，这里就没有详细研究的必要了。

想要详细了解的朋友，博主提供一个 mips 架构下的 archscripts 的定义：
```makefile
archscripts: scripts_basic
    $(Q)$(MAKE) $(build)=arch/mips/tools elf-entry
    $(Q)$(MAKE) $(build)=arch/mips/boot/tools relocs
```

##### scripts

scripts 表示在编译过程中需要的脚本程序，下面是它的定义:
```makefile
scripts: scripts_basic scripts_dtc
    $(Q)$(MAKE) $(build)=$(@)
```
命令部分同样是 `$(build)`,并传入 obj=scripts/,同样的，找到 scripts/ 下的 Makefile，
Makefile 下的内容是这样的：
```makefile
hostprogs-$(CONFIG_BUILD_BIN2C)  += bin2c
hostprogs-$(CONFIG_KALLSYMS)     += kallsyms
hostprogs-$(CONFIG_LOGO)         += pnmtologo
hostprogs-$(CONFIG_VT)           += conmakehash
hostprogs-$(BUILD_C_RECORDMCOUNT) += recordmcount
hostprogs-$(CONFIG_BUILDTIME_EXTABLE_SORT) += sortextable
hostprogs-$(CONFIG_ASN1)     += asn1_compiler
...
hostprogs-y += unifdef
build_unifdef: $(obj)/unifdef
    @:
```
整个 scripts/Makefile 编译了一系列的主机程序，包括 bin2c、kallsyms、pnmtologo等，
都是需要在编译中与编译后使用到的主机程序。

接着再来看 scripts 依赖的两个目标：scripts_basic 和 scripts_dtc。

scripts_basic 在之前的 make menuconfig 中已经有过介绍，它将生成 fixdep 程序，该程序
专门用于解决头文件依赖问题。

##### scripts_dtc

scripts_dtc 的定义是这样的：
```makefile
scripts_dtc: scripts_basic
    $(Q)$(MAKE) $(build)=scripts/dtc
```
同样的，进入 scripts/dtc/Makefile ，发现文件中有这么一部分：
```makefile
hostprogs-$(CONFIG_DTC) := dtc
always      := $(hostprogs-y)
dtc-objs    := dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
           srcpos.o checks.o util.o
dtc-objs    += dtc-lexer.lex.o dtc-parser.tab.o
...
```
可以发现，该目标的作用就是生成 dtc (设备树编译器)。


##### prepare1

回到 archprepare 的最后一个目标：prepare1，同样地，查看它的定义：
```makefile
prepare1: prepare3 outputMakefile asm-generic $(version_h) $(autoksyms_h) \
                        include/generated/utsrelease.h
    $(cmd_crmodverdir)
```
它执行了 `$(cmd_crmodverdir)` 命令，这个命令的原型为：
```makefile
cmd_crmodverdir = $(Q)mkdir -p $(MODVERDIR) \
                  $(if $(KBUILD_MODULES),; rm -f $(MODVERDIR)/*)
```
`$(MODVERDIR)` 仅与编译外部模块相关，这里针对外部模块的处理，如果指定编译外部模块，
则不做任何事，如果没有指定编译外部模块，清除`$(MODVERDIR)/`目录下所有内容。

`$(MODVERDIR)`变量是专门针对于编译外部模块的变量。

了解了 prepare1 的命令部分，接下来逐个解析它的依赖部分的目标：

##### prepare3

通常，当我们指定了 O=DIR 参数时，发现如果源代码中在之前的编译中有残留的之前编译过的
中间文件，编译过程就会报错，系统会提示我们使用 make mrprope 将之前的所有编译中间文件
清除，再重新编译。

这是因为，如果源码根目录下有一份配置文件，而使用 O=DIR 编译，在指定目录 DIR 下也有
一份同名文件，将会导致编译过程的二义性，所以 Kbuild 强制性地要求源码的整洁。

其实，这一部分的检查，就是 prepare3 在做，接着看 prepare3 的定义：
```makefile
prepare3: include/config/kernel.release
ifneq ($(srctree),.)
    @$(kecho) '  Using $(srctree) as source for kernel'
    $(Q)if [ -f $(srctree)/.config -o \
         -d $(srctree)/include/config -o \
         -d $(srctree)/arch/$(SRCARCH)/include/generated ]; then \
        echo >&2 "  $(srctree) is not clean, please run 'make mrproper'"; \
        echo >&2 "  in the '$(srctree)' directory.";\
        /bin/false; \
    fi;
endif
```
从源码中可以看出，prepare3 将会检查的文件有 `$(srctree)/include/config`、
`$(srctree)/arch/$(SRCARCH)/include/generated`、`$(srctree)/.config`。

这是在源码编译时需要重点注意的一点，如果你的编译报这种错误，你就需要检查这些文件，
否则，直接执行 make mrproper 将会导致你的内核源码需要完全地重新编译一次。

##### outputMakefile

prepare1 的第二个依赖目标，这个目标在 make menuconfig 中有过讲解，它表示在指定
O=DIR 进行编译时，将top Makefile 拷贝到到指定 DIR 下。(不是直接的文件拷贝，但是
效果是一致的)。
**asm-generic、version_h、autoksyms_h、 include/generated/utsrelease.h**
这些都是 prepare1 的依赖文件，而且这些依赖都代表一些通用头文件，asm-generic 表示
架构相关的头文件。

由于这一部分和内核编译的框架关系不大，细节我们就不再深究。


## 总结

### prepare小结

到这里，整个 prepare 的部分就分析完毕了，由于其中的调用关系较复杂，在最后，我觉得
有必要再对其进行一次整理，整个流程大致是这样的：

* prepare : 没有命令部分。
    * prepare0：生成 scripts/mod/devicetable-offsets.h 文件，并编译生成 modpost
      和`mk_elfconfig`主机程序
        * archprepare：没有命令部分
            * archheaders：规则定义在对应的 arch/\$(ARCH)/Makefile 下,负责生成
              `$(uapi)/unistd-common.h`、`$(uapi)/unistd-oabi.h`、`$(uapi)/unistd-eabi.h`
              等架构体系相关通用头文件
            * archscripts：完全地架构相关的处理脚本，甚至在多数架构中不存在
            * prepare1：在内核编译而不是外部模块编译时，清除`$(MODVERDIR)/`目录下
              所有内容
                * prepare3：当使用 O=DIR 指定生成目标文件目录时，检查源代码根目录
                  是否与指定目标目录中文件相冲突
                * outputMakefile ： 当使用 O=DIR 指定生成目标文件目录时，复制一份
                  top Makefile 到目标 DIR 目录
                * `asm-generic $(version_h) $(autoksyms_h) include/generated/utsrelease.h`：
                  一系列架构相关的头文件处理。
            * scripts :编译了一系列的主机程序，包括 bin2c、kallsyms、pnmtologo等，
              都是需要在编译中与编译后使用到的主机程序
                * scripts_basic ： 编译生成主机程序 fixdep
                * scripts_dtc ： 编译生成 dtc
* prepare-objtool

整个 prepare 部分也就像它的名称一样，不做具体的编译工作，只是默默地做一些检查、
生成主机程序、生成编译依赖文件等辅助工作。

### vmlinux的编译流程回顾

再一次回顾 vmlinux 的编译流程：
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分
* vmlinux的依赖：vmlinux-deps 的内容，`$(KBUILD_VMLINUX_OBJS)` `$(KBUILD_VMLINUX_LIBS)`
  分别对应需要编译的内核模块和内核库。
* 把视线放到vmlinux 编译规则的命令部分，所有目标文件编译完成后的链接工作，调用脚本
  文件对生成的各文件进行链接工作，最后生成 vmlinux。

下一章节，我们将继续解析编译 vmlinux 的另外两个部分：vmlinux-dep依赖的生成 和 镜像
的链接。


# Kbuild中vmlinux以及镜像的生成(1)

在上一章节中，我们讲到了编译生成 vmlinux 的框架以及 prepare 部分，从这一章节开始，
我们继续解析后续的两部分。

先回顾一下，在上一章节中我们将vmlinux的编译过程分解的三部分：
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分
* vmlinux-deps 的内容，`$(KBUILD_VMLINUX_OBJS)` `$(KBUILD_VMLINUX_LIBS)` 分别对应
  需要编译的内核模块和内核库。
* 看到 vmlinux 的命令部分，模块编译完成的链接工作，调用脚本文件对生成的各文件进行
  链接工作，最后生成 vmlinux。

## 源码的编译

接下来我们要看的就是第二部分，源码部分的编译。这部分对应的变量为 vmlinux-deps 以及
vmlinux-dirs。

### vmlinux-deps

vmlinux-deps 的被调用流程为： all -> vmlinux -> vmlinux-deps ,而 vmlinux-deps 的
值为：
```makefile
// 各变量的具体值
init-y      := init/
drivers-y   := drivers/ sound/
drivers-$(CONFIG_SAMPLES) += samples/
net-y       := net/
libs-y      := lib/
core-y      := usr/
virt-y      := virt/
// 每个变量的值原本是目录，将目录名后加上 built-in.a 后缀
init-y      := $(patsubst %/, %/built-in.a, $(init-y))
core-y      := $(patsubst %/, %/built-in.a, $(core-y))
drivers-y   := $(patsubst %/, %/built-in.a, $(drivers-y))
net-y       := $(patsubst %/, %/built-in.a, $(net-y))
libs-y1     := $(patsubst %/, %/lib.a, $(libs-y))
libs-y2     := $(patsubst %/, %/built-in.a, $(filter-out %.a, $(libs-y)))
virt-y      := $(patsubst %/, %/built-in.a, $(virt-y))

// vmlinux 相关的目标文件
KBUILD_VMLINUX_OBJS := $(head-y) $(init-y) $(core-y) $(libs-y2) \
                  $(drivers-y) $(net-y) $(virt-y)
// vmlinux 的链接脚本
KBUILD_LDS          := arch/$(SRCARCH)/kernel/vmlinux.lds
// vmlinux 相关的库
KBUILD_VMLINUX_LIBS := $(libs-y1)

vmlinux-deps := $(KBUILD_LDS) $(KBUILD_VMLINUX_OBJS) $(KBUILD_VMLINUX_LIBS)
```
从 vmlinux-deps 的名称可以看出，这个变量属于 vmlinux 的依赖，同时从定义可以看出，
它的主要部分就是源文件根目录下的各子目录中的 built-in.a。

在之前 Makefile.build 的章节中我们有提到编译的总体规则：通常，源码目录如`drivers/`
`net/`等目录下的 Makefile 只需要提供被编译的文件和需要递归进行编译的目录，每个对应
的目录下都将所有编译完成的目标文件打包成一个 built-in.a 的库文件，然后上级的 built-in.a
库文件同时会将子目录中的 built-in.a 文件打包进自己的built-in.a,层层包含。

最后，所有的 built-in.a 文件都被集中在根目录的一级子目录中，也就是上文中提到的
init/built-in.a，lib/built-in.a、drivers/built-in.a 等等，vmlinux-deps 变量的值
就是这些文件列表。

那么，这个递归的过程是怎么开始的呢？也就是说，top makefile 是从哪里开始进入到子目录
中对其进行递归编译的呢？

我们还得来看另一个依赖 vmlinux-dirs。


### vmlinux-dirs

vmlinux-dirs 是 vmlinux-deps 的依赖目标，同样的，我们可以轻松地找到vmlinux-dirs的定义部分：
```makefile
// init-y 等值的定义
init-y      := init/
drivers-y   := drivers/ sound/
drivers-$(CONFIG_SAMPLES) += samples/
net-y       := net/
libs-y      := lib/
core-y      := usr/
virt-y      := virt/

// vmlinux-dirs 的赋值
vmlinux-dirs    := $(patsubst %/,%,$(filter %/, $(init-y) $(init-m) \
             $(core-y) $(core-m) $(drivers-y) $(drivers-m) \
             $(net-y) $(net-m) $(libs-y) $(libs-m) $(virt-y)))

// vmlinux-dirs 的编译部分
$(vmlinux-dirs): prepare
    $(Q)$(MAKE) $(build)=$@ need-builtin=1
```
第一部分 vmlinux-dirs 的赋值，就是将原本 `$(init-y) $(init-m) $(core-y) $(core-m)`中
的 / 去掉，也就是说只剩下目录名，接下来对于其中的每一个元素(init、drivers、net、
sound等等)，都调用：
```makefile
$(Q)$(MAKE) $(build)=$@ need-builtin=1
```
我相信你肯定还记得 `$(build)` 的作用，就是将 scripts/Makefile.build 作为目标 makefile
执行，并包含指定目录下的 Makefile ，然后执行第一个有效目标。

如果你看到这里还不懂，鉴于这一部分是各级目录下文件编译的核心部分，博主就再啰嗦一点：
```makefile
$(vmlinux-dirs): prepare
    $(Q)$(MAKE) $(build)=$@ need-builtin=1
```
由于 `$(vmlinux-dirs)` 的值为init、drivers、 sound、等等源码根目录下一级子目录名，
根据 makefile 规则，相当于执行以下一系列的命令：
```makefile
init: prepare
    $(Q)$(MAKE) $(build)=init need-builtin=1
drivers: prepare
    $(Q)$(MAKE) $(build)=drivers need-builtin=1
sound: prepare
    $(Q)$(MAKE) $(build)=sound need-builtin=1
...
```
所以，makefile 的执行流程跳到 Makefile.build 中，并包含 init ，取出 init/Makefile 中
obj-y/m 等变量，经过 Makefile.lib 处理，然后执行Makefile.build 的默认目标`__build`
进行编译工作，编译完本makefile中的目标文件时，再递归地进入子目录进行编译。

最后递归返回的时候，将所有的目标文件打包成 built-in.a 传递到 init/built-in.a 中，
同样的道理适用于 drivers sound等一干目录,lib/ 目录稍有不同，它同时对应 lib.a 和
built-in.a 。

至此，vmlinux 的依赖文件 vmlinux-deps ，也就是各级子目录下的 built-in.a 库文件就
已经生成完毕了。


## 链接与符号处理

当所有的目标文件已经就绪，那么就是 vmlinux 生成三部曲的最后一部分：将目标文件链接
成 vmlinux。

回到 vmlinux 编译的规则定义：
```makefile
cmd_link-vmlinux =                                                 \
    $(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
    $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
    +$(call if_changed,link-vmlinux)
```
在此之前，vmlinux 的依赖文件处理情况是这样的：
* scripts/link-vmlinux.sh,这是源码自带的用于链接的脚本文件。
* `$(vmlinux-deps)` 根目录的一级子目录下的各个 built-in.a 文件。
* autoksyms_recursive：内核的符号优化

### autoksyms_recursive

对于 autoksyms_recursive ，它也有对应的生成规则:
```makefile
autoksyms_recursive: $(vmlinux-deps)
ifdef CONFIG_TRIM_UNUSED_KSYMS
    $(Q)$(CONFIG_SHELL) $(srctree)/scripts/adjust_autoksyms.sh \
      "$(MAKE) -f $(srctree)/Makefile vmlinux"
endif
```
如果在源码的配置阶段定义了 CONFIG_TRIM_UNUSED_KSYMS 这个变量，就执行：
```makefile
$(Q)$(CONFIG_SHELL) $(srctree)/scripts/adjust_autoksyms.sh \
      "$(MAKE) -f $(srctree)/Makefile vmlinux"
```
首先，CONFIG_TRIM_UNUSED_KSYMS 默认配置下是没有被定义的，所以的默认情况下，这里的
脚本并不会执行。

其次，这个命令做了什么呢？

从名称就可以看出，它的作用是裁剪掉内核中定义但未被使用的内核符号，以此达到精简内核
文件大小的目的。

当这个变量被设置和不被设置时，可以发现，System.map 文件的大小有比较大的变化，默认
情况下不设置，System.map 的文件内容比设置该变量大了1/4(5.2版本内核)，但是这样的坏处
自然是内核提供了更少的服务，尽管这些服务很可能用不上。

### 链接部分

所有的依赖文件准备就绪，就来看 vmlinux 的命令部分：
```makefile
cmd_link-vmlinux =                                                 \
    $(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
    $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
    +$(call if_changed,link-vmlinux)
```
根据 if_changed 的函数定义，该函数将转而执行 `cmd_$1`,也就是 cmd_link-vmlinux。

从上文中可以看到，对于架构 ARCH_POSTLINK 需要做特殊处理，其他的就是直接执行
scripts/link-vmlinux.sh 脚本程序。

scripts/link-vmlinux.sh 的执行过程是这样的： 链接 `$(KBUILD_VMLINUX_OBJS)`，即
根目录一级子目录下的 built-in.a 链接 `$(KBUILD_VMLINUX_LIBS)`，即 lib/lib.a 等。
目标文件中的所有符号处理，提取出所有目标文件中的符号，并记录下内核符号`__kallsyms` ，
基于平台计算出符号在系统中运行的地址，并生成相应的记录文件，包括：System.map、
include/generate/autoksyms.h 等等供系统使用，到系统启动之后 modprobe 还需要根据
这些符号对模块进行处理。

### 生成镜像

vmlinux 可以作为内核镜像使用，但是，通常在嵌入式系统中，基于内存的考虑，系统并不会
直接使用 vmlinux 作为镜像，而是对 vmlinux 进行进一步的处理，最普遍的是两步处理：

去除 vmlinux 中的符号，将符号文件独立出来，减小镜像所占空间，然后，对镜像进行压缩，
同时在镜像前添加自解压代码，这样可以进一步地减小镜像的大小。

别小看省下的这些空间，在嵌入式硬件上，空间就代表着成本。

最后生成的镜像通常被命名为：vmlinuz、Image、zImage、bzImage(这些文件名的区别在前文
中可参考) 等等。

对于其中的压缩部分，我们暂时不去理会，我们可以看看裁剪 vmlinux 中的符号是怎么完成
的，当然，这些部分都是平台相关的，所以被放置在 arch/\$(ARCH)/Makefile 目录下进行
处理，定义部分是这样的( 以arm平台为例 )：
```makefile
//boot 对应目录
boot := arch/arm/boot
//实际镜像
BOOT_TARGETS    = zImage Image xipImage bootpImage uImage
//镜像生成规则
$(BOOT_TARGETS): vmlinux
    $(Q)$(MAKE) $(build)=$(boot) MACHINE=$(MACHINE) $(boot)/$@
```
根据 `$(build)` 的实现，进入到 arch/arm/boot/Makefile 中，以 Image 为例，Image 对于
vmlinux 的操作仅是去除符号，没有压缩操作：
```makefile
//Image 生成的规则
$(obj)/Image: vmlinux FORCE
    $(call if_changed,objcopy)
```
但是在当前 makefile 中找不到对应的 cmd_objcopy，在一番搜索后，发现其在 scripts/Makefile.lib
中被定义, 看来虽然这个命令即使是在 arch/$(ARCH)/ 下调用，也是一个通用命令，下面是它的定义部分：
```makefile
//在arch/$(ARCH)/boot/Makefile 下定义
OBJCOPYFLAGS    :=-O binary -R .comment -S
//scripts/Makefile.lib
cmd_objcopy = $(OBJCOPY) $(OBJCOPYFLAGS) $(OBJCOPYFLAGS_$(@F)) $< $@
```
objcopy 是linux下一个对二进制文件进行操作的命令，主要功能就是将文件中指定的部分复制
到目标文件中。

结合上面 Image 的生成规则，发现这一部分的工作主要就是使用 objcopy 将 vmlinux 中的
内容复制到 Image 中，其中我们需要关注它的 flag 部分：
```makefile
-O binary -R .comment -S
```
查询手册可以看到该 flag 对应的功能：
* -O binary：表示输出格式为 binary，也就是二进制
* -R .comment：表示移除二进制文件中的 .comment 段，这个段主要用于 debug
* -S ： 表示移除所有的符号以及重定位信息
以上就是由 vmlinux 生成 Image 的过程。

同样的，zImage 是怎么生成的呢？ 从 zImage 的前缀 z 可以了解到，这是经过 zip 压缩的
Image，它的规则是这样的：
```makefile
$(obj)/zImage:  $(obj)/compressed/vmlinux FORCE
    $(call if_changed,objcopy)
```
与 Image 不同的是，zImage 依赖于 `$(obj)/compressed/vmlinux` ，而不是原生的 vmlinux，
可以看出是经过压缩以及添加解压缩代码的 vmlinux，具体细节就不再深入了，有兴趣的朋友
可以深入研究。

## 小结

到这里，整个linux内核镜像的编译就结束了，你可以在 arch/\$(ARCH)/boot 目录下找到你
想要的启动文件。

总结一下，镜像编译过程三个步骤：
* 准备阶段，对应的规则为 prepare，主要在 top makefile 和 arch/\$(ARCH)/Makefile 中
  实现，其中`arch*`部分的目标生成都依赖于`arch/$(ARCH)/Makefile`。
* 递归编译阶段，这一阶段将递归地编译内核源码，并在每个根目录的一级子目录下生成
  built-in.a 库。
* 链接阶段，链接所有的 built-in.a 库到 vmlinux.o，并处理所有相关的符号，生成vmlinux，
  最后为生成镜像，对 vmlinux 进行符号精简以及压缩操作。
