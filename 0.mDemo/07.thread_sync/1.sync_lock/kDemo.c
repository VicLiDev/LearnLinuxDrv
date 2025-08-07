/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/


#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
/* file opt */
#include <linux/uaccess.h>
#include <linux/fs.h>

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

/* 定义一个互斥锁 */
static DEFINE_MUTEX(mutex_s);
/* 声明一个互斥锁 */
static struct mutex mutex_d;

/* 声明一个自旋锁 */
static spinlock_t spinlock_d;

/* 定义一个读写锁 */
static DEFINE_RWLOCK(rwlock_s);
/* 声明一个读写锁 */
static rwlock_t rwlock_d;

/* 共享资源 */
static int shared_data = 0;

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

/* 一个访问共享资源的函数 */
static void access_shared_resource(void)
{
    unsigned long flags;
    (void) flags;

    /* 获取互斥锁 */
    mutex_lock(&mutex_s);
    mutex_lock(&mutex_d);
    /* 对共享资源进行操作 */
    shared_data++;
    msleep(100); /* 模拟耗时操作 */
    shared_data--;
    /* 打印当前计数器的值 */
    pr_info("======> shared_data value: %d\n", shared_data);
    /* 释放互斥锁 */
    mutex_unlock(&mutex_d);
    mutex_unlock(&mutex_s);
    /* 获取自旋锁 */
    /* 考虑这种情况：
     *   线程 A 正在执行并加锁（spin_lock()）
     *   中断突然发生（比如 timer IRQ），也试图获取同一个锁
     *   中断无法获取锁 → 自旋等待
     *   但线程 A 无法继续运行（被中断了）→ 死锁发生
     */
    // 在获取自旋锁之前，关闭本地 CPU 的中断（local_irq_disable）。
    // 并且保存当前中断状态到 flags关中断并保存中断状态
    // spin_lock_irqsave(&spinlock_d, flags);
    // 只是获取锁，不影响中断状态
    spin_lock(&spinlock_d);
    /* 对共享资源进行操作 */
    shared_data++;
    udelay(1000); /* 模拟耗时操作，自选锁不能时间太长，因此这里单独处理耗时 */
    shared_data--;
    /* 打印当前计数器的值 */
    pr_info("======> shared_data value: %d\n", shared_data);
    /* 释放自旋锁 */
    spin_unlock(&spinlock_d);
    // 先解锁 spinlock，然后再恢复中断状态。
    // spin_unlock_irqrestore(&spinlock_d, flags);

    /* 获取读写锁 */
    write_lock(&rwlock_s);
    write_lock(&rwlock_d);
    /* 执行写临界区代码 */
    shared_data++;
    shared_data--;
    /* 释放读写锁 */
    write_unlock(&rwlock_d);
    write_unlock(&rwlock_s);
}

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

    /* 初始化互斥锁 */
    mutex_init(&mutex_d);
    /* 初始化自旋锁 */
    spin_lock_init(&spinlock_d);
    /* 初始化读写锁 */
    rwlock_init(&rwlock_d);

    /* 在这里调用访问共享资源的函数 */
    access_shared_resource();

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
