# 内存管理

## 页

内核把物理页作为内存管理的基本单位。体系结构不同，支持的页大小也不尽相同，还有些
体系结构甚至支持几种不同大小的页。大多数32位体系结构支持4KB的页，而64位体系结构
一般会支持8KB的页。

内核使用 struct page 结构表示系统中的每个物理页，该结构位于`include/linux/mm_types.h`中，
```
struct page {
    unsigned long flags;        /* Atomic flags, some possibly
                     * updated asynchronously */
    /*
     * Five words (20/40 bytes) are available in this union.
     * WARNING: bit 0 of the first word is used for PageTail(). That
     * means the other users of this union MUST NOT use the bit to
     * avoid collision and false-positive PageTail().
     */
    union {
        struct {    /* Page cache and anonymous pages */
            /**
             * @lru: Pageout list, eg. active_list protected by
             * lruvec->lru_lock.  Sometimes used as a generic list
             * by the page owner.
             */
            union {
                struct list_head lru;

                /* Or, for the Unevictable "LRU list" slot */
                struct {
                    /* Always even, to negate PageTail */
                    void *__filler;
                    /* Count page's or folio's mlocks */
                    unsigned int mlock_count;
                };

                /* Or, free page */
                struct list_head buddy_list;
                struct list_head pcp_list;
            };
            /* See page-flags.h for PAGE_MAPPING_FLAGS */
            struct address_space *mapping;
            union {
                pgoff_t index;        /* Our offset within mapping. */
                unsigned long share;    /* share count for fsdax */
            };
            /**
             * @private: Mapping-private opaque data.
             * Usually used for buffer_heads if PagePrivate.
             * Used for swp_entry_t if PageSwapCache.
             * Indicates order in the buddy system if PageBuddy.
             */
            unsigned long private;
        };
        struct {    /* page_pool used by netstack */
            /**
             * @pp_magic: magic value to avoid recycling non
             * page_pool allocated pages.
             */
            unsigned long pp_magic;
            struct page_pool *pp;
            unsigned long _pp_mapping_pad;
            unsigned long dma_addr;
            atomic_long_t pp_ref_count;
        };
        struct {    /* Tail pages of compound page */
            unsigned long compound_head;    /* Bit zero is set */
        };
        struct {    /* ZONE_DEVICE pages */
            /** @pgmap: Points to the hosting device page map. */
            struct dev_pagemap *pgmap;
            void *zone_device_data;
            /*
             * ZONE_DEVICE private pages are counted as being
             * mapped so the next 3 words hold the mapping, index,
             * and private fields from the source anonymous or
             * page cache page while the page is migrated to device
             * private memory.
             * ZONE_DEVICE MEMORY_DEVICE_FS_DAX pages also
             * use the mapping, index, and private fields when
             * pmem backed DAX files are mapped.
             */
        };

        /** @rcu_head: You can use this to free a page by RCU. */
        struct rcu_head rcu_head;
    };

    union {        /* This union is 4 bytes in size. */
        /*
         * If the page can be mapped to userspace, encodes the number
         * of times this page is referenced by a page table.
         */
        atomic_t _mapcount;

        /*
         * If the page is neither PageSlab nor mappable to userspace,
         * the value stored here may help determine what this page
         * is used for.  See page-flags.h for a list of page types
         * which are currently stored here.
         */
        unsigned int page_type;
    };

    /* Usage count. *DO NOT USE DIRECTLY*. See page_ref.h */
    atomic_t _refcount;

#ifdef CONFIG_MEMCG
    unsigned long memcg_data;
#endif

    /*
     * On machines where all RAM is mapped into kernel address space,
     * we can simply calculate the virtual address. On machines with
     * highmem some memory is mapped into kernel virtual memory
     * dynamically, so we need a place to store that address.
     * Note that this field could be 16 bits on x86 ... ;)
     *
     * Architectures with slow multiplication can define
     * WANT_PAGE_VIRTUAL in asm/page.h
     */
#if defined(WANT_PAGE_VIRTUAL)
    void *virtual;            /* Kernel virtual address (NULL if
                       not kmapped, ie. highmem) */
#endif /* WANT_PAGE_VIRTUAL */

#ifdef LAST_CPUPID_NOT_IN_PAGE_FLAGS
    int _last_cpupid;
#endif

#ifdef CONFIG_KMSAN
    /*
     * KMSAN metadata for this page:
     *  - shadow page: every bit indicates whether the corresponding
     *    bit of the original page is initialized (0) or not (1);
     *  - origin page: every 4 bytes contain an id of the stack trace
     *    where the uninitialized value was created.
     */
    struct page *kmsan_shadow;
    struct page *kmsan_origin;
#endif
} _struct_page_alignment;
```
关注其中的如下变量
* `flag` 每一个bit表示一种状态，这些标志定义在`linux/page-flgas.h`中
* `_count` 存放page的引用计数，即当前页被引用了多少次，内核代码一般不直接检查该项，
  而是通过`page_count()`函数进行检查，当页面空闲时，对于`_count`的值是负的，而
  `page_count()`返回0表示空闲页
