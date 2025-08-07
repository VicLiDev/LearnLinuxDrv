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
/* list */
#include <linux/list.h>
#include <linux/init.h>


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

/*
 * global storage for device Major number
 * 多个设备可以对应一个驱动
 */
static int dev_major = 0;

/*
 * sysfs class structure
 * 多个设备对应一个驱动，自然也对应同一个class
 */
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

/* 链表节点结构体 */
struct person {
    int id;
    char name[16];
    struct list_head link;
};

static void init_list(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}

static void add_person_tail(struct list_head *head, int id, const char *name)
{
    struct person *p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (!p)
        return;
    p->id = id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';

    list_add_tail(&p->link, head);  // 尾插
    pr_info("Added %s (ID=%d) to tail\n", p->name, p->id);
}

static void add_person_head(struct list_head *head, int id, const char *name)
{
    struct person *p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (!p)
        return;
    p->id = id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';

    list_add(&p->link, head);  // 头插
    pr_info("Added %s (ID=%d) to head\n", p->name, p->id);
}

static void insert_person_sorted(struct list_head *head, int id, const char *name)
{
    struct person *p, *new_p;

    new_p = kmalloc(sizeof(*new_p), GFP_KERNEL);
    if (!new_p)
        return;
    new_p->id = id;
    strncpy(new_p->name, name, sizeof(new_p->name) - 1);
    new_p->name[sizeof(new_p->name) - 1] = '\0';

    /* 没保留下一个节点，所以当前节点是不可以删除的，删除的话就找不到后续链表节点了 */
    list_for_each_entry(p, head, link) {
        if (id < p->id) {
            list_add_tail(&new_p->link, &p->link);  // 插在p之前
            pr_info("Inserted %s (ID=%d) before %s\n", new_p->name, new_p->id, p->name);
            return;
        }
    }

    list_add_tail(&new_p->link, head);  // 插到最后
    pr_info("Inserted %s (ID=%d) at end (sorted insert)\n", new_p->name, new_p->id);
}

static void print_list_forward(struct list_head *head)
{
    struct person *p;

    pr_info("Forward traversal:\n");
    list_for_each_entry(p, head, link) {
        pr_info(" - %s (ID=%d)\n", p->name, p->id);
    }
}

static void print_list_reverse(struct list_head *head)
{
    struct person *p;

    pr_info("Reverse traversal:\n");
    list_for_each_entry_reverse(p, head, link) {
        pr_info(" - %s (ID=%d)\n", p->name, p->id);
    }
}

static void delete_person_by_id(struct list_head *head, int id)
{
    struct person *p, *tmp;

    /* 因为保留了下一个节点，所以当前节点是可以删除的，因此是安全的 */
    list_for_each_entry_safe(p, tmp, head, link) {
        if (p->id == id) {
            pr_info("Deleting %s (ID=%d)\n", p->name, p->id);
            list_del(&p->link);
            kfree(p);
            return;
        }
    }
    pr_info("ID=%d not found\n", id);
}

static void move_head_to_tail(struct list_head *head)
{
    if (!list_empty(head)) {
        struct list_head *first = head->next;
        list_move_tail(first, head);
        pr_info("Moved head to tail\n");
    }
}

static void merge_lists_demo(struct list_head *head)
{
    static LIST_HEAD(extra_list);
    struct person;

    add_person_tail(&extra_list, 300, "Merge1");
    add_person_tail(&extra_list, 400, "Merge2");
    list_splice_tail(&extra_list, head);

    // 清空 extra_list，只保留在 list 中
    INIT_LIST_HEAD(&extra_list);

    pr_info("Merged extra list into list\n");
}

static void show_list_entries(struct list_head *head)
{
    struct person *p;

    pr_info("------ Dumping all entries ------\n");

    list_for_each_entry(p, head, link) {
        pr_info("Person ID: %d, Name: %s\n", p->id, p->name);
    }

    pr_info("------ Using first_entry and last_entry ------\n");

    if (!list_empty(head)) {
        struct person *first = list_first_entry(head, struct person, link);
        struct person *last  = list_last_entry(head, struct person, link);
        pr_info("First: ID=%d, Name=%s\n", first->id, first->name);
        pr_info("Last : ID=%d, Name=%s\n", last->id, last->name);
    }

    pr_info("------ Using *_entry_or_null safely ------\n");

    p = list_first_entry_or_null(head, struct person, link);
    if (p)
        pr_info("Safe First: ID=%d, Name=%s\n", p->id, p->name);

    /* Show next and previous entries relative to first */
    if (!list_empty(head)) {
        struct person *first = list_first_entry(head, struct person, link);
        struct person *next = list_next_entry(first, link);
        if (&next->link != head)
            pr_info("Next after first: ID=%d, Name=%s\n", next->id, next->name);

        struct person *last = list_last_entry(head, struct person, link);
        struct person *prev = list_prev_entry(last, link);
        if (&prev->link != head)
            pr_info("Prev before last: ID=%d, Name=%s\n", prev->id, prev->name);
    }
}

static void clear_list(struct list_head *head)
{
    struct person *p, *tmp;

    list_for_each_entry_safe(p, tmp, head, link) {
        pr_info("Freeing %s (ID=%d)\n", p->name, p->id);
        list_del(&p->link);
        kfree(p);
    }
}

static int __init m_chr_init(void)
{
    int err, idx;
    dev_t devno;

    struct list_head person_list_head;

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

    /* 初始化链表 */
    init_list(&person_list_head);
    // 或者直接静态初始化
    // static LIST_HEAD(person_list_head);  // 初始化链表头

    add_person_tail(&person_list_head, 20, "Alice");
    add_person_tail(&person_list_head, 40, "Charlie");
    add_person_head(&person_list_head, 10, "Bob");
    insert_person_sorted(&person_list_head, 30, "David");

    print_list_forward(&person_list_head);
    print_list_reverse(&person_list_head);

    delete_person_by_id(&person_list_head, 40);
    move_head_to_tail(&person_list_head);
    merge_lists_demo(&person_list_head);
    print_list_forward(&person_list_head);
    show_list_entries(&person_list_head);

    clear_list(&person_list_head);

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
