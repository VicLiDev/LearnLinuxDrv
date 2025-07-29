/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/

/*
 * reference:
 * https://olegkutkov.me/2018/03/14/simple-linux-character-device-driver/
 */

/* ============================================================================
 * Linux驱动的两种加载方式:
 *   1.直接编译进内核,在linux启动时加载
 *   2.采用内核模块的方式,可动态加载与卸载
 *
 * 编译进内核(这里只简要叙述):
 *   1.复制源码到相应目录下(可能是/drivers/char)
 *   2.Kconfig添加相应内容
 *   3.Makefile添加相应内容
 *   4.make menuconfig 选择添加的模块
 *   5.进行编译
 * 编译进模块，必须包含以下接口:
 *   module_init(your_init_func)  //模块初始化接口
 *   module_exit(your_exit_func)  //模块卸载接口
 *
 * ============================================================================
 * 模块运行在内核态,不能使用用户态C库函数中的printf函数,而要使用printk函数打印
 * 调试信息。由于printk打印分8个等级，等级高的被打印到控制台上，而等级低的却输
 * 出到日志文件中,数越小等级越高。
 * // include/linux/kern_levels.h
 * #define KERN_EMERG	KERN_SOH "0"	// system is unusable
 * #define KERN_ALERT	KERN_SOH "1"	// action must be taken immediately
 * #define KERN_CRIT	KERN_SOH "2"	// critical conditions
 * #define KERN_ERR	KERN_SOH "3"	    // error conditions
 * #define KERN_WARNING	KERN_SOH "4"	// warning conditions
 * #define KERN_NOTICE	KERN_SOH "5"	// normal but significant condition
 * #define KERN_INFO	KERN_SOH "6"	// informational
 * #define KERN_DEBUG	KERN_SOH "7"	// debug-level messages
 * 
 * #define KERN_DEFAULT	""		        // the default kernel loglevel
 *
 * 执行:cat /proc/sys/kernel/printk 可以看到四个数,分别表示:
 *   控制台日志级别：优先级高于该值的消息将被打印至控制台
 *   默认的消息日志级别：将用该优先级来打印没有优先级的消息
 *   最低的控制台日志级别：控制台日志级别可被设置的最小值(最高优先级)
 *   默认的控制台日志级别：控制台日志级别的缺省值
 * 如果有需要可以修改，不够打印级别的信息会被写到日志中可通过dmesg 命令来查看
 *
 * 这里的控制台一般是指串口，查看串口的方法，不太确定三种命令的数据如何分辨，待完善：
 *   cat /proc/tty/drivers 或 cat /proc/tty/driver/serial 或 cat /proc/cmdline
 *
 * 设置串口属性：
 *   # stty --help
 *   Usage:  stty [-F DEVICE] [--file=DEVICE] [SETTING]...
 *   　 or:  stty [-F DEVICE] [--file=DEVICE] [-a|--all]
 *   　 or:  stty [-F DEVICE] [--file=DEVICE] [-g|--save]
 *   [选项]
 *   -a, --all :     以容易阅读的方式打印当前的所有配置；
 *   -g, --save:     以stty终端可读方式打印当前的所有配置。
 *   -F, --file:     打印当前的所有设置打开指定的设备,并用此设备作为输入来代替标准输入
 *   [参数]
 *   终端设置：指定终端命令行的设置选项。
 *
 *   使用举例：
 *   用stty查看串口参数：stty -F /dev/ttySn -a  #ttySn为要查看的串口
 *   用stty设置串口参数：
 *   stty -F /dev/ttyS0 ispeed 115200 ospeed 115200 cs8 -parenb -cstopb  -echo
 *   该命令将串口1（/dev/ttyS0）输入输出都设置成115200波特率，8位数据模式，
 *   奇偶校验位-parenb，停止位-cstopb,同时-echo禁止终端回显。
 *
 * ============================================================================
 */