* `virtual` 表示页的虚拟地址。通常情况下，它就是页在虚拟内存中的地址，有些内存（高端
  内存）并不永久的映射到内核地址空间上。在这种情况下，这个域的值为NULL，需要的时候，
  必须动态地映射这些页

需要注意的是，page结构与物理页相关，该结构对页的描述只是短暂的。即使页中所包含的
数据继续存在，由于交换等原因，他们也可能并不再和同一个page结构相关联。内核仅仅用
这个数据结构来描述当前时刻在相关的物理页中存放的东西。这种数据结构的目的在与描述
物理内存本身，而不是描述包含在其中的数据。

内核用page结构管理系统中所有的物理页，因此系统中的每个物理页都要分配一个这样的
结构体。



## 区

### 逻辑地址、线性地址、虚拟地址、物理地址

1. 逻辑地址（Logical Address）：
   * 在80x86的分段结构中，逻辑地址由段选择符（或称为段标识符）和段内偏移量组成。
     段选择符确定了数据或指令所在的段，而段内偏移量则确定了在该段内的具体位置。
   * 逻辑地址也是虚拟地址，但是虚拟地址不一定是逻辑地址。
   * 逻辑地址的另一个定义：内核空间中线性映射到物理地址上的的地址，可以用偏移量
     或者应用位掩码将其转换为物理地址。使用`__pa`（地址）宏可以将物理地址转换为
     逻辑地址，使用`__va`（地址）宏可以做相反的操作。

2. 线性地址（Linear Address）或虚拟地址（Virtual Address）：
   * 线性地址或虚拟地址是操作系统为进程提供的一个连续的、从0开始的地址空间。每个进程
     都有自己的虚拟地址空间，用于隔离不同进程之间的内存访问。
   * 线性地址空间是平坦的（flat），意味着所有的地址都是相对于同一原点的偏移量。
     这使得地址的计算和转换相对简单。
   * 在Linux中，内存是以页面（page）为单位来管理的，每个页面的大小为4KB。线性地址
     空间中的每个页面都有一个唯一的线性地址（或虚拟地址）。
   * 线性地址或虚拟地址在内存管理单元（MMU）的参与下，通过页表（Page Table）的映射，
     最终被转换为物理地址。

3. 物理地址（Physical Address）：
   * 物理地址是用于内存芯片级内存单元寻址的地址。它与处理器和CPU连接的地址总线相对应。
   * 物理地址是内存中的实际地址，CPU通过物理地址来访问内存中的数据或指令。
   * 在Linux中，物理地址的分配和管理是由内核负责的。内核通过页表将虚拟地址映射到
     物理地址，以确保进程能够正确地访问内存。

关系：
* 逻辑地址和虚拟地址在概念上有时可以视为等同，它们都是相对于程序或进程的地址空间
  而言的。然而，逻辑地址通常与特定的体系结构（如80x86的分段结构）相关，而虚拟地址
  则更广泛地用于描述操作系统的内存管理。
* 线性地址和虚拟地址在Linux中通常被视为同一概念，它们都是进程虚拟地址空间中的地址。
* 逻辑地址（或虚拟地址）经过页表的映射，最终被转换为物理地址，以便CPU能够访问内存
  中的数据或指令。即，逻辑地址的一个特性是与物理地址有一个特定的偏移，减去这个偏移
  就可以与物理地址一一对应

### 高端内存和低端内存的区别

内核中的虚拟地址空间（3G/1G中的1G）分为两部分：
* 低端内存或LOWMEM：第一个896M
* 高端内存或HIGHMEM：顶部的128M

