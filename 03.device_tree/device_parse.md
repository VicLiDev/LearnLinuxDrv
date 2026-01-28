# 设备树展开

references:

[设备树节点转换为设备节点device_node、和平台设备资源platform_device](https://blog.csdn.net/shenlong1356/article/details/105937866?utm_medium=distribute)

[（二）从解析DTS到创建device_DTS的匹配和解析（结合源码）](https://blog.csdn.net/qq_35065875/article/details/82852902)

[（三）从解析DTS到创建device_从device_node到并入设备驱动模型（结合源码）](https://blog.csdn.net/qq_35065875/article/details/82868976?utm_medium=distribute)

[设备树(二):dtb展开为device_node](https://blog.csdn.net/qq_35552527/article/details/105499408)

[Linux 设备驱动开发 —— 设备树在platform设备驱动中的使用](https://blog.csdn.net/zqixiao_09/article/details/50889458)


## 内核入口

linux最底层的初始化部分在HEAD.s中，这是汇编代码，我们暂且不作过多讨论。

在head.s完成部分初始化之后，就开始调用C语言函数，而被调用的第一个C语言函数就是
**start_kernel**(linux/init/main.c)，而对于设备树的处理，基本上就在setup_arch()
这个函数中。

可以看到，在start_kernel()中调用了**setup_arch**(&command_line) 注意这里的
setup_arch 跟架构有关，每个架构有自己的 setup_arch

```c
void __init setup_arch(char **cmdline_p)
{
    const struct machine_desc *mdesc;

    // 根据传入的设备树dtb的首地址完成一些初始化操作
    mdesc = setup_machine_fdt(__atags_pointer);

    // ...

    // 保证设备树dtb本身存在于内存中而不被覆盖
    arm_memblock_init(mdesc);

    // ...
    // 对设备树具体的解析
    unflatten_device_tree();
    // ...
}

```

这三个被调用的函数就是主要的设备树处理函数：

1. setup_machine_fdt()：根据传入的设备树dtb的首地址完成一些初始化操作。**(cpu架构相关)**
2. arm_memblock_init()：主要是内存相关函数，为设备树保留相应的内存空间，保证设备树
dtb本身存在于内存中而不被覆盖。用户可以在设备树中设置保留内存，这一部分同时作了
保留指定内存的工作。**(cpu架构相关)**
3. unflatten_device_tree()：对设备树具体的解析，事实上在这个函数中所做的工作就是
**将设备树各节点转换成相应的`struct device_node`结构体**。**(cpu架构无关)**


## 设备树dtb转换成device_node

在dts文件里，每个大括号{ }代表一个节点，比如根节点里有个大括号，对应一个device_node
结构体；memory也有一个大括号，也对应一个device_node结构体。

节点里面有各种属性，也可能里面还有子节点，所以它们还有一些父子关系。

根节点下的memory、chosen、led等节点是并列关系，兄弟关系。对于父子关系、兄弟关系，
在device_node结构体里面有成员来描述这些关系。

```C
// include/linux/of.h
struct device_node {
    const char *name;         // 来自节点中的name属性, 如果没有该属性, 则设为"NULL"
    const char *type;         // 来自节点中的device_type属性, 如果没有该属性, 则设为"NULL"
    phandle phandle;
    const char *full_name;    // 节点的名字, node-name[@unit-address]
    struct fwnode_handle fwnode;

    struct  property *properties;   // 节点的属性，包含链表信息
    struct  property *deadprops;    /* removed properties */
    struct  device_node *parent;    // 节点的父亲
    struct  device_node *child;     // 节点的孩子(子节点)
    struct  device_node *sibling;   // 节点的兄弟(同级节点)
#if defined(CONFIG_OF_KOBJ)
    struct  kobject kobj;
#endif
    unsigned long _flags;
    void    *data;
#if defined(CONFIG_SPARC)
    const char *path_component_name;
    unsigned int unique_id;
    struct of_irq_controller *irq_trans;
#endif
};
```

device_node表示一个节点，property结构体表示节点的具体属性。properties结构体的定义如下：

```c
// include/linux/of.h
struct property {
    char    *name;        // 属性名字, 指向dtb文件中的字符串 例如reg  interrupts
    int     length;       // 属性值的长度
    void    *value;       // 属性值, 指向dtb文件中value所在位置, 数据仍以big endian存储
    struct property *next;
#if defined(CONFIG_OF_DYNAMIC) || defined(CONFIG_SPARC)
    unsigned long _flags;
#endif
#if defined(CONFIG_OF_PROMTREE)
    unsigned int unique_id;
#endif
#if defined(CONFIG_OF_KOBJ)
    struct bin_attribute attr;
#endif
};
```



### setup_machine_fdt

```c
    const struct machine_desc *mdesc;

    // 根据传入的设备树dtb的首地址完成一些初始化操作
    mdesc = setup_machine_fdt(__atags_pointer);
```

`__atags_pointer`这个全局变量存储的就是r2的寄存器值，是设备树在内存中的起始地址,
将设备树起始地址传递给setup_machine_fdt，对设备树进行解析。

```c
const struct machine_desc * __init setup_machine_fdt(unsigned int dt_phys)
{
    const struct machine_desc *mdesc, *mdesc_best = NULL;
    // 内存地址检查
    if (!dt_phys || !early_init_dt_verify(phys_to_virt(dt_phys)))
        return NULL;

    // 读取 compatible 属性
    mdesc = of_flat_dt_match_machine(mdesc_best, arch_get_next_mach);

    // 扫描各个子节点
    early_init_dt_scan_nodes();
    // ...
}
```

setup_machine_fdt 主要是获取了一些设备树提供的总览信息。

#### 内存地址检查

先将设备树在内存中的物理地址转换为虚拟地址，然后再检查该地址上是否有设备树的
魔数(magic)，魔数就是一串用于识别的字节码：
* 如果没有或者魔数不匹配，表明该地址没有设备树文件，函数返回失败
* 否则验证成功，将设备树地址赋值给全局变量initial_boot_params。

#### 读取compatible属性

逐一读取设备树**根目录下的** compatible 属性，与内核中的描述比对。

整个过程可以看作内核用一个ID，去内存中查找该ID对应的设备树，因为可能有不止一棵设备树。

```c
/**
 * of_flat_dt_match_machine - Iterate match tables to find matching machine.
 *
 * @default_match: A machine specific ptr to return in case of no match.
 * @get_next_compat: callback function to return next compatible match table.
 *
 * Iterate through machine match tables to find the best match for the machine
 * compatible string in the FDT.
 */
const void * __init of_flat_dt_match_machine(const void *default_match, const void * (*get_next_compat)(const char * const**))
{
    const void *data = NULL;
    const void *best_data = default_match;
    const char *const *compat;
    unsigned long dt_root;
    unsigned int best_score = ~1, score = 0;

    // 获取首地址
    dt_root = of_get_flat_dt_root();
    // 遍历
    while ((data = get_next_compat(&compat))) {
        // 将compatible中的属性一一与内核中支持的硬件单板相对比，
        // 匹配成功后返回相应的 machine_desc 结构体指针。
        score = of_flat_dt_match(dt_root, compat);
        if (score > 0 && score < best_score) {
            best_data = data;
            best_score = score;
        }
    }

    // ...

    pr_info("Machine model: %s\n", of_flat_dt_get_machine_name());

    return best_data;
}
```

machine_desc 结构体中描述了单板相关的一些硬件信息，这里不过多描述。

主要的的行为就是根据这个 compatible 属性选取相应的硬件单板描述信息。
一般 compatible 属性名就是"厂商，芯片型号"。

#### 扫描各子节点

第三部分就是扫描设备树中的各节点，主要分析这部分代码。

```c
void __init early_init_dt_scan_nodes(void)      // 该接口与架构无关
{
    of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
    of_scan_flat_dt(early_init_dt_scan_root, NULL);
    of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}
```

出人意料的是，这个函数中只有一个函数的三个调用，每次调用时，参数不一样。


##### of_scan_flat_dt

首先of_scan_flat_dt()这个函数接收两个参数，一个是函数指针it，一个为相当于函数it
执行时的参数。

```c
/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
int __init of_scan_flat_dt(int (*it)(unsigned long node, const char *uname, int depth, void *data), void *data)
{
    unsigned long p = ((unsigned long)initial_boot_params) + be32_to_cpu(initial_boot_params->off_dt_struct);
    int rc = 0;
    int depth = -1;

    do {
        u32 tag = be32_to_cpup((__be32 *)p);
        const char *pathp;

        p += 4;
        if (tag == OF_DT_END_NODE) {
            depth--;
            continue;
        }
        if (tag == OF_DT_NOP)
            continue;
        if (tag == OF_DT_END)
            break;
        if (tag == OF_DT_PROP) {
            u32 sz = be32_to_cpup((__be32 *)p);
            p += 8;
            if (be32_to_cpu(initial_boot_params->version) < 0x10)
                p = ALIGN(p, sz >= 8 ? 8 : 4);
            p += sz;
            p = ALIGN(p, 4);
            continue;
        }
        if (tag != OF_DT_BEGIN_NODE) {
            pr_err("Invalid tag %x in flat device tree!\n", tag);
            return -EINVAL;
        }
        depth++;
        pathp = (char *)p;
        p = ALIGN(p + strlen(pathp) + 1, 4);
        if (*pathp == '/')
            pathp = kbasename(pathp);
        rc = it(p, pathp, depth, data);
        if (rc != 0)
            break;
    } while (1);

    return rc;
}
```

结论：of_scan_flat_dt() 函数的作用就是扫描设备树中的节点，然后对各节点分别调用
传入的回调函数。

那么重点关注函数指针，在上述代码中，传入的参数分别为
```C
early_init_dt_scan_chosen
early_init_dt_scan_root
early_init_dt_scan_memory
```
从名称可以猜测，这三个函数分别是处理chosen节点、root节点中除子节点外的属性信息、memory节点。



##### early_init_dt_scan_chosen

```c
of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
```

boot_command_line 是一个静态数组，存放着启动参数，

```c
int __init early_init_dt_scan_chosen(unsigned long node, const char *uname, int depth, void *data)
{
    unsigned long l;
    char *p;

    pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

    if (depth != 1 || !data ||
        (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
        return 0;

    early_init_dt_check_for_initrd(node);

    /* Retrieve command line */
    // 找到设备树中的的chosen节点中的bootargs，并作为cmd_line
    p = of_get_flat_dt_prop(node, "bootargs", &l);
    if (p != NULL && l > 0)
        strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));

   // ...

    pr_debug("Command line is: %s\n", (char*)data);

    /* break now */
    return 1;
}
```

经过代码分析，early_init_dt_scan_chosen的作用是获取从chosen节点中获取bootargs，
然后将bootargs放入boot_command_line中，作为启动参数。而非字面意思的处理整个chosen。

在支持设备树的嵌入式系统中，实际上：
* uboot基本上可以不通过显式的bootargs=xxx来传递给内核，而是在env拿出，并存放进
设备树中的chosen节点中
* Linux也开始在设备树中的chosen节点中获取出来这样子就可以做到针对uboot与Linux
在bootargs传递上的统一。



##### early_init_dt_scan_root

```c
int __init early_init_dt_scan_root(unsigned long node, const char *uname,int depth, void *data)
{
    dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
    dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

    prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
    if (prop)
        dt_root_size_cells = be32_to_cpup(prop);
    prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
    if (prop)
        dt_root_addr_cells = be32_to_cpup(prop);
    // ...
}
```

通过代码分析，early_init_dt_scan_root 将 root 节点中的`#size-cells`和`#address-cells`
属性提取出来，放到全局变量dt_root_size_cells和dt_root_addr_cells中，并非获取root节点
中所有的属性。

父节点的`#address-cells`和`#size-cells`分别决定了子节点 reg 属性的 address 和 length
字段的长度，例如：`#address-cells = <2>  #size-cells = <1>` 则对应的reg形式为 
```c
reg = < address1_1 address1_2 length1 [address2_1 address2_2 length2 ...] >
```

不同的字段长度有不同的含义：
* `#address-cells = <1>`;就是说你的地址是32位
* `#address-cells = <2>`;就是说你的地址是64位

现在的设备树中表示地址范围的#address-cells还没有超过2的`#address-cells`只能是1和2

**例如：**
* 对于32位的地址`#address-cells = <1>`：`reg =  <0x02000000 0x10000>`这种情况就是
0x02000000~0x02000000+0x10000地址空间
* 对于64位的地址`#address-cells = <2>`：`reg =  <0x10000000 0x02000000 0x10000>`
这种情况64位的地址就是高32位是0x10000000，低32位是0x02000000的64位地址所以他代表
的地址空间就是：0x1000000002000000 + 0x10000的地址空间

reg 的组织形式为:
```c
// address 和 length 对 表示一个地址范围， address 是起点，终点是 address+length-1，
// 如果存在两个 address 表示是64位的地址
// address 和 length 都是四字节长度
reg = <address1 length1 [address2 length2] [address3 length3]>
```

```c
// 表示数据起始地址由一个四字节描述
#address-cells = 1
// 表示数据大小为一个4字节描述，32位
#size-cells = 1

// 而reg属性由四个四字节组成，所以存在两组地址描述，
// 第一组是起始地址为0x12345678，长度为0x100，
// 第二组起始地址为0x22，长度为0x4, 
// 因为在<>中，所有数据都是默认为32位。
reg = <0x12345678 0x100 0x22 0x4>  
```


##### early_init_dt_scan_memory

```c
int __init early_init_dt_scan_memory(unsigned long node, const char *uname,int depth, void *data){
    // ...
    if (!IS_ENABLED(CONFIG_PPC32) || depth != 1 || strcmp(uname, "memory") != 0)
      return 0;
    reg = of_get_flat_dt_prop(node, "reg", &l);
    endp = reg + (l / sizeof(__be32));

    while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
        base = dt_mem_next_cell(dt_root_addr_cells, &reg);
        size = dt_mem_next_cell(dt_root_size_cells, &reg);
        early_init_dt_add_memory_arch(base, size);
    }
}
```

函数先判断节点的unit name是memory,如果不是，则返回。如果是则将所有memory相关的
reg属性取出来，根据address-cell和size-cell的值进行解析，然后调用
`early_init_dt_add_memory_arch()`来申请相应的内存空间。

```c
    memory@0 {
        device_type = "memory";
        reg = <0x0 0x0 0x0 0x80000000>, <0x8 0x00000000 0x0 0x80000000>;
    };
```

到这里，setup_machine_fdt()函数对于设备树的第一次扫描解析就完成了，主要是获取了
一些设备树提供的总览信息。



### arm_memblock_init

```c
// arch/arm/mm/init.c
void __init arm_memblock_init(const struct machine_desc *mdesc)
{
    // ...
    early_init_fdt_reserve_self();
    early_init_fdt_scan_reserved_mem();
    // ...
}
```

对于设备树的初始化而言，主要做了两件事：

1. 调用early_init_fdt_reserve_self，根据设备树的大小为设备树分配空间，设备树的
totalsize在dtb头部中有指明，因此当系统启动之后，设备树就一直存在在系统中。
2. 扫描设备树节点中的"reserved-memory"节点，为其分配保留空间。

arm_memblock_init 对于设备树的部分解析就完成了，主要是为设备树指定保留内存空间。



### unflatten_device_tree

这一部分就进入了设备树的解析部分：注意of_root这个对象，我们后续文章中会提到它。
实际上，解析以后的数据都是放在了这个对象里面。

```c
void __init unflatten_device_tree(void)
{
    // 展开设备树
    __unflatten_device_tree(initial_boot_params, NULL, &of_root,early_init_dt_alloc_memory_arch, false);

    // 扫描设备树
    of_alias_scan(early_init_dt_alloc_memory_arch);
    // ...
}
```

#### 展开设备树

**property原型**

```c
struct property {
    char    *name;
    int    length;
    void    *value;
    struct property *next;
    // ...
};
```

在设备树中，对于属性的描述是 key = value，这个结构体中的name和value分别对应
key和value，而length表示value的长度；
next指针指向下一个struct property结构体（用于构成单链表）。

**__unflatten_device_tree**

```c
__unflatten_device_tree(initial_boot_params, NULL, &of_root,early_init_dt_alloc_memory_arch, false);
```

我们再来看最主要的设备树解析函数：

```c
void *__unflatten_device_tree(const void *blob,struct device_node *dad,
                              struct device_node **mynodes,void *(*dt_alloc)(u64 size, u64 align),bool detached)
{
    int size;
    // ...
    size = unflatten_dt_nodes(blob, NULL, dad, NULL);
    // ...
    mem = dt_alloc(size + 4, __alignof__(struct device_node));
    // ...
    unflatten_dt_nodes(blob, mem, dad, mynodes);
}
```

主要的解析函数为unflatten_dt_nodes()，在__unflatten_device_tree()函数中，
unflatten_dt_nodes()被调用两次：
* 第一次是扫描得出设备树转换成device node需要的空间，然后系统申请内存空间；
* 第二次就进行真正的解析工作，我们继续看unflatten_dt_nodes()函数：

值得注意的是，在第二次调用unflatten_dt_nodes()时传入的参数为
`unflatten_dt_nodes(blob, mem, dad, mynodes)`;


**unflatten_dt_nodes**

* 第一个参数是设备树存放首地址
* 第二个参数是申请的内存空间
* 第三个参数为父节点，初始值为NULL
* 第四个参数为mynodes，初始值为of_node
```c
static int unflatten_dt_nodes(const void *blob,void *mem,struct device_node *dad,struct device_node **nodepp)
{
    // ...
    for (offset = 0;offset >= 0 && depth >= initial_depth;offset = fdt_next_node(blob, offset, &depth)) {
        populate_node(blob, offset, &mem,nps[depth],fpsizes[depth],&nps[depth+1], dryrun);
        // ...
    }
}
```
这个函数中主要的作用就是从根节点开始，对子节点依次调用`populate_node()`，从函数
命名上来看，这个函数就是填充节点，为节点分配内存。


**device_node原型**
```c
// include/linux/of.h
struct device_node {
    const char *name;        // 设备节点中的name属性转换而来。
    const char *type;        // 由设备节点中的device_type转换而来。
    phandle phandle;         // 由设备节点中的"phandle"和"linux,phandle"属性转换而来，
                             // 特殊的还可能由"ibm,phandle"属性转换而来。
    const char *full_name;   // 这个指针指向整个结构体的结尾位置，在结尾位置存储着
                             // 这个结构体对应设备树节点的unit_name，意味着一个
                             // struct device_node结构体占内存空间为
                             // sizeof(struct device_node)+strlen(unit_name)+字节对齐。
    // ...
    struct    property *properties;  // 这是一个设备树节点的属性链表，属性可能有很多种，
                                     // 比如："interrupts","timer"，"hwmods"等等。
    struct    property *deadprops;    /* removed properties */

    // 与当前属性链表节点相关节点，所以相关链表节点构成整个device_node的属性节点。
    struct    device_node *parent;
    struct    device_node *child;
    struct    device_node *sibling;

    struct    kobject kobj;  // 用于在/sys目录下生成相应用户文件。
    unsigned long _flags;
    void    *data;
    // ...
};
```


```c
static unsigned int populate_node(const void *blob,int offset,void **mem,
                  struct device_node *dad,unsigned int fpsize,struct device_node **pnp,bool dryrun){
    struct device_node *np;
    // 申请内存
    // 注，allocl是节点的unit_name长度(类似于chosen、memory这类子节点描述开头时的名字，并非.name成员)
    np = unflatten_dt_alloc(mem, sizeof(struct device_node) + allocl,__alignof__(struct device_node));

    // 初始化node（设置kobj，接着设置node的fwnode.ops。）
    of_node_init(np);

    // 将device_node的full_name指向结构体结尾处，
    // 即，将一个节点的unit name放置在一个struct device_node的结尾处。
    np->full_name = fn = ((char *)np) + sizeof(*np);

    // 设置其 父节点 和 兄弟节点（如果有父节点）
    if (dad != NULL) {
        np->parent = dad;
        np->sibling = dad->child;
        dad->child = np;
    }

    // 为节点的各个属性分配空间
    populate_properties(blob, offset, mem, np, pathp, dryrun);

    // 获取，并设置device_node节点的name和type属性
    np->name = of_get_property(np, "name", NULL);
    np->type = of_get_property(np, "device_type", NULL);
    if (!np->name)
        np->name = "<NULL>";
    if (!np->type)
        np->type = "<NULL>";
    // ...
}
```

一个设备树中节点转换成一个struct device_node结构的过程渐渐就清晰起来，
现在我们接着看看populate_properties()这个函数，看看属性是怎么解析的，

```c
static void populate_properties(const void *blob,int offset,void **mem,struct device_node *np,const char *nodename,bool dryrun){
    // ...
    for (cur = fdt_first_property_offset(blob, offset);
         cur >= 0;
         cur = fdt_next_property_offset(blob, cur)) 
    {
        fdt_getprop_by_offset(blob, cur, &pname, &sz);
        unflatten_dt_alloc(mem, sizeof(struct property),__alignof__(struct property));
        if (!strcmp(pname, "phandle") ||  !strcmp(pname, "linux,phandle")) {
            if (!np->phandle)
                np->phandle = be32_to_cpup(val);

            pp->name   = (char *)pname;
            pp->length = sz;
            pp->value  = (__be32 *)val;
            *pprev     = pp;
            pprev      = &pp->next;
            // ...
        }
    }
}
```

从属性转换部分的程序可以看出，对于大部分的属性，都是直接填充一个 struct property 属性；
而对于 "phandle" 属性和 "linux,phandle" 属性，直接填充 struct device_node 的 phandle 字段，
不放在属性链表中。

#### 扫描节点：of_alias_scan

从名字来看，这个函数的作用是解析根目录下的alias

```c
struct device_node *of_chosen;
struct device_node *of_aliases;

void of_alias_scan(void * (*dt_alloc)(u64 size, u64 align)){
    of_aliases = of_find_node_by_path("/aliases");
    of_chosen = of_find_node_by_path("/chosen");
    if (of_chosen) {
        if (of_property_read_string(of_chosen, "stdout-path", &name))
            of_property_read_string(of_chosen, "linux,stdout-path",
                                    &name);
        if (IS_ENABLED(CONFIG_PPC) && !name)
            of_property_read_string(of_aliases, "stdout", &name);
        if (name)
            of_stdout = of_find_node_opts_by_path(name, &of_stdout_options);
    }
    for_each_property_of_node(of_aliases, pp) {
        // ...
        ap = dt_alloc(sizeof(*ap) + len + 1, __alignof__(*ap));
        if (!ap)
            continue;
        memset(ap, 0, sizeof(*ap) + len + 1);
        ap->alias = start;
        of_alias_add(ap, np, id, start, len);
        // ...
    }
}
```

of_alias_scan() 函数先是处理设备树chosen节点中的"stdout-path"或者"stdout"属性(两者
最多存在其一)，然后将stdout指定的path赋值给全局变量of_stdout_options，并将返回的
全局 struct device_node 类型数据赋值给of_stdout，指定系统启动时的log输出。

接下来为aliases节点申请内存空间，如果一个节点中同时没有name/phandle/linux,phandle，
则被定义为特殊节点，对于这些特殊节点将不会申请内存空间。

然后，使用of_alias_add()函数将所有的aliases内容放置在aliases_lookup链表中。



### 转换过程总结

![img](device_tree.assets/4.png)

此后，内核就可以根据 device_node 来创建设备。



## device_node转platfrom_device

由 **设备树dtb转换成device_node** 部分的内容可知：每个设备树子节点都将转换成
一个对应的device_node节点。

### 设备树对于驱动

设备树的产生就是为了替代driver中过多的platform_device部分的静态定义，将硬件资源
抽象出来，由系统统一解析，这样就可以避免各驱动中对硬件资源大量的重复定义。这样，
几乎可以肯定的是，设备树中的节点最终目标是转换成`platform device`结构，在驱动开发
时就只需要添加相应的`platform driver`部分进行匹配即可。


### device_node转换为platform_device的条件

首先，对于所有的device_node，如果要转换成platform_device，除了节点中必须有compatible
属性以外，必须满足以下条件：

1. 一般情况下，只对设备树中根的第1级节点（/xx）注册成platform device，也就是对它们的
子节点`（/xx/*）`并不处理。
2. 如果一个结点的compatile属性含有这些特殊的值("simple-bus","simple-mfd","isa","arm,
amba-bus")之一，并且自己成功注册成了`platform_device`，那么它的子结点(需含compatile
属性)也可以转换为platform_device（当成总线看待）。
3. 根节点（/）是例外的，生成platfrom_device时，即使有compatible属性也不会处理

注意：i2c, spi等总线节点会转换为platform_device，但是，spi、i2c下的子节点无论
compatilbe是否为: “simple-bus”,“simple-mfd”,“isa”,"arm,amba-bus "都应该交给对应
的总线驱动程序来处理而不会被转换为platform_device

举例：
```c
/ {
          mytest {  //根节点下的子节点并且包含compatile，因此可以转换为platform_device
              compatile = "mytest", "simple-bus";
              mytest@0 {  //该节点不属于根节点的子节点，但父节点包含 "simple-bus"  因此会转换为platform_device
                    compatile = "mytest_0";
              };
          };

          i2c { //根节点下的子节点并且包含compatile，因此可以转换为platform_device
              compatile = "samsung,i2c";
              at24c02 {   //不属于根节点的子节点，并且还是i2c的子节点，一般交给i2c驱动转换为i2c_client
                    compatile = "at24c02";
              };
          };

          spi {  //根节点下的子节点并且包含compatile，因此可以转换为platform_device
              compatile = "samsung,spi";
              flash@0 {   //不属于根节点的子节点，并且还是spi的子节点，一般交给spi驱动转换为spi_device
                    compatible = "winbond,w25q32dw";
                    spi-max-frequency = <25000000>;
                    reg = <0>;
                  };
          };
 };
```


### 设备树结点会被转换的属性

如果是device_node转换成platform device，这个转换过程又是怎么样的呢？

在老版本的内核中，platform_device部分是静态定义的，其实最主要的部分就是resources部分，
这一部分描述了当前驱动需要的硬件资源，一般是IO，中断等资源。在设备树中，这一类资源
通常通过reg属性来描述，中断则通过interrupts来描述；所以，**设备树中的reg和interrupts
资源将会被转换成platform_device内的struct resources资源**。可以通过platform_get_resource
等平台API获取资源，对于IRQ而言，platform_getresource()还有一个进行了封装的变体
platform_get_irq()。关于reg在设备树中的表达在前边小节的 early_init_dt_scan_root
接口介绍中给出，关于中断的表达形式如下介绍：

```
/{
        compatible = "acme,coyotes-revenge";
        #address-cells = <1>;
        #size-cells = <1>;
        interrupt-parent = <&intc>;

        gpio@1-101f3000{
                compatible = "arm,p1061";
                reg = <0x101f3000 0x1000
                             0x101f4000 0x0010>;
                interrupts = < 3 0 >;
        };
        
        intc: interrupt-controller@10140000{
                compatible = "arm,pll90";
                reg = <0x10140000 0x1000>;
                interrupt-controller;
                #interrupt-cells = <2>;
        };

};

对于中断控制器：
1. 通过 interrupt-controller 属性表明当前节点为中断控制器，该属性为空
2. 通过 #interrupt-cells 表明连接此中断控制器的设备的中断属性的cell大小，含义可以
   参考 reg 中的 #address-cells 和 #size-cells

其他中断控制器相关的属性：
1. interrupt-parent 设备节点通过该属性指定它所依附的中断控制器的 phandle，当节点
   没有指定 interrupt-parent 时，则从父节点继承，
   例如上例中的 interrupt-parent = <&intc>;
2. interrupts  该属性具体含有多少个cell取决于它所依附的中断控制器中的#interrupt-cells
   属性。而每个cell又是什么含义，一般由驱动的实现决定。
```

那么，设备树中其他属性是怎么转换的呢？

答案是：不需要转换，在platform_device中有一个成员struct device dev，这个dev中又有一个
指针成员struct device_node *of_node。linux的做法就是将这个of_node指针直接指向由设备树
转换而来的device_node结构；留给驱动开发者自行处理。

例如，有这么一个struct platform_device of_test。我们可以直接通过 of_test.dev.of_node 
来访问设备树中的信息。

又例如存在一个自定义属性：`rockchip,taskqueue-node = <2>;`可以使用接口
of_property_read_u32(dev->of_node, "rockchip,taskqueue-node", &taskqueue_node);
读取属性值


### platform_device转换的开始

**of_platform_default_populate_init**

函数的执行入口是，在系统启动的早期进行的 of_platform_default_populate_init：

```c
// drivers/of/platform.c
static int __init of_platform_default_populate_init(void)
{
    struct device_node *node;

    if (!of_have_populated_dt())
        return -ENODEV;

    /*
     * Handle ramoops explicitly, since it is inside /reserved-memory,
     * which lacks a "compatible" property.
     */
    node = of_find_node_by_path("/reserved-memory");
    if (node) {
        node = of_find_compatible_node(node, NULL, "ramoops");
        if (node)
            of_platform_device_create(node, NULL, NULL);
    }

    /* Populate everything else. */
    of_platform_default_populate(NULL, NULL, NULL);

    return 0;
}
arch_initcall_sync(of_platform_default_populate_init);
```

在函数 of_platform_default_populate_init() 中，调用了 of_platform_default_populate(NULL, NULL, NULL); ，传入三个空指针：

```c
int of_platform_default_populate(struct device_node *root,const struct of_dev_auxdata *lookup,struct device *parent)
{
    return of_platform_populate(root, of_default_bus_match_table, lookup,
                    parent);
}
```

of_platform_default_populate()调用了of_platform_populate()，我们注意下
of_default_bus_match_table

```c
const struct of_device_id of_default_bus_match_table[] = {
    { .compatible = "simple-bus", },
    { .compatible = "simple-mfd", },
    { .compatible = "isa", },
    #ifdef CONFIG_ARM_AMBA
        { .compatible = "arm,amba-bus", },
    #endif /* CONFIG_ARM_AMBA */
        {} /* Empty terminated list */
};
```

如果节点的属性值为 "simple-bus","simple-mfd","isa","arm,amba-bus "之一的话，那么
它子节点就可以转化成platform_device。

接下来看 **of_platform_populate**

```c
int of_platform_populate(struct device_node *root,
            const struct of_device_id *matches,
            const struct of_dev_auxdata *lookup,
            struct device *parent)
{
    struct device_node *child;
    int rc = 0;

    // 从设备树中获取根节点的device_node结构体
    root = root ? of_node_get(root) : of_find_node_by_path("/");
    if (!root)
        return -EINVAL;

    pr_debug("%s()\n", __func__);
    pr_debug(" starting at: %pOF\n", root);

    //遍历所有的子节点
    for_each_child_of_node(root, child) {
        // 然后对每个根目录下的一级子节点 创建 bus
        // 例如， /r1 , /r2，而不是 /r1/s1
        rc = of_platform_bus_create(child, matches, lookup, parent, true);
        if (rc) {
            of_node_put(child);
            break;
        }
    }
    of_node_set_flag(root, OF_POPULATED_BUS);

    of_node_put(root);
    return rc;
}
EXPORT_SYMBOL_GPL(of_platform_populate);
```

在调用of_platform_populate()时传入了的matches参数是of_default_bus_match_table[]；
这个table是一个静态数组，这个静态数组中定义了一系列的compatible属性："simple-bus"、
"simple-mfd"、"isa"、"arm,amba-bus"。
按照我们上文中的描述，当某个根节点下的一级子节点的compatible属性为这些属性其中之一
时，它的一级子节点也将由device_node转换成platform_device.

**of_platform_bus_create**

```c
/**
 * of_platform_bus_create() - Create a device for a node and its children.
 * @bus: device node of the bus to instantiate
 * @matches: match table for bus nodes
 * @lookup: auxdata table for matching id and platform_data with device nodes
 * @parent: parent for new device, or NULL for top level.
 * @strict: require compatible property
 *
 * Creates a platform_device for the provided device_node, and optionally
 * recursively create devices for all the child nodes.
 */
static int of_platform_bus_create(struct device_node *bus,
                  const struct of_device_id *matches,
                  const struct of_dev_auxdata *lookup,
                  struct device *parent, bool strict)
{
    const struct of_dev_auxdata *auxdata;
    struct device_node *child;
    struct platform_device *dev;
    const char *bus_id = NULL;
    void *platform_data = NULL;
    int rc = 0;

    // ...

    // 创建of_platform_device、赋予私有数据
    dev = of_platform_device_create_pdata(bus, bus_id, platform_data, parent);

    // 判断当前节点的compatible属性是否包含上文中compatible静态数组中的元素
    // 如果不包含，函数返回0，即，不处理子节点。
    if (!dev || !of_match_node(matches, bus))
        return 0;

    for_each_child_of_node(bus, child) {
        pr_debug("   create child: %pOF\n", child);
        // 创建 of_platform_bus
        /* 
           如果当前compatible属性中包含静态数组中的元素，
           即当前节点的compatible属性有"simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"其中一项，
           把子节点当作对应的总线来对待，递归地对当前节点调用`of_platform_bus_create()`
           即，将符合条件的子节点转换为platform_device结构。
        */
        rc = of_platform_bus_create(child, matches, lookup, &dev->dev, strict);
        if (rc) {
            of_node_put(child);
            break;
        }
    }
    of_node_set_flag(bus, OF_POPULATED_BUS);
    return rc;
}
```

**of_platform_device_create_pdata**
这个函数实现了：创建of_platform_device、赋予私有数据
此时的参数platform_data为NULL。

```c
/**
 * of_platform_device_create_pdata - Alloc, initialize and register an of_device
 * @np: pointer to node to create device for
 * @bus_id: name to assign device
 * @platform_data: pointer to populate platform_data pointer with
 * @parent: Linux device model parent device.
 *
 * Returns pointer to created platform device, or NULL if a device was not
 * registered.  Unavailable devices will not get registered.
 */
static struct platform_device *of_platform_device_create_pdata(
                    struct device_node *np,
                    const char *bus_id,
                    void *platform_data,
                    struct device *parent)
{
    // 终于看到了平台设备
    struct platform_device *dev;

    // ...

    // 创建实例，将传入的device_node链接到成员：dev.of_node中
    dev = of_device_alloc(np, bus_id, parent);
    if (!dev)
        goto err_clear_flag;

    // 登记到 platform 中
    dev->dev.bus = &platform_bus_type;
    dev->dev.platform_data = platform_data;
    of_msi_configure(&dev->dev, dev->dev.of_node);

    // 添加当前生成的platform_device。
    if (of_device_add(dev) != 0) {
        platform_device_put(dev);
        goto err_clear_flag;
    }

    return dev;

err_clear_flag:
    of_node_clear_flag(np, OF_POPULATED);
    return NULL;
}
```



**of_device_alloc**

```c
struct platform_device *of_device_alloc(struct device_node *np,const char *bus_id,struct device *parent)
{
    //统计reg属性的数量
    while (of_address_to_resource(np, num_reg, &temp_res) == 0)
        num_reg++;
    //统计中断irq属性的数量
    num_irq = of_irq_count(np);
    //根据num_irq和num_reg的数量申请相应的struct resource内存空间。
    if (num_irq || num_reg) {
        res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
        if (!res) {
            platform_device_put(dev);
            return NULL;
        }
        //设置platform_device中的num_resources成员
        dev->num_resources = num_reg + num_irq;
        //设置platform_device中的resource成员
        dev->resource = res;

        //将device_node中的reg属性转换成platform_device中的struct resource成员。
        for (i = 0; i < num_reg; i++, res++) {
            rc = of_address_to_resource(np, i, res);
            WARN_ON(rc);
        }
        //将device_node中的irq属性转换成platform_device中的struct resource成员。
        if (of_irq_to_resource_table(np, res, num_irq) != num_irq)
            pr_debug("not all legacy IRQ resources mapped for %s\n",
                np->name);
    }
    //将platform_device的dev.of_node成员指针指向device_node。
    dev->dev.of_node = of_node_get(np);
    //将platform_device的dev.fwnode成员指针指向device_node的fwnode成员。
    dev->dev.fwnode = &np->fwnode;
    //设备parent为platform_bus
    dev->dev.parent = parent ? : &platform_bus;
}
```

首先，函数先统计设备树中reg属性和中断irq属性的个数，然后分别为它们申请内存空间，
链入到platform_device中的struct resources成员中。
除了设备树中"reg"和"interrupt"属性之外，还有可选的"reg-names"和"interrupt-names"
这些io中断资源相关的设备树节点属性也在这里被转换。
将相应的设备树节点生成的device_node节点链入到platform_device的dev.of_node中。
最终，我们能够通过在自己的驱动中，使用struct device_node *node = pdev->dev.of_node;
获取到设备树节点中的数据。



**of_device_add**

```c
int of_device_add(struct platform_device *ofdev){
    // ...
    return device_add(&ofdev->dev);
}
```

将当前platform_device中的struct device成员注册到系统device中，并为其在用户空间
创建相应的访问节点。
这一步会调用platform_match，因此最终也会执行设备树的match，以及probe。


## 根节点的设备匹配

设备节点的compatible属性值是为了匹配Linux内核中的驱动程序，通过根节点的compatible
属性可以知道我们所使用的设备，一般第一个值描述了所使用的硬件设备名字，第二个值描述
设备所使用的SOC。Linux内核会通过根节点的compatible属性查看是否支持次设备，如果支持，
则会启动Linux内核。

### 使用设备树之前的设备匹配方法

没有使用设备树之前，Uboot回向Linux内核传递一个叫做machine_id的值，machine_id也就是
设备ID，告诉Linux内核自己是什么设备，看看Linux内核是否支持。Linux内核支持很多设备。
针对每一种设备（板子），Linux内核都用MACHINE_START和MACHINE_END来定义一个machine_desc
结构体来描述这个设备，其中MACHINE_START和MACHINE_END定义在文件
`arch/arm/include/asm/mach/arch.h`或文件`arch/arm/include/asm/mach_desc.h`中，
定义如下：
```c
// arch/arm/include/asm/mach/arch.h
#define MACHINE_START(_type,_name)            \
static const struct machine_desc __mach_desc_##_type    \
 __used                            \
 __section(".arch.info.init") = {            \
    .nr         = MACH_TYPE_##_type,        \
    .name       = _name,

#define MACHINE_END                \
};

// arch/arm/include/asm/mach_desc.h
#define MACHINE_START(_type, _name)            \
static const struct machine_desc __mach_desc_##_type    \
__used __section(".arch.info.init") = {            \
    .name        = _name,

#define MACHINE_END                \
};
```

` __section(".arch.info.init")`表示该变量存储在.arch.info.init段中。

对于arm架构，宏展开之后的nr成员参数负责与uboot传来的参数进行匹配，Uboot会给Linux
内核传递machine_id参数，Linux会检查这个machine_id，其实就是将machine_id与.nr成员
的值进行比较，看看有没有相等的，如果有相等的值，则表示Linux内核支持这个设备，如果
不支持，那么这个设备就无法启动Linux内核。

宏MACHINE_START通常会将.nr的值展开为一个定义，定义的位置咱不深究，新版本的kernel
可能已经去除


### 使用设备树之后的设备匹配方法

当Linux内核引入设备树以后就不再使用MACHINE_START了，而是换为DT_MACHINE_START。
DT_MACHINE_START也定义在文件`arch/arm/include/asm/mach/arch.h`中，定义如下：
```c
// arch/arm/include/asm/mach/arch.h
#define DT_MACHINE_START(_name, _namestr)        \
static const struct machine_desc __mach_desc_##_name    \
 __used                            \
 __section(".arch.info.init") = {            \
    .nr         = ~0,                \
    .name       = _namestr,
```

可以看出DT_MACHINE_START和MACHINE_START基本相同，只是.nr的设置不同，在DT_MACHINE_START
里面直接将.nr设置为~0。说明引入设备树以后不会再根据machine_id来检查Linux内核是否支持
某个设备了。

machine_desc定义如下：
```c
struct machine_desc {
    const char      *name;
    const char      **dt_compat;
    void            (*init_early)(void);
    void            (*init_per_cpu)(unsigned int);
    void            (*init_machine)(void);
    void            (*init_late)(void);

};
```

machine_desc 结构体中有个.dt_compat成员变量，此成员变量保存着本设备兼容属性，其
从设备树中解析得到。

观察Linux内核启动时，设备树匹配的过程：
```
start_kernel
    --> setup_arch
        --> setup_machine_fdt
            --> of_flat_dt_match_machine
```

of_flat_dt_match_machine的定义如下：
```c
const void * __init of_flat_dt_match_machine(const void *default_match,
        const void * (*get_next_compat)(const char * const**))
{
    const void *data = NULL;
    const void *best_data = default_match;
    const char *const *compat;
    unsigned long dt_root;
    unsigned int best_score = ~1, score = 0;

    dt_root = of_get_flat_dt_root();
    while ((data = get_next_compat(&compat))) {
        score = of_flat_dt_match(dt_root, compat);
        if (score > 0 && score < best_score) {
            best_data = data;
            best_score = score;
        }
    }
    if (!best_data) {
        const char *prop;
        int size;

        pr_err("\n unrecognized device tree list:\n[ ");

        prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
        if (prop) {
            while (size > 0) {
                printk("'%s' ", prop);
                size -= strlen(prop) + 1;
                prop += strlen(prop) + 1;
            }
        }
        printk("]\n\n");
        return NULL;
    }

    pr_info("Machine model: %s\n", of_flat_dt_get_machine_name());

    return best_data;
}
```


## 设备树的操作函数

Linux 内核给我们提供了一系列的函数来获取设备树中的节点或者属性信息，这一系列的
函数都有一个统一的前缀“of_”（“open firmware”即开放固件。 ），所以在很多资料里面
也被叫做 OF 函数。

函数的声明基本都在`include/linux/of.h`中，定义在`driver/of`下

#### 查找节点的OF函数

Linux内核用 device_node 结构体来描述一个节点，此结构体定义在`include/linux/of.h`
中，定义如下：

```c
struct device_node {
    const char *name;                       /* 节点名字 */
    phandle phandle;
    const char *full_name;                  /* 节点全名 */
    struct fwnode_handle fwnode;

    struct  property *properties;           /* 属性 */
    struct  property *deadprops;            /* removed 属性 */
    struct  device_node *parent;            /* 父节点 */
    struct  device_node *child;             /* 子节点 */
    struct  device_node *sibling;
#if defined(CONFIG_OF_KOBJ)
    struct  kobject kobj;
#endif
    unsigned long _flags;
    void    *data;
#if defined(CONFIG_SPARC)
    unsigned int unique_id;
    struct of_irq_controller *irq_trans;
#endif
};
```

与查找节点有关的 OF 函数有 5 个：

1. of_find_node_by_name 函数

of_find_node_by_name 函数通过节点名字查找指定的节点，函数原型如下：
```c
// from: 开始查找的节点，如果为NULL表示从根节点开始查找整个设备树
// name: 要查找的节点名字
// ret:  找到的节点，如果为NULL，则表示查找失败
struct device_node *of_find_node_by_name(struct device_node *from, const char *name);
```

2.  of_find_node_by_type 函数

`of_find_node_by_type`函数通过 device_type 属性查找指定的节点，函数原型如下：
```c
// from: 开始查找的节点，如果为NULL表示从根节点开始查找整个设备树
// type: 要查找的节点对应的type字符串，也就是 device_type属性值
// ret:  找到的节点，如果为NULL，则表示查找失败
struct device_node *of_find_node_by_type(struct device_node *from, const char *type);
```

3. of_find_compatible_node 函数

`of_find_compatible_node`函数根据device_type和compatible这两个属性查找指定的节点，
函数原型如下：
```c
// from: 开始查找的节点，如果为NULL表示从根节点开始查找整个设备树
// type: 要查找的节点对应的type字符串，也就是 device_type属性值，可以为NULL，表示忽略该值
// compatible: 要查找的节点对应的compatible属性列表
// ret:  找到的节点，如果为NULL，则表示查找失败
struct device_node *of_find_compatible_node(struct device_node *from,const char *type,const char *compatible);
```

4. of_find_matching_node_and_match 函数

`of_find_matching_node_and_match`函数通过`of_device_id`匹配表来查找指定的节点，
函数原型如下：
```c
// from: 开始查找的节点，如果为NULL表示从根节点开始查找整个设备树
// matches: of_device_id 匹配表，也就是在此匹配表里面查找节点
// match: 找到的匹配的of_device_id
// ret: 找到的节点，如果为NULL，则表示查找失败
struct device_node *of_find_matching_node_and_match(struct device_node *from,const struct of_device_id *matches,const struct of_device_id **match);
```

5. of_find_node_by_path 函数

of_find_node_by_path 函数通过路径来查找指定的节点，函数原型如下：
```c
// path: 带有全路径的节点名，可以使用节点的别名，比如：“/backlight”就是backlight
//       这个节点的全路径
// ret:  找到的节点，如果为NULL，则表示查找失败
inline struct device_node *of_find_node_by_path(const char *path);
```

#### 查找父/子节点的OF函数

1. of_get_parent()函数

of_get_parent()函数用于获取指定节点的父节点（如果有父节点的话），函数原型如下
```C
// node: 要查找的父节点
// ret:  找到的父节点
struct device_node *of_get_parent(const struct device_node *node);
```

2. of_get_next_child()函数

of_get_next_child()函数用迭代的方法查找子节点，函数原型如下：
```C
// node: 父节点
// prev: 前一个子节点，也就是从哪一个子节点开始迭代地查找下一个子节点。可以设置为
//       NULL，表示从第一个子节点开始
// ret:  找到的下一个子节点
struct device_node *of_get_next_child(const struct device_node *node,
                            struct device_node *prev);
```


#### 提取属性值的 OF 函数

Linux 内核中使用结构体 property 表示属性，此结构体同样定义在文件`include/linux/of.h`
中，内容如下：

```c
struct property {
    char    *name;
    int length;
    void    *value;
    struct property *next;
#if defined(CONFIG_OF_DYNAMIC) || defined(CONFIG_SPARC)
    unsigned long _flags;
#endif
#if defined(CONFIG_OF_PROMTREE)
    unsigned int unique_id;
#endif
#if defined(CONFIG_OF_KOBJ)
    struct bin_attribute attr;
#endif
};
```

Linux 内核也提供了提取属性值的 OF 函数 ：

1. of_find_property 函数

`of_find_property`函数用于查找指定的属性，函数原型如下：
```C
// np: 设备节点
// name: 属性名字
// lenp: 属性值的字节数
// ret:  找到的属性
property *of_find_property(const struct device_node *np,const char *name,int *lenp);
```

2. of_property_count_elems_of_size 函数

`of_property_count_elems_of_size`函数用于获取属性中元素的数量，比如 reg 属性值
是一个数组，那么使用此函数可以获取到这个数组的大小，此函数原型如下：
```c
// np: 设备节点
// propname: 需要统计元素数量的属性名字
// elem_size: 元素长度
// ret: 得到的属性元素数量
int of_property_count_elems_of_size(const struct device_node *np,const char *propname,int elem_size);
```

3. of_property_read_u32_index()函数

of_property_read_u32_index()函数用于从属性中获取指定标号的u32类型数据值（无符号u32
位），比如某个属性有多个u32类型的值，那么就可以使用此函数来获取指定标号的数据值，
次函数原型如下：
```c
// np: 设备节点
// propname: 要读取的属性名字
// index: 要读取的值标号
// out_value: 要读取的值
// ret:  0表示读取成功
//       负数表示读取失败
//       -EINVAL表示属性不存在
//       -ENODATA表示没有要读取的数据
//       -EOVERFLOW表示属性值列表太小
int of_property_read_u32_index(const struct device_node *np,
                        const char *propname,
                        u32 index, u32 *out_value);
```


4. 读取 u8、 u16、 u32 和 u64 类型的数组数据

比如大多数的reg属性都是书组数据，可以使用这四个函数一次性读取reg属性中的所有数据，
函数原型如下：
```C
// np: 设备节点
// propname: 要读取的属性名字
// out_value: 读取到的数组值，分别为u8、u16、u32和u64
// sz: 要读取的数组元素数量
// ret:  0表示读取成功
//       负值表示读取失败
//       -EINVAL表示属性不存在
//       -ENODATA表示没有要读取的数据
//       -EOVERFLOW表示属性值列表大小
static inline int of_property_read_u8_array(const struct device_node *np, const char *propname, u8 *out_values, size_t sz)
static inline int of_property_read_u16_array(const struct device_node *np, const char *propname, u16 *out_values, size_t sz)
static inline int of_property_read_u32_array(const struct device_node *np, const char *propname, u32 *out_values, size_t sz)
static inline int of_property_read_u64_array(const struct device_node *np, const char *propname, u64 *out_values, size_t sz)
```

5. 读取 u8、 u16、 u32 和 u64 类型属性值

读取只有一个整形值的属性，函数原型如下：
```c
// np: 设备节点
// propname: 要读取的属性名字
// out_value: 读取到的数组值
// ret:  0表示读取成功
//       负值表示读取失败
//       -EINVAL表示属性不存在
//       -ENODATA表示没有要读取的数据
//       -EOVERFLOW表示属性值列表大小
static inline int of_property_read_u8(const struct device_node *np, const char *propname, u8 *out_value)
static inline int of_property_read_u16(const struct device_node *np, const char *propname, u16 *out_value)
static inline int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
static inline int of_property_read_s32(const struct device_node *np, const char *propname, s32 *out_value)
int of_property_read_u64(const struct device_node *np, const char *propname, u64 *out_value);
```

6. of_property_read_string 函数

of_property_read_string 函数用于读取属性中字符串值，函数原型如下：
```c
// np: 设备节点
// propname: 要读取的属性名字
// out_string: 读取到的字符串值
// ret:  0表示读取成功，负值表示读取失败
int of_property_read_string(struct device_node *np,const char *propname,const char **out_string)
```

7. of_n_addr_cells()函数

of_n_addr_cells()函数用于获取`#address-cells`属性值，函数原型如下：
```C
// np: 设备节点
// ret: 获取到的 #address-cells 属性值
extern int of_n_addr_cells(struct device_node *np);
```

8. of_n_size_cells()函数

of_n_size_cells()函数用于获取`#size-cells`属性值，函数原型如下：
```C
// np: 设备节点
// ret: 获取到的#size-cells属性值
extern int of_n_size_cells(struct device_node *np);
```

9. of_property_match_string()函数

of_property_match_string()函数用于查找字符串在指定属性值（字符串列表）中出现的索引：
```C
// np：设备节点；
// propname：属性名称；
// string：指向在字符串列表中查找的字符串的指针；
int of_property_match_string(const struct device_node *np,
                                    const char *propname,
                                    const char *string);
```

10. of_parse_phandle()函数

of_parse_phandle()函数，用于解析 device tree 中的 phandle 引用。phandle 在
device tree 中是一种指针，用于指向另一个节点。
```C
// np：指向当前节点的指针。
// phandle_name：要解析的 phandle 的名字。
// index：要解析的 phandle 的索引。如果有多个同名的 phandle，可以通过指定不同的
//        索引来解析不同的 phandle。
// ret：解析出的节点的指针。
static inline struct device_node *of_parse_phandle(const struct device_node *np,
                                            const char *phandle_name, int index)
```

### 其他常用的OF函数

1. of_device_is_compatible()函数

of_device_is_ompatible()函数用于查看节点的compatible属性是否包含compat孩子定的
字符串，也就是检查设备节点的兼容性，函数原型如下：
```c
// include/linux/of.h

// device: 设备节点
// compat: 要查看的字符串
// ret:    0表示节点的compatible属性中不包含compat指定的字符串
//         正数，字节的compatible属性中包含compat指定的字符串
extern int of_device_is_compatible(const struct device_node *device,
                    const char *);
```

2. of_get_address()函数

of_get_address()函数用于获取地址相关属性，主要是“reg”或者“asigned-addresses”属性值，
函数原型如下：
```C
// include/linux/of_address.h

// dev: 设备节点
// index: 要读取的地址标号
// size: 地址长度
// flags: 参数，比如IORESOURCE_IO、IORESOURCE_MEM等
// ret:  读取到的地质数据首地址，若为NULL，则表示读取失败
static inline const __be32 *of_get_address(struct device_node *dev, int index,
                        u64 *size, unsigned int *flags)
```

3. of_translate_address()函数

of_translate_address()函数负责将设备树读取到的地址转换为物理地址，函数原型如下：
```C
// include/linux/of_address.h

// dev: 设备节点
// in_addr: 要转换的地址
// ret: 得到的物理地址，如果为OF_BAD_ADDR，则表示转换失败
extern u64 of_translate_address(struct device_node *np, const __be32 *addr);
```

4. of_address_to_resource()函数

I2C、SPI、GPIO等这些外设都有对应的寄存器，这些寄存器其实就是一组内存空间，Linux
内核使用resource结构体来描述一段内存空间，resource翻译过来就是“资源”，因此用
resource结构体描述的都是设备资源信息，resource结构体定义在文件`include/linux/ioport.h`
中，定义如下：
```C
// include/linux/ioport.h

/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct resource {
    resource_size_t start;  // 开始地址
    resource_size_t end;    // 结束地址
    const char *name;       // 资源名字
    unsigned long flags;    // 资源标志位，一般表示资源类型，可选的标志定义在
                            // include/linux/ioport.h 中
    unsigned long desc;
    struct resource *parent, *sibling, *child;
};
```

资源标志位如下
```C
// include/linux/ioport.h

#define IORESOURCE_BITS       0x000000ff    /* Bus-specific bits */

#define IORESOURCE_TYPE_BITS  0x00001f00    /* Resource type */
#define IORESOURCE_IO         0x00000100    /* PCI/ISA I/O ports */
#define IORESOURCE_MEM        0x00000200
#define IORESOURCE_REG        0x00000300    /* Register offsets */
#define IORESOURCE_IRQ        0x00000400
#define IORESOURCE_DMA        0x00000800
#define IORESOURCE_BUS        0x00001000

#define IORESOURCE_PREFETCH      0x00002000    /* No side effects */
#define IORESOURCE_READONLY      0x00004000
#define IORESOURCE_CACHEABLE     0x00008000
#define IORESOURCE_RANGELENGTH   0x00010000
#define IORESOURCE_SHADOWABLE    0x00020000

#define IORESOURCE_SIZEALIGN     0x00040000    /* size indicates alignment */
#define IORESOURCE_STARTALIGN    0x00080000    /* start field is alignment */

#define IORESOURCE_MEM_64    0x00100000
#define IORESOURCE_WINDOW    0x00200000    /* forwarded by bridge */
#define IORESOURCE_MUXED     0x00400000    /* Resource is software muxed */

#define IORESOURCE_EXT_TYPE_BITS 0x01000000    /* Resource extended types */
#define IORESOURCE_SYSRAM        0x01000000    /* System RAM (modifier) */

/* IORESOURCE_SYSRAM specific bits. */
#define IORESOURCE_SYSRAM_DRIVER_MANAGED   0x02000000 /* Always detected via a driver. */
#define IORESOURCE_SYSRAM_MERGEABLE        0x04000000 /* Resource can be merged. */

#define IORESOURCE_EXCLUSIVE    0x08000000    /* Userland may not map this resource */

#define IORESOURCE_DISABLED    0x10000000
#define IORESOURCE_UNSET       0x20000000    /* No address assigned yet */
#define IORESOURCE_AUTO        0x40000000
#define IORESOURCE_BUSY        0x80000000    /* Driver has marked this resource busy */
```

一般最常见的资源标志位就是IORESOURCE_MEM、IORESOURCE_REG和IORESOURCE_IRQ等。接下来
回到of_address_to_resource()函数，此函数看名字像是从设备树里提取资源值，但是从本质
上就是读reg属性值转换为resource结构体类型，函数原型如下所示：
```c
// drivers/of/address.h

// dev: 设备节点
// index: 地址资源标号
// r:  得到的resource类型的资源值
// ret: 0: 成功，负值: 失败
int of_address_to_resource(struct device_node *dev, int index,
                struct resource *r)
```

5. of_iomap()函数

of_iomap()函数用于直接内存映射，以前我们会通过ioremap()函数来完成物理地址到虚拟地址
的映射，采用设备树以后就可以直接通过of_iomap()函数来获取内存地址所对应的虚拟地址，
不需要使用ioremap()函数了。当然也可以使用ioremap()函数来完成物理地址到虚拟地址的
内存映射，只是在采用设备树以后，大部分的驱动都使用of_iomap()函数了。of_iomap()函数
本质上也是将reg属性中的地址信息转换为虚拟地址，如果reg属性有多段，那么可以通过index
参数指定要完成内存映射的是哪一段，of_iomap()函数原型如下：
```C
// np: 设备节点
// index: reg属性中要完成内存映射的段，如果reg属性中只有一段，则index设置为0
// ret: 经过内存映射后的虚拟内存首地址，如果为NULL，则表示内存映射失败
void __iomem *of_iomap(struct device_node *node, int index)
```

