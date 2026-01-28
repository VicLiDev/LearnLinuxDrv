参考博客：

[深入讲解，linux内核——设备驱动模型](https://zhuanlan.zhihu.com/p/501817318)

[](https://blog.51cto.com/u_13267193/5370840)

# Linux 设备模型

Linux 设备模型(LDM)
* 类的概念，对于相同类型的设备或提供相同功能的设备进行分组（如鼠标和键盘都是输入
设备）
* 通过名为sysfs的虚拟文件系统与用户空间进行通信，以便用户空间管理和枚举设备及其
公开的属性
* 管理对象生命周期，使用引用计数（在管理资源中大量使用）
* 电源管理，以处理设备关闭的顺序
* 代码的可重用性。类和框架提供接口，就像合约一样，任何向他们注册的驱动程序都必须
遵守
* LDM在内核中引入了面向对象（OO）的编程风格

LDM框架依赖总线、设备驱动程序、设备。


## LDM数据结构

### 总线

总线是设备和处理器之间的通道链路。管理总线并使用相关协议与设备通信的硬件实体成为
总线控制器。自身作为设备的总线控制器必须像其他设备一样注册。总线控制器作为总线上
其他设备的父设备。

```C
// include/linux/device/bus.h
struct bus_type {
    const char        *name;
    const char        *dev_name;
    struct device        *dev_root;
    // 以下三个数据均指向 attribute_group 数组
    // drv_groups：表示总线上驱动的默认属性，驱动注册到总线上时使用
    // dev_groups：表示总线上设备的默认属性，设备注册到总线上时使用
    //             这些属性可以在 /sys/bus/<bus-name>/devices/<device-name>内的
    //             设备目录中找到
    // bus_groups：总线注册到内核时，作为默认属性使用
    const struct attribute_group **bus_groups;
    const struct attribute_group **dev_groups;
    const struct attribute_group **drv_groups;

    // 回调，新 驱动/设备 添加到总线时调用，匹配设备和驱动，匹配成功返回非零值，
    // 大多数情况下，验证通过简单的字符串（设备和驱动程序名称，表格和设备树的兼容
    // 属性）比较实现。对于枚举设备（PCI、USB），验证通过将驱动程序支持的设备ID
    // 与制定设备的设备ID记性比较来实现。
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    // 回调，设备和驱动匹配发生后调用，会分配设备结构体，并且会调用驱动程序的
    // probe函数，进行设备初始化
    int (*probe)(struct device *dev);
    void (*sync_state)(struct device *dev);
    // 回调，设备从总线上移除时调用
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    int (*online)(struct device *dev);
    int (*offline)(struct device *dev);

    // 回调，总线上的设备需要进入睡眠时调用
    int (*suspend)(struct device *dev, pm_message_t state);
    // 回调，总线上的设备需要退出睡眠时调用
    int (*resume)(struct device *dev);

    int (*num_vf)(struct device *dev);

    int (*dma_configure)(struct device *dev);

    // 这是一组总线电源管理相关的操作，它会调用指定设备驱动程序的pm-ops
    const struct dev_pm_ops *pm;

    const struct iommu_ops *iommu_ops;

    struct subsys_private *p;
    struct lock_class_key lock_key;

    bool need_parent_lock;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
};
```

总线控制器本身也是设备，在99%的情况下，总线是平台设备（包括提供枚举的总线），
它作为该总线上其他设备的父设备。

总线注册函数
```C
int bus_register(struct bus_type * bus);
```

### 驱动

数据结构：
```C
// include/linux/device/driver.h
struct device_driver {
    // 驱动的名称，可以用于匹配，将它与设备名称进行比较
    const char        *name;
    // 当前驱动所在的总线
    struct bus_type   *bus;

    // 拥有该驱动的模块，99%的情况下该字段设置为 THIS_MODULE
    struct module        *owner;
    const char        *mod_name;    /* used for built-in modules */

    bool suppress_bind_attrs;    /* disables bind/unbind via sysfs */
    enum probe_type probe_type;

    // of_device_id 结构用于设备树匹配
    const struct of_device_id    *of_match_table;
    const struct acpi_device_id    *acpi_match_table;

    // 在尝试将驱动程序绑定到设备上时运行它。总线驱动程序负责调用设备驱动程序的
    // probe 函数。
    int (*probe) (struct device *dev);
    void (*sync_state)(struct device *dev);
    int (*remove) (struct device *dev);
    void (*shutdown) (struct device *dev);
    // 回调，提供电源管理功能，当设备从系统中物理移除或者引用计数达到0时，
    // 调用remove回调，系统重新启动期间也会调用remove回调
    int (*suspend) (struct device *dev, pm_message_t state);
    // 回调，提供电源管理功能
    int (*resume) (struct device *dev);
    // 作为默认属性使用
    const struct attribute_group **groups;
    const struct attribute_group **dev_groups;

    const struct dev_pm_ops *pm;
    void (*coredump) (struct device *dev);

    struct driver_private *p;
};
```

驱动注册函数
```C
int driver_register(struct device_driver *drv);
```
该函数是用于向总线注册驱动的底层函数，它将驱动添加到总线的驱动列表中。在驱动注册
到总线后，内核遍历总线的设备列表，对于没有关联驱动程序的每个设备调用总线的匹配
回调函数，完成满足条件的所有匹配

当匹配成功时，设备和驱动程序绑定到一起。设备与驱动程序关联的过程称为绑定。

### 设备

该数据结构用于描述和表示系统中的每个设备，无论他是否是物理设备

```C
// include/linux/device.h
struct device {
    struct kobject kobj;
    // 当前设备的父设备，当注册到总线时，总线驱动程序用总线设备设置该字段的值
    struct device        *parent;

    struct device_private    *p;

    const char        *init_name; /* initial name of the device */
    // 设备的类型
    const struct device_type *type;

    // 当前设备所在的总线
    struct bus_type    *bus;        /* type of bus device is on */
    struct device_driver *driver;    /* which driver has allocated this
                       device */
    // 指向当前设备特有的平台数据的指针
    void        *platform_data;    /* Platform specific data, device
                       core doesn't touch it */
    // 指向驱动程序私有数据的指针
    void        *driver_data;    /* Driver data, set and get with
                       dev_set_drvdata/dev_get_drvdata */
    struct mutex        mutex;    /* mutex to synchronize calls to
                     * its driver.
                     */

    struct dev_links_info    links;
    struct dev_pm_info    power;
    struct dev_pm_domain    *pm_domain;

#ifdef CONFIG_ENERGY_MODEL
    struct em_perf_domain    *em_pd;
#endif

#ifdef CONFIG_PINCTRL
    struct dev_pin_info    *pins;
#endif
    struct dev_msi_info    msi;
#ifdef CONFIG_DMA_OPS
    const struct dma_map_ops *dma_ops;
#endif
    u64        *dma_mask;    /* dma mask (if dma'able device) */
    u64        coherent_dma_mask;/* Like dma_mask, but for
                         alloc_coherent mappings as
                         not all hardware supports
                         64 bit addresses for consistent
                         allocations such descriptors. */
    u64        bus_dma_limit;    /* upstream dma constraint */
    const struct bus_dma_region *dma_range_map;

    struct device_dma_parameters *dma_parms;

    struct list_head    dma_pools;    /* dma pools (if dma'ble) */

#ifdef CONFIG_DMA_DECLARE_COHERENT
    struct dma_coherent_mem    *dma_mem; /* internal for coherent mem
                         override */
#endif
#ifdef CONFIG_DMA_CMA
    struct cma *cma_area;        /* contiguous memory area for dma
                       allocations */
#endif
#ifdef CONFIG_SWIOTLB
    struct io_tlb_mem *dma_io_tlb_mem;
#endif
    /* arch specific additions */
    struct dev_archdata    archdata;

    // 执行设备相关的OF节点的指针（设备树相关），总线驱动程序设置该字段
    struct device_node    *of_node; /* associated device tree node */
    struct fwnode_handle    *fwnode; /* firmware device node */

#ifdef CONFIG_NUMA
    int        numa_node;    /* NUMA node this device is close to */
#endif
    dev_t            devt;    /* dev_t, creates the sysfs "dev" */
    u32            id;    /* device instance */

    spinlock_t        devres_lock;
    struct list_head    devres_head;

    // 指向设备所属类的指针
    struct class        *class;
    const struct attribute_group **groups;    /* optional groups */

    // 设备引用计数达到零时调用的回调
    void    (*release)(struct device *dev);
    struct iommu_group    *iommu_group;
    struct dev_iommu    *iommu;

    struct device_physical_location *physical_location;

    enum device_removable    removable;

    bool            offline_disabled:1;
    bool            offline:1;
    bool            of_node_reused:1;
    bool            state_synced:1;
    bool            can_match:1;
#if defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_DEVICE) || \
    defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) || \
    defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU_ALL)
    bool            dma_coherent:1;
#endif
#ifdef CONFIG_DMA_OPS_BYPASS
    bool            dma_ops_bypass : 1;
#endif
};
```

设备注册函数
```C
int device_add(struct device *dev);
```

该函数用于向总线注册设备，调用该函数后，总线会遍历驱动的列表，查找支持此设备的
驱动，然后将该设备添加到该总线的设备列表中。device_register()在内部调用
device_add()。

无论何时添加设备，都会调用总线驱动的匹配方法(bus_type->match)。如果匹配函数发现
此设备的驱动，则会调用总线驱动程序的probe方法(bus_type->probe)，给出设备和驱动
程序作为参数。然后由总线驱动程序调用设备驱动程序的probe方法(driver->probe)


## 设备模型（LDM）

### kobject

kobject 是设备模型的核心，内核类似于OO的编程风格需要依赖kobject，相当于面向对象
体系架构中的总基类。

* kobject可以建立设备之间的层次结构
* kobject提供了最基本的设备对象管理能力，每一个在内核中注册的kobject对象都对应于
  sysfs文件系统中的一个目录(而不是文件)
* kobject是各种对象最基本的单元，提供一些公用型服务如：对象引用计数、维护对象
  链表、对象上锁、对用户空间的展示
* 设备驱动模型中的各种对象其内部都会包含一个kobject

kobject引入了通用的通用对象属性（如使用引用计数）的封装概念：
```C
struct kobject {
    // 当前 kobject 的名字，可使用 kobject_set_name()接口修改，也作为一个目录的名字
    const char        *name;
    //连接下一个kobject结构
    struct list_head    entry;
    // 如果父设备存在，指向当前 object 父关系 的指针，用于构建对象之间的关系层次结构
    struct kobject        *parent;
    // 指向kset集合
    struct kset        *kset;
    // 指向kobject的属性描述符
    struct kobj_type    *ktype;
    // 表示该结构体内 sysfs 节点中的这个 kobject，对应sysfs的文件目录
    struct kernfs_node    *sd; /* sysfs directory entry */
    // 当前kobject 的引用计数
    struct kref        kref;
#ifdef CONFIG_DEBUG_KOBJECT_RELEASE
    struct delayed_work    release;
#endif
    // 表示该kobject是否初始化
    unsigned int state_initialized:1;
    // 表示是否添加到sysfs中
    unsigned int state_in_sysfs:1;
    unsigned int state_add_uevent_sent:1;
    unsigned int state_remove_uevent_sent:1;
    unsigned int uevent_suppress:1;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
};
```

```C
// 必须使用 kobject_create 函数分配kobject
struct kobject *kobject_create(void)

// 如果需要加入到kset中，在这个位置添加
kobj->kset = my_kset;

// kobject_create 返回的空 kobject 需要用 kobj_init()进行初始化，
// 看内核源码发现kobject_create 内部也会调用kobject_init，因此调用kobj_init()
// 不是必需的
// 以分配但未初始化的kobject指针及其kobj_type指针作为参数
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)

// 添加kobject到sysfs中，同时根据其层次结构创建目录及其默认属性
int kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
// 与其相反功能的函数如下，内部会调用 kobject_put 接口
void kobject_del(struct kobject *kobj)

// 内部调用 kobject_create 和 kobject_add
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)

// 递增/递减引用计数
// 如果kobject的引用计数减少到0，则与该kobject关联的ktype中的析构函数将被调用
struct kobject *kobject_get(struct kobject *kobj)
void kobject_put(struct kobject *kobj)
```

使用举例：
```C
// ex1:
static struct kobject *mykobj;

mykobj = kobject_create();
if (mykobj) {
    kobject_init(mykobj, &mytype);
    if (kobject_add(mykobj, NULL, "%s", "hello")) {
        err = -1;
        printk("ldm: kobject_add() failed\n");
        kobject_put(mykobj);
        mykobj = NULL;
    }
    err = 0;
}


// ex2:
static struct kobject *class_kobj = NULL;
static struct kobject *devices_kobj = NULL;

// 创建 sys/class
class_kobj = kobject_create_and_add("class", NULL);
if (!class_kobj) {
    return -ENOMEM;
}

// 创建 sys/devices
devices_kobj = kobject_create_and_add("devices", NULL);
if (!devices_kobj) {
    return -ENOMEM;
}

// 如果 kobject 的父项为NULL，那么kobject_add将其父项设置为kset，如果两者均为NULL，
// 对象将成为顶级sys目录的子成员
```

### ktype

使用该kobject设备的共同属性，很多书中简称为ktype，每一个kobject都需要绑定一个
ktype来提供相应功能，ktype的存在是为了描述一族kobject所具有的普遍特性。如此一来，
不再需要每个kobject都分别定义自己的特性，而是将这些普遍的特性在ktype结构中一次定义，
然后所有“同类”的kobject都能共享一样的特性。

struct kobj_type 结构描述kobject的行为。它将控制在创建、销毁kobject时以及读取或
写入属性时发生的操作。每个kobject都有 kobj_tyep 类型的字段。
```C
struct kobj_type {
    // 释放kobject和其占用的函数，在kobject引用计数减少到0时调用，类似析构函数
    void (*release)(struct kobject *kobj);
    // 内核对象可以共享的公共操作，无论这些对象是否在功能上相关
    // sysfs_ops 指向sysfs操作，是访问sysfs属性时调用的一组回调函数
    // ！！重要，提供该对象在sysfs中的操作方法（show和store），即描述了sysfs文件
    // 读写时的特性。
    const struct sysfs_ops *sysfs_ops;
    // 定义默认属性，如果kobject添加到sysfs中，这些属性会相应的导出，在sysfs中
    // 以文件形式存在
    struct attribute **default_attrs;    /* use default_groups instead */
    const struct attribute_group **default_groups;
    const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
    void (*get_ownership)(struct kobject *kobj, kuid_t *uid, kgid_t *gid);

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
};


// 在用户读写sysfs中的属性文件时，实际调用的就是sysfs_ops中的接口
struct sysfs_ops {
    // show()函数用于读取一个属性到用户空间
    //  函数的第1个参数是要读取的kobject的指针，它对应要读的目录
    //  第2个参数是要读的属性
    //  第3个参数是存放读到的属性的缓存区
    //  当函数调用成功后，会返回实际读取的数据长度，这个长度不能超过PAGESIZE个字节大小
    ssize_t    (*show)(struct kobject *, struct attribute *, char *);
    // store()函数将属性写入内核中
    //  函数的第1个参数是与写相关的kobject的指针，它对应要写的目录
    //  第2个参数是要写的属性
    //  第3个参数是要写入的数据
    //  第4个参数是要写入的参数长度，这个长度不能超过PAGE-SIZE个字节大小。只有当拥有属性有写权限时，才能调用store函数。
    ssize_t    (*store)(struct kobject *, struct attribute *, const char *, size_t);
};

// kobj_sysfs_ops作为操作接口挂到kobj_type中（kset_ktype.sysfs_ops）
const struct sysfs_ops kobj_sysfs_ops = {
    .show   = kobj_attr_show,
    .store  = kobj_attr_store,
};

// kset_ktype 是提供给 kset 使用的，关于kset的描述见后续内容
static const struct kobj_type kset_ktype = {
    .sysfs_ops  = &kobj_sysfs_ops,
    .release    = kset_release,
    .get_ownership  = kset_get_ownership,
};
```

### kset

kset主要将相关的内核对象组合在一起，kset是kobject的集合。换句话说，kset将相关的
kobject收集到一个位置，例如所有块设备：
```C
struct kset {
    // kset 中所有kobject的链表
    struct list_head list;
    // 保护链表访问的自旋锁
    spinlock_t list_lock;
    // 该集合的基类
    struct kobject kobj;
    const struct kset_uevent_ops *uevent_ops;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
} __randomize_layout;
```

每个注册的（添加到系统的）kset对应sysfs目录。可以使用kset_create_and_add()函数
创建和添加kset，使用kset_unregister()函数将其删除

将kobject添加到kset中，只需要对kobject中的kset简单赋值即可，举例：
```C
mkset = kset_create_and_add("kset_example", NULL, kernel_kobj);
mkobj.kset = mkset;
mkobj2.kset = mkset;
retval = kobject_init_and_add(&kobj, &mktype, NULL, "mkobj_name");
retval = kobject_init_and_add(&kobj2, &mktype2, NULL, "mkobj2_name");
```

在 exit 函数内，在kobject及其属性被删除后：
```C
kset_unregister(mkset);
```

### kref

```C
// include/linux/kref.h
struct kref {
    refcount_t refcount;
};
```
虽然只有一个字段，但还是使用了结构体，这是为了便于进行类型检测。

在使用kref前，必须使用kref_init()接口进行初始化：
```C
static inline void kref_init(struct kref *kref)
{
    refcount_set(&kref->refcount, 1);
}
```
该函数将原子变量初始化为1，所以kref一旦被初始化，它的引用计数便固定为1

增加/减少引用：
```C
static inline void kref_get(struct kref *kref)
{
    refcount_inc(&kref->refcount);
}

static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (refcount_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}
```

开发人员不需要在内核中自己实现atmoic_t类型的get/put，最好的方法是直接使用kref和它
提供的相应类型。


### 属性

属性是kobject导出到用户空间的sysfs文件。可以从用户空间读取/写入属性，也就是说，
每个kobject及其子类，都可以公开kobject自身提供的默认属性或自定义属性。换句话说，
属性将内核数据映射到sysfs中的文件

```C
struct attribute {
    // 属性名，也是在sysfs中的文件名
    const char        *name;
    // 文件的读写权限
    umode_t            mode;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    bool            ignore_lockdep:1;
    struct lock_class_key    *key;
    struct lock_class_key    skey;
#endif
};
```

从文件系统添加/删除属性的内核函数如下：
```C
static inline int __must_check sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
static inline void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
```

**属性组**

```C
struct attribute_group {
    const char        *name;
    umode_t            (*is_visible)(struct kobject *,
                          struct attribute *, int);
    umode_t            (*is_bin_visible)(struct kobject *,
                          struct bin_attribute *, int);
    struct attribute    **attrs;
    struct bin_attribute    **bin_attrs;
};
```
重点关注`struct attribute **attrs`，这可以将属性打包，帮助管理属性，可以避免创建
sysfs文件时，每个属性都需要调用一次sysfs_create_file，节省操作。

向文件系统添加/删除组属性的内核函数如下：
```C
int __must_check sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
```

### kobject、ktype、kset 的关系

**kset和ktype：**
* kset可以把kobject集中到一个集合中
* ktype描述同类kobject所共有的特性
* 具有相同ktype的kobject可以被分组到不同的kset，即：Linux内核中，只有少数的ktype，
  但会有多个kset

kobject与ktype关联，ktype定义了一些kobject相关的默认属性：析构、sysfs行为以及一些
默认属性。kobject又归入了kset，kset主要是把kobject集合在一起。

在sysfs中，kobject将以独立的目录出现在文件系统中，相关的目录或者一个给定目录的
子目录，可能属于同一个kset。

## class

参考文章：
[linux设备模型之Class](https://blog.csdn.net/lizuobin2/article/details/51592253)
[Linux 内核：设备驱动模型（3）class与device](https://www.cnblogs.com/schips/p/linux_device_model_3.html)

设备驱动模型中，还有一个抽象概念叫做类（CLass），准确来说，叫做设备类。

设备类是一个设备的高层视图，是指提供的用户接口相似的一类设备的集合，它抽象出了
底层的实现细节，从而允许用户空间使用设备所提供的功能，而不用关心设备是如何连接
和工作的。可以理解为抽象出了一套通用的接口。常见的类设备有 Input、tty、usb、rtc等。

class 与 bus 类似，我们在设备总线驱动模型中创建设备时，要指定它所属的 Bus ，那么
在创建类设备的时候也需要指定它所从属的类，可以发现他与总线设备驱动模型很类似。

类成员通常由kernel代码所控制，而无需驱动的明确支持。但有些情况下驱动也需要直接处理类。

```C
struct class {
    // 设别类名称，会在“/sys/class/”目录下体现。实际使用的是内部kobj包含的动态创建的名称。
    const char        *name;

    // 该class的默认attribute，会在class注册到内核时，自动在“/sys/class/xxx_class”
    // 下创建对应的attribute文件。
    const struct attribute_group    **class_groups;
    // 该class下每个设备的attribute，会在设备注册到内核时，自动在该设备的sysfs目录
    // 下创建对应的attribute文件。
    const struct attribute_group    **dev_groups;

    // 设备发出uevent消息时添加环境变量用的
    // 在core.c中的dev_uevent()函数，其中就包含对设备所属bus或class中dev_uevent()
    // 方法的调用，只是bus结构中定义方法用的函数名是uevent。
    int (*dev_uevent)(const struct device *dev, struct kobj_uevent_env *env);
    // 返回设备节点的相对路径名，在core.c的device_get_devnode()中有调用到。
    char *(*devnode)(const struct device *dev, umode_t *mode);

    // 释放方法
    // 用于回收这个设备类本身自身的回调函数。
    void (*class_release)(const struct class *class);
    // 用于release class内设备的回调函数。在device_release接口中，会依次检查Device、
    // Device Type以及Device所在的class，是否注册release接口，如果有则调用相应的
    // release接口release设备指针。具体在drivers/base/core.c的device_release()函数
    // 中调用。
    void (*dev_release)(struct device *dev);

    int (*shutdown_pre)(struct device *dev);

    // 命名空间
    const struct kobj_ns_type_operations *ns_type;
    const void *(*namespace)(const struct device *dev);

    void (*get_ownership)(const struct device *dev, kuid_t *uid, kgid_t *gid);

    // 电源管理有关
    const struct dev_pm_ops *pm;
};
```

创建/删除一个class
```C
// drivers/base/class.c

// This is used to create a struct class pointer that can then be used
// in calls to device_create().

// owner：有的版本需要设置owner，一般设置为THIS_MODULE
// name: class的名字
// 创建class会在/sys/class生成对应的类文件夹，如果有依附该class创建的设备节点，
// 也会在该文件夹下存在到设备节点的软连接
// 内部会调用class_register
struct class *class_create(const char *name)

// 内部会调用class_unregister
void class_destroy(const struct class *cls)
```
其他class操作可以直接看内核源码


分析设备总线驱动模型的时候可以知道，将一个设备调用 device_add 函数注册到内核中
的时候，如果指定了设备号，那么用户空间的 mdev 会根据 sysfs 文件系统中的设备信息
去自动创建设备节点。观察 device_create 接口，参数里还有设备号，里边也间接调用了
device_add


## sysfs

sysfs 是非持久性的文件系统，它提供系统的全局视图，并通过它们的kobject显示内核对象
的层次结构。sysfs是为了替代/proc的存在。每个kobject作为目录和目录中的文件体现，
目录中的文件则代表相应的属性，可以读取或写入。

```
 $ tree -L 1 /sys
/sys
|-- block
|-- bus
|-- class
|-- dev
|-- devices
|-- firmware
|-- fs
|-- hypervisor
|-- kernel
|-- module
`-- power
```
顶层sysfs目录位于/sys目录下
* 系统上的每个块设备，block都包含一个目录，目录下包含设备上分区的子目录。
* bus包含系统上注册的总线。
* dev以原始的方式（无层次结构）包含已注册的设备节点，每个节点都是/sys/devices目录
  中真实设备的符号链接。
* /sys/devices给出系统内设备的拓扑结构视图。
* firmware显示系统相关的底层子系统树，如ACPI、EFI、OF(DT)。
* fs列出系统上实际使用的文件系统。
* kernel保存内核配置选项和状态信息。
* module是加载的模块列表

sysfs中每个目录都对应一个kobject，其中一些作为内核符号导出。
* kernel_kobj：对应于/sys/kernel
* power_kobj：对应于/sys/power
* firmware_kobj：对应于/sys/firmware，导出在drivers/base/firmware.c源文件中
* hypervisor_kobj：对应于/sys/hypervisor，导出在drivers/base/hypervisor.c中
* fs_kobj：对应于/sys/fs，导出在fs/namespace.c文件中

一些文件目录的创建说明：
* class/、dev/、devices/是在启动期间由内核源代码内drivers_base/core.c中的devices_init
  函数创建的
* block/在block/genhd.c中创建
* bus/在drivers/base/bus.c中被创建为kset


### 向sysfs中添加和删除kobject

已注册的kobject在sysfs中创建目录时，目录的位置取决于kobject的父项（也是kobject），
在父项下创建子目录。如果父指针为NULL，则将其添加为kset->kobj内的子目录。如果其父
和kset字段都未设置，它将映射到sysfs内的根目录（/sys）

可以使用sysfs_{create|remove}_link函数创建/删除现有对象（目录）的符号连接，
这可以使对象存在多个位置。create函数创建名为name的符号连接，指向target kobject的
sysfs项。当target kobject需要删除时，要记得删除相应的符号链接。

```C
// 添加kobject到sysfs中，同时根据其层次结构创建目录及其默认属性
// kobject_create 见kobject章节的描述
int kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
// 与其相反功能的函数如下，内部会调用 kobject_put 接口
void kobject_del(struct kobject *kobj)

// 内部调用 kobject_create 和 kobject_add
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)
```

### 向sysfs中添加文件

#### 默认属性（kobject）

文件的默认属性是通过kobject和kset中的ktype字段中的default_attrs字段提供的。因此，
所有具有相同类型的kobject在他们对应的sysfs目录下都拥有相同的默认文件集合。大多数
情况下，默认属性就足够了。但有时ktype实例可能需要自己的属性来提供所需的数据或功能。

添加/删除新属性（或一组属性）的底层函数如下：
```C
static inline int __must_check sysfs_create_file(struct kobject *kobj, const struct attribute *attr);
static inline void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

int __must_check sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
```

注意这里使用的`struct attribute *attr`应该作为一个父类来理解，至少会存在如下子类：
```C
// include/linux/kobject.h
struct kobj_attribute {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
            char *buf);
    ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
             const char *buf, size_t count);
};

// include/linux/device/bus.h
struct bus_attribute {
    struct attribute    attr;
    ssize_t (*show)(const struct bus_type *bus, char *buf);
    ssize_t (*store)(const struct bus_type *bus, const char *buf, size_t count);
};

// include/linux/device/driver.h
struct driver_attribute {
    struct attribute attr;
    ssize_t (*show)(struct device_driver *driver, char *buf);
    ssize_t (*store)(struct device_driver *driver, const char *buf,
             size_t count);
};

// include/linux/device.h
struct device_attribute {
    struct attribute    attr;
    ssize_t (*show)(struct device *dev, struct device_attribute *attr,
            char *buf);
    ssize_t (*store)(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count);
};
```

默认属性中使用的是kobject，因此这里声明attribute时建议通过__ATTR完成：
```C
#define __ATTR(_name, _mode, _show, _store) {                \
    .attr = {.name = __stringify(_name),                \
         .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },        \
    .show    = _show,                        \
    .store    = _store,                        \
}

