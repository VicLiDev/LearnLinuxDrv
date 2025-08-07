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

#include <linux/device.h>


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
    struct device *dev;
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

/*
 * 以下是内核打印信息的几种主要方法（宏）：
 * | 宏/函数       | 日志级别          | 说明                                         |
 * | ------------- | ----------------- | -------------------------------------------- |
 * | `printk()`    | 默认 KERN_DEFAULT | 最基本的打印函数                             |
 * | `pr_emerg()`  | KERN_EMERG        | 紧急：系统不可用                             |
 * | `pr_alert()`  | KERN_ALERT        | 警报：必须立刻处理的问题                     |
 * | `pr_crit()`   | KERN_CRIT         | 严重：严重错误                               |
 * | `pr_err()`    | KERN_ERR          | 错误：一般错误信息                           |
 * | `pr_warn()`   | KERN_WARNING      | 警告信息（以前叫 `pr_warning()`）            |
 * | `pr_notice()` | KERN_NOTICE       | 通知：重要但不是错误                         |
 * | `pr_info()`   | KERN_INFO         | 信息：默认日志，普通状态打印                 |
 * | `pr_debug()`  | KERN_DEBUG        | 调试信息（需打开 `DEBUG` 支持才输出）        |
 * | `dev_info()`  | KERN_INFO         | 面向设备打印，用于 device struct（设备驱动） |
 * | `dev_err()`   | KERN_ERR          | 同上，打印设备相关的错误信息                 |
 */
static int m_print_method(struct device *dev)
{
    printk(KERN_EMERG   "printk(KERN_EMERG): System is unusable\n");
    printk(KERN_ALERT   "printk(KERN_ALERT): Action must be taken immediately\n");
    printk(KERN_CRIT    "printk(KERN_CRIT): Critical condition\n");
    printk(KERN_ERR     "printk(KERN_ERR): Error condition\n");
    printk(KERN_WARNING "printk(KERN_WARNING): Warning condition\n");
    printk(KERN_NOTICE  "printk(KERN_NOTICE): Normal but significant condition\n");
    printk(KERN_INFO    "printk(KERN_INFO): Informational\n");
    printk(KERN_DEBUG   "printk(KERN_DEBUG): Debug-level message\n");

    pr_emerg("pr_emerg(): System is unusable\n");
    pr_alert("pr_alert(): Action must be taken immediately\n");
    pr_crit("pr_crit(): Critical condition\n");
    pr_err("pr_err(): Error condition\n");
    pr_warn("pr_warn(): Warning condition\n");
    pr_notice("pr_notice(): Important notice\n");
    pr_info("pr_info(): Informational message\n");
    pr_debug("pr_debug(): Debug message (only shown if DEBUG is enabled)\n");

    dev_info(dev, "Device started at address %p\n", dev);
    dev_err(dev, "Device failed to initialize info!\n");


    /*
     * | 特性     | `snprintf()`                        | `scnprintf()`                      |
     * | -------- | ----------------------------------- | ---------------------------------- |
     * | 返回值   | 原本想要写入的字符数（可能 > size） | 实际写入的字符数（不包括 `\0`）    |
     * | 截断判断 | 需判断返回值是否 ≥ 缓冲区大小       | 返回值就是写入长度，适合串联写入   |
     * | 推荐场景 | 一次性格式化输出                    | 多次追加、拼接格式化输出           |
     *
     * | 场景                   | 用哪个？    | 原因               |
     * | ---------------------- | ----------- | ------------------ |
     * | 单次格式化输出         | `snprintf`  | 简洁、语义清晰     |
     * | 需要多次拼接格式化内容 | `scnprintf` | 能精确追踪写入长度 |
     * | 关心是否被截断         | `snprintf`  | 返回预期长度       |
     * | 追求高安全性和可控输出 | `scnprintf` | 不会越界           |
     */
    char buf[128];
    int len1, len2, len3;

    // 1. 使用 snprintf（一次性写入）
    len1 = snprintf(buf, sizeof(buf), "Hello, %s! The value is %d.\n", "world", 42);
    pr_info("snprintf: wrote %d chars: %s", len1, buf);

    // 2. 使用 scnprintf 逐步拼接
    // /dev/kmsg 是一个结构化的内核日志接口，它对内容做了特殊处理，
    // /dev/kmsg 的每一条消息是一整行，因此这里的换行效果看不到，
    // 但是在 dmesg 是可以看到换行效果的
    len2 = scnprintf(buf, sizeof(buf), "Step 1: %d\n", 100);         // 第一次写入
    len2 += scnprintf(buf + len2, sizeof(buf) - len2, "Step 2: %s\n", "ok");  // 追加
    len2 += scnprintf(buf + len2, sizeof(buf) - len2, "Step 3: %.2d\n", 314); // 追加
    pr_info("scnprintf total %d chars:\n%s", len2, buf);

    // 3. 如果 buffer 太小的情况（演示截断）
    char small_buf[10];
    len3 = snprintf(small_buf, sizeof(small_buf), "123456789ABCDEFG");
    pr_info("snprintf (small_buf): len=%d content='%s'", len3, small_buf);

    return 0;
}

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    struct cdev *cdev = inode->i_cdev;
    struct m_chr_device_data *my_dev = container_of(cdev, struct m_chr_device_data, cdev);
    struct device *dev = my_dev->dev;

    printk("M_CHRDEV: Device open\n");
    m_print_method(dev);
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
        m_chrdev_data[idx].dev = device_create(m_chrdev_class, NULL, MKDEV(dev_major, idx), NULL, "m_chrdev_%d", idx);
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
