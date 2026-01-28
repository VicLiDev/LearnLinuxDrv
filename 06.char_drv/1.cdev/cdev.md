# 字符设备

```c
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    struct list_head list;
    dev_t dev;                   /* 设备号 32位，其中12位为主设备号，20位为次设备号 */
    unsigned int count;
} __randomize_layout;
```


## 设备号

通过查看 cat /proc/devices 文件可以得到系统中注册的设备，第一列为主设备号，第二列
为设备名称
```shell
Character devices:
  1 mem
  4 /dev/vc/0
  4 tty
  4 ttyS
  5 /dev/tty
  5 /dev/console
  5 /dev/ptmx
  5 ttyprintk
  6 lp
  7 vcs
 10 misc
 13 input
128 ptm
136 pts
180 usb
188 ttyUSB
189 usb_device

Block devices:
  7 loop
  8 sd
  9 md
 11 sr
 65 sd
 66 sd
 67 sd
135 sd
253 device-mapper
254 mdp
259 blkext
```

### 主设备号和次设备号

字符设备通过文件系统中的设备节点进行访问。这些节点称为特殊文件或设备文件或简称为
文件系统树的节点，它们通常位于 /dev 目录中。

<font color=red>
设备节点是驱动和设备面向用户的体现，用于通过操作设备节点达到操作设备的目的，因此
设备节点也需要通过相关的方式关联到当前设备节点对应的驱动和设备。
</font>

原则上，**主设备号**标识设备节点对应的驱动程序，但现代的 Linux 内核也允许多个驱动
程序共享主设备号。**次设备号**标识设备节点对应的设备，由内核使用，可以通过次设备号
获得一个指向内核设备的指针，也可以将次设备号当作设备本地数据的索引。

**主设备号可以理解为驱动程序的索引**，与设备的驱动程序关联。例如，/dev/null和
/dev/zero均由驱动程序 1 管理，而虚拟控制台和串行终端由驱动程序 4 管理；同样，
vcs1和 vcsa1设备均由驱动程序 7 管理。打开设备时，内核在使用主设备号将执行操作分
派给适当的驱动程序。

**次设备号仅由主设备号指定的驱动程序使用**，内核的其他部分不使用它，而只是将其
传递给驱动程序。一个驱动程序控制多个设备是很常见的（如列表所示），次要编号为驱动
程序提供了区分它们的方法。

<font color=red>
根据主设备号，操作系统能选择正确的驱动程序。

根据次设备号，驱动程序能知道要操作的设备。
</font>

<font color=red>user < - - >dev file <-- [主设备号] --> driver <-- [次设备号] --> dev <- -> file_operations </font>

linux操作系统中为设备文件编号分配了32位无符号整数，其中前12位是主设备号，后20位为
次设备号，所以在向系统申请设备文件时主设备号不能超过4095，次设备号不能超过2^20 -1。

获得 dev_t 的主设备号或次设备号：
```c
MAJOR(dev_t dev);  // 获得主设备号
MINOR(dev_t dev);  // 获得从设备号
```

将主设备号和次设备号转换成 dev_t 类型：
```c
dev_t devNo = MKDEV(int major, int minor);
```

ls -l输出的第一列中的“c”标识表示字符设备，块设备由“b”标识。本章的重点是字符设备，
但以下大部分信息也适用于块设备。

使用`ls -l`命令，可以在修改日期之前看到两个数字（以逗号分隔，其有时候显示文件长度），
这些数字是特定设备的主设备号和次设备号。

以下列表显示了系统上出现的一些设备。它们的主设备号是 1、4、7 和 10，次设备号是
1、3、5、64、65 和 129。
```shell
 crw-rw-rw- 1 root   root    1, 3   Feb 23 1999  null
 crw------- 1 root   root   10, 1   Feb 23 1999  psaux
 crw------- 1 rubini tty     4, 1   Aug 16 22:22 tty1
 crw-rw-rw- 1 root   dialout 4, 64  Jun 30 11:19 ttyS0
 crw-rw-rw- 1 root   dialout 4, 65  Aug 16 00:00 ttyS1
 crw------- 1 root   sys     7, 1   Feb 23 1999  vcs1
 crw------- 1 root   sys     7, 129 Feb 23 1999  vcsa1
 crw-rw-rw- 1 root   root    1, 5   Feb 23 1999  zero
```

### 申请设备号（注册设备）

2.4 版内核引入了一项新的（可选）功能，即设备文件系统或devfs（devfs在2.6的内核中被
舍弃，使用udev替代他）。如果使用该文件系统，设备文件的管理会变得简单，并且会有很大
不同。前面的描述和下面有关添加新驱动程序和特殊文件的说明假设devfs不存在。

#### 申请单个设备号

