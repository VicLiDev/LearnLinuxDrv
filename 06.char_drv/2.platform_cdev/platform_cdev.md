# platform device

一些常见的总线，例如：USB、I2S、I2C、UART、SPI、PCI、STAT等。这种总线是被称为
控制器的硬件设备，由于他们是SOC的一部分，因此无法删除，不可发现，称为平台设备。

人们常说的平台设备是片上设备（嵌入在SOC中）。但实际上，链接到片上总线的外设，
如果是不可被发现的，也被称为平台设备。同样也可能有片上PCI或者USB设备，如果是
可发现的，那它们就不是平台设备。

通常把在SoC内部继承的I2C、RTC、LCD、看门狗等控制器都归纳为paltform_device，
而他们本身就是字符设备。

从SOC的角度看，这些平台设备通过专用总线链接，而且大部分是芯片制造商专有的。
从内核的角度看，这些设备是不可发现的，没有连接到系统中的。这时就需要伪平台总线。

伪平台总线也称作平台总线，是内核虚拟总线，用于不再内核所知物理总线上的设备。

## 重要的数据结构

### 设备 platform_device

```c
// include/linux/platform_device.h
struct platform_device {
    const char    *name;
    int        id;
    bool        id_auto;
    struct device    dev;
    u32        num_resources;
    struct resource    *resource;

    const struct platform_device_id    *id_entry;
    char *driver_override; /* Driver name to force a match */

    /* MFD cell pointer */
    struct mfd_cell *mfd_cell;

    /* arch specific additions */
    struct pdev_archdata    archdata;
};
```

```c
// include/linux/device.h
struct device {
    struct device        *parent;

    struct device_private    *p;

    struct kobject kobj;
    const char        *init_name; /* initial name of the device */
    const struct device_type *type;

    struct mutex        mutex;    /* mutex to synchronize calls to
                     * its driver.
                     */

    struct bus_type    *bus;        /* type of bus device is on */
    struct device_driver *driver;    /* which driver has allocated this
                       device */
    void        *platform_data;    /* Platform specific data, device
                       core doesn't touch it */
    void        *driver_data;    /* Driver data, set and get with
                       dev_set/get_drvdata */
    struct dev_links_info    links;
    struct dev_pm_info    power;
    struct dev_pm_domain    *pm_domain;

#ifdef CONFIG_GENERIC_MSI_IRQ_DOMAIN
    struct irq_domain    *msi_domain;
#endif
#ifdef CONFIG_PINCTRL
    struct dev_pin_info    *pins;
#endif
#ifdef CONFIG_GENERIC_MSI_IRQ
    struct list_head    msi_list;
#endif

#ifdef CONFIG_NUMA
    int        numa_node;    /* NUMA node this device is close to */
#endif
    const struct dma_map_ops *dma_ops;
    u64        *dma_mask;    /* dma mask (if dma'able device) */
    u64        coherent_dma_mask;/* Like dma_mask, but for
                         alloc_coherent mappings as
                         not all hardware supports
                         64 bit addresses for consistent
                         allocations such descriptors. */
    u64        bus_dma_mask;    /* upstream dma_mask constraint */
    unsigned long    dma_pfn_offset;

    struct device_dma_parameters *dma_parms;

    struct list_head    dma_pools;    /* dma pools (if dma'ble) */

    struct dma_coherent_mem    *dma_mem; /* internal for coherent mem
                         override */
#ifdef CONFIG_DMA_CMA
    struct cma *cma_area;        /* contiguous memory area for dma
                       allocations */
#endif
    struct removed_region *removed_mem;
    /* arch specific additions */
    struct dev_archdata    archdata;

    struct device_node    *of_node; /* associated device tree node */
    struct fwnode_handle    *fwnode; /* firmware device node */

    dev_t            devt;    /* dev_t, creates the sysfs "dev" */
    u32            id;    /* device instance */

    spinlock_t        devres_lock;
    struct list_head    devres_head;

    struct klist_node    knode_class;
    struct class        *class;
    const struct attribute_group **groups;    /* optional groups */

    void    (*release)(struct device *dev);
    struct iommu_group    *iommu_group;
    struct iommu_fwspec    *iommu_fwspec;

    bool            offline_disabled:1;
    bool            offline:1;
    bool            of_node_reused:1;
    bool            state_synced:1;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
    ANDROID_KABI_RESERVE(5);
    ANDROID_KABI_RESERVE(6);
    ANDROID_KABI_RESERVE(7);
    ANDROID_KABI_RESERVE(8);
};
```

