/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/


#include <linux/init.h>         // __init   __exit
#include <linux/module.h>       // module_init  module_exit
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
//-- file opt
#include <linux/uaccess.h>
#include <linux/fs.h>
//-- fifo
#include <linux/kfifo.h>


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

// initialize file_operations
static const struct file_operations m_chrdev_fops = {
    .owner      = THIS_MODULE,
    .open       = m_chrdev_open,
    .release    = m_chrdev_release,
    .unlocked_ioctl = m_chrdev_ioctl,
    .read       = m_chrdev_read,
    .write       = m_chrdev_write
};

// device data holder, this structure may be extended to hold additional data
struct m_chr_device_data {
    struct cdev cdev;
};

// global storage for device Major number
// 多个设备可以对应一个驱动
static int dev_major = 0;

// sysfs class structure
// 多个设备对应一个驱动，自然也对应同一个class
static struct class *m_chrdev_class = NULL;

// array of m_chr_device_data for
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

// 定义 kfifo 的大小
#define FIFO_SIZE 128

// 声明 kfifo 变量
struct kfifo my_fifo;

// 缓冲区，用于存储 kfifo 中的数据
char buffer[FIFO_SIZE];

static int __init m_chr_init(void)
{
    int err, idx;
    dev_t devno;

    // 可以使用 cat /dev/kmsg 实时查看打印
    printk(KERN_INFO "module %s init desc:%s\n", __func__, init_desc);

    printk(KERN_INFO "git version:%s\n", DEMO_GIT_VERSION);

    // Dynamically apply for device number
    err = alloc_chrdev_region(&devno, 0, MAX_DEV, "m_chrdev");

    // 注意这里设备号会作为/dev中设备节点和驱动的一个纽带
    // 设备初始化、注册需要用到设备号
    // 创建设备节点也需要用到设备号
    dev_major = MAJOR(devno);

    // create sysfs class
    // 创建设备节点的时候也用到了class，因此在/sys/class/m_chrdev_cls下可以看到
    // 链接到设备的软链接
    m_chrdev_class = class_create("m_chrdev_cls");
    m_chrdev_class->dev_uevent = m_chrdev_uevent;

    // Create necessary number of the devices
    for (idx = 0; idx < MAX_DEV; idx++) {
        // init new device
        cdev_init(&m_chrdev_data[idx].cdev, &m_chrdev_fops);
        m_chrdev_data[idx].cdev.owner = THIS_MODULE;

        // add device to the system where "idx" is a Minor number of the new device
        cdev_add(&m_chrdev_data[idx].cdev, MKDEV(dev_major, idx), 1);

        // create device node /dev/m_chrdev_x where "x" is "idx", equal to the Minor number
        device_create(m_chrdev_class, NULL, MKDEV(dev_major, idx), NULL, "m_chrdev_%d", idx);
    }

    // 初始化 kfifo，传入缓冲区、大小和标志
    if (kfifo_init(&my_fifo, buffer, sizeof(buffer)) != 0) {
        printk(KERN_ERR "Failed to initialize kfifo\n");
        return -1;
    }

    // 向 kfifo 写入数据
    char data_to_write[] = "Hello, kfifo!";
    size_t len = strlen(data_to_write);
    if (kfifo_in(&my_fifo, data_to_write, len) != len) {
        printk(KERN_ERR "Failed to write to kfifo\n");
        return -1;
    }

    // 从 kfifo 读取数据
    char read_buffer[len + 1]; // 确保有足够的空间存储数据（包括空字符）
    size_t read_len = kfifo_out(&my_fifo, read_buffer, len);
    if (read_len == len) {
        read_buffer[read_len] = '\0'; // 添加空字符以形成字符串
        printk(KERN_INFO "======> Read from kfifo: %s\n", read_buffer);
    } else {
        printk(KERN_ERR "Failed to read from kfifo\n");
    }

    return 0;
}

// 模块卸载函数
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    // 在这个简单的例子中，我们不需要在模块卸载时做特别的清理，
    // 因为 kfifo 使用的缓冲区是静态分配的，并且在模块卸载时自动释放。

    for (idx = 0; idx < MAX_DEV; idx++) {
        device_destroy(m_chrdev_class, MKDEV(dev_major, idx));
    }

    class_destroy(m_chrdev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    return;
}

module_init(m_chr_init);
module_exit(m_chr_exit);


// 内核模块领域可接受的LICENSE包括 “GPL”、“GPL v2”、“GPL and additional rights”、
// “Dual BSD/GPL”、“Dual MPL/GPL”和“Proprietary”（关于模块是否可采用非GPL许可权，
// 如 Proprietary，这个在学术界是有争议的）
// 大多数情况下内核模块应该遵守GPL兼容许可权。Linux内核模块最常见的是使用GPL v2
MODULE_LICENSE("GPL v2");                       // 描述模块的许可证
// MODULE_xxx这种宏作用是用来添加模块描述信息（可选）
MODULE_AUTHOR("Lhj <872648180@qq.com>");        // 描述模块的作者
MODULE_DESCRIPTION("base demo for learning");   // 描述模块的介绍信息
MODULE_ALIAS("base demo");                      // 描述模块的别名信息
// 设置内核模块版本，可以通过modinfo kDemo.ko查看
// 如果不使用MODULE_VERSION设置模块信息，modinfo会看不到 version 信息
MODULE_VERSION(DEMO_GIT_VERSION);
