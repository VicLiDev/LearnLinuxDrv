/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/

/*
 * ============================================================================
 * 参考博客：
 * https://www.cnblogs.com/arnoldlu/p/8879800.html
 * https://blog.csdn.net/STN_LCD/article/details/79088975
 * https://blog.csdn.net/weixin_45264425/article/details/128233119
 *
 * ============================================================================
 *
 * 使用内核态文件读写函数的注意点：
 * 1. 其实Linux Kernel组成员不赞成在kernel中独立的读写文件(这样做可能会影响到策略
 *    和安全问题)，对内核需要的文件内容，最好由应用层配合完成。
 * 2. 在可加载的kernel module中使用这种方式读写文件可能使模块加载失败，原因是内核
 *    可能没有EXPORT你所需要的所有这些函数。
 * 3. 分析相关函数的参数可以看出，这些函数的正确运行需要依赖于进程环境，因此，
 *    有些函数不能在中断的handle或Kernel中不属于任可进程的代码中执行，否则可能出现
 *    崩溃，要避免这种情况发生，可以在kernel中创建内核线程，将这些函数放在线程
 *    环境下执行(创建内核线程的方式请参数kernel_thread()函数)。
 *
 * ============================================================================
 *
 * 旧版本kernel的限制
 * ssize_t vfs_read(struct file* filp, char __user* buffer, size_t len, loff_t* pos);
 * ssize_t vfs_write(struct file* filp, const char __user* buffer, size_t len, loff_t* pos);
 * filp：文件指针，由filp_open()函数返回。
 * buffer：缓冲区，从文件中读出的数据放到这个缓冲区，向文件中写入数据也在这个缓冲区。
 * len：从文件中读出或者写入文件的数据长度。
 * pos：为文件指针的位置，即从什么地方开始对文件数据进行操作。
 * 
 * 注意这两个函数的第二个参数buffer，前面都有__user修饰符，这就要求这两个buffer
 * 指针都应该指向用户空间的内存，如果对该参数传递kernel空间的指针，这两个函数都会
 * 返回失败-EFAULT。但在Kernel中，我们一般不容易生成用户空间的指针，或者不方便独立
 * 使用用户空间内存。要使这两个读写函数使用kernel空间的buffer指针也能正确工作，
 * 需要使用set_fs()函数或宏(set_fs()可能是宏定义)，其原形如下：
 *     void set_fs(mm_segment_t fs);
 * 该函数的作用是改变kernel对内存地址检查的处理方式，其实该函数的参数fs只有两个
 * 取值：USER_DS，KERNEL_DS，分别代表用户空间和内核空间，
 * 默认情况下，kernel取值为USER_DS，即对用户空间地址检查并做变换。那么要在这种
 * 对内存地址做检查变换的函数中使用内核空间地址，就需要使用set_fs(KERNEL_DS)
 * 进行设置。get_fs()一般也可能是宏定义，它的作用是取得当前的设置，这两个函数的
 * 一般用法为：
 *   mm_segment_t old_fs;
 *   old_fs = get_fs();
 *   set_fs(KERNEL_DS);
 *   ...... //与内存有关的操作
 *   set_fs(old_fs);
 * 还有一些其它的内核函数也有用__user修饰的参数，在kernel中需要用kernel空间的内存
 * 代替时，都可以使用类似办法。
 * 使用vfs_read()和vfs_write()最后需要注意的一点是最后的参数loff_t * pos，pos所
 * 指向的值要初始化，表明从文件的什么地方开始读写。
 * 
 * demo: 
 *   mm_segment_t fs;
 * 
 *   获取当前线程的thread_info->addr_limit
 *   fs =get_fs();
 * 
 *   KERNEL_DS范围很大，到0xffffffffffffffff。
 *   USER_DS范围较小，到0x7ffffffff000。
 *   由Linux内存分布图可知，KERNEL_DS意味着可以访问整个内存所有空间，
 *   USER_DS只能访问用户空间内存。
 *   通过set_fs可以改变thread_info->addr_limit的大小。
 *   这里将能访问的空间thread_info->addr_limit扩大到KERNEL_DS
 *   set_fs(KERNEL_DS);
 * 
 *   将thread_info->addr_limit切换回原来值
 *   set_fs(fs);
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
#define FILE_NAME "/home/lhj/test/tmp.txt"

static struct file *fp;
static char m_buf[] ="from kernel\n";
static char m_buf1[32];

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";
module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

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

static int m_chrdev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device open\n");

    /*
     * filp_open() 是一个异步执行函数。 它将异步打开指定的文件。
     * 如果打开文件后没有做其他事情就结束该功能，很有可能不会做打开的动作
     * filename：要打开/创建文件的名称在内核中打开的文件时需要注意打开的时机，
     *           很容易出现需要打开文件的驱动很早就加载并打开文件，但需要打开的
     *           文件所在设备还不有挂载到文件系统中，而导致打开失败。
     * open_mode：文件的打开方式，其取值与标准库中的open相应参数类似，
     *            可以取O_CREAT,O_RDWR,O_RDONLY等。
     * mode：创建文件时使用，设置创建文件的读写权限，其它情况可以匆略设为0
     * ret：该函数返回strcut file*结构指针，供后继函数操作使用，该返回值用IS_ERR()
     */
    fp =filp_open(FILE_NAME, O_RDWR | O_CREAT, 0644);
    printk("fs file address:0x%p\n", fp);
    /* 判断指针是否有效，不建议通过等于NULL来判断 */
    if (IS_ERR(fp)){
        printk("create file error\n");
        return -1;
    }

    return 0;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device close\n");

    /*
     * 关闭文件
     * 第一个参数是filp_open返回的file结构体指针
     * 第二个参数一般传递NULL值，也有用current->files作为实参
     */
    filp_close(fp, NULL);

    return 0;
}

