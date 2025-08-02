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
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/gfp.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

#define KMALLOC_SIZE     1024
#define VMALLOC_SIZE     (2 * 1024 * 1024)
#define ORDER            2  // 2^2 = 4 pages = 16KB
#define PAGE_ORDER       2   // 2^2 = 4 pages = 16KB
#define FREE_PAGES_ORDER 1   // 2 pages = 8KB
#define SLAB_OBJ_SIZE    256

static void *kmalloc_mem;
static void *vmalloc_mem;
static void *page_mem;
static void *kzalloc_mem;
static void *vzalloc_mem;
static void *free_pages_mem;
static void *mapped_high_mem = NULL;
static struct page *highmem_page = NULL;
static struct kmem_cache *my_cache = NULL;
static void *slab_obj = NULL;

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

    printk(KERN_INFO "== Kernel memory allocation demo ==\n");

    // 1. kmalloc
    kmalloc_mem = kmalloc(KMALLOC_SIZE, GFP_KERNEL);
    if (!kmalloc_mem) {
        printk(KERN_ERR "kmalloc failed\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "kmalloc: allocated %d bytes at %px\n", KMALLOC_SIZE, kmalloc_mem);
    // kzalloc
    kzalloc_mem = kzalloc(KMALLOC_SIZE, GFP_KERNEL);
    if (!kzalloc_mem)
        goto fail;
    printk(KERN_INFO "kzalloc: %d bytes at %px\n", KMALLOC_SIZE, kzalloc_mem);

    // 2. vmalloc
    vmalloc_mem = vmalloc(VMALLOC_SIZE);
    if (!vmalloc_mem) {
        printk(KERN_ERR "vmalloc failed\n");
        kfree(kmalloc_mem);
        return -ENOMEM;
    }
    printk(KERN_INFO "vmalloc: allocated %d bytes at %px\n", VMALLOC_SIZE, vmalloc_mem);
    // vzalloc
    vzalloc_mem = vzalloc(VMALLOC_SIZE);
    if (!vzalloc_mem)
        goto fail;
    printk(KERN_INFO "vzalloc: %d bytes at %px\n", VMALLOC_SIZE, vzalloc_mem);

    // 3. alloc_pages
    struct page *page = alloc_pages(GFP_KERNEL, ORDER);
    if (!page) {
        printk(KERN_ERR "alloc_pages failed\n");
        vfree(vmalloc_mem);
        kfree(kmalloc_mem);
        return -ENOMEM;
    }
    page_mem = page_address(page);
    printk(KERN_INFO "alloc_pages: allocated %lu bytes at %px (order=%d)\n",
           PAGE_SIZE << ORDER, page_mem, ORDER);
    // __get_free_pages
    free_pages_mem = (void *)__get_free_pages(GFP_KERNEL, FREE_PAGES_ORDER);
    if (!free_pages_mem)
        goto fail;
    printk(KERN_INFO "__get_free_pages: %lu bytes at %px\n", PAGE_SIZE << FREE_PAGES_ORDER, free_pages_mem);

    // kmap/kmap_atomic (仅演示用，非真实高端内存)
#if defined(CONFIG_HIGHMEM)
    highmem_page = alloc_pages(GFP_HIGHMEM, 0);
    if (!highmem_page)
        goto fail;
    mapped_high_mem = kmap(highmem_page);  // 或使用 kmap_atomic
    if (!mapped_high_mem)
        goto fail;
    printk(KERN_INFO "kmap: mapped highmem page to %px\n", mapped_high_mem);
#else
    (void)mapped_high_mem;
    (void)highmem_page;
    printk(KERN_INFO "Highmem not supported on this arch, skipping kmap demo\n");
#endif

    // kmem_cache
    my_cache = kmem_cache_create("my_slab_cache", SLAB_OBJ_SIZE, 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!my_cache)
        goto fail;
    slab_obj = kmem_cache_alloc(my_cache, GFP_KERNEL);
    if (!slab_obj)
        goto fail;
    printk(KERN_INFO "kmem_cache_alloc: object at %px\n", slab_obj);

    return 0;

fail:
    printk(KERN_ERR "Allocation failed, cleaning up.\n");
    return -ENOMEM;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device close\n");

    printk(KERN_INFO "== Cleaning up memory allocations ==\n");

    if (slab_obj && my_cache)
        kmem_cache_free(my_cache, slab_obj);
    if (my_cache)
        kmem_cache_destroy(my_cache);

#if defined(CONFIG_HIGHMEM)
    if (mapped_high_mem && highmem_page)
        kunmap(highmem_page);
    if (highmem_page)
        __free_pages(highmem_page, 0);
#endif

    if (free_pages_mem)
        free_pages((unsigned long)free_pages_mem, FREE_PAGES_ORDER);
    if (page_mem)
        __free_pages(virt_to_page(page_mem), ORDER);

    if (vzalloc_mem)
        vfree(vzalloc_mem);
    if (vmalloc_mem)
        vfree(vmalloc_mem);

    if (kzalloc_mem)
        kfree(kzalloc_mem);
    if (kmalloc_mem)
        kfree(kmalloc_mem);

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
