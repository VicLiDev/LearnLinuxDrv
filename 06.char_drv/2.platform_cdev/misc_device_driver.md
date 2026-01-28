# misc device driver

​		有部分类似globalmeme、globalfifo等的字符设备，确实无法分类为某一个设备类型时，一般推荐大家采用miscdevice框架结构。miscdevice本质上也是字符设备，只是在miscdevice核心曾的misc_init()函数中，通过regiister_chrdev(MISC_MAJOR, "misc", &misc_fops)注册了字符设备，而具体miscdevice实例调用misc_register()的时候又自动完成了device_create()、获取动态次设备号的动作。miscdevice的主设备号是固定的，MISC_MAJOR定义为10。

​		miscdevice结构体内file_operations中的成员函数实际上是由drivers/char/misc.c中的misc驱动核心曾的misc_fops成员函数间接调用的，比如misc_open()就会间接调用底层注册的msicdevice的fops->open。



## 1. 重要数据结构和接口

```c
struct miscdevice{
    int minor;
    const char *name;
    const struct file_operations *fops;
    struct list_head list;
    struct device *parent;
    struct device *this_device;
    const char *nodename;
    umode_t mode;
}
```

如果miscdevice结构体中的minor为MISC_DYNAMIC_MINOR，miscdevice核心曾会自动找一个空闲的次设备号，否则用minor指定的次设备号



miscdevice驱动的注册和注销分别用下面两个api：

```c
int misc_register(struct miscdevice *misc);
int misc_deregister(struct miscdevice *misc);
```

miscdevice驱动的一般结构如下：

```c
static const struct file_operations xxx_fops = {
    .unlocked_ioctl = xxx_ioctl,
    .mmap			= xxx_mmap,
    ...
};
static struct miscdevice xxx_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "xxx",
    .fops  = &xxx_fops
};
static int __init xxx_init(void)
{
    pr_info("ARC Hostlink driver mmap at 0x%p\n", __HOSTLINK__);
    return misc_register(&xxx_dev);
}
```



在调用misc_register(&xxx_dev)时，该函数内部会自动调用device_create()，而device_create()会议xxx_dev作为drvdata参数。其次，在miscdevice核心曾misc_open()函数的帮助下，在file_operations的成员函数中，xxx_dev会自动成为file的private_data(misc_open会完成file->private_data的赋值操作)。



## 2. demo

关注 globalfifo/ch12/globalfifo.c ，来自宋宝华的《Linux设备驱动开发详解》中的demo