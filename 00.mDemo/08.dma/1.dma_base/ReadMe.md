# DMA Demo - Linux内核DMA API演示

这是一个展示Linux内核DMA API各种用法的演示驱动程序，适用于x86平台。

## 功能特性

本demo涵盖了以下DMA使用模式：

### 1. 一致性DMA (Coherent DMA)
- `dma_alloc_coherent()` / `dma_free_coherent()`
- 用于需要长期存在、频繁访问的缓冲区
- 自动维护缓存一致性，CPU和设备可以同时访问
- IOCTL命令: `DMA_IOCTL_ALLOC_COHERENT`, `DMA_IOCTL_FREE_COHERENT`

### 2. 流式DMA单次映射 (Streaming DMA Single Mapping)
- `dma_map_single()` / `dma_unmap_single()`
- 用于一次性传输的缓冲区
- 需要手动同步缓存: `dma_sync_single_for_cpu()` / `dma_sync_single_for_device()`
- IOCTL命令: `DMA_IOCTL_MAP_SINGLE`, `DMA_IOCTL_UNMAP_SINGLE`, `DMA_IOCTL_SYNC_SINGLE`

### 3. 散列-聚集DMA (Scatter-Gather DMA)
- `dma_map_sg()` / `dma_unmap_sg()`
- 用于非连续物理内存的DMA传输
- 使用`sg_table`管理多个scatter-gather条目
- 需要手动同步: `dma_sync_sg_for_cpu()` / `dma_sync_sg_for_device()`
- IOCTL命令: `DMA_IOCTL_MAP_SG`, `DMA_IOCTL_UNMAP_SG`, `DMA_IOCTL_SYNC_SG`

### 4. DMA池 (DMA Pool)
- `dma_pool_create()` / `dma_pool_alloc()` / `dma_pool_free()` / `dma_pool_destroy()`
- 用于分配固定大小的小块DMA内存
- 适合频繁分配/释放相同大小的场景
- IOCTL命令: `DMA_IOCTL_POOL_CREATE`, `DMA_IOCTL_POOL_ALLOC`, `DMA_IOCTL_POOL_FREE`, `DMA_IOCTL_POOL_DESTROY`

### 5. DMA掩码配置 (DMA Mask Configuration)
- `dma_set_mask_and_coherent()`
- 设置设备的DMA寻址能力（32位或64位）
- IOCTL命令: `DMA_IOCTL_SET_MASK`

### 6. DMA信息查询
- 获取当前DMA资源的状态信息
- IOCTL命令: `DMA_IOCTL_GET_INFO`

## 编译和安装

### 1. 编译模块和测试程序
```bash
make modules
```

### 2. 加载模块
```bash
make init
```

### 3. 运行测试
```bash
# 显示帮助信息
make test

# 运行所有DMA测试
make test-all

# 运行特定测试
make test-coherent   # 测试一致性DMA
make test-single     # 测试流式DMA单次映射
make test-sg         # 测试Scatter-Gather DMA
make test-pool       # 测试DMA池
make test-info       # 获取DMA信息
```

### 4. 查看内核日志
```bash
# 实时查看DMA相关日志
make log

# 显示最近的DMA日志
make log-show
```

### 5. 卸载模块
```bash
make exit
```

## 测试程序选项

用户态测试程序`uDemo`支持以下选项：

```
Options:
  -b              Run base test (original test)
  -a, --all       Run all DMA tests
  -c, --coherent  Run coherent DMA test
  -s, --single    Run streaming DMA single mapping test
  -g, --sg        Run scatter-gather DMA test
  -p, --pool      Run DMA pool test
  -i, --info      Get DMA information
  -m, --mask      Test DMA mask configuration
  -t N            Run specific test case (1-6)
  -v, --verbose   Enable verbose output
  -h, --help      Show this help message
```

### 示例

```bash
# 运行所有测试
./uDemo -a

# 运行一致性DMA测试，显示详细输出
./uDemo -c -v

# 运行特定测试用例
./uDemo -t 1  # Coherent DMA
./uDemo -t 2  # Streaming DMA Single Mapping
./uDemo -t 3  # Scatter-Gather DMA
./uDemo -t 4  # DMA Pool
./uDemo -t 5  # DMA Information
./uDemo -t 6  # DMA Mask Configuration
```

## DMA API使用说明

### 一致性DMA

一致性DMA缓冲区在分配时就建立了CPU和设备之间的缓存一致性，不需要手动同步。

```c
/* 分配一致性DMA缓冲区 */
void *cpu_addr = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);

/* 访问缓冲区 - CPU和设备可以直接访问 */
memcpy(cpu_addr, data, size);

/* 释放一致性DMA缓冲区 */
dma_free_coherent(dev, size, cpu_addr, dma_handle);
```

### 流式DMA