static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("M_CHRDEV: Device ioctl\n");
    return 0;
}

static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    loff_t pos;
    int ret;

    pos = 0;
    /* 调用vfs_read读取内容，新版本kernel不再导出vfs_read符号，改用kernel_read */
    ret = kernel_read(fp, m_buf1, sizeof(m_buf), &pos);
    printk("func:%s ret=%d read contet=%s\n", __func__, ret, m_buf1);

    return count;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    loff_t pos;
    int ret;

    pos = 0;
    /* 调用vfs_write写内容，新版本kernel不再导出vfs_write符号，改用kernel_write */
    ret = kernel_write(fp, m_buf, sizeof(m_buf), &pos);
    printk("func:%s ret=%d write contet=%s\n", __func__, ret, m_buf);

    return count;
}

static int __init m_chr_init(void)
{    
    int err, idx;
    dev_t devno;

    /* 可以使用 cat /dev/kmsg 实时查看打印 */
    printk(KERN_INFO "module %s init desc:%s\n", __func__, init_desc);

    /* Dynamically apply for device number */
    err = alloc_chrdev_region(&devno, 0, MAX_DEV, "m_chrdev");

    /*
     * 注意这里设备号会作为/dev中设备节点和驱动的一个纽带
     * 设备初始化、注册需要用到设备号
     * 创建设备节点也需要用到设备号
     */
    dev_major = MAJOR(devno);

    /* create sysfs class */
    m_chrdev_class = class_create(THIS_MODULE, "m_chrdev_cls");
    m_chrdev_class->dev_uevent = m_chrdev_uevent;

    /* Create necessary number of the devices */
    for (idx = 0; idx < MAX_DEV; idx++) {
        /* init new device */
        cdev_init(&m_chrdev_data[idx].cdev, &m_chrdev_fops);
        m_chrdev_data[idx].cdev.owner = THIS_MODULE;

        /* add device to the system where "idx" is a Minor number of the new device */
        cdev_add(&m_chrdev_data[idx].cdev, MKDEV(dev_major, idx), 1);

        /* create device node /dev/m_chrdev-x where "x" is "idx", equal to the Minor number */
        device_create(m_chrdev_class, NULL, MKDEV(dev_major, idx), NULL, "m_chrdev_%d", idx);
    }

    return 0;
}

static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    for (idx = 0; idx < MAX_DEV; idx++) {
        device_destroy(m_chrdev_class, MKDEV(dev_major, idx));
    }

    class_unregister(m_chrdev_class);
    class_destroy(m_chrdev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    return;
}

module_init(m_chr_init);
module_exit(m_chr_exit);


MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("base demo");                      /* 描述模块的别名信息 */