在32位系统中，一个程序最大虚拟空间位4G，最直接的做法是把内核看做一个程序，使其
与其他程序一样具有4G空间，但这样做会使系统不断切换用户程序页表和系统程序页表，
影响效率，解决方法是划分3G/1G结构，这样可以保证内核空间不变，切换程序时只改程序
页表，不用改内核页表，这样做的缺点是内核的程序空间变小了。

1. 低端内存
* 内核地址空间的第一个896MB空间构成低端内存区域。在启动早期，内核永久映射这896MB
  的空间。该映射产生的地址称为逻辑地址。这些逻辑地址同样是虚拟地址，只是减去固定
  偏移量后就可以将其转化为物理地址。
* 896MB不仅在内核空间的1G中是低端内存，在实际的物理内存上，也是从0开始的896MB。

2. 高端内存
* 内核地址空间顶部的128MB称为高端内存区域。内核用它映射1GB以上的物理内存。当需要
  访问1GB（更确切的说是896MB）以上的物理内存时，内核会使用这128MB创建到虚拟地址空间
  的临时映射，从而实现访问所有物理页面的目标。也可把高端内存定义为逻辑地址不存在
  的内存，不会将其永久映射到内核地址空间。访问高端内存的映射由内核动态创建，访问
  完成后销毁。这使得高端内存访问速度变慢。64位系统上不存在高端内存这个概念，因为
  地址空间巨大，3G/1G拆分没有任何意义。


换一个角度理解高端内存：
* 从进程地址空间的角度：对于32位操作系统来说，能访问的地址只有4G，其中3G分给用户
  空间，1G分给内核空间，这时内核空间可访问的地址只有0xC0000000-0xFFFFFFFF，从
  地址空间的角度，注意这里是地址空间的角度，kernel只有1G的空间，即kernel只能访问
  1G的空间
* 从物理内存的角度：如果物理内存大于1G的话，内存是无法访问超过1G的内存的，这样的话
  1G之外的内存就没有意义，因此就有了高端内存，从物理内存的角度，来动态映射其他的
  内存区域

在32位系统中，高端内存和低端内存的管理是必要的，因为内核地址空间的限制。然而，随着
64位系统的普及，地址空间远远大于物理内存，因此高端内存的概念变得不那么重要了。64位
系统可以轻松地将所有物理内存直接映射到内核地址空间，从而简化了内存管理。


### 区的划分和作用

Linux把系统的页划分为区，形成不同的内存池，这样就可以根据用途进行分配了。区的划分
没有任何物理意义，只不过是内核为了管理页面而采取的一种逻辑上的分组。某些分配可能
要从特定的区中获取页，而另一些分配可能可以从多个区中获取页。

内核把页划分为不同的区(zone)，内核使用区对具有相似特性的页进行分组。Linux需要处理
两种由于硬件缺陷而引起的内存寻址问题：
1. 一些硬件只能用某些特定的内存地址来执行DMA（直接内存访问）
2. 一些体系结构的内存的物理寻址范围比虚拟寻址范围大得多。这样，就有一些内存不能
   永久的映射到内核空间上。

由于以上约束条件，Linux主要使用了4种区：
* ZONE_DMA: 这个区包含的页能用来执行DMA操作
* ZONE_DMA32: 和`ZONE_DMA`类似，可以用来执行DMA操作，而和ZONE_DMA的不同之处在于，
  这些page只能被32位设备访问，某些体系结构中，该区比`ZONE_DMA`更大
* ZONE_NORMAL: 该区包含的都是能正常映射的页
* ZONE_HIGHEMEM: 高端内存，其中的页并不能永久地映射到内核地址空间，其余内存都是低端内存

区的实际使用和分布是与体系结构相关的。ZONE_HIGHEMEM 的工作方式也是这样，能否直接
映射取决于体系结构。在32位x86系统上，ZONE_HIGHEMEM为高于896MB的所有物理内存。在其他
体系结构上，由于所有内存都被直接映射，所以ZONE_HIGHEMEM为空

在x86上，ZONE_NORMAL是从16MB到896MB的独有物理内存，在其他的体系结构上，ZONE_NORMAL
是所有的可用物理内存

并不是所有的体系结构都定义了全部区，有些64位的体系结构，如Interl的x86-64体系结构
可以映射和处理64位的存储空间，所以x86-64没有ZONE_HIGHEMEM区，所有的物理内存都处于
ZONE_DMA和ZONE_NORMAL区