ex:
static struct kobj_attribute m_attr = __ATTR("m_attr", 0766,
                             m_show, m_store);
```

kobj_type 中的sysfs_ops定义了用户读写属性时，内核实际调用的接口
```C
struct sysfs_ops {
    // 读 sysfs 文件时，该方法被调用，拷贝attr提供的属性值到buffer指定的缓冲区中，
    // 缓冲区的大小为PAGE_SIZE字节，在x86体系中PAGE_SIZE为4096字节，该函数如果执行
    // 成功，则返回实际写入buffer的字节数，如果失败，则返回负的错误码
    ssize_t    (*show)(struct kobject *, struct attribute *, char *);
    // 写 sysfs 文件时，该方法被调用，从buffer中读取size大小的字节，并将其存放到
    // attr表示的属性结构体变量中。缓冲区的大小总是PAGE_SIZE或者更小些，该函数如果
    // 执行成功，则将返回实际从buffer中读取的字节数，如果失败则返回负数的错误码
    ssize_t    (*store)(struct kobject *, struct attribute *, const char *, size_t);
};
```
由于这组函数必须对所有的属性都进行文件I/O请求处理，所以他们通常需要维护某些通用
映射来调用每个属性所特有的处理函数

在调用show/store函数时，通常通过attribute找其父类，例如：在默认属性中，通过
attribute，找到kobj_attribute，然后执行kobj_attribute中的/shwo/store函数
```C
kattr = container_of(attr, struct kobj_attribute, attr);
```


#### 创建新属性

通常kobject相关联的ktype所提供的默认属性是足够的，但有些特别的情况下，kobject希望
有自己的属性，为此，内核提供了添加新属性的接口：
```C
static inline int __must_check sysfs_create_file(struct kobject *kobj,
                         const struct attribute *attr)
