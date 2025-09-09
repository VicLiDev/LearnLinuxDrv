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

#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <net/sock.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"
#define PROC_NAME "dump_fds"

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

static pid_t target_pid = -1;
module_param(target_pid, int, 0644);
MODULE_PARM_DESC(target_pid, "Target PID to dump fds (-1 means current)");

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

static int dump_fds_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    struct files_struct *files;
    struct fdtable *fdt;
    int i;

    if (target_pid == -1) {
        task = current;  // 当前 cat /proc/dump_fds 的进程
        get_task_struct(task);
    } else {
        rcu_read_lock();
        task = pid_task(find_vpid(target_pid), PIDTYPE_PID);
        if (!task) {
            rcu_read_unlock();
            seq_printf(m, "PID %d not found\n", target_pid);
            return 0;
        }
        get_task_struct(task);
        rcu_read_unlock();
    }

    files = task->files;
    if (!files) {
        seq_printf(m, "No files for PID %d\n", task->pid);
        goto out_put_task;
    }

    spin_lock(&files->file_lock);
    fdt = files_fdtable(files);

    seq_printf(m, "Dumping FDs for PID %d (%s):\n", task->pid, task->comm);

    for (i = 0; i < fdt->max_fds; i++) {
        struct file *f = fdt->fd[i];
        if (f) {
            char *tmp, *pathname;
            struct inode *inode = file_inode(f);

            tmp = (char *)__get_free_page(GFP_KERNEL);
            if (!tmp) {
                seq_printf(m, "fd %d: (OOM)\n", i);
                continue;
            }

            pathname = d_path(&f->f_path, tmp, PAGE_SIZE);
            if (IS_ERR(pathname))
                seq_printf(m, "fd %d -> (path error)\n", i);
            else
                seq_printf(m, "fd %d -> %s\n", i, pathname);

            free_page((unsigned long)tmp);

            /* 基本 file 信息 */
            seq_printf(m, "    flags: 0x%x, mode: 0x%x, pos: %lld, f_count: %ld\n",
                       f->f_flags, f->f_mode, f->f_pos, file_count(f));

            if (inode) {
                seq_printf(m, "    inode: %lu, dev: %u:%u, mode: %o, size: %lld\n",
                           inode->i_ino,
                           MAJOR(inode->i_sb->s_dev), MINOR(inode->i_sb->s_dev),
                           inode->i_mode,
                           i_size_read(inode));

                if (f->f_path.dentry && f->f_path.dentry->d_sb &&
                    f->f_path.dentry->d_sb->s_type)
                    seq_printf(m, "    fs: %s\n",
                               f->f_path.dentry->d_sb->s_type->name);

                /* 如果是 socket，额外打印 */
                if (S_ISSOCK(inode->i_mode)) {
// #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
//                     struct socket *sock = sock_from_file(f, NULL);
// #else
                    struct socket *sock = sock_from_file(f);
// #endif
                    if (sock && sock->sk) {
                        seq_printf(m, "    socket: family=%d, type=%d, proto=%d, state=%d\n",
                                   sock->sk->sk_family,
                                   sock->sk->sk_type,
                                   sock->sk->sk_protocol,
                                   sock->sk->sk_state);
                    }
                }
            }
        }
    }

    spin_unlock(&files->file_lock);

out_put_task:
    put_task_struct(task);
    return 0;
}

static int dump_fds_open(struct inode *inode, struct file *file)
{
    return single_open(file, dump_fds_show, NULL);
}

static const struct proc_ops dump_fds_fops = {
    .proc_open    = dump_fds_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

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

    proc_create(PROC_NAME, 0444, NULL, &dump_fds_fops);

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    remove_proc_entry(PROC_NAME, NULL);

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