每个区都用 struct zone 表示，在`include/linux/mmzone.h`中定义：
```C
struct zone {
    /* Read-mostly fields */

    /* zone watermarks, access with *_wmark_pages(zone) macros */
    unsigned long _watermark[NR_WMARK];
    unsigned long watermark_boost;

    unsigned long nr_reserved_highatomic;

    /*
     * We don't know if the memory that we're going to allocate will be
     * freeable or/and it will be released eventually, so to avoid totally
     * wasting several GB of ram we must reserve some of the lower zone
     * memory (otherwise we risk to run OOM on the lower zones despite
     * there being tons of freeable ram on the higher zones).  This array is
     * recalculated at runtime if the sysctl_lowmem_reserve_ratio sysctl
     * changes.
     */
    long lowmem_reserve[MAX_NR_ZONES];

#ifdef CONFIG_NUMA
    int node;
#endif
    struct pglist_data    *zone_pgdat;
    struct per_cpu_pages    __percpu *per_cpu_pageset;
    struct per_cpu_zonestat    __percpu *per_cpu_zonestats;
    /*
     * the high and batch values are copied to individual pagesets for
     * faster access
     */
    int pageset_high_min;
    int pageset_high_max;
    int pageset_batch;

#ifndef CONFIG_SPARSEMEM
    /*
     * Flags for a pageblock_nr_pages block. See pageblock-flags.h.
     * In SPARSEMEM, this map is stored in struct mem_section
     */
    unsigned long        *pageblock_flags;
#endif /* CONFIG_SPARSEMEM */

    /* zone_start_pfn == zone_start_paddr >> PAGE_SHIFT */
    unsigned long        zone_start_pfn;

    /*
     * spanned_pages is the total pages spanned by the zone, including
     * holes, which is calculated as:
     *     spanned_pages = zone_end_pfn - zone_start_pfn;
     *
     * present_pages is physical pages existing within the zone, which
     * is calculated as:
     *    present_pages = spanned_pages - absent_pages(pages in holes);
     *
     * present_early_pages is present pages existing within the zone
     * located on memory available since early boot, excluding hotplugged
     * memory.
     *
     * managed_pages is present pages managed by the buddy system, which
     * is calculated as (reserved_pages includes pages allocated by the
     * bootmem allocator):
     *    managed_pages = present_pages - reserved_pages;
     *
     * cma pages is present pages that are assigned for CMA use
     * (MIGRATE_CMA).
     *
     * So present_pages may be used by memory hotplug or memory power
     * management logic to figure out unmanaged pages by checking
     * (present_pages - managed_pages). And managed_pages should be used
     * by page allocator and vm scanner to calculate all kinds of watermarks
     * and thresholds.
     *
     * Locking rules:
     *
     * zone_start_pfn and spanned_pages are protected by span_seqlock.
     * It is a seqlock because it has to be read outside of zone->lock,
     * and it is done in the main allocator path.  But, it is written
     * quite infrequently.
     *
     * The span_seq lock is declared along with zone->lock because it is
     * frequently read in proximity to zone->lock.  It's good to
     * give them a chance of being in the same cacheline.
     *
     * Write access to present_pages at runtime should be protected by
     * mem_hotplug_begin/done(). Any reader who can't tolerant drift of
     * present_pages should use get_online_mems() to get a stable value.
     */
    atomic_long_t        managed_pages;
    unsigned long        spanned_pages;
    unsigned long        present_pages;
#if defined(CONFIG_MEMORY_HOTPLUG)
    unsigned long        present_early_pages;
#endif
#ifdef CONFIG_CMA
    unsigned long        cma_pages;
#endif

    const char        *name;

#ifdef CONFIG_MEMORY_ISOLATION
    /*
     * Number of isolated pageblock. It is used to solve incorrect
     * freepage counting problem due to racy retrieving migratetype
     * of pageblock. Protected by zone->lock.
     */
    unsigned long        nr_isolate_pageblock;
#endif

#ifdef CONFIG_MEMORY_HOTPLUG
    /* see spanned/present_pages for more description */
    seqlock_t        span_seqlock;
#endif

    int initialized;

    /* Write-intensive fields used from the page allocator */
    CACHELINE_PADDING(_pad1_);

    /* free areas of different sizes */
    struct free_area    free_area[NR_PAGE_ORDERS];

#ifdef CONFIG_UNACCEPTED_MEMORY
    /* Pages to be accepted. All pages on the list are MAX_PAGE_ORDER */
    struct list_head    unaccepted_pages;
#endif

    /* zone flags, see below */
    unsigned long        flags;

    /* Primarily protects free_area */
    spinlock_t        lock;

    /* Write-intensive fields used by compaction and vmstats. */
    CACHELINE_PADDING(_pad2_);

    /*
     * When free pages are below this point, additional steps are taken
     * when reading the number of free pages to avoid per-cpu counter
     * drift allowing watermarks to be breached
     */
    unsigned long percpu_drift_mark;

#if defined CONFIG_COMPACTION || defined CONFIG_CMA
    /* pfn where compaction free scanner should start */
    unsigned long        compact_cached_free_pfn;
    /* pfn where compaction migration scanner should start */
    unsigned long        compact_cached_migrate_pfn[ASYNC_AND_SYNC];
    unsigned long        compact_init_migrate_pfn;
    unsigned long        compact_init_free_pfn;
#endif

#ifdef CONFIG_COMPACTION
    /*
     * On compaction failure, 1<<compact_defer_shift compactions
     * are skipped before trying again. The number attempted since
     * last failure is tracked with compact_considered.
     * compact_order_failed is the minimum compaction failed order.
     */
    unsigned int        compact_considered;
    unsigned int        compact_defer_shift;
    int            compact_order_failed;
#endif

#if defined CONFIG_COMPACTION || defined CONFIG_CMA
    /* Set to true when the PG_migrate_skip bits should be cleared */
    bool            compact_blockskip_flush;
#endif

    bool            contiguous;

    CACHELINE_PADDING(_pad3_);
    /* Zone statistics */
    atomic_long_t        vm_stat[NR_VM_ZONE_STAT_ITEMS];
    atomic_long_t        vm_numa_event[NR_VM_NUMA_EVENT_ITEMS];
} ____cacheline_internodealigned_in_smp;
```

