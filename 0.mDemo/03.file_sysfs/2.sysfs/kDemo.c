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
/* kmalloc */
#include <linux/slab.h>

#include <linux/sysfs.h>

#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";
module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

struct kobject* m_kobj;
struct kobj_attribute m_attribute;
char *m_internal_buffer;

char *demo1_export_test = "this is export symbol from module kDemo_1_base";
EXPORT_SYMBOL(demo1_export_test);

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
    printk("Reading device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    return count;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    printk("Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    return count;
}

ssize_t m_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int len;

    printk("func:%s line:%d context:%s\n", __func__, __LINE__, m_internal_buffer);

    len = sprintf(buf, "%s\n", m_internal_buffer);
    if (len <= 0)
        printk("func:%s line:%d show error\n", __func__, __LINE__);

    return len;
}

ssize_t m_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int len;

    printk("func:%s line:%d context:%s\n", __func__, __LINE__, buf);

    len = sprintf(m_internal_buffer, "%s\n", buf);
    if (len <= 0)
        printk("func:%s line:%d show error\n", __func__, __LINE__);

    return count;
}

static int __init m_chr_init(void)
{    
    int err, idx;
    dev_t devno;

    /* 可以使用 cat /dev/kmsg 实时查看打印 */
    printk(KERN_INFO "module %s init desc:%s\n", __func__, init_desc);

    printk(KERN_INFO "git version:%s\n", DEMO_GIT_VISION);

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

    /* sysfs中每个文件节点都对应一个kobject */
    m_kobj = kobject_create_and_add("m_demo_kobj", NULL);

    /* m_attribute = __ATTR(m_demo_attr, 0766, m_show, m_store); */
    m_attribute.attr.name = "m_demo_attr";
    m_attribute.attr.mode = 0766;
    m_attribute.show = m_show;
    m_attribute.store = m_store;
    m_internal_buffer = (char *)kmalloc(100, GFP_KERNEL);
    err = sysfs_create_file(m_kobj, &m_attribute.attr);
    if (err)
        printk(KERN_INFO "sysfs create file failed ret:%d\n", err);

    return 0;
}

static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    for (idx = 0; idx < MAX_DEV; idx++) {
        device_destroy(m_chrdev_class, MKDEV(dev_major, idx));
    }

    class_destroy(m_chrdev_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    /* sysfs */
    sysfs_remove_file(m_kobj, &m_attribute.attr);
    kobject_del(m_kobj);

    return;
}

module_init(m_chr_init);
module_exit(m_chr_exit);


MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("base demo");                      /* 描述模块的别名信息 */
MODULE_VERSION(DEMO_GIT_VISION);