static inline void sysfs_remove_file(struct kobject *kobj,
                     const struct attribute *attr)
```
如果成功返回0，如果失败返回错误码

kobject关联的ktype所对应的sysfs_ops需要负责处理新属性，现有的show()和store()方法
必须能够处理新属性

#### 其他属性

在sysfs文件系统中，除了创建自己的ktype或kobject来添加属性外，还可以使用当前存在的
设备、驱动、总线、和类属性

**设备属性**

数据结构：
```C
struct device_attribute {
    struct attribute    attr;
    ssize_t (*show)(struct device *dev, struct device_attribute *attr,
            char *buf);
    ssize_t (*store)(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count);
};
```
它是对标准 struct attribute 的封装，以及一组用于显示/存储属性值的回调

声明通过DEVICE_ATTR宏完成：
```C
#define DEVICE_ATTR(_name, _mode, _show, _store) \
    struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)
```
使用DEVICE_ATTR声明属性时，都会在属性名称上添加前缀dev_attr_

可以使用如下函数添加/删除：
```C
int device_create_file(struct device *device, const struct device_attribute *entry);
void device_remove_file(struct device *dev, const struct device_attribute *attr);
```

**总线属性**

数据结构：
```C
struct bus_attribute {
    struct attribute    attr;
    ssize_t (*show)(const struct bus_type *bus, char *buf);
    ssize_t (*store)(const struct bus_type *bus, const char *buf, size_t count);
};