## 页操作

内核提供了请求内存的底层机制，并提供了对他进行访问的几个接口。所有的这些接口都以
页为单位进行分配内存，定义于`include/linux/gfp.h`中。其中最核心的函数是：
```c
struct page *alloc_pages(gfp_t gfp, unsigned int order);
```
该函数分配2^order个连续的物理页，并返回一个指针，该指针指向第一个页的page结构体；
如果出错就返回NULL。

可以使用以下函数，将给定的页转换成它的逻辑地址：
```C
static inline void *page_address(const struct page *page)
```
返回一个指针，指向给定物理页当前所在的逻辑地址。如果无需用到struct page，可以调用：
```C
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
```
这个函数与alloc_pages()作用相同，不过他直接返回所请求的第一个页的逻辑地址，因为页
是连续的，所以其他页会紧随其后

如果只需要一个页，可以使用如下两个封装好的函数：
```C
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

#define __get_free_page(gfp_mask) \
        __get_free_pages((gfp_mask), 0)
```

如果希望函数返回的页内容全部为0，可以使用：
```C
unsigned long get_zeroed_page(gfp_t gfp_mask)
```
该函数与`__get_free_pages`工作方式相同，只不过把分配好的页都填充了0


对以上内容进行列表如下：

| 函数 | 描述 |
|--|--|
| alloc_page(gfp_mask)                | 只分配一页，返回指向页结构的指针                    |
| alloc_pages(gfp_mask, order)        | 分配2^order个页，返回指向第一页页结构的指针         |
| `__get_free_page(gfp_mask)`         | 只分配一页，返回指向其逻辑地址的指针                |
| `__get_free_pages(gfp_mask, order)` | 分配2^order页，返回指向第一页逻辑地址的指针         |
| get_zeroed_page(gfp_mask)           | 只分配一页，让其内容填充0，返回指向其逻辑地址的指针 |

同样有对应的释放函数：
```C
void __free_pages(struct page *page, unsigned int order)
void free_pages(unsigned long addr, unsigned int order)
void free_page(unsigned long page)
```

## kmalloc

### kmalloc/kfree

当需要以页为单位的连续物理页时，低级的页函数很有用，但是对于常用的以字节为单位的
分配来说，内核提供的函数是kmalloc

kmalloc依赖于SLAB分配器。kmalloc返回的内存具有内核逻辑地址，因为它是从LOW_MEM区域
分配的，除非制定了HIGH_MEM。

