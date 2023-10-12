参考博客：
[module_init()加载设备驱动](https://blog.csdn.net/anglexuchao/article/details/80391928?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-1-80391928-blog-108724568.pc_relevant_sortByAnswer&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-1-80391928-blog-108724568.pc_relevant_sortByAnswer&utm_relevant_index=1)
[Linux内核大法之模块化机制module_init](https://blog.csdn.net/weixin_41884251/article/details/108724568)

# 重要数据
## device
```c
// include/linux/device.h
struct device {
    struct kobject kobj;
    struct device        *parent;

    struct device_private    *p;

    const char        *init_name; /* initial name of the device */
    const struct device_type *type;

    struct bus_type    *bus;        /* type of bus device is on */
    struct device_driver *driver;    /* which driver has allocated this
                       device */
    void        *platform_data;    /* Platform specific data, device
                       core doesn't touch it */
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

    struct device_node    *of_node; /* associated device tree node */
    struct fwnode_handle    *fwnode; /* firmware device node */

#ifdef CONFIG_NUMA
    int        numa_node;    /* NUMA node this device is close to */
#endif
    dev_t            devt;    /* dev_t, creates the sysfs "dev" */
    u32            id;    /* device instance */

    spinlock_t        devres_lock;
    struct list_head    devres_head;

    struct class        *class;
    const struct attribute_group **groups;    /* optional groups */

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
## driver
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
    const struct attribute_group **dev_groups;

    const struct dev_pm_ops *pm;
    void (*coredump) (struct device *dev);

    struct driver_private *p;
};
```
## bus
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

# module_init 实现原理

module_init()定义在include/linux/module.h中，其中有两部分定义，即设备驱动的两种加载方式：一种是编译进内核，一种是以模块的方式加载。\#ifndef MODULE 表明当设备驱动编译进内核时。
```c
#ifndef MODULE
/**
 * module_init() - driver initialization entry point
 * @x: function to be run at kernel boot time or module insertion
 *
 * module_init() will either be called during do_initcalls() (if
 * builtin) or at module insertion time (if a module).  There can only
 * be one per module.
 */
#define module_init(x)    __initcall(x);

/**
 * module_exit() - driver exit entry point
 * @x: function to be run when driver is removed
 *
 * module_exit() will wrap the driver clean-up code
 * with cleanup_module() when used with rmmod when
 * the driver is a module.  If the driver is statically
 * compiled into the kernel, module_exit() has no effect.
 * There can only be one per module.
 */
#define module_exit(x)    __exitcall(x);

#else /* MODULE */

/*
 * In most cases loadable modules do not need custom
 * initcall levels. There are still some valid cases where
 * a driver may be needed early if built in, and does not
 * matter when built as a loadable module. Like bus
 * snooping debug drivers.
 */
#define early_initcall(fn)        module_init(fn)
#define core_initcall(fn)        module_init(fn)
#define core_initcall_sync(fn)        module_init(fn)
#define postcore_initcall(fn)        module_init(fn)
#define postcore_initcall_sync(fn)    module_init(fn)
#define arch_initcall(fn)        module_init(fn)
#define subsys_initcall(fn)        module_init(fn)
#define subsys_initcall_sync(fn)    module_init(fn)
#define fs_initcall(fn)            module_init(fn)
#define fs_initcall_sync(fn)        module_init(fn)
#define rootfs_initcall(fn)        module_init(fn)
#define device_initcall(fn)        module_init(fn)
#define device_initcall_sync(fn)    module_init(fn)
#define late_initcall(fn)        module_init(fn)
#define late_initcall_sync(fn)        module_init(fn)

#define console_initcall(fn)        module_init(fn)

/* Each module must use one module_init(). */
#define module_init(initfn)                    \
    static inline initcall_t __maybe_unused __inittest(void)        \
    { return initfn; }                    \
    int init_module(void) __copy(initfn)            \
        __attribute__((alias(#initfn)));        \
    __CFI_ADDRESSABLE(init_module, __initdata);

/* This is only required if you want to be unloadable. */
#define module_exit(exitfn)                    \
    static inline exitcall_t __maybe_unused __exittest(void)        \
    { return exitfn; }                    \
    void cleanup_module(void) __copy(exitfn)        \
        __attribute__((alias(#exitfn)));        \
    __CFI_ADDRESSABLE(cleanup_module, __exitdata);

#endif
```

## 编译进 kernel 的驱动加载

### kernel C语言跟踪
```c
module_init()    // include/linux/module.h
    --> __initcall(fn)   // include/linux/init.h
      --> device_initcall(fn)  // include/linux/init.h
        --> __define_initcall(fn, id)   // #define device_initcall(fn)     __define_initcall(fn, 6)
          --> ___define_initcall(fn, id, .initcall##id)
            --> __unique_initcall(fn, id, __sec, __initcall_id(fn))
              -->     ____define_initcall(fn,                    \
                      __initcall_stub(fn, __iid, id),            \
                      __initcall_name(initcall, __iid, id),        \
                      __initcall_section(__sec, __iid))
                --> static initcall_t __name __used             \
                      __attribute__((__section__(__sec))) = fn;

根据前边传递的参数，可以看到在最后的 __attribute__ 部分为： __attribute__((__section__(.initcall6))) = fn;
__section__ 的作用是将一个函数或者变量放到指定的段，这里即放到 .initcall6 段

至此 module_init() 的 C 语言跟踪结束，接下来需要分析 kernel 编译过程的处理
```
### kernel 链接脚本跟踪
这里以  arch/arm64/kernel/vmlinux.lds.S 为例

- 关于链接脚本：

链接脚本的主要目的是描述输入文件的section如何映射到输出文件，以及如何控制输出文件的内存布局。除此之外，大多数链接脚本不做什么其它的事。然而如果需要，链接脚本也可以指导链接器执行其它许多操作。

链接器总是使用链接脚本，如果没有提供，它使用缺省脚本，使用--verbose命令行选项，可以显示缺省的链接脚本。一些命令如-r或者-N，会影响缺省链接脚本。

可以使用-T选项指定自己的链接脚本，这么做时，你的脚本完全替换缺省链接脚本。

```c
脚本细节待完善，这里暂时先明确最终函数接口放到了 .initcall6 段即可

```

### kernel启动时加载驱动
```c
汇编结束之后，start_kernel 是第一个调用的C语言接口
start_kernel  // init/main.c
  --> arch_call_rest_init() // init/main.c
    --> rest_init()  // init/main.c
      --> user_mode_thread
        --> kernel_init  // init/main.c
          --> kernel_init_freeable  // init/main.c
            --> do_basic_setup  // init/main.c
              --> do_initcalls // init/main.c
                --> do_initcall_level // init/main.c
                  --> do_one_initcall // init/main.c

在接口 do_initcalls 中会计算 initcall_levels 的大小，并循环调用 do_initcall_level。
initcall_levels 的定义就是本.c文件中，并且定义为 __initdata，即放置在 init.data 段中。

#define __initdata  __section(".init.data") // include/linux/init.h

typedef int (*initcall_t)(void);  // include/linux/init.h
typedef initcall_t initcall_entry_t;  // include/linux/init.h

static initcall_entry_t *initcall_levels[] __initdata = {
    __initcall0_start,
    __initcall1_start,
    __initcall2_start,
    __initcall3_start,
    __initcall4_start,
    __initcall5_start,
    __initcall6_start,
    __initcall7_start,
    __initcall_end,
};

1. initcall_levels 定义为指针数组，即数组中每个元素都是 initcall_entry_t 类型的指针
2. 在同文件可以发现 extern initcall_entry_t __initcallx_start[]; 
   即 initcall_levels 中每个 __initcallx_start 指针指向一个 initcall_entry_t 数组

可以发现 do_initcalls、do_initcall_level、do_one_initcall 与 initcall_levels、__initcallx_start、init_func 一一对应，其中 init_func 是驱动的 init 函数。

最后在 do_one_initcall 函数中，可以看到对驱动 init 接口的调用

```

## 编译成 模块 的驱动加载

这里只做简单记录，不做细节挖掘
```c
#define SYSCALL_METADATA(sname, nb, ...)

#define SYSCALL_DEFINEx(x, sname, ...)                \
    SYSCALL_METADATA(sname, x, __VA_ARGS__)            \
    __SYSCALL_DEFINEx(x, sname, __VA_ARGS__)

#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)  // include/linux/syscalls.h
      
--> SYSCALL_DEFINE3(init_module, void __user *, umod, unsigned long, len, const char __user *, uargs)
    // sys_init_module
  --> load_module
    --> do_init_module  // // kernel/module/main.c
      --> do_one_initcall  // init/main.c

insmod 会调用 sys_init_module
```

# 字符设备

创建设备节点的两种方法：

mknod

device_create
