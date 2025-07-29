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
/* workqueue */
#include <linux/kthread.h>
#include <linux/workqueue.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

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

/*
 * global storage for device Major number
 * 多个设备可以对应一个驱动
 */
static int dev_major = 0;

/*
 * sysfs class structure
 * 多个设备对应一个驱动，自然也对应同一个class
 */
static struct class *m_chrdev_class = NULL;

/* array of m_chr_device_data for */
static struct m_chr_device_data m_chrdev_data[MAX_DEV];

static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

/* 定义工作数据结构体 */
struct my_work_data {
    struct kthread_work work;
    int id;
};

/* 定义工作函数 */
static void my_work_func(struct kthread_work *work)
{
    struct my_work_data *data = container_of(work, struct my_work_data, work);
    struct timespec64 ts64;

    printk(KERN_INFO "======> Executing work: %d\n", data->id);

    /* 这里可以执行实际的工作 */
    ktime_get_real_ts64(&ts64);
    ts64.tv_sec = ts64.tv_sec;
    ts64.tv_nsec = ts64.tv_nsec;

    printk(KERN_INFO "======> Current time: %ld.%09ld\n", (long)ts64.tv_sec, (long)ts64.tv_nsec);

    kfree(data); /* 释放工作数据 */
}

static void init_work(struct my_work_data *data, int id)
{
    kthread_init_work(&data->work, my_work_func);
    data->id = id;
}

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    struct kthread_worker *worker;
    struct task_struct *task;
    struct my_work_data *data, *data2;

    printk("M_CHRDEV: Device open\n");

    /*
     * kthread_worker主要是将工作队列和内核线程关联起来，kthread_worker需要用户
     * 提供一个处理函数，这个函数作为一个线程来使用，然后有任务需要处理时，就将
     * 任务入列到woker进行处理，通过使用 kthread_worker 机制，可以将耗时或阻塞的
     * 操作放到后台执行，从而避免阻塞用户进程或影响系统的响应性能。
     *
     * 因此，worker、work、kthread之间的关系如下：
     * 1. worker 与 kthread 绑定，这里的线程函数一般固定为kthread_worker_fn，是
     *    kernel已经定义好的函数，可以理解为worker的调度函数，这里调用 kthread_run
     *    的话会有问题，理解不调用的话应该会有一个默认的类似 kthread_worker_fn的
     *    函数，这可能跟kernel的版本有关系
     * 2. 有任务到来时，需要创建一个work，这个work有自己的函数，用来处理自己的任务,
     *    然后将其入列到worker中，待后续执行处理
     */

    /* create kthread_worker */
    worker = kthread_create_worker(0, "my_kworker");
    if (IS_ERR(worker)) {
        printk(KERN_ERR "Failed to create kthread_worker\n");
        return PTR_ERR(worker);
    }

    /*
     * 绑定 kthread_worker_fn 到worker
     * task = kthread_run(kthread_worker_fn, worker, "my_kthread");
     * if (IS_ERR(task)) {
     *     printk(KERN_ERR "Failed to create kthread\n");
     *     kthread_destroy_worker(worker);
     *     return PTR_ERR(task);
     * }
     */

    /* 分配并初始化工作项 */
    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data) {
        printk(KERN_ERR "Failed to allocate work data\n");
        kthread_stop(task);
        kthread_destroy_worker(worker);
        return -ENOMEM;
    }

    data2 = kmalloc(sizeof(*data2), GFP_KERNEL);
    if (!data2) {
        printk(KERN_ERR "Failed to allocate work data\n");
        kthread_stop(task);
        kthread_destroy_worker(worker);
        return -ENOMEM;
    }

    init_work(data, 1); /* 初始化工作项 */
    init_work(data2, 2); /* 初始化工作项 */

    /* 提交工作项到worker */
    kthread_queue_work(worker, &data->work);
    kthread_queue_work(worker, &data2->work);

    printk(KERN_INFO "Work item submitted to kthread_worker\n");

    /*
     * 等待工作项完成（在实际应用中，你可能不需要等待）
     * 这里为了示例简单，我们使用schedule()来让出CPU
     */
    schedule();
    return 0;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device close\n");

    /*
     * 在这里，我们假设worker和线程已经在其他地方被正确地清理了
     * 在实际应用中，你需要确保在模块卸载时正确地停止线程并销毁worker
     */

    return 0;
}

static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("M_CHRDEV: Device ioctl\n");
    return 0;
}

static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    printk("enter func:%s line:%d\n", __func__, __LINE__);
    return count;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    printk("enter func:%s line:%d\n", __func__, __LINE__);
    return count;
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