当devfs没有被使用时，向系统添加一个新的驱动程序意味着为其分配一个主设备号。应在
驱动程序（模块）初始化时通过调用以下函数进行分配：
```c
// include/linux/fs.h

// major: 主设备号，使用0时表示动态申请，2.0内核支持128个设备；2.2 和 2.4 将该
//        数字增加到 256（同时保留值 0 和 255 以供将来使用）。
// name: 该系列设备的名称，它将出现在 /proc/devices 中
// fops: 与该设备关联的文件操作
// ret:  返回errno，负值表示错误，返回0表示成功
int register_chrdev(unsigned int major, const char *name,
                    struct file_operations *fops);
// 释放设备(号)
int unregister_chrdev(unsigned int Major, const char *name);
```

一旦驱动程序在内核表中注册，其文件操作(file_operations)就会与给定的主设备号相关联。
**每当操作与该主设备号关联的字符设备节点时，内核都会从该file_operations结构中查找
并调用正确的函数**。

#### 申请多个设备号

```c
// fs/char_dev.c
// 参数说明：
// first: 要分配的设备编号范围的起始值，常被置为0
// count: 所请求的连续设备编号的个数，如果count非常大，则所请求的范围可能和下一个
//        主设备号重叠，但只要我们所请求的编号范围是可用的，就不会带来任何问题
// name: 和该编号范围关联的设备名称，它将出现在 /proc/devices 和 sysfs 中
int register_chrdev_region(dev_t from, unsigned count, const char *name)

// 如果已知要使用的起始设备号，使用 register_chardev_region ，如果不知道，使用这个
// 接口
//
// 参数说明：
// dev: output parameter for first assigned number
// baseminor: first of the requested range of minor numbers
// count: the number of minor numbers required
// name: the name of the associated device or driver

// Allocates a range of char device numbers.  The major number will be
// chosen dynamically, and returned (along with the first minor number)
// in @dev.  Returns zero or a negative error code.

int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,
                        const char *name)
```
register_chrdev_region 和 alloc_chardev_region 内部都调用了 __register_chrdev_region 
去分配设备号，不同的是register_chrdev_region传入的是开发者传递的主设备号，而
alloc_chrdev_region传入的主设备号是0。看__register_chrdev_region函数的实现非常简单，
首先会判断major是否为0，如果为0，就在内核维护的设备表寻找一个空闲的位置并把该位置的
下标作为主设备号返回给驱动。

简言之，已知设备号的话使用register_chrdev_region注册设备，未知设备号的话使用
alloc_chardev_region动态申请设备号并注册设备

释放设备号
```c
// 释放设备号：
void unregister_chrdev_region(dev_t first, unsigned int count);
```

#### 创建设备节点


可以使用 device_create 接口创建设备节点，细节参考udev一节，或者直接参考demo，
这里简单描述一下老方法。

由以上内容可知，必须将设备名称插入到 /dev目录中并与驱动程序的主设备号和次设备号
相关联，才可以使用驱动程序。

在文件系统上创建设备节点的命令是 mknod；此操作需要超级用户权限。除了正在创建的文件
的名称之外，该命令还采用三个参数。例如，命令
```shell
mknod /dev/scull0 c 254 0
```
创建一个 char 设备 ( c)，其主设备号为 254，次设备号为 0。次设备号应在 0 到 255 范围
内，因为由于历史原因，它们有时存储在单个字节中。有充分的理由扩展可用次要编号的范围，
但目前八位限制仍然有效。

请注意，一旦由mknod创建，特殊设备文件就会保留下来，除非明确删除它，就像存储在磁盘上
的任何信息一样。可以通过rm /dev/scull0来删除本示例中创建的设备。



#### 其他

动态分配设备号的一些说明：
```
一些主设备号静态分配给最常见的设备。这些设备的列表可以在Documentation/devices.txt中
找到 。由于已经分配了许多编号，因此为新驱动程序选择唯一的编号会很困难，自定义驱动
比可用的主设备号要多得多。调试时可以使用为“实验或本地使用”保留的主要编号之一：[14]，
但如果您尝试多个“本地”驱动程序或发布驱动程序供第三方使用，将再次遇到选择一个合适数字
的问题。

幸运的是可以请求动态分配主号码。major如果在调用 register_chrdev 时将参数设置为 0 ，
则该函数会选择一个空闲号码并将其返回。返回的主编号始终为正，而负返回值为错误代码。
请注意，两种情况下的行为略有不同：如果调用者请求动态编号，则该函数返回分配的主编号，
但在成功注册预定义主编号时返回 0（不是主编号）。

对于私有驱动，我们强烈建议您使用动态分配的方式来获取您的主设备号，而不是从当前空闲的
设备号中随机选择一个号。另一方面，如果您的驱动程序旨在对整个社区有用并包含在官方内核
树中，则您需要申请分配一个主编号以供专用。

之前创建设备节点需要使用脚本方法，比较麻烦，但是新的kernel版本可以直接使用接口
device_create创建设备节点，具体查看udev章节的内容
```

