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
/* wait queue */
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/init.h>
#include <linux/sched.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

/* demo 1 */
#define TIMEOUT_INTERVAL (2 * HZ) /* 超时时间为2秒 */
static DECLARE_WAIT_QUEUE_HEAD(wq); /* 声明并初始化等待队列 */
static int condition_timeout = 0; /* 条件变量 */

/* demo 2 */
struct task_struct *thread;
wait_queue_head_t wait_queue;
int condition = 0;

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

static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device open\n");
    condition_timeout = 1;
    condition = 1;
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

/* 线程函数，用于等待条件，带有超时 */
static int waiter_thread_fn(void *data)
{
    long ret;

    printk(KERN_INFO "======> func:%s Waiting for the condition_timeout to be true with a timeout\n", __func__);

    /* 使用 wait_event_interruptible_timeout 来等待条件，并设置超时时间 */
    ret = wait_event_interruptible_timeout(wq, condition_timeout, TIMEOUT_INTERVAL);
    if (ret == 0) {
        /* 超时 */
        printk(KERN_INFO "======> func:%s Wait timed out\n", __func__);
    } else if (ret < 0) {
        /* 被信号中断 */
        printk(KERN_INFO "======> func:%s Wait was interrupted by a signal\n", __func__);
    } else {
        /* 条件满足 */
        printk(KERN_INFO "======> func:%s condition_timeout is true, thread is continuing\n", __func__);
    }

    return 0;
}

/* 线程的工作函数 */
int thread_func(void *data)
{
    while (!kthread_should_stop()) {
        /* 进入可中断的等待状态，直到condition变为非零 */
        if (wait_event_interruptible(wait_queue, condition)) {
            /* 如果等待被信号中断，例如停止线程，打印消息并退出 */
            pr_info("======> func:%s Thread was interrupted by a signal\n", __func__);
            return 0;
        }

        /* 如果条件满足，执行相应的工作 */
        pr_info("======> func:%s Condition is met, doing work...\n", __func__);
        condition = 0; /* 重置条件，以便下次等待 */
    }

    return 0;
}

static int __init m_chr_init(void)
{
    int err, idx;
    dev_t devno;
    struct task_struct *waiter_task;

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

    /* demo 1 wait_event_interruptible_timeout */
    /* 创建一个内核线程来等待条件 */
    waiter_task = kthread_create(waiter_thread_fn, NULL, "waiter_thread");
    if (IS_ERR(waiter_task)) {
        printk(KERN_ERR "======> Failed to create waiter thread\n");
        return PTR_ERR(waiter_task);
    }
    wake_up_process(waiter_task);

    /* demo 2 wait_event_interruptible */
    /* 初始化等待队列 */
    init_waitqueue_head(&wait_queue);
    /* 创建线程 创建并启动线程，相当于 kthread_create + wake_up_process */
    thread = kthread_run(thread_func, NULL, "my_thread");
    if (IS_ERR(thread)) {
        pr_err("Failed to create thread\n");
        return PTR_ERR(thread);
    }

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    /* 停止线程 */
    if (!IS_ERR(thread))
        kthread_stop(thread);

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