### 驱动 platform_driver

```c
// include/linux/platform_device.h
struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int (*suspend)(struct platform_device *, pm_message_t state);
    int (*resume)(struct platform_device *);
    struct device_driver driver;
    const struct platform_device_id *id_table;
    bool prevent_deferred_probe;
};
```

```c
// include/linux/device/driver.h
struct device_driver {
    const char        *name;
    struct bus_type        *bus;

    struct module        *owner;
    const char        *mod_name;    /* used for built-in modules */

    bool suppress_bind_attrs;    /* disables bind/unbind via sysfs */
    enum probe_type probe_type;

    const struct of_device_id    *of_match_table;
    const struct acpi_device_id    *acpi_match_table;

    int (*probe) (struct device *dev);
    void (*sync_state)(struct device *dev);
    int (*remove) (struct device *dev);
    void (*shutdown) (struct device *dev);
    int (*suspend) (struct device *dev, pm_message_t state);
    int (*resume) (struct device *dev);
    const struct attribute_group **groups;

    const struct dev_pm_ops *pm;
    void (*coredump) (struct device *dev);

    struct driver_private *p;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
    ANDROID_KABI_RESERVE(3);
    ANDROID_KABI_RESERVE(4);
};
```

### 总线 bus_type

作为 device_driver 的成员

```c
// include/linux/device/bus.h
struct bus_type {
    const char        *name;
    const char        *dev_name;
    struct device        *dev_root;
    const struct attribute_group **bus_groups;
    const struct attribute_group **dev_groups;
    const struct attribute_group **drv_groups;

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct device *dev);
    void (*sync_state)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    int (*online)(struct device *dev);
    int (*offline)(struct device *dev);

    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    int (*num_vf)(struct device *dev);

    int (*dma_configure)(struct device *dev);

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

## 驱动注册和注销

驱动注册和注销的过程类似cdev的 module_init 和 module_exit 上挂的函数，只不过这里
挂的是 platform_driver->probe 和 platform_driver->remove

**定义注册用的结构体**
```c
static struct platform_driver m_driver_demo = {
    .probe = m_drv_probe,
    .remove = m_drv_remove,
    .driver = {
        .name = "m_plt_drv_demom",
        .of_match_table = of_match_ptr(m_dt_ids),
    },
};
```

驱动注册/注销
```c
 // 注册： 调用 platform_driver->probe ,类似 module_init
#define platform_driver_register(drv) \
    __platform_driver_register(drv, THIS_MODULE)
extern int __platform_driver_register(struct platform_driver *,    struct module *);

// 注销： 调用 platform_driver->remove ,类似 module_exit
extern void platform_driver_unregister(struct platform_driver *)
```

也可以使用`platform_driver_probe`接口，其内部会调用`platform_driver_register`
`platform_driver_probe`和`platform_driver_register`比较：
* `platform_driver_probe`
  * 优点：放在__init段，probe之后释放内存
  * 缺点：只在注册的时候probe一次，需要100%保证设备在系统中，不然可能会失败
* `platform_driver_register`
  * 优点：常驻系统内存，可以多次probe
  * 缺点：会一直占用内存

如果init/exit函数只是向平台总线核心注册/取消注册，未做其他任何操作，在这种情况下
可以去掉module_init和module_exit，使用module_platform_driver，这个宏将模块注册到
平台驱动程序核心。不需要module_init/module_exit，也不需要init和exit函数，
也可以用同时包含注册和注销的接口
```c
module_platform_driver(__platform_driver)
        --> extern int __platform_driver_register(struct platform_driver *, struct module *)       // module_init()
                --> int driver_register(struct device_driver *drv)
        --> void platform_driver_unregister(struct platform_driver *drv)      // module_exit()
                --> void driver_unregister(struct device_driver *drv)