```

总线属性使用宏来声明：
```C
#define BUS_ATTR_RW(_name) \
    struct bus_attribute bus_attr_##_name = __ATTR_RW(_name)
#define BUS_ATTR_RO(_name) \
    struct bus_attribute bus_attr_##_name = __ATTR_RO(_name)
#define BUS_ATTR_WO(_name) \
    struct bus_attribute bus_attr_##_name = __ATTR_WO(_name)

```
所有使用宏声明的总线属性都会将前缀bus_attr_添加到属性变量名称

可以使用如下函数添加/删除：
```C
int bus_create_file(const struct bus_type *bus, struct bus_attribute *attr)
void bus_remove_file(const struct bus_type *bus, struct bus_attribute *attr)
```

**驱动属性**

数据结构：
```C
struct driver_attribute {
    struct attribute attr;
    ssize_t (*show)(struct device_driver *driver, char *buf);
    ssize_t (*store)(struct device_driver *driver, const char *buf,
             size_t count);
};
```

总线属性使用宏来声明：
```C
#define DRIVER_ATTR_RW(_name) \
    struct driver_attribute driver_attr_##_name = __ATTR_RW(_name)
#define DRIVER_ATTR_RO(_name) \
    struct driver_attribute driver_attr_##_name = __ATTR_RO(_name)