kmalloc 可以获得以字节为单位的一块内核内存，返回一个指向内存块的指针，所分配的区域
在物理上是连续的，出错时返回NULL，除非没有足够的内存使用，否则不会失败

kfree函数释放由kmalloc分配出来的内存块。如果释放的内存不是kmalloc分配的，或者想要
释放的内存早就释放了，调用该函数会有严重后果。

一般情况下，kmalloc的上限是128KB。然而，如果打开了KMALLOC_MAX_SIZE这个宏，可以
申请的内存会更大。具体的最大限制取决于KMALLOC_MAX_SIZE的大小。

### gfp_mask 标志

gfp_mask是分配器标志，这些标志可以分为3类：行为修饰符、去修饰符、类型。
* 行为修饰符表示内存应当如何分配所需的内存。在某些特定情况下，只能使用某些特定的
  方法分配内存。例如，中断处理函数就要求分配内存时不能睡眠。
* 区修饰符表示从哪个区分配内存
* 类型标志组合了行为修饰符和区修饰符，将各种可能用到的组合归纳为不同类型，简化了
  修饰符的使用，这样只需要指定一个类型标志就可以了。`GFP_KERNEL`就是一种类型标志，
  内核中进程上下文相关的代码可以使用它。

所有的这些标志，都是在`include/linux/fgp_types.h`中声明的。另外，一般只使用
类型修饰符就足够了。

一下内容来自chatgpt：
1. 行为标志：这些标志控制分配器的行为。
   * GFP_KERNEL：最常见的标志，用于普通的内核内存分配。调用者可以睡眠，这意味着分配可能会触发页置换或直接回收页面。
   * GFP_ATOMIC：用于原子上下文中，调用者不能睡眠。通常用于中断处理程序或持有自旋锁的代码中。
   * GFP_USER：用于用户空间页面的分配，调用者可以睡眠。
   * GFP_HIGHUSER：类似于GFP_USER，但是从高端内存区域分配，以减少对核心内存的占用。
2. 区标志：这些标志指示从哪个内存区域分配内存。
   * GFP_DMA：要求分配适合DMA的内存，即低端内存。
   * GFP_HIGHMEM：允许从高端内存区域分配内存。
3. 回收标志：这些标志指示内核在分配内存时可以采取的回收策略。
   * GFP_NOWAIT：不等待内存分配，如果当前没有可用的内存，立即失败。
   * GFP_NOFS：在分配内存时，不允许执行文件系统操作。
   * GFP_NOIO：在分配内存时，不允许执行I/O操作。
4. 类型标志：这些标志指示分配的内存类型。
   * GFP_COLD：倾向于从冷页面列表中分配页面。
   * GFP_HOT：倾向于从热页面列表中分配页面。
5. 调试标志：这些标志用于调试目的。
   * GFP_MEMALLOC：即使在内存不足的情况下，也要允许内存分配，通常用于内存分配的关键路径。
6. 组合标志：这些标志是上述标志的组合，用于常见的内存分配场景。
   * GFP_ATOMIC | GFP_DMA：用于原子上下文中，且需要DMA内存的分配。
   * GFP_KERNEL | GFP_HIGHMEM：用于普通的内核内存分配，且可以从高端内存区域分配。

flags 的参考用法：
　|– 进程上下文，可以睡眠          GFP_KERNEL
　|– 进程上下文，不可以睡眠        GFP_ATOMIC
　|　　|– 中断处理程序             GFP_ATOMIC
　|　　|– 软中断                   GFP_ATOMIC
　|　　|– Tasklet                  GFP_ATOMIC
　|– 用于DMA的内存，可以睡眠       GFP_DMA | GFP_KERNEL
　|– 用于DMA的内存，不可以睡眠     GFP_DMA |GFP_ATOMIC

## kzalloc

kzalloc() 函数与 kmalloc() 非常相似，参数及返回值是一样的，可以说是前者是后者的
一个变种，因为 kzalloc() 实际上只是额外附加了`__GFP_ZERO`标志。所以它除了申请内核
内存外，还会对申请到的内存内容清零。
```c
/*
 * kzalloc - allocate memory. The memory is set to zero.
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate (see kmalloc).
 */
static inline void *kzalloc(size_t size, gfp_t flags){return kmalloc(size, flags | __GFP_ZERO);}
```
kzalloc() 对应的内存释放函数也是 kfree()。

## vmalloc