```

注册完`platform_driver <driverModuleName>`之后可以发现路径/sys/bus/platform/drivers
目录下多出了一个名字叫`<driverModuleName>`的子目录。

**注意：**
module_platform_driver()宏所定义的模块加载和卸载函数只是通过
platform_driver_register()、platform_driver_unregister()函数进行platform_driver
的注册与注销，<font color="red">而注册和注销驱动之外的工作需要移到
platform_driver的probe()和remove()成员函数中。</font>

<font color="red">驱动注册成功之后，会在目录 /sys/bus/platform/drivers/ 
下看到对应名字的驱动</font>

每个总线都有特定的宏来注册驱动程序，以下列表是其中一部分：
```C
#define module_platform_driver(__platform_driver)  // 用于平台驱动程序，专用于传统物理总线以外的设备
#define module_spi_driver(__spi_driver) // 用于SPI驱动程序
#define module_i2c_driver(__i2c_driver) // 用于I2C驱动程序
#define module_pci_driver(__pci_driver) // 用于PCI驱动程序
#define module_usb_driver(__usb_driver) // 用于USB驱动程序
...
```

其他函数
```c
// 将入参数据 data 放到 platform_device->dev->driver_data 中
// 注意这里使用的是device，不是注册时使用的driver
static inline void platform_set_drvdata(struct platform_device *pdev,    void *data)
```


## 设备注册到内核

老的设备注册方法需要到处修改文件以定义一个新的设备或者调用platform_add_devices，
现在通常采用直接在设备树中定义的方法，在展开设备树时会自动生成并注册相应的设备。

对于paltform_device的定义通常在BSP板文件中实现，在板文件中，将platform_device归纳
为一个数组，最终通过platform_add_devices()函数统一注册。platform_add_devices()函数
可以将平台设备添加到系统中，该函数的内部调用了platform_device_register()函数以
注册单个的平台设备，这个函数原型为：
```c
// devs: 平台设备数组的指针
// num: 平台设备的数量
int platform_add_devices(struct platform_device **devs, int num)
```
<font color=red>Linux 3.x 之后，ARM Linux不太喜欢人们以编码的形式去填写
platform_device和注册，而倾向于根据设备树中的内容自动展开platform_device。
设备树展开相关的介绍可参阅本项目设备树相关的内容。</font>

<font color="red">设备注册成功之后，可以在 /sys/devices/platform 下看到对应名字
的设备</font>

<font color="red">**注意：平台设备不会在 /dev 目录中创建节点，它只是提供一种伪平台
的设备驱动匹配机制，具体的驱动实现一般是在驱动注册过程中创建需要的节点，完成相应
的驱动**</font>

**老的注册设备的方法**

注册完名为`<driverModuleName>`的platform_driver之后，需要在板文件
`arch/arm/mach-<soc>/mach-<>.c`中添加相应的代码，例如：

```c
static struct platform_device globalfifo_device = {
    .name = "globalfifo",
    .id   = -1,
};
```

并最终通过类似platform_add_devices()的函数把这个platform_device注册进系统。
如果一切顺利，可以在/sysdevices/platform目录下看到一个名字叫globalfifo的子目录，
/sys/devices/platform/globalfifo中会有一个driver文件，它是指向/sys/bus/platform/
drivers/globalfifo的符号链接，这就证明驱动和设备匹配上了。



## 设备和总线的绑定

### 绑定处理

在匹配发生之前，Linux会调用platform_match接口。平台设备通过字符串与驱动程序匹配。
根据Linux设备模型，总线是最重要的部分。每个总线都维护一个注册的驱动程序和设备的
列表。总线驱动程序负责设备和驱动的匹配，每当添加新的设备或者向总线添加新的驱动
程序时，总线都会启动匹配循环。

举例：假设使用I2C核心提供的函数注册新的I2C设备。内核完成以下动作：
1. 调用由I2C总线驱动程序注册的I2C核心匹配函数进行设备和驱动的匹配
2. 如果没有匹配成功无事发生
3. 如果匹配成功，内核将通知（通过netlinke套接字通信机制）设备管理器（udev/mdev），
   由他加载（如果尚未加载）与设备匹配的驱动程序。一旦驱动程序加载完成，其probe函数
   就会被立即调用执行。

* 驱动侧 platform_driver、device_driver、bus_type 的关系：
  `platform_driver.device_driver->bus_type`
* 设备侧 platform_device、device、bus_type 的关系：
  `platform_device.device->bus_type`

驱动在注册到内核时，通过接口`module_platform_driver`向下搜索可以发现注册最终发生
在接口`__platform_driver_register`中，在该接口中会将结构体`platform_bus_type`赋
给驱动的`bus`，在`platform_bus_type`中保存着设备与驱动的匹配接口，设备与驱动的
匹配通过`bus_type`结构体中的`match`接口`paltform_match`完成。

设备在注册到内核时，通过接口`platform_add_devices`完成，通过接口
`platform_add_devices`向下搜索可以发现最终在接口`platform_device_add`中，将结构体
`platform_bus_type`赋值给设备结构体。如果是从设备树解析得到`paltfrom`设备的话会
调用`of_platform_device_create_pdata`接口生成`paltform`设备，同样会将
`platform_bus_type`赋值给设备结构体。

综上，驱动和设备使用的是同一个`bus_type`结构体，或者说，所有的`platform`驱动和
设备都是使用结构体`platform_bus_type`。

根据设备模型中总线、驱动、设备分离的思想，向系统中注册驱动/设备时，会寻找匹配的
设备/驱动，如果找到匹配的设备/驱动，就会调用相应的match接口，进行匹配。

### 设备与驱动的匹配函数

platform_bus_type 中注册的接口 platform_match 负责完成总线和设备的匹配：
```c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    /* When driver_override is set, only bind to the matching driver */
    if (pdev->driver_override)
        return !strcmp(pdev->driver_override, drv->name);

    /* Attempt an OF style match first */
    if (of_driver_match_device(dev, drv))
        return 1;

    /* Then try ACPI style match */
    if (acpi_driver_match_device(dev, drv))
        return 1;

    /* Then try to match against the id table */
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    /* fall-back to driver name match */
    return (strcmp(pdev->name, drv->name) == 0);
}
```

从以上代码中可以看出，匹配 paltform_device 和 paltform_driver 有4种可能性：

1. 基于设备树风格的匹配
2. 基于ACPI风格的匹配
3. 匹配ID表（即paltform_drivce设备名是否出现在platform_driver的ID表内）
4. 匹配platform_device设备名和驱动的名字

以上四种方式都是基于字符串比较


#### 基于设备树风格的匹配

OF（Open Firmware）匹配是平台核心执行的第一种匹配机制，它使用设备树的 compatible
属性来匹配 of_match_table 中的设备项。每个设备节点都有compatible属性，他是字符串
或字符串列表。驱动程序只要声明compatible属性中列出的字符串之一，就可以触发匹配。

of_match_table 在驱动中的位置：platform_driver.device_driver.of_match_table。驱动
会被注册到系统中。

of_match_table 是 of_device_id  类型的列表，of_device_id的定义如下：
```C
struct of_device_id {
    char    name[32];
    char    type[32];
    // 用来匹配DT中设备节点兼容属性的字符串
    char    compatible[128];
    // 可以指向任何结构，这个结构可以用作每个设备类型的配置数据
    const void *data;
};
```

#### 基于ACPI风格的匹配

使用 device_driver->acpi_device_id 进行匹配，不做详细介绍

#### 匹配ID表

使用 xxx_driver->id_table[].name 进行匹配，所有设备ID结构都在
include/linux/mod_devicetable.h 中定义。如果要找到正确的结构名称，需要在device_id
前加上设备驱动程序所在的总线名称，例如：对于I2C，结构名称是 struct i2c_device_id，
平台设备是 struct platform_device_id(不在实际物理总线上)。

xxx_device_id在使用时会被注册到xxx_driver的id_table列表中，例如：使用i2c_device_id
会被放到i2c_driver驱动结构体的id_table列表中，然后调用对应的驱动注册接口，完成
驱动的注册。

与匹配ID表对应的name也需要在设备中指定，这样在进行匹配时才能够正确匹配到，匹配
之后会将对应的xxx_device_id 放到 设备的 id_entry 中，例如：platform_device->id_entry

每个设备也可以使用特定的ID表来匹配数据，这里不做详细介绍，可以查阅：
Linux设备驱动开发（法，约翰·马迪厄）

#### 匹配platform_device设备名和驱动的名字

现在大多数平台驱动程序不提供任何ID表，他们只在驱动程序名称字段填写驱动程序本身
的名称。但是匹配仍然可行，观察platform_match函数就会发现匹配最后会回到名字匹配，
比较驱动程序和设备的名字。


## platform设备资源和数据

```c
struct resource{
    resource__size_t start;
    resource_size_t end;
    const char *name;
    unsigned long flags;
    struct resource *parent, *sibling, *child;
};
```

通常关系start、end和flags这三个字段，他们分别标明了资源的开始值、结束值和类型，flags可以为IORESOURCE_IO、IORESOURCE_MEM、IORESOURCE_IRQ、IORESOURCE_DMA等。start、end的含义会随着flags而变更，如果当flags为IORESOURCE_MEM时，start、end分别表示该platform_device占据的内存的开始地址和结束地址；当flags为IORESOURCE_IRQ时，start、end分别表示该platform_device使用的中断号的开始值和结束值，如果只使用了一个中断号，开始和结束值相同。对于同种类型的资源而言，可以有多分，例如某设备占据了两个内存区域，则可以定义两个IORESOURCE_MEM资源。

对于resource的定义也通常在BSP的板文件中进行，而在具体的设备驱动中通过platform_get_resource()这样的api来获取，此api原型为：

```c
struct resource *platform_get_resource(struct platform_device *,unsigned int, unsigned int);
```

对于IRQ而言，platform_getresource()还有一个进行了封装的变体platform_get_irq()，其原型为：

```c
int platform_get_irq(struct platform_device *dev, unsigned int num);
```

它实际上调用了"platform_getresource(dev, IORESOURCE_IRO, num);"。



设备除了可以在BSP中定义资源以外，还可以附加一些数据信息，这些配置信息也依赖于板，不宜直接放置在设备驱动上，因此platform也提供了platform_data的支持，platform_data的形式是由每个驱动自定义的，如对于DM9000网卡而言，paltform_data为一个dm9000_plat_data结构体，该结构体存放在 platform_device.dev.platform_data 字段下。

通过如下方式可以拿到paltform_data：

```c
static inline void *dev_get_platdata(const struct device *dev) 
```

其中pdev为platfrom_device 的指针。



引入platform的概念有如下优点：

1. 使得设备被挂接在一个总线上，符合Linux 2.6以后内核的设备模型。其结果是使配套的sysfs节点、设备电源管理都成为可能。
2. 隔离BSP和驱动。在BSP中定义platform设备和设备使用的资源、设备的具体配置信息，而在驱动中只需要通过通用api去获取资源和数据，做到了板相关代码和驱动代码的分离，使得驱动具有更好的可扩展性和跨平台性。
3. 让一个驱动支持多个设备。例如DM9000的驱动只有一份，但是我们可以在板级添加多份DM9000的platform_device，他们都可以与唯一的驱动匹配。

在Linux 3.x之后的内核中，DM9000驱动实际上已经可以通过设备树的方法被枚举。


## misc device

### 介绍

misc device中文翻译称为“杂项设备”，杂项设备本质就是字符设备。嵌入式硬件上存在
各类设备，如ADC、DAC、按键、蜂鸣器等，一方面不便于单独分类，另一方面驱动设备号
分配有限，因此Linux系统引入“杂项设备”，驱动工程师将这些没有明显区分的设备统一
归类为杂项设备。

杂项设备不是新的设备类型，只是是字符设备的一类，是最简单的字符设备。同时，关于
设备归类也是由驱动工程决策，并无严格规定。比如，一个ADC驱动，可以归类为普通字符
设备，也可以归类为杂项设备。

#### 杂项设备特点

杂项设备的主设备号（MISC_MAJOR）为10，通过次设备号区分不同杂项设备。杂项设备注册
时，所有的杂项设备以链表形式存在，系统应用访问设备时，内核会根据次设备号匹配到
对应的杂项设备，然后调用“file_operations”结构中注册的文件操作接口（open/read/ioctl等）
进行操作。

* 杂项设备是字符设备的一个子类，是最简单的字符设备。
* 杂项设备的主设备号（MISC_MAJOR）为10。
* Linux内核提供杂项设备注册、释放框架，简化字符设备注册过程。

#### 杂项设备优点

* 便于实现，简化设备驱动实现过程。
* 整个系统更规范化，多样化设备统一由内核杂项设备管理。
* 节约系统资源，如主设备号。


### 杂项设备分析

reference:
[【Linux驱动编程】Linux字符驱动之misc device](https://blog.csdn.net/qq_20553613/article/details/103285204)

#### 抽象

杂项设备将字符设备进一步封装一层，有专门的驱动框架，对于驱动工程师来说可以简化
字符设备注册过程。
```C
// include/linux/miscdevice.h
struct miscdevice  {
    // 次设备号。
    int minor;
    // 设备名称，“/dev”目录下显示。
    const char *name;
    // 设备文件操作接口，open/read/write等。
    const struct file_operations *fops;
    // misc设备链表节点
    struct list_head list;
    // 当前设备父设备，一般为NULL。
    struct device *parent;
    // 当前设备，即是linux基本设备驱动框架。
    struct device *this_device;
    const struct attribute_group **groups;
    const char *nodename;
    umode_t mode;
};
```

编写杂项设备时，我们主要实现前三个变量。杂项设备的主设备号是10，次设备号需程序员
指定，用以区分不同杂项设备。Linux内核在“include/linux/miscdevice.h”预定义了部分次
设备号。编写程序时，可以使用预定义的次设备号，也可以自定义次设备号，前提该次设备号
没有被其他杂项设备使用，否则设备会注册失败。当分配的次设备号为255（MISC_DYNAMIC_MINOR），
则表示由内核自动分配次设备号。

设备名称，设备注册成功后，在“/dev”目录生成一个该名称的设备文件。应用层调用杂项
设备，最终是调驱动程序注册的fops文件操作集合。


#### 注册

传统字符设备注册过程步骤一般为：
```C
alloc_chrdev_region();    /* 申请设备号 */
cdev_init();              /* 初始化字符设备 */
cdev_add();               /* 添加字符设备 */
class_create();           /* 创建类 */
device_create();          /* 创建基本驱动设备 */
```

Linux把杂项设备注册封装为一个接口函数“misc_register”，简化了传统字符设备注册
过程，注册成功返回0，失败返回负数。关于注册函数原型和分析，其实也是创建传统
字符设备的步骤过程。

#### 注销

与注册过程一样，杂项设备注销也只需要执行一个函数即可。传统字符串设备注销过程步骤
一般为：
```C
device_destory();              /* 删除基本驱动设备*/
class_destory();               /* 删除类 */
cdev_del();                    /* 删除字符设备 */
unregister_chrdev_region();    /* 注销设备号 */
```

杂项设备注销函原型和注释，注销成功返回0，失败返回负数：`misc_deregister`


## 6. demo

关注 globalfifo/ch12/globalfifo.c ，来自宋宝华的《Linux设备驱动开发详解》中的demo
