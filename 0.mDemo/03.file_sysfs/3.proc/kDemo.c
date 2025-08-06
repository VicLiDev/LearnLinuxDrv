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

struct proc_dir_entry *generic_entry, *singel_entry, *sig_data_entry, *proc_data_entry, *proc_seq_entry;

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

/* ---- proc_create_data 示例 ---- */
/*
 * proc_create_data() 是 Linux 内核中创建 支持私有数据（void *data） 的 /proc
 * 文件的通用函数。
 * 可以将它看作是比 proc_create() 更强大的版本：它允许传入一个自定义的数据指针，
 * 这个指针会保存在 proc_dir_entry 中，以便在 file/inode 操作中通过 PDE_DATA() 访问。
 *
 * 创建的文件可读可写
 *
 * 访问私有数据的方法：
 * void *data = PDE_DATA(inode);                  // 在 open 时通过 inode 获取
 * void *data = PDE_DATA(file_inode(file));       // 在 read/write 时通过 file 获取
 */
// 编译有问题，这里自己定义
#ifndef PDE_DATA
#define PDE_DATA(inode) ((inode)->i_private)
#endif

#define DATA_ENTRY_NAME "entry_data"

struct m_data {
    char msg[128];
    int counter;
};

static struct m_data m_info = {
    .msg = "Hello from /proc data entry!",
    .counter = 0,
};

static int m_show(struct seq_file *m, void *v)
{
    struct m_data *d = m->private;
    seq_printf(m, "Message: %s\nCounter: %d\n", d->msg, d->counter);
    return 0;
}

static int m_open(struct inode *inode, struct file *file)
{
    return single_open(file, m_show, PDE_DATA(inode));
}

static ssize_t m_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
    struct m_data *d = PDE_DATA(file_inode(file));
    char tmp[128];

    if (len >= sizeof(tmp))
        return -EINVAL;

    if (copy_from_user(tmp, buf, len))
        return -EFAULT;

    tmp[len] = '\0';

    strncpy(d->msg, tmp, sizeof(d->msg) - 1);
    d->counter++;

    return len;
}

static const struct proc_ops m_proc_data_ops = {
    .proc_open    = m_open,
    .proc_read    = seq_read,
    .proc_write   = m_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ---- proc_create seq 示例 ---- */
/*
 * 总体原理
 *   虽然没手写 proc_read()，但实际上用了 seq_file 接口，它内部封装了读取逻辑，
 *   简化了开发 /proc 文件的过程。
 *
 * 提供了：
 *   .proc_read = seq_read,
 * 也就是说：
 *   用户空间执行 cat /proc/m_seq
 *   内核调用 proc_read() → 实际对应到 seq_read()（标准封装）
 *   seq_read() 内部使用你定义的 seq_operations 来获取数据
 * 整体调用流程图
 *   用户空间：cat /proc/m_seq
 *         ↓
 *   VFS：调用 .read 方法 → 实际是 proc_read → seq_read
 *         ↓
 *   seq_read():
 *       → seq->op->start()    // 你提供的 m_seq_start
 *       → seq->op->show()     // 你提供的 m_seq_show
 *       → seq->op->next()     // m_seq_next
 *       → seq->op->stop()     // m_seq_stop
 * 也就是说：
 *   没实现 read()，但注册了 seq_read，它实际完成了所有分页和偏移管理
 *   seq_file 的设计是为了简化大量输出的 /proc 实现（比如 /proc/meminfo）
 *
 * 细节解释：seq_file 是怎么工作的？
 * seq_read() 会在每次读取时循环调用 seq_operations 的函数，直到读取完整个内容：
 * | 函数名	   | 你的实现          | 作用                                   |
 * | --------- | ----------------- | -------------------------------------- |
 * | start()   | m_seq_start()	   | 初始化读取，返回数据项的指针           |
 * | show()	   | m_seq_show()	   | 输出一项内容到 seq_file（类似 printf） |
 * | next()	   | m_seq_next()	   | 移动到下一项                           |
 * | stop()	   | m_seq_stop()	   | 清理收尾（可不实现）                   |
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

#define SEQ_BUF_SIZE 1024

static char m_seq_buf[SEQ_BUF_SIZE];
static size_t m_seq_buf_len;
static DEFINE_MUTEX(m_seq_buf_lock);

// seq_file 相关函数

static void *m_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= 1)  // 只支持一条记录，pos大于等于1则结束
        return NULL;
    return m_seq_buf;
}

static void m_seq_stop(struct seq_file *s, void *v)
{
    // 不需要释放资源
}

static void *m_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    return NULL;
}

static int m_seq_show(struct seq_file *s, void *v)
{
    mutex_lock(&m_seq_buf_lock);
    seq_write(s, m_seq_buf, m_seq_buf_len);
    mutex_unlock(&m_seq_buf_lock);
    return 0;
}

static const struct seq_operations m_seq_ops = {
    .start = m_seq_start,
    .next  = m_seq_next,
    .stop  = m_seq_stop,
    .show  = m_seq_show,
};

// open 调用 seq_open 并传递 seq_operations
static int m_seq_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &m_seq_ops);
}

// 写操作，把用户数据写到 m_seq_buf
static ssize_t m_seq_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (count > SEQ_BUF_SIZE - 1)
        return -EINVAL;

    mutex_lock(&m_seq_buf_lock);
    if (copy_from_user(m_seq_buf, buf, count)) {
        mutex_unlock(&m_seq_buf_lock);
        return -EFAULT;
    }
    m_seq_buf[count] = '\0';
    m_seq_buf_len = count;
    mutex_unlock(&m_seq_buf_lock);

    ret = count;
    return ret;
}

static const struct proc_ops m_seq_proc_ops = {
    .proc_open    = m_seq_proc_open,
    .proc_read    = seq_read,
    .proc_write   = m_seq_proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = seq_release,
};


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

    /*
     * 升级版本读写的 data entry
     *
     * struct proc_dir_entry *proc_create_data(const char *name, umode_t mode,
     *      struct proc_dir_entry *parent, const struct proc_ops *proc_ops, void *data);
     *
     * 其中：
     *   `name`: proc 文件名（如 `"m_proc_entry"`）
     *   `mode`: 权限（如 `0444`, `0666` 等）
     *   `parent`: 父目录（如传 `NULL` 表示 `/proc` 根目录）
     *   `proc_ops`: 文件操作结构体，定义 `.proc_read`, `.proc_write`, `.proc_open` 等
     *   `data`: 自定义数据指针（可通过 `PDE_DATA(inode)` 或 `PDE_DATA(file_inode(file))` 获取）
     */
    proc_data_entry = proc_create_data(DATA_ENTRY_NAME, 0666, demo_dir, &m_proc_data_ops, &m_info);
    if (!proc_data_entry) {
        pr_err("Failed to create /proc/%s/%s\n", PROC_DIRNAME, "sig_data_entry");
        return -ENOMEM;
    }
    
    /* */
    // 初始化缓冲区
    mutex_lock(&m_seq_buf_lock);
    strcpy(m_seq_buf, "Initial kernel buffer content\n");
    m_seq_buf_len = strlen(m_seq_buf);
    mutex_unlock(&m_seq_buf_lock);

    proc_seq_entry = proc_create("entry_seq", 0666, demo_dir, &m_seq_proc_ops);
    if (!proc_seq_entry) {
        pr_err("Failed to create /proc/%s\n", "entry_seq");
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
    remove_proc_entry(DATA_ENTRY_NAME, NULL);
    proc_remove(proc_seq_entry);
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