#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
/* file opt */
#include <linux/uaccess.h>
#include <linux/fs.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";
/*
 * 可以为模块定义一个参数，在加载模块时，用户可以像模块传递参数：
 *     module_param(<paraName>, <paraType>, <para Read/write permissions>)
 * 安装传参，如果有多个参数需要以空格进行分隔：
 *     insmod/modprobe <module>.ko <paraName>=<paraValue>
 * 如果不传递，参数将使用模块内定义的缺省值。如果模块被内置（编译进kernel），
 * 就无法insmod了，但是bootloader可以在bootargs里以"<modName>.<paraName>=<value>"
 * 的形式给该内置的模块传递参数
 *
 * 参数的类型可以是byte、short、ushort、int、uint、long、ulong、charp（字符指针）、
 * bool、invbool（布尔的反），在模块被编译时会将 module_param 中声明的类型与变量
 * 定义的类型进行比较，判断是否一致。
 *
 * 模块也可以拥有参数组：
 * module_param_array(<arrayName>, <arrayType>, <arrayLen>, <para Read/write permissions>)
 *
 * 模块被加载之后，在 /sys/module/ 目录下将出现以此模块命名的目录。
 * 当参数为0时，表示该参数不可读写，参数文件也不会导出到用户可访问的文件系统
 * 当参数不为0时，此模块的目录下将出现parameters目录，其中包含一系列以参数命名的
 * 文件节点，这些文件的权限就是传入module_param()的权限，文件的内容为参数的值
 *
 * 权限：
 * #define S_IRWXUGO	(S_IRWXU|S_IRWXG|S_IRWXO)
 * #define S_IALLUGO	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXUGO)
 * #define S_IRUGO		(S_IRUSR|S_IRGRP|S_IROTH)
 * #define S_IWUGO		(S_IWUSR|S_IWGRP|S_IWOTH)
 * #define S_IXUGO		(S_IXUSR|S_IXGRP|S_IXOTH)
 */
module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

char *demo1_export_test = "this is export symbol from module kDemo_1_base";
/*
 * Linux的 /proc/kallsyms 文件对应着内核符号表，它记录了符号以及符号所在的内存地址。
 * 模块可以使用如下宏导出符号到内核符号表中，导出的符号可以被其他模块使用，只需要
 * 使用前声明一下就可以。
 * EXPORT_SYMBOL(<symbolName>)
 * EXPORT_SYMBOL_GPL(<symbolName>)  // 只适用于包含GPL许可权的模块
 */
EXPORT_SYMBOL(demo1_export_test);

static int m_chrdev_open(struct inode *inode, struct file *file);
static int m_chrdev_release(struct inode *inode, struct file *file);
static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

/* initialize file_operations */
static const struct file_operations m_chrdev_fops = {
    .owner      = THIS_MODULE,
    .open       = m_chrdev_open,
    .release    = m_chrdev_release,
    .unlocked_ioctl = m_chrdev_ioctl,
    .read       = m_chrdev_read,
    .write       = m_chrdev_write
};

/* device data holder, this structure may be extended to hold additional data */
struct m_chr_device_data {
    struct cdev cdev;
};

/* global storage for device Major number */
/* 多个设备可以对应一个驱动 */
static int dev_major = 0;

/* sysfs class structure */
/* 多个设备对应一个驱动，自然也对应同一个class */
static struct class *m_chrdev_class = NULL;

/* array of m_chr_device_data for */
static struct m_chr_device_data m_chrdev_data[MAX_DEV];

static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device open\n");
    return 0;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device close\n");
    return 0;
}

static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("M_CHRDEV: Device ioctl\n");
    return 0;
}

static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    uint8_t *data = "Hello from the kernel world!\n";
    size_t datalen = strlen(data);

    printk("Reading device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (count > datalen) {
        count = datalen;
    }

    if (copy_to_user(buf, data, count)) {
        return -EFAULT;
    }

    return count;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    size_t maxdatalen = 30, ncopied;
    uint8_t databuf[30];

    printk("Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (count < maxdatalen) {
        maxdatalen = count;
    }

    ncopied = copy_from_user(databuf, buf, maxdatalen);

    if (ncopied == 0) {
        printk("Copied %zd bytes from the user\n", maxdatalen);
    } else {
        printk("Could't copy %zd bytes from the user\n", ncopied);
    }

    databuf[maxdatalen] = 0;

    printk("Data from the user: %s\n", databuf);

    return count;
}