流式DMA需要手动映射和同步，效率更高，但使用更复杂。

```c
/* 分配并映射缓冲区 */
void *cpu_addr = kmalloc(size, GFP_KERNEL);
dma_addr_t dma_handle = dma_map_single(dev, cpu_addr, size, DMA_TO_DEVICE);

/* CPU写入数据 */
memcpy(cpu_addr, data, size);

/* 同步给设备 */
dma_sync_single_for_device(dev, dma_handle, size, DMA_TO_DEVICE);

/* 设备完成后，同步回CPU */
dma_sync_single_for_cpu(dev, dma_handle, size, DMA_FROM_DEVICE);

/* 解除映射 */
dma_unmap_single(dev, dma_handle, size, DMA_BIDIRECTIONAL);
kfree(cpu_addr);
```

### Scatter-Gather DMA

用于处理非连续物理内存。

```c
/* 分配多个页面 */
struct page *pages[NENTS];
for (i = 0; i < NENTS; i++) {
    pages[i] = alloc_page(GFP_KERNEL);
}

/* 设置scatter-gather表 */
struct sg_table sg_table;
sg_alloc_table(&sg_table, NENTS, GFP_KERNEL);
for (i = 0; i < NENTS; i++) {
    sg_set_page(&sg_table.sgl[i], pages[i], PAGE_SIZE, 0);
}

/* 映射scatter-gather */
int nents_mapped = dma_map_sg(dev, sg_table.sgl, NENTS, DMA_BIDIRECTIONAL);

/* 同步 */
dma_sync_sg_for_device(dev, sg_table.sgl, nents_mapped, DMA_TO_DEVICE);
dma_sync_sg_for_cpu(dev, sg_table.sgl, nents_mapped, DMA_FROM_DEVICE);

/* 解除映射 */
dma_unmap_sg(dev, sg_table.sgl, nents_mapped, DMA_BIDIRECTIONAL);
sg_free_table(&sg_table);
```

### DMA池

用于频繁分配固定大小的小块内存。

```c
/* 创建DMA池 */
struct dma_pool *pool = dma_pool_create(name, dev, size, align, boundary);

/* 从池中分配 */
void *cpu_addr = dma_pool_alloc(pool, GFP_KERNEL, &dma_handle);

/* 释放回池中 */
dma_pool_free(pool, cpu_addr, dma_handle);

/* 销毁DMA池 */
dma_pool_destroy(pool);
```

### DMA方向

```c
enum dma_data_direction {
    DMA_TO_DEVICE,         /* 数据从CPU到设备 */
    DMA_FROM_DEVICE,       /* 数据从设备到CPU */
    DMA_BIDIRECTIONAL,     /* 双向传输 */
    DMA_NONE               /* 未指定方向 */
};
```

## 架构说明

### 内核态模块 (kDemo.c)

主要组件：
- `struct m_chr_device_data`: 设备数据结构，包含所有DMA资源
- IOCTL处理函数: 处理用户态的DMA请求
- DMA操作函数: 实现各种DMA功能

### 用户态测试程序 (userDemoBase.c)

测试函数：
- `test_coherent_dma()`: 测试一致性DMA
- `test_single_mapping()`: 测试流式DMA单次映射
- `test_scatter_gather()`: 测试Scatter-Gather DMA
- `test_dma_pool()`: 测试DMA池
- `test_dma_info()`: 获取DMA信息
- `test_dma_mask()`: 测试DMA掩码配置

## 设备节点

- `/dev/m_chrdev_0`: 主设备，用于DMA操作
- `/dev/m_chrdev_1`: 次设备

## 注意事项

1. **x86平台**: 本demo针对x86平台设计，DMA地址默认使用64位
2. **权限**: 需要root权限加载模块和访问设备节点
3. **内核版本**: 建议使用较新的内核（4.0+）
4. **日志监控**: 使用`make log`或`dmesg`查看内核输出
5. **资源清理**: 模块卸载时会自动清理所有DMA资源

## 不同平台的兼容性

不同平台可能需要处理以下兼容性问题：

### class_create参数
```c
/* 老版本内核 */
m_chrdev_class = class_create(THIS_MODULE, "m_chrdev_cls");

/* 新版本内核 (5.4+) */
m_chrdev_class = class_create("m_chrdev_cls");
```

### uevent回调
```c
/* 老版本内核 */
static int m_chrdev_uevent(struct device *dev, struct kobj_uevent_env *env)

/* 新版本内核 */
static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
```

如果需要为其他平台编译，请参考`0.mDemo/1.base/ReadMe`中的补丁说明。

## 参考资料

- Linux内核文档: Documentation/DMA-API.txt
- Linux设备驱动程序 (LDD3)
- include/linux/dma-mapping.h
- include/linux/dmapool.h
