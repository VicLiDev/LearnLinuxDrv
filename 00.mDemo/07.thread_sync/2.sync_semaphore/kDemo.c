/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/

/*
 *
 * 用例介绍:
 *  计数信号量（semaphore）：限制多个线程访问共享资源的数量。
 *  二值信号量（binary semaphore）：只允许一个线程访问资源。
 *  互斥量（mutex）：保护关键区，保证同一时刻只有一个线程访问资源。
 *  读写信号量（rw_semaphore）：允许多个读者并发访问，但写者必须独占。
 *  RCU（Read-Copy Update）：优化读多写少的场景，读者不会阻塞。
 *
 * 代码解析：
 *  计数信号量：sema_init(&sem, 2); 允许最多 2 个线程同时访问资源。
 *  二值信号量：类似于互斥量，只允许 1 个线程访问。
 *  互斥量：使用 mutex_lock() 和 mutex_unlock() 确保临界区互斥访问。
 *  读写信号量：
 *   down_read() 允许多个读者访问。
 *   down_write() 让写者独占资源。
 *  RCU 机制：
 *   读者使用 rcu_read_lock() 读取数据。
 *   写者创建新数据，并用 rcu_assign_pointer() 替换旧数据。
 *   synchronize_rcu() 确保旧数据不会被读者使用后才释放。
 */


#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
/* file opt */
#include <linux/uaccess.h>
#include <linux/fs.h>

#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rcupdate.h>


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

// 计数信号量
static struct semaphore sem;
// 二值信号量
static struct semaphore bin_sem;
// 互斥量
static struct mutex my_mutex;
// 读写信号量
static struct rw_semaphore rwsem;
// RCU 保护的数据结构
struct my_data {
    int value;
    struct rcu_head rcu;
};
static struct my_data *rcu_ptr = NULL;



// 计数信号量线程
static int semaphore_thread(void *arg)
{
    while (!kthread_should_stop()) {
        down(&sem);
        pr_info("Semaphore Thread: Accessing resource\n");
        msleep(500);
        up(&sem);
        msleep(1000);
    }
    return 0;
}

// 二值信号量线程
static int binary_semaphore_thread(void *arg)
{
    while (!kthread_should_stop()) {
        down(&bin_sem);
        pr_info("Binary Semaphore Thread: Exclusive access\n");
        msleep(500);
        up(&bin_sem);
        msleep(1000);
    }
    return 0;
}

// 互斥量线程
static int mutex_thread(void *arg)
{
    while (!kthread_should_stop()) {
        mutex_lock(&my_mutex);
        pr_info("Mutex Thread: Critical Section\n");
        msleep(500);
        mutex_unlock(&my_mutex);
        msleep(1000);
    }
    return 0;
}

// 读者线程
static int reader_thread(void *arg)
{
    while (!kthread_should_stop()) {
        down_read(&rwsem);
        pr_info("Reader Thread: Reading data\n");
        msleep(300);
        up_read(&rwsem);
        msleep(800);
    }
    return 0;
}

// 写者线程
static int writer_thread(void *arg)
{
    while (!kthread_should_stop()) {
        down_write(&rwsem);
        pr_info("Writer Thread: Writing data\n");
        msleep(500);
        up_write(&rwsem);
        msleep(1200);
    }
    return 0;
}

// RCU 读者线程
static int rcu_reader_thread(void *arg)
{
    while (!kthread_should_stop()) {
        rcu_read_lock();
        struct my_data *data = rcu_dereference(rcu_ptr);
        if (data)
            pr_info("RCU Reader Thread: Reading value %d\n", data->value);
        rcu_read_unlock();
        msleep(700);
    }
    return 0;
}

// RCU 写者线程
static int rcu_writer_thread(void *arg)
{
    while (!kthread_should_stop()) {
        struct my_data *new_data = kmalloc(sizeof(struct my_data), GFP_KERNEL);
        if (!new_data)
            continue;
        new_data->value = get_random_u32() % 100;
        rcu_assign_pointer(rcu_ptr, new_data);
        synchronize_rcu();
        pr_info("RCU Writer Thread: Updated value to %d\n", new_data->value);
        msleep(1500);
    }
    return 0;
}

// 线程指针
static struct task_struct *sem_thread, *bin_sem_thread, *mutex_thread_task;
static struct task_struct *reader_thread_task, *writer_thread_task;
static struct task_struct *rcu_reader_thread_task, *rcu_writer_thread_task;

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

    pr_info("Initializing Synchronization Module\n");

    sema_init(&sem, 2);         // 计数信号量，最多 2 个线程访问
    sema_init(&bin_sem, 1);     // 二值信号量，最多 1 个线程访问
    mutex_init(&my_mutex);      // 初始化互斥量
    init_rwsem(&rwsem);         // 初始化读写信号量
    rcu_ptr = kmalloc(sizeof(struct my_data), GFP_KERNEL);
    if (rcu_ptr)
        rcu_ptr->value = 0;

    sem_thread = kthread_run(semaphore_thread, NULL, "sem_thread");
    bin_sem_thread = kthread_run(binary_semaphore_thread, NULL, "bin_sem_thread");
    mutex_thread_task = kthread_run(mutex_thread, NULL, "mutex_thread");
    reader_thread_task = kthread_run(reader_thread, NULL, "reader_thread");
    writer_thread_task = kthread_run(writer_thread, NULL, "writer_thread");
    rcu_reader_thread_task = kthread_run(rcu_reader_thread, NULL, "rcu_reader_thread");
    rcu_writer_thread_task = kthread_run(rcu_writer_thread, NULL, "rcu_writer_thread");

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    pr_info("Exiting Synchronization Module\n");

    if (sem_thread) kthread_stop(sem_thread);
    if (bin_sem_thread) kthread_stop(bin_sem_thread);
    if (mutex_thread_task) kthread_stop(mutex_thread_task);
    if (reader_thread_task) kthread_stop(reader_thread_task);
    if (writer_thread_task) kthread_stop(writer_thread_task);
    if (rcu_reader_thread_task) kthread_stop(rcu_reader_thread_task);
    if (rcu_writer_thread_task) kthread_stop(rcu_writer_thread_task);

    synchronize_rcu();
    kfree(rcu_ptr);

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