/*
 * 模块加载函数
 * 在Linux中，所有表示为__init 的函数如果直接编译进内核，在连接时都会放在.init.text
 * 这个区段内
 * 所有的__init 函数在在区段.initcall.init中还保留了一份函数指针，在初始化时，内核会
 * 通过这些函数指针调用这些__init函数并在初始化完成后释放init区段（包括.init.text、
 * .initcall.init等）的内存
 * 除了函数以外，数据也可以被定义为__initdata，对于知识初始化阶段需要的数据，内核
 * 在初始化完后，也可以释放他们占用的内存
 * #define __init		__section(".init.text")
 * #define __initdata	__section(".init.data")
 * #define __exitdata	__section(".exit.data")
 * #define __exit_call	__used __section(".exitcall.exit")
 *
 * 在Linux内核中，可以使用request_module(const char *fmt, ...)函数加载模块内核模块
 */
static int __init m_chr_init(void)
{
    int err, idx;
    dev_t devno;

    /* 可以使用 cat /dev/kmsg 实时查看打印 */
    printk(KERN_INFO "module %s init desc:%s\n", __func__, init_desc);

    printk(KERN_INFO "git version:%s\n", DEMO_GIT_VERSION);

    /* Dynamically apply for device number */
    err = alloc_chrdev_region(&devno, 0, MAX_DEV, "m_chrdev");

    /*
     * 注意这里设备号会作为/dev中设备节点和驱动的一个纽带
     * 设备初始化、注册需要用到设备号
     * 创建设备节点也需要用到设备号
     */
    dev_major = MAJOR(devno);

    /*
     * create sysfs class
     * 创建设备节点的时候也用到了class，因此在/sys/class/m_chrdev_cls下可以看到
     * 链接到设备的软链接
     */
    m_chrdev_class = class_create("m_chrdev_cls");
    m_chrdev_class->dev_uevent = m_chrdev_uevent;

    /* Create necessary number of the devices */
    for (idx = 0; idx < MAX_DEV; idx++) {
        /* init new device */
        cdev_init(&m_chrdev_data[idx].cdev, &m_chrdev_fops);
        m_chrdev_data[idx].cdev.owner = THIS_MODULE;

        /* add device to the system where "idx" is a Minor number of the new device */
        cdev_add(&m_chrdev_data[idx].cdev, MKDEV(dev_major, idx), 1);

        /* create device node /dev/m_chrdev_x where "x" is "idx", equal to the Minor number */
        device_create(m_chrdev_class, NULL, MKDEV(dev_major, idx), NULL, "m_chrdev_%d", idx);
    }

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    for (idx = 0; idx < MAX_DEV; idx++) {
        device_destroy(m_chrdev_class, MKDEV(dev_major, idx));
    }

    class_destroy(m_chrdev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    return;
}

module_init(m_chr_init);
module_exit(m_chr_exit);


/*
 * 内核模块领域可接受的LICENSE包括 “GPL”、“GPL v2”、“GPL and additional rights”、
 * “Dual BSD/GPL”、“Dual MPL/GPL”和“Proprietary”（关于模块是否可采用非GPL许可权，
 * 如 Proprietary，这个在学术界是有争议的）
 * 大多数情况下内核模块应该遵守GPL兼容许可权。Linux内核模块最常见的是使用GPL v2
 */
MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
/* MODULE_xxx这种宏作用是用来添加模块描述信息（可选） */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("base demo");                      /* 描述模块的别名信息 */
/*
 * 设置内核模块版本，可以通过modinfo kDemo.ko查看
 * 如果不使用MODULE_VERSION设置模块信息，modinfo会看不到 version 信息
 */
MODULE_VERSION(DEMO_GIT_VERSION);