vmalloc 函数的工作方式类似kmalloc，他们得到的虚拟地址都是连续的，区别是vmalloc物理
地址无需连续，kmalloc物理地址连续。

vmalloc返回的内存始终来自HIGH_MEM区域，返回的地址不能转化为物理地址或总线地址，
因为不能保证物理上是连续的，因此vmalloc返回的内存不能在处理器之外使用（例如DMA）

很多内核代码都用kmalloc来获取内存，而不是vmalloc，这主要是出于性能的考虑，因为vmalloc
必须检索内存、构建页面表，甚至重新映射到虚拟连续的范围。vmalloc只有在不得已时才会
使用，典型的就是申请大内存。

由于 vmalloc() 没有保证申请到的是连续的物理内存，因此对申请的内存大小没有限制。

vmalloc申请释放的函数如下：
```C
void *vmalloc(unsigned long size)
void vfree(const void *addr)
```
这两个函数都会睡眠，不能在中断上下文使用。


## 内核栈

每个进程，在内核空间也同样有一个栈

中断处理程序曾经使用被中断的进程的内核栈，这样做虽然简单，但是会给内核栈更多的约束。
当使用只有一个页面的内核栈时，中断处理程序就不放在栈中了。

后来开发了一个：中断栈。中断栈为每个进程提供一个用于中断处理程序的栈。

内核栈可以是1页，也可以是2页，这取决于编译时的配置选项，当1页栈的选项激活时，中断
处理就会有自己的栈


## 高端内存的映射

Linux系统将896MB地址空间永久映射到物理内存较低的896MB（低端内存）。在4GB系统上，
内核仅剩128MB用于映射剩余的3.2GB物理内存（高端内存）。由于低端内存采用永久一对一
映射，因此内核可以直接寻址。而对于高端内存（高于896MB的内存），内核必须将所请求
的高端内存区域映射到其他地址空间，前边提到的128MB空间就是专门为此保留。

1. 永久映射：
```C
void *kmap(struct page *page)
void kunmap(struct page *page)
```
该函数在高端内存或者低端内存上都能使用，可以睡眠。因为永久映射的数量是有限的，
当不需要高端内存时，应该解除映射。

2. 临时映射(原子映射)
```C
void *kmap_atomic(struct page *page, enum km_type_type)
void kunmap_atomic(void *kvaddr, enum km_type type)
```
不会阻塞，不睡眠


# mmu

参考《Linux设备驱动开发》完善


# 进程地址空间

每个进程的用户空间都是完全独立的、互不相干的，用户进程各自由不同的页表。而内核
空间是由内核负责映射，他并不会跟着进程改变，是固定的。内核空间的虚拟地址到物理地址
映射是被所有进程共享的，内核的虚拟空间独立于其他程序。

参考《Linux设备驱动开发》完善

# 内存映射

内存映射主要包括，将内存映射到内核空间和映射到用户空间，映射到内核空间在前边高端
内存的部分讲过，不做赘述，这里主要讲映射到用户空间。

remap_pfn_range将物理内存（通过内核逻辑地址）映射到用户进程空间。这对于实现mmap()
系统调用特别有用。在文件上调用mmap()系统调用之后，cpu将切换到特权模式，运行相应的
file_operations.mmap()内核函数，它调用remap_pfn_range。这将产生映射区域的PTE，并
将其赋给进程

对于I/O内存映射到用户空间。需要使用io_remap_pfn_range，它的参数与 remap_pfn_range
相同，唯一改变的是PFN的来源。需要明确一下与 ioremap 的区别，ioremap 是将内存映射
到内核空间，而 io_remap_pfn_range 是映射到用户空间。

内核mmap函数是 file_operations 的一部分，当用户执行系统调用 mmap 时，把物理内存
映射到用户空间，内核将对内存空间的所有访问转换为文件操作。甚至可以将设备物理内存
直接映射到用户空间，可以像写文件一样操作设备。

出于安全考虑，用户空间进程不能直接访问设备内存。因此，用户空间进程使用mmap系统调用
请求将设备映射到进程的虚拟地址空间。在映射之后，用户空间进程可以通过返回的地址
直接写入内存。

当用户调用mmap()时，内核会进行如下操作：
1. 在进程的虚拟空间查找一块vma
2. 将这块vma进行映射
3. 如果she被驱动程序或者文件系统的file_operations定义了mmap()操作，则调用它
4. 讲这个vma插入进程的vma链表中