注销设备的一些说明：
```
申请设备好的过程意味着注册设备，驱动退出的时候同样需要注销设备。

参数是主设备号和关联设备的名称。内核将该名称与拥有该主设备号的注册名称（如果有）
进行比较：如果不同，则返回-EINVAL。如果主设备号超出允许的范围，内核也会返回-EINVAL，
如果相同则注销该设备。

未能在清理函数中取消注册资源会产生不良影响。/proc/devices下次尝试读取它时会产生错误，
因为其中一个字符串 name仍然指向已注销驱动模块的内存，而该内存不再被映射。这种错误
称为 oops，这是内核在尝试访问无效地址时打印的消息。

当您卸载驱动程序而不取消注册主设备号时，恢复将很困难，因为unregister_chrdev 中的
strcmp函数必须取消引用指向原始模块的指针。如果您无法取消已经注册的主设备号，则必须
重新加载同一模块和另一个专门为取消已注册的主设备号而构建的模块。如果您没有更改代码，
那么幸运的是，有问题的模块将获得相同的地址，并且字符串将位于相同的位置。当然，
更安全的选择是重新启动系统。

除了卸载模块之外，您还经常需要清理已注销驱动程序的设备文件。该任务可以通过与加载时
使用的脚本配对的脚本来完成。该脚本scull_unload为我们的示例设备完成工作；作为替代方案，
您可以调用scull.init stop。

如果未从 /dev中删除动态设备文件，则可能会出现意外错误：如果两个驱动程序都使用动态
主设备号，开发者电脑上空闲的 /dev/framegrabber 可能会在一个月后引发警报。“没有这样的
文件或目录”是打开 /dev/framegrabber 比新驱动程序产生的更友好的响应。
```


### dev_t 和 kdev_t

到目前为止我们已经讨论了主设备号。现在是时候讨论次设备号以及驱动程序如何使用它
来区分设备了。

每次内核调用设备驱动程序时，它都会告诉驱动程序正在对哪个设备进行操作。主设备号和
次设备号以同一种数据类型配对，驱动程序使用该数据类型来标识特定设备。组合设备号
（主设备号和次设备号连接在一起）位于结构体字段i_rdev中 ，inode稍后介绍。某些驱动
程序函数接收指向struct inode的指针作为第一个参数。因此，使用指针 inode（就像大多数
驱动程序编写者所做的那样），可以通过查看inode->i_rdev来提取设备号。

从历史上看，Unix 声明dev_t（设备类型）来保存设备号。它曾经是<sys/types.h>中定义
的 16 位整数值。如今，有时需要超过 256 个次要编号，但更改 dev_t很困难，因为如果
结构更改，有些应用程序就会崩溃。因此，虽然已经为更大的设备编号奠定了大部分基础，
但它们目前仍为 16 位整数。

然而，在 Linux 内核中， kdev_t使用了不同的类型 。该数据类型被设计为每个内核函数的
黑匣子。用户程序根本不知道kdev_t，内核函数也不知道kdev_t. 如果 kdev_t保持隐藏状态，
它可以在不同版本中定义为不同的样子，而无需更改每个人的设备驱动程序。

有关的信息kdev_t定义在<linux/kdev_t.h>。无需在驱动程序中显式包含标头，因为
<linux/fs.h>它会为您完成。

您可以执行以下宏和函数的操作kdev_t：
```
MAJOR(kdev_t dev);
    Extract the major number from a kdev_t structure.

MINOR(kdev_t dev);
    Extract the minor number.

MKDEV(int ma, int mi);
    Create a kdev_t built from major and minor numbers.

kdev_t_to_nr(kdev_t dev);
    Convert a kdev_t type to a number (a dev_t).

to_kdev_t(int dev);
    Convert a number to kdev_t. Note that dev_t is not defined in kernel mode, and therefore int is used.
```


