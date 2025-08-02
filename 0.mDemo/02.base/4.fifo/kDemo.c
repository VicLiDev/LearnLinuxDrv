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
/* fifo */
#include <linux/kfifo.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

#define FIFO_SIZE   1024   // 必须是2的幂

// select one
#define USE_INIT_DECLARE
// #define USE_INIT_DEFINE
// #define USE_INIT_DYNAMIC

/*
 * 在 kernel 中 DECLARE 和 DEFINE 的关系为：
 * DECLARE 只是声明了变量，但没有给初始值
 * DEFINE 使用 DECLARE 声明了变量，并同时进行了初始化
 * 因此如果是在结构体中声明变量的时候，应该用 DECLARE
 *
 * #define DEFINE_KFIFO(fifo, type, size) \
 * DECLARE_KFIFO(fifo, type, size) = \
 * (typeof(fifo)) { \
 * 	{ \
 * 		{ \
 * 		.in	= 0, \
 * 		.out	= 0, \
 * 		.mask	= __is_kfifo_ptr(&(fifo)) ? \
 * 			  0 : \
 * 			  ARRAY_SIZE((fifo).buf) - 1, \
 * 		.esize	= sizeof(*(fifo).buf), \
 * 		.data	= __is_kfifo_ptr(&(fifo)) ? \
 * 			NULL : \
 * 			(fifo).buf, \
 * 		} \
 * 	} \
 * }
 *
 * DECLARE_KFIFO 最终会按照如下处理
 * #define __STRUCT_KFIFO_COMMON(datatype, recsize, ptrtype) \
 * union { \
 * 	struct __kfifo	kfifo; \
 * 	datatype	*type; \
 * 	const datatype	*const_type; \
 * 	char		(*rectype)[recsize]; \
 * 	ptrtype		*ptr; \
 * 	ptrtype const	*ptr_const; \
 * }
 *
 */
#ifdef USE_INIT_DECLARE
DECLARE_KFIFO(m_fifo, char, FIFO_SIZE);
#endif

#ifdef USE_INIT_DEFINE
static DEFINE_KFIFO(m_fifo, char, FIFO_SIZE);
#endif

#ifdef USE_INIT_DYNAMIC
static struct kfifo m_fifo;
static char buffer[FIFO_SIZE];
#endif

static DEFINE_SPINLOCK(fifo_lock);

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

#ifdef USE_INIT_DYNAMIC
    if (kfifo_init(&m_fifo, buffer, sizeof(buffer)) != 0) {
        printk(KERN_ERR "Failed to initialize kfifo\n");
        return -1;
    }
#endif

#ifdef USE_INIT_DECLARE
    /*
     * INIT_KFIFO 是 用于初始化通过 DECLARE_KFIFO 静态声明的 kfifo 缓冲区的宏。
     * 虽然 DECLARE_KFIFO 通常已经自动完成初始化，但某些情况下（比如你把 FIFO
     * 包在结构体里、需要手动清空/重用）会用到 INIT_KFIFO。
     *
     *
     * INIT_KFIFO vs kfifo_reset
     *
     * | 对比点                  | `INIT_KFIFO()`                                     | `kfifo_reset()`                      |
     * | ----------------------- | -------------------------------------------------- | ------------------------------------ |
     * | 是否重新设置 buffer     | ✅ 是（重新设置 `.buffer`, `.size`, `.esize` 等）  | ❌ 否，仅清空 `.in`, `.out`          |
     * | 是否重建结构            | ✅ 是，对 `struct kfifo` 进行完整初始化            | ❌ 否，仅重置数据指针                |
     * | 是否会影响 buffer 内容  | ❌ 不清除内容，只是重设状态                        | ❌ 同上                              |
     * | 是否可用于 uninit FIFO  | ✅ 是（比如你手动写了 `struct kfifo myfifo;` 之后）| ❌ 不可，必须先已初始化              |
     * | 安全性                  | ✅ 完全初始化（含校验参数）                        | ⚠️ 前提：`kfifo` 必须是合法初始化过的 |
     * | 推荐使用场景            | 初始化一个新的 FIFO、重新分配 buffer 后重新设置    | 清空已有 FIFO 的数据、重用现有结构   |
     *
     */
    INIT_KFIFO(m_fifo);
#endif

    return 0;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    printk("M_CHRDEV: Device close\n");
    return 0;
}


static int fifo_demo(void)
{
    char inbuf[] = "KFIFO_DEMO";
    char outbuf[32] = {};
    char ch;
    unsigned int copied, i;

    spin_lock(&fifo_lock);

    // 1. 重置 FIFO
    kfifo_reset(&m_fifo);

    // 2. 使用 kfifo_in() 将字符串写入 FIFO
    copied = kfifo_in(&m_fifo, inbuf, sizeof(inbuf));
    pr_info("kfifo_in: inserted %u bytes: %s\n", copied, inbuf);

    // 3. 使用 kfifo_out() 从 FIFO 中读取数据
    copied = kfifo_out(&m_fifo, outbuf, sizeof(outbuf));
    pr_info("kfifo_out: got %u bytes: %s\n", copied, outbuf);

    // 4. 使用 kfifo_put() 插入单个字符
    for (i = 0; i < 5; i++) {
        ch = 'a' + i;
        if (kfifo_put(&m_fifo, ch))
            pr_info("kfifo_put: put %c\n", ch);
    }

    // 5. 使用 kfifo_peek() 看第一个字符
    if (kfifo_peek(&m_fifo, &ch))
        pr_info("kfifo_peek: %c\n", ch);

    // 6. 使用 kfifo_get() 逐个取出
    while (kfifo_get(&m_fifo, &ch))
        pr_info("kfifo_get: got %c\n", ch);

    spin_unlock(&fifo_lock);
    return 0;
}

static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("M_CHRDEV: Device ioctl\n");

    fifo_demo();

    return 0;
}

static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    unsigned int copied;
    int ret;

    printk("Reading device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (kfifo_is_empty(&m_fifo))
        return 0;

    spin_lock(&fifo_lock);
    ret = kfifo_to_user(&m_fifo, buf, count, &copied);
    spin_unlock(&fifo_lock);

    return ret ? ret : copied;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    unsigned int copied;
    int ret;

    printk("Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (kfifo_is_full(&m_fifo))
        return -ENOSPC;

    spin_lock(&fifo_lock);
    ret = kfifo_from_user(&m_fifo, buf, count, &copied);
    spin_unlock(&fifo_lock);

    return ret ? ret : copied;
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

    /*
     * 在这个简单的例子中，我们不需要在模块卸载时做特别的清理，
     * 因为 kfifo 使用的缓冲区是静态分配的，并且在模块卸载时自动释放。
     */

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
