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

#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

/*
 * 总体原理
 *   虽然没手写 proc_read()，但实际上用了 seq_file 接口，它内部封装了读取逻辑，
 *   简化了开发 /proc 文件的过程。
 *
 * 提供了：
 *   .proc_read = seq_read,
 * 也就是说：
 *   用户空间执行 cat /proc/my_seq
 *   内核调用 proc_read() → 实际对应到 seq_read()（标准封装）
 *   seq_read() 内部使用你定义的 seq_operations 来获取数据
 * 整体调用流程图
 *   用户空间：cat /proc/my_seq
 *         ↓
 *   VFS：调用 .read 方法 → 实际是 proc_read → seq_read
 *         ↓
 *   seq_read():
 *       → seq->op->start()    // 你提供的 my_seq_start
 *       → seq->op->show()     // 你提供的 my_seq_show
 *       → seq->op->next()     // my_seq_next
 *       → seq->op->stop()     // my_seq_stop
 * 也就是说：
 *   没实现 read()，但注册了 seq_read，它实际完成了所有分页和偏移管理
 *   seq_file 的设计是为了简化大量输出的 /proc 实现（比如 /proc/meminfo）
 *
 * 细节解释：seq_file 是怎么工作的？
 * seq_read() 会在每次读取时循环调用 seq_operations 的函数，直到读取完整个内容：
 * | 函数名	   | 你的实现          | 作用                                   |
 * | --------- | ----------------- | -------------------------------------- |
 * | start()   | my_seq_start()	   | 初始化读取，返回数据项的指针           |
 * | show()	   | my_seq_show()	   | 输出一项内容到 seq_file（类似 printf） |
 * | next()	   | my_seq_next()	   | 移动到下一项                           |
 * | stop()	   | my_seq_stop()	   | 清理收尾（可不实现）                   |
 * 这样就能像写一个“数据项流”一样来输出内容，cat 可以一次读完或多次分页读，内核
 * 自动处理。
 *
 * 那如果要输出多条数据怎么办？
 * 只需要让 start() 返回不同的记录指针（比如链表的下一个节点），并在 next() 中
 * 前进，就可以遍历输出多个记录！
 *
 * 类比场景：
 * 1. 先打开本子（start）：准备从第1条数据开始读
 * 2. 读出当前记录（show）：告诉他第1条记录，即实际的数据读操作
 * 3. 翻页到下一条（next）：准备第2条数据
 * 4. 重复 show/next... 直到读完
 * 5. 关本子（stop）：清理状态
 */

#define PROC_NAME "my_seq"
#define BUFFER_SIZE 1024

static char my_buffer[BUFFER_SIZE];
static size_t my_buf_len;
static DEFINE_MUTEX(my_buffer_lock);

// seq_file 相关函数

static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= 1)  // 只支持一条记录，pos大于等于1则结束
        return NULL;
    return my_buffer;
}

static void my_seq_stop(struct seq_file *s, void *v)
{
    // 不需要释放资源
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    return NULL;
}

static int my_seq_show(struct seq_file *s, void *v)
{
    mutex_lock(&my_buffer_lock);
    seq_write(s, my_buffer, my_buf_len);
    mutex_unlock(&my_buffer_lock);
    return 0;
}

static const struct seq_operations my_seq_ops = {
    .start = my_seq_start,
    .next  = my_seq_next,
    .stop  = my_seq_stop,
    .show  = my_seq_show,
};

// open 调用 seq_open 并传递 seq_operations
static int my_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &my_seq_ops);
}

// 写操作，把用户数据写到 my_buffer
static ssize_t my_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (count > BUFFER_SIZE - 1)
        return -EINVAL;

    mutex_lock(&my_buffer_lock);
    if (copy_from_user(my_buffer, buf, count)) {
        mutex_unlock(&my_buffer_lock);
        return -EFAULT;
    }
    my_buffer[count] = '\0';
    my_buf_len = count;
    mutex_unlock(&my_buffer_lock);

    ret = count;
    return ret;
}

static const struct proc_ops my_proc_ops = {
    .proc_open    = my_proc_open,
    .proc_read    = seq_read,
    .proc_write   = my_proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = seq_release,
};

static struct proc_dir_entry *my_proc_entry;

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


    // 初始化缓冲区
    mutex_lock(&my_buffer_lock);
    strcpy(my_buffer, "Initial kernel buffer content\n");
    my_buf_len = strlen(my_buffer);
    mutex_unlock(&my_buffer_lock);

    my_proc_entry = proc_create(PROC_NAME, 0666, NULL, &my_proc_ops);
    if (!my_proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }
    pr_info("/proc/%s created\n", PROC_NAME);

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    proc_remove(my_proc_entry);
    pr_info("/proc/%s removed\n", PROC_NAME);

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
