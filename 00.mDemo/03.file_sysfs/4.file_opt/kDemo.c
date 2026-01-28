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

#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/version.h>


#define DEMO_FILE_PATH   "/tmp/demo_kernel_file.txt"
#define DEMO_FILE_DATA   "Hello from kernel space!\n"
#define READ_BUF_SIZE    128


static struct file *file_test = NULL;
static struct file *log_file = NULL;

/* 打开文件（若不存在则创建） */
static struct file *open_file(const char *path, int flags, umode_t mode)
{
    struct file *fp;

    fp = filp_open(path, flags, mode);
    if (IS_ERR(fp)) {
        pr_err("Failed to open file: %s\n", path);
        return NULL;
    }
    return fp;
}

/* 写入数据 */
static ssize_t write_to_file(struct file *fp, const char *data, size_t len)
{
    loff_t pos = 0;
    ssize_t ret;

    ret = kernel_write(fp, data, len, &pos);
    if (ret < 0)
        pr_err("Write failed: %zd\n", ret);
    else
        pr_info("Wrote %zd bytes to file\n", ret);
    return ret;
}

/* 从文件读取内容 */
static ssize_t read_from_file(struct file *fp, char *buf, size_t len)
{
    loff_t pos = 0;
    ssize_t ret;

    ret = kernel_read(fp, buf, len - 1, &pos);
    if (ret < 0) {
        pr_err("Read failed: %zd\n", ret);
        return ret;
    }

    buf[ret] = '\0';
    pr_info("Read from file: %s\n", buf);
    return ret;
}

/* 获取文件大小 */
static loff_t get_file_size(struct file *fp)
{
    loff_t size;

    size = vfs_llseek(fp, 0, SEEK_END);
    vfs_llseek(fp, 0, SEEK_SET);  // 回到开头
    pr_info("File size: %lld bytes\n", size);
    return size;
}

/* 删除文件 */
static int delete_file(const char *path)
{
    struct path p;
    struct dentry *dentry;
    struct inode *delegated_inode = NULL;
    int err;

    err = kern_path(path, LOOKUP_FOLLOW, &p);
    if (err) {
        pr_err("Failed to find file path: %s\n", path);
        return err;
    }

    dentry = p.dentry;

    inode_lock(dentry->d_parent->d_inode);
    err = vfs_unlink(mnt_idmap(p.mnt), dentry->d_parent->d_inode, dentry, &delegated_inode);
    inode_unlock(dentry->d_parent->d_inode);

    path_put(&p);

    if (err)
        pr_err("Failed to delete file: %d\n", err);
    else
        pr_info("File deleted: %s\n", path);

    return err;
}

/* 是否为目录 */
static bool is_directory(const char *path_str)
{
    struct path path;
    struct kstat stat;
    int err;

    err = kern_path(path_str, LOOKUP_FOLLOW, &path);
    if (err) {
        pr_err("kern_path failed for: %s\n", path_str);
        return false;
    }

    err = vfs_getattr(&path, &stat, STATX_BASIC_STATS, AT_STATX_SYNC_AS_STAT);
    path_put(&path);
    if (err) {
        pr_err("vfs_getattr failed for: %s\n", path_str);
        return false;
    }

    return S_ISDIR(stat.mode);
}


static int open_log_file(const char *filepath)
{
    log_file = filp_open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(log_file)) {
        pr_err("Failed to open log file: %s\n", filepath);
        log_file = NULL;
        return PTR_ERR(log_file);
    }
    return 0;
}

static void close_log_file(void)
{
    if (log_file) {
        filp_close(log_file, NULL);
        log_file = NULL;
    }
}

// 写数据到文件，返回写入字节数或负值错误码
static ssize_t write_log(const char *buf, size_t len)
{
    ssize_t ret;
    loff_t pos;

    if (!log_file)
        return -EINVAL;

    // 从文件末尾写入（附加模式）
    pos = log_file->f_pos;
    ret = kernel_write(log_file, buf, len, &pos);
    if (ret >= 0)
        log_file->f_pos = pos;

    return ret;
}

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

    char *read_buf;

    /* 打开或创建文件 */
    file_test = open_file(DEMO_FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (!file_test)
        return -ENOENT;

    /* 写内容 */
    write_to_file(file_test, DEMO_FILE_DATA, strlen(DEMO_FILE_DATA));

    /* 获取大小 */
    get_file_size(file_test);

    /* 读内容 */
    read_buf = kzalloc(READ_BUF_SIZE, GFP_KERNEL);
    if (read_buf)
        read_from_file(file_test, read_buf, READ_BUF_SIZE);
    kfree(read_buf);

    /* 是否是目录 */
    pr_info("Is /tmp a directory? %s\n", is_directory("/tmp") ? "Yes" : "No");

    /* 同步 */
    /* fflush() 是用户态刷新缓冲区，写入内核
     * vfs_fsync() 是内核态刷写缓存，写入磁盘
     */
    vfs_fsync(file_test, 0);

    /* 关闭文件 */
    filp_close(file_test, NULL);
    file_test = NULL;

    /* 删除文件 */
    delete_file(DEMO_FILE_PATH);


    /* 一个简单的 log 写出 demo */
    {
        // const char *path = "/var/log/m_kernel_log.txt";
        const char *path = "/tmp/m_kernel_log.txt";
        char *test_msg = "Hello from kernel log demo!\n";

        if (open_log_file(path) < 0)
            return -EIO;

        write_log(test_msg, strlen(test_msg));
        close_log_file();
    }

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
