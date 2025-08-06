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

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

#define PROC_DIRNAME "proc_demo"
static struct proc_dir_entry *demo_dir;

struct proc_dir_entry *generic_entry, *singel_entry, *sig_data_entry;

/*
 * proc_ops 结构体核心函数
 * struct proc_ops {
 *     int (*proc_open)(struct inode *, struct file *);
 *     ssize_t (*proc_read)(struct file *, char __user *, size_t, loff_t *);
 *     ssize_t (*proc_write)(struct file *, const char __user *, size_t, loff_t *);
 *     int (*proc_release)(struct inode *, struct file *);
 *     // 其它可选字段
 * };
 * open：打开 /proc 文件
 * read：读 /proc 文件内容
 * write：写 /proc 文件内容
 * release：关闭 /proc 文件
 *
 * proc_read 中如何保证对 buf 的操作不越界？
 * proc_read 函数中有几个关键点避免越界：
 * 1. 检测当前读偏移 *ppos：
 *    如果 *ppos 已经超过或等于数据长度，返回 0，表示 EOF，告诉用户空间没数据了。
 * 2. 限制读取长度 count：
 *    如果请求读取的 count 超过了剩余数据长度（len - *ppos），就缩小读取长度为剩余长度。
 * 3. 分段复制：
 *    只复制合法范围内的数据到用户空间，避免越界访问。
 * 举个简单示范代码：
 *   #define PROC_NAME "m_proc_demo"
 *   #define BUF_SIZE 128
 #   static char proc_data[BUF_SIZE] = "Hello from kernel proc\n";
 *   size_t len = strlen(proc_data);
 *   if (*ppos >= len)
 *       return 0;
 *   if (count > len - *ppos)
 *       count = len - *ppos;
 *   if (copy_to_user(buf, proc_data + *ppos, count))
 *       return -EFAULT;
 *   *ppos += count;
 *   return count;
 * 这里的 *ppos 是文件读写的偏移，内核帮你管理，不用担心用户空间访问越界，只要
 * 保证 count 合理即可。
 *
 * | 接口名称                  | 功能简介                                                  |
 * | ------------------------- | --------------------------------------------------------- |
 * | `proc_create`             | 最通用的创建方法，支持读写                                |
 * | `proc_create_single`      | 简化版本，适用于只读数据输出，不可写入                    |
 * | `proc_create_single_data` | 类似 `proc_create_single`，只读不写，但可携带私有数据指针 |
 *
 * 如果想让每个条目传递不同的数据结构，推荐使用 proc_create_single_data。
 */

/* ---- proc_create 示例 ---- */
#define BUF_SIZE 128
static char proc_buf[BUF_SIZE] = "Hello from kernel proc generic\n";
static DEFINE_MUTEX(buf_lock);

static ssize_t generic_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
// 以下两种写法均可
#if 0
    size_t len = strlen(proc_buf);

    if (*ppos >= len)  // 读完了，返回0表示EOF
        return 0;

    if (count > len - *ppos)
        count = len - *ppos;

    if (copy_to_user(buf, proc_buf + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
#else

    ssize_t ret;

    mutex_lock(&buf_lock);
    ret = simple_read_from_buffer(buf, count, ppos, proc_buf, strnlen(proc_buf, BUF_SIZE));
    mutex_unlock(&buf_lock);
    return ret;
#endif
}

static ssize_t generic_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (count > BUF_SIZE - 1)
        return -EINVAL;

    if (copy_from_user(proc_buf, buf, count))
        return -EFAULT;

    proc_buf[count] = '\0';
    return count;
}

static const struct proc_ops generic_proc_ops = {
    .proc_read  = generic_read,
    .proc_write = generic_write,
};

/* ---- proc_create_single 示例 ---- */
static int single_show(struct seq_file *m, void *v)
{
    seq_puts(m, "Hello from proc_create_single!\n");
    return 0;
}

/* ---- proc_create_single_data 示例 ---- */
static int data_show(struct seq_file *m, void *v)
{
    const char *msg = m->private;
    seq_printf(m, "Message: %s\n", msg);
    return 0;
}


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


	// 创建目录
    demo_dir = proc_mkdir(PROC_DIRNAME, NULL);
    if (!demo_dir)
        return -ENOMEM;

    /* 返回值：这三个函数都返回一个 struct proc_dir_entry *，可用于后续移除或检查
     * 是否创建成功。*/

    /*
     * 普通可读写 entry
     *
     * struct proc_dir_entry *proc_create(const char *name, umode_t mode,
     *      struct proc_dir_entry *parent, const struct proc_ops *proc_ops);
     * 其中：
     *   `name`：在 `/proc` 中的文件名
     *   `mode`：文件权限（如 `0444`, `0666` 等）
     *   `parent`：父目录（通常用 `NULL` 表示 `/proc` 根目录）
     *   `proc_ops`：指定文件操作，如 `.proc_read`、`.proc_write`、`.proc_open` 等
     */
    generic_entry = proc_create("entry_generic", 0666, demo_dir, &generic_proc_ops);
    if (!generic_entry) {
        pr_err("Failed to create /proc/%s/%s\n", PROC_DIRNAME, "entry_generic");
        return -ENOMEM;
    }

    /*
     * 只读的 single entry，直接传 show 函数
     *
     * struct proc_dir_entry *proc_create_single(const char *name, umode_t mode,
     *      struct proc_dir_entry *parent, int (*show)(struct seq_file *, void *));
     *
     * 自动设置 `.proc_open`, `.proc_read`, `.proc_release` 等函数（基于 `seq_file`）
     * 只能读，不支持写
     * 不支持传私有数据
     */
    singel_entry = proc_create_single("entry_single", 0444, demo_dir, single_show);
    if (!singel_entry) {
        pr_err("Failed to create /proc/%s/%s\n", PROC_DIRNAME, "singel_entry");
        return -ENOMEM;
    }

    /*
     * 带私有数据的 single entry，传 data_show 和自定义数据
     *
     * struct proc_dir_entry *proc_create_single_data(const char *name, umode_t mode,
     *      struct proc_dir_entry *parent, int (*show)(struct seq_file *, void *), void *data);
     *
     * 功能同 `proc_create_single`
     * 支持传入一个私有 `data`，可通过 `m->private` 在 `show()` 中访问
     * 不支持写入
     */
    const char *msg = "custom data from kernel";
    sig_data_entry = proc_create_single_data("entry_single_data", 0444, demo_dir, data_show, (void *)msg);
    if (!sig_data_entry) {
        pr_err("Failed to create /proc/%s/%s\n", PROC_DIRNAME, "sig_data_entry");
        return -ENOMEM;
    }

    pr_info("proc_demo module loaded.\n");

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    proc_remove(generic_entry);
    proc_remove(singel_entry);
    proc_remove(sig_data_entry);
    // 也可以省略单独移除文件，直接删除文件夹
    remove_proc_subtree(PROC_DIRNAME, NULL);
    printk(KERN_INFO "proc_demo module unloaded.\n");

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