### 内核管理设备号
[Linux设备驱动-内核如何管理设备号](https://zhuanlan.zhihu.com/p/446439925#:~:text=%E6%AC%A1%E8%AE%BE%E5%A4%87%E5%8F%B7%E7%94%B1%E9%A9%B1%E5%8A%A8%E7%A8%8B%E5%BA%8F%E4%BD%BF%E7%94%A8%EF%BC%8C%E9%A9%B1%E5%8A%A8%E7%A8%8B%E5%BA%8F%E7%94%A8%E6%9D%A5%E6%8F%8F%E8%BF%B0%E4%BD%BF%E7%94%A8%E8%AF%A5%E9%A9%B1%E5%8A%A8%E7%9A%84%E8%AE%BE%E5%A4%87%E7%9A%84%E5%BA%8F%E5%8F%B7%EF%BC%8C%E5%BA%8F%E5%8F%B7%E4%B8%80%E8%88%AC%E4%BB%8E%200%20%E5%BC%80%E5%A7%8B%E3%80%82%20Linux%20%E8%AE%BE%E5%A4%87%E5%8F%B7%E7%94%A8%20dev_t%20%E7%B1%BB%E5%9E%8B%E7%9A%84%E5%8F%98%E9%87%8F%E8%BF%9B%E8%A1%8C%E6%A0%87%E8%AF%86%EF%BC%8C%E8%BF%99%E6%98%AF%E4%B8%80%E4%B8%AA%2032%E4%BD%8D,%2A%2F%20%E2%80%8B%20typedef%20u32%20__kernel_dev_t%3B%20typedef%20__kernel_dev_t%20dev_t%3B)

## udev

### 介绍

在对待设备文件的问题上，Linux改变了几次策略。在Linux早期，设备文件仅仅是是一些
带有适当的属性集的普通文件，它由mknod命令创建，文件存放在/dev目录下。后来，采用了
devfs,一个基于内核的动态设备文件系统，他首次出现在2.3.46 内核中。Mandrake，Gentoo
等Linux分发版本采用了这种方式。devfs创建的设备文件是动态的。但是devfs有一些严重的
限制，从 2.6.13版本后移走了。目前取代他的便是文本要提到的udev－－一个用户空间程序。

在Linux2.6内核中，使用udev取代了devfs。Linux设计中强调的一个基本观点是机制和策略的
分离。机制是做某件事情的固定步骤、方法，策略时每一个步骤所采取的不同方式。机制是
相对固定的，而每个步骤采用的策略是不固定的。类似于在一个大的协议框架下（机制），
每个细节的算法实现（策略）可以不同。

举例来讲，在内核文件系统中创建一个文件，属于机制的范畴，但是创建的文件名字叫啥，
设置什么属性，这是策略的范畴，策略是提供给使用者的，这也是为什么使用位于用户空间
的udev取代位于内核空间的devfs，内核中devfs对策略做了一些不好的干预。

udev完全工作在用户态，利用设备加入或移除时内核所发送的热插拔事件来工作。在热插拔时，
设备的详细信息会由内核通过netlink套接字发送出来，发出来的事情称为uevent。udev的
设备命名策略、权限控制和事件处理都是在用户态下完成的，它利用从内核收到的信息来进行
创建设备文件节点等工作。

在此之前的设备文件管理方法（静态文件和devfs）有几个缺点：

* 不确定的设备映射。特别是那些动态设备，例如USB设备，设备文件到实际设备的映射并
  不可靠和确定。举一个例子：如果你有两个USB打印机。一个可能称为 /dev/usb/lp0,
  另外一个便是/dev/usb/lp1。但是到底哪个是哪个并不清楚，lp0,lp1和实际的设备没有
  一一对应的关系，因为他可能因为发现设备的顺序，打印机本身关闭等原因而导致这种
  映射并不确定。理想的方式应该是：两个打印机应该采用基于他们的序列号或者其他标识
  信息的唯一设备文件来映射。但是静态文件和devfs都无法做到这点。

* 没有足够的主/辅设备号。我们知道，每一个设备文件是有两个8位的数字：一个是主设备号，
  另外一个是辅设备号来分配的。这两个8位的数字加上设备类型（块设备或者字符设备）
  标识一个设备。不幸的是，关联这些设备的的数字并不足够。

* /dev目录下文件太多。一个系统采用静态设备文件关联的方式，那么这个目录下的文件
  必然是足够多。而同时你又不知道在你的系统上到底有那些设备文件是激活的。

* 命名不够灵活。尽管devfs解决了以前的一些问题，但是它自身又带来了一些问题。其中
  一个就是命名不够灵活；你别想非常简单的就能修改设备文件的名字。缺省的devfs命令
  机制本身也很奇怪，他需要修改大量的配置文件和程序。

* 内核内存使用，devfs特有的另外一个问题是，作为内核驱动模块，devfs需要消耗大量的
  内存，特别当系统上有大量的设备时（比如上面我们提到的系统一个上有好几千磁盘时）

udev的目标是想解决上面提到的这些问题，他采用用户空间(user-space)工具来管理/dev/
目录树，他和文件系统分开。知道如何改变缺省配置能让你知道如何定制自己的系统，比如
创建设备字符连接，改变设备文件属组，权限等。

### uevent 机制

reference:
[sysfs、udev 和 它们背后的 Linux 统一设备模型](http://liujunming.top/2019/08/11/sysfs%E3%80%81udev%20%E5%92%8C%20%E5%AE%83%E4%BB%AC%E8%83%8C%E5%90%8E%E7%9A%84%20Linux%20%E7%BB%9F%E4%B8%80%E8%AE%BE%E5%A4%87%E6%A8%A1%E5%9E%8B/)

uevent是Kobject的一部分，用于在Kobject状态发生改变时，例如增加、移除等，通知用户
空间程序。用户空间程序收到这样的事件后，会做相应的处理。

该机制通常是用来支持热拔插设备的，例如U盘插入后，USB相关的驱动软件会动态创建用于
表示该U盘的device结构（相应的也包括其中的kobject），并告知用户空间程序，为该U盘
动态的创建/dev/目录下的设备节点，更进一步，可以通知其它的应用程序，将该U盘设备
mount到系统中，从而动态的支持该设备。

![](./pic/1.gif)

由此可知，uevent的机制是比较简单的，设备模型中任何设备有事件需要上报时，会触发
uevent提供的接口。uevent模块准备好上报事件的格式后，可以通过两个途径把事件上报到
用户空间：一种是通过kmod模块，直接调用用户空间的可执行文件；另一种是通过netlink
通信机制，将事件从内核空间传递给用户空间。

### udev配置文件

主要的udev配置文件是/etc/udev/udev.conf。这个文件通常很短，他可能只是包含几行#开头
的注释，然后有几行选项

这里不做深入剖析，了解细节可参考：
[LINUX下 Udev详解](https://zhuanlan.zhihu.com/p/373517974)
[udev机制简介](http://just4coding.com/2022/11/30/udev/)

### 创建设备节点

```C
// dirvers/base/core.h

// class：新构建的class
// parent：新kobject对象的上一层节点，一般为NULL
// dev_t：属性文件记录该设备号
// drvdata：私有数据，一般为NULL
// fmt：变参参数，一般用来设置kobject对象的名字，体现在/dev/下
// ret:  返回子设备device指针，如果成功会在dev下创建文件
struct device *device_create(const struct class *class, struct device *parent,
                 dev_t devt, void *drvdata, const char *fmt, ...)

void device_destroy(const struct class *class, dev_t devt)
```
### 老方法创建设备节点

在使用udev之前，需要使用其他方法创建设备节点

动态分配的缺点是您无法提前创建设备节点，因为不能保证分配给模块的主设备号始终相同。
这意味着将无法使用驱动程序的按需加载。对于驱动程序的正常使用来说，这几乎不是问题，
因为一旦分配了编号，您就可以从 中读取它/proc/devices。因此，要使用动态主设备号加载
驱动程序，可以用一个简单的脚本来替换insmod的调用，该脚本在调用insmod后 读取/proc/devices
以创建特殊文件。

典型的/proc/devices文件如下所示：
```shell
Character devices:
 1 mem
 2 pty
 3 ttyp
 4 ttyS
 6 lp
 7 vcs
 10 misc
 13 input
 14 sound
 21 sg
180 usb

Block devices:
 2 fd
 8 sd
 11 sr
 65 sd
 66 sd
 ```
因此，可以使用awk 等工具编写脚本，加载已分配动态编号的模块。
以下脚本(scull_load.sh)是scull发行版的一部分。以模块形式分发驱动程序的用户可以从系统rc.local文件中调用此类脚本，或者在需要该模块时手动调用它。
```shell
#!/bin/sh
module="scull"
device="scull"
mode="664"

# invoke insmod with all arguments we were passed
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod -f ./$module.o $* || exit 1

# remove stale nodes
rm -f /dev/${device}[0-3]

major=`awk "\\$2==\"$module\" {print \\$1}" /proc/devices`

mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1
mknod /dev/${device}2 c $major 2
mknod /dev/${device}3 c $major 3

# give appropriate group/permissions, and change the group.
# Not all distributions have staff; some have "wheel" instead.
group="staff"
grep '^staff:' /etc/group > /dev/null || group="wheel"

chgrp $group /dev/${device}[0-3]
chmod $mode /dev/${device}[0-3]
```

通过重新定义变量和调整mknod 行，可以将该脚本改编为另一个驱动程序的加载脚本。以上
脚本创建了四个设备，因为scull源中的默认值是四个。

脚本的最后几行可能看起来很晦涩：为什么要更改设备的组和模式？原因是该脚本必须由
超级用户运行，因此新创建的特殊文件归root所有。默认的权限位使得只有 root 具有写
访问权限，而任何人都可以获得读访问权限。通常，设备节点需要不同的访问策略，因此
必须以某种方式更改访问权限。我们的脚本中的默认设置是授予一组用户访问权限，但需求
可能会有所不同。后续在 sculluid 的代码 将演示驱动程序如何强制执行自己的设备访问
授权。然后可以使用scull_unload脚本 来清理/dev目录并删除模块。

作为使用一对脚本进行加载和卸载的替代方法，您可以编写一个 init 脚本，放置在这些
脚本的目录中。作为 scull源代码的一部分，我们提供了一个相当完整且可配置的 init 脚本
示例，称为 scull.init; 它接受常规参数——“start”或“stop”或“restart”——并执行
scull_load 和 scull_unload的角色。

如果重复创建和销毁/dev节点听起来有点大材小用，那么有一个有用的解决方法。如果您仅加载
和卸载单个驱动程序，则可以在第一次使用脚本创建特殊文件后使用rmmod和insmod ：动态数字
不是随机的，如果您选择相同的数字，则可以指望选择相同的数字。不要与其他（动态）模块
混淆。在开发过程中避免冗长的脚本很有用。但显然，这个技巧一次不能扩展到多个驱动程序。

我们认为，分配主编号的最佳方法是默认动态分配，同时让您可以选择在加载时甚至编译时
指定主编号。scull实现使用全局变量来scull_major保存所选的数字。该变量被初始化为
SCULL_MAJOR，在 scull.h中定义。分布式源中的默认值为 SCULL_MAJOR0，表示“使用动态分配”。
scull_major 用户可以接受默认值或选择特定的主编号，方法是在编译前修改宏或在insmod命令
行上指定值。insmod 指定可以通过scull_load脚本实现，用户可以在命令行上将参数传递给insmod。

这是我们在scull源代码 中用于获取主设备号的代码：
```shell
 result = register_chrdev(scull_major, "scull", &scull_fops);
 if (result < 0) {
  printk(KERN_WARNING "scull: can't get major %d\n",scull_major);
  return result;
 }
 if (scull_major == 0) scull_major = result; /* dynamic */
```

## 字符设备驱动注册和注销

### 重要数据
```c
// include/linux/device/driver.h
struct device_driver {
    const char        *name;
    struct bus_type        *bus;

    struct module        *owner;
    const char        *mod_name;    /* used for built-in modules */

    bool suppress_bind_attrs;    /* disables bind/unbind via sysfs */
    enum probe_type probe_type;

    const struct of_device_id    *of_match_table;
    const struct acpi_device_id    *acpi_match_table;

    int (*probe) (struct device *dev);
    void (*sync_state)(struct device *dev);
    int (*remove) (struct device *dev);
    void (*shutdown) (struct device *dev);
    int (*suspend) (struct device *dev, pm_message_t state);
    int (*resume) (struct device *dev);
    const struct attribute_group **groups;
    const struct attribute_group **dev_groups;

    const struct dev_pm_ops *pm;
    void (*coredump) (struct device *dev);

    struct driver_private *p;
};
```


### 接口
```c
// linux/include/linux/module.h
module_init
module_exit
```

## 字符设备注册和注销

### 重要数据
```c
// linux/include/linux/cdev.h
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    struct list_head list;
    dev_t dev;
    unsigned int count;
} __randomize_layout;
```

### 接口

申请空间，如果不使用cdev指针的话，也可以不用cdev_alloc
```c
// linux/fs/char_dev.c
struct cdev *my_cdev = cdev_alloc();
```

初始化已分配到的结构体，指定文件操作/方法
```c
void cdev_init(struct cdev *cdev, struct file_operations *fops);
my_cdev->owner = THIS_MODULE;  // 所有者
my_cdev->ops = &my_fops;
```

注册设备到内核
```c
// linux/fs/char_dev.c

// 参数说明：
// dev:  cdev结构体
// num:  该设备对应的地一个设备编号
// count: 应该和该设备关联的设备编号的数量
// 注意：可能存在注册失败的问题，因此要关注返回值！
int cdev_add(struct cdev *dev, dev_t num, unsigned int count);
```

从内核中注销
```c
// linux/fs/char_dev.c
void cdev_del(strut cdev *dev);
```

内核中的老接口
```c
int register_chrdev(unsigned int major, const char *name, struct file_operations *fops);  // 注册
int unregister_chrdev(unsigned int major, const char *name); // 注销
```

### 创建设备节点

- mknod

mknod DEVNAME {b | c}  MAJOR  MINOR
1. DEVNAME是要创建的设备文件名，如果想将设备文件放在一个特定的文件夹下，就需要
   先用mkdir在dev目录下新建一个目录
2. b和c 分别表示块设备和字符设备
3. MAJOR和MINOR分别表示主设备号和次设备号：

- 更好的方法是使用 device_create 参考 udev章节或者demo示例


## 重要的数据结构

大部分基本的驱动程序操作涉及到三个重要的内核数据结构，分别是 file_operations、file和inode

### 文件操作

文件操作一般理解为设备一侧的，在注册设备的时候指定，操作设备的时候会调用这些接口，
而不是作为驱动一侧

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
    ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
    int (*iopoll)(struct kiocb *kiocb, bool spin);
    int (*iterate) (struct file *, struct dir_context *);
    int (*iterate_shared) (struct file *, struct dir_context *);
    __poll_t (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    unsigned long mmap_supported_flags;
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **, void **);
    long (*fallocate)(struct file *file, int mode, loff_t offset,
              loff_t len);
    void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
    unsigned (*mmap_capabilities)(struct file *);
#endif
    ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
            loff_t, size_t, unsigned int);
    loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
                   struct file *file_out, loff_t pos_out,
                   loff_t len, unsigned int remap_flags);
    int (*fadvise)(struct file *, loff_t, loff_t, int);
} __randomize_layout;

```

### file结构

* file结构与用户空间的FILE没有任何关联，FILE在C库中定义，且不会出现在内核空间
* file结构代表一个打开的文件（它不仅仅限定于设备驱动程序，系统中每个打开的文件
  在内核空间都有一个file结构）
* file指的是结构本身，filp则是指向该结构的指针

```c
struct file {
    union {
        struct llist_node   fu_llist;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path     f_path;
    struct inode        *f_inode;   /* cached value */
    const struct file_operations    *f_op;

    /*
     * Protects f_ep, f_flags.
     * Must not be taken from IRQ context.
     */
    spinlock_t      f_lock;
    enum rw_hint        f_write_hint;
    atomic_long_t       f_count;
    unsigned int        f_flags;
    fmode_t         f_mode;
    struct mutex        f_pos_lock;
    loff_t          f_pos;
    struct fown_struct  f_owner;
    const struct cred   *f_cred;
    struct file_ra_state    f_ra;

    u64         f_version;
#ifdef CONFIG_SECURITY
    void            *f_security;
#endif
    /* needed for tty driver, and maybe others */
    void            *private_data;

#ifdef CONFIG_EPOLL
    /* Used by fs/eventpoll.c to link all the hooks to this file */
    struct hlist_head   *f_ep;
#endif /* #ifdef CONFIG_EPOLL */
    struct address_space    *f_mapping;
    errseq_t        f_wb_err;
    errseq_t        f_sb_err; /* for syncfs */
} __randomize_layout
  __attribute__((aligned(4)));  /* lest something weird decides that 2 is OK */

```

### inode结构

内核用inode结构在内部表示文件，因此它和file结构不同，file表示打开的文件描述符。
对于单个文件，可能会有 许多个 表示打开的文件描述符(file)，但他们都指向 单个
inode结构。

inode结构中包含了大量有关文件的信息。但通常只有下面两个字段对编写驱动程序代码有用：

dev_t i_rdev
- 对表示设备文件的inode结构，该字段包含了真正的设备编号

struct cdev *i_cdev
- struct cdev 是表示字符设备在内核中的结构。当inode指向一个字符设备文件时，该字段
  包含了指向struct cdev结构体的指针

为了鼓励编写可移植性更强的代码，建议使用如下方法从 inode 中获取主设备号和从设备号
```c
unsigned int iminor(struct inode *inode);
unsigned int iminor(struct inode *inode);
```

## 其他

### open 和 release

open方法提供给驱动程序以初始化的能力，从而为以后的操作做准备，open大多数情况下
应该完成如下工作：
- 检测设备特定的错误（诸如设备未就绪或类似的硬件问题）
- 如果设备是首次打开，则对其进行初始化
- 如果有必要，更新 f_op 指针
- 分配并填写置于 filp->private_data 里的数据

原型：
```c
int (*open)(struct inode *inoed, struct file *filep);
```

其中 inode 参数在 i_cdev 字段中包含了我们所需要的信息，即 我们先前设置的 cdev 结构体

这里一个函数可能需要使用到：
```c
container_of(pointer, container_type, container_field);  // 定义在<linux/kernel.h>中
```

该函数根据指定结构体(container_type)和其内部的成员(container_field)，计算入参
(pointer)对应的结构体(container_type)实例入口，这里的入参(pointer)也是结构体
(container_type)的成员(container_field)

当我们创建一个自己使用的结构体时，如果在结构体内部放了 cdev，则可以根据结构体内部
放的 cdev设备 (open函数中的 inode->i_cdev) 计算出我们自己创建的结构体的入口

release 方法与 open方法相反，该方法应该完成以下工作：
- 释放由 open 分配、保存在 filp->private_data 中的所有内容
- 在最后一次关闭操作时关闭设备

如何保证关闭一个设备文件的次数不会比打开的次数多？

内核对每个 file 结构都会记录被引用的次数。无论是 fork 还是 dup，都不会创建新的
数据结构（仅由 open 创建），他们只是增加已有结构体中的计数。只有在 file 结构体
归零时，close系统调用才会执行 release 方法，这只在删除这个结构体时才会发生，
因此release和close的系统调用间的关系保证了对于每次open驱动程序只会看到对应的
一次release调用。


### read 和 write

read 拷贝数据到应用程序空间，write拷贝数据到内核空间，原型：
```c
// filp: 文件指针
// count: 数据传输长度
// buff: 指向用户空间的缓冲区
// offp: 指向"long offset type（长偏移量类型）"对象的指针，这个对象指明用户在
//       文件中进行存取操作的位置
ssize_t read(struct file *filp, char __user *buff, size_t count, loff_t *offp);
ssize_t write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);
```

出错时，read 和 write 方法都返回一个负值。大于等于0的返回值告诉调用程序成功传输了
多少字节。如果在正确传输部分数据之后发生了错误，则返回值是成功传输的字节数，这个
错误只能在下一次函数调用时才会得到报告。

尽管内核函数通过返回负值来表示错误，但运行在用户空间的程序看到的始终是返回-1。为了
找到出错原因，用户空间爱呢的程序必须访问 errno 变量。

read 方法
- 如果返回值等于传递给read系统调用的count参数，则说明所请求的字节数传输成功完成了
- 如果返回值是正的，但是比count小，说明只有部分数据成功传输。这种情况因设备的
  不同可能有许多原因。大部分情况下程序会重新读书据。例如，如果用fread函数读
  数据，这个库函数就会不断调用系统调用，直到所请求的数据传输完毕为止
- 如果返回值为 0，表示已经到达了文件尾
- 负值意味着发生了错误，该值指明了发生了什么错误，错误码在<linux/errno.h>中定义

write 方法
- 如果返回值等于count，则完成了所请求数目的字节传送
- 如果返回值是正的，但小于 count， 则只传输了部分数据。程序很可能再次写入余下的数据
- 如果值为0,意味着什么也没写入。这个结果不是错误。类似read方法，标准库会重复调用write
- 负值意味着发生了错误，类似read方法

read 和 write 方法的 buff 参数是用户空间的指针。因此，内核代码不能直接引用其中的
内容，原因如下：
- 由于驱动程序所运行的架构不同或内核配置不同，在内核模式中运行时，用户空间的
  指针可能是无效的。该地址可能根本无法被映射到内核空间，或者可能指向某些随机数据
- 即使该指针在内核空间中代表相同的东西，但用户空间的内存是分页的，而在系统接口
  被调用时，涉及到的内存可能根本就不在RAM中。对用户空间内存的直接引用将导致页
  错误，而这对于内核代码来说是不允许发生的事情。其结果可能是一个"oops"，他将导致
  调用该系统接口的进程死亡。
- 我们讨论的指针可能由用户程序提供，而该程序可能存在缺陷或者是个恶意程序。如果
  我们的驱动程序盲目引用用户提供的指针，将导致系统出现打开的后门，从而允许用户
  空间程序随意访问或覆盖系统中的内存。

从内核空间访问用户空间数据的接口：
```c
unsigned long copy_to_user(void __user *to, const void *from, unsigned long count);
unsigned long copy_from_user(void *to, const void __user *from, unsigned long count);
```

这两个内核函数的作用并不限于数据传输，他们还检查用户空间的指针是否有效。如果指针
无效，就不会进行拷贝；另外如果在拷贝过程中遇到无效地址，则仅仅会复制部分数据。
在这两种情况下返回值是还需要拷贝的内存数量值。

如果不需要检查用户空间的指针，或者已经检查过，则可以调用：
```c
"__copy_to_user" 和 "__copy_from_user"
```

`__user`:
`__user`是一个宏，表明其后的指针指向用户空间，实际上更多的充当了代码自注释的功能

内核空间虽然可以访问用户空间的缓冲区，但是在访问之前，一般需要先检查其合法性，
通过 access_ok(type, addr, size)进行判断，以确定传入的缓冲区的确属于用户空间，例如：
```C
if(!access_ok(VERIFY_WRITE, buf, count))
    return -EFAULT;
  
if(__put_user(inb(i),tmp) < 0)
    return -EFAULT;
```

如果要复制的内存是简单类型，如：char、int、long等，则可以使用简单的 put_user()
(内核->用户)和 get_user()(用户->内核) __put_user()和put_user()的区别在于前者不
进行类似access_ok()的检查，后者会进行这一检查。

## demo

可以直接参考 0.mDemo/1.base