#define DRIVER_ATTR_WO(_name) \
    struct driver_attribute driver_attr_##_name = __ATTR_WO(_name)
```
所有使用宏声明的总线属性都会将前缀driver_attr_添加到属性变量名称

可以使用如下函数添加/删除：
```C
int __must_check driver_create_file(struct device_driver *driver,
                    const struct driver_attribute *attr);
void driver_remove_file(struct device_driver *driver,
            const struct driver_attribute *attr);
```

**类属性**

数据结构：
```C
struct class_attribute {
    struct attribute attr;
    ssize_t (*show)(const struct class *class, const struct class_attribute *attr,
            char *buf);
    ssize_t (*store)(const struct class *class, const struct class_attribute *attr,
             const char *buf, size_t count);
};
```

总线属性使用宏来声明：
```C
#define CLASS_ATTR_RW(_name) \
    struct class_attribute class_attr_##_name = __ATTR_RW(_name)
#define CLASS_ATTR_RO(_name) \
    struct class_attribute class_attr_##_name = __ATTR_RO(_name)
#define CLASS_ATTR_WO(_name) \
    struct class_attribute class_attr_##_name = __ATTR_WO(_name)
```
所有使用宏声明的总线属性都会将前缀class_attr_添加到属性变量名称

可以使用如下函数添加/删除：
```C
static inline int __must_check class_create_file(const struct class *class,
                         const struct class_attribute *attr)
static inline void class_remove_file(const struct class *class,
                     const struct class_attribute *attr)
```

## proc

待完善
