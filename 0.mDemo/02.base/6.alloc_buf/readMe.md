
## 常见内存分配方法概览

| 分配方法      | API 函数                         | 分配单位            | 分配大小限制          | 是否物理连续 | 可否睡眠                     | 适用场景                       |
| ------------- | -------------------------------- | ------------------- | --------------------- | ------------ | ---------------------------- | ------------------------------ |
| Slab 分配器   | `kmalloc`/`kzalloc`              | 字节（byte）        | 一般小于 1 MB         | ✅ 是        | ✅ 可以                      | 常规小对象，缓存、结构体分配等 |
| vmalloc       | `vmalloc`/`vzalloc`              | 字节                | 可达几百 MB（非连续） | ❌ 否        | ✅ 可以                      | 分配大内存但不要求物理连续性   |
| 页面分配器    | `alloc_pages`/`__get_free_pages` | 页面（4K 的整数倍） | 通常最多 8MB（连续）  | ✅ 是        | ❌ 不可                      | 内核页表、内存管理子系统使用等 |
| 高端内存映射  | `kmap`/`kmap_atomic`             | 页面                | —                     | ✅ 是        | `kmap`: ✅ `kmap_atomic`: ❌ | 访问高端内存                   |
| 内存池/缓存池 | `kmem_cache_alloc`               | 对象                | —                     | ✅ 是        | ✅ 可以                      | 对象频繁分配释放的场景         |

---

| 分配方式               | 申请函数（举例）           | 对应释放函数                                 | 说明                                  |
| ---------------------- | -------------------------- | -------------------------------------------- | ------------------------------------- |
| `kmalloc` / `kzalloc`  | `kmalloc()`, `kzalloc()`   | `kfree(ptr)`                                 | 用于释放 `kmalloc` 分配的物理连续内存 |
| `vmalloc` / `vzalloc`  | `vmalloc()`, `vzalloc()`   | `vfree(ptr)`                                 | 用于释放虚拟连续但物理不连续的内存    |
| `alloc_pages`          | `alloc_pages()`            | `__free_pages(page, order)`                  | 释放 `struct page *`                  |
| `__get_free_pages`     | `__get_free_pages()`       | `free_pages((unsigned long)ptr, order)`      | 注意参数是虚拟地址 cast               |
| `kmem_cache_alloc`     | `kmem_cache_alloc()`       | `kmem_cache_free()` + `kmem_cache_destroy()` | 释放对象和整个缓存池                  |
| `kmap` / `kmap_atomic` | `kmap(page)`               | `kunmap(page)` / `kunmap_atomic()`           | 解除高端页映射                        |
| `highmem page` 分配    | `alloc_pages(GFP_HIGHMEM)` | `__free_pages()`                             | 释放高端页                            |

---

## 内存分配方法详解

### 1. `kmalloc` / `kzalloc`

```c
void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags); // 分配并清零
```

* 分配的内存是 **物理连续** 的。
* 内存可以直接用于 DMA 等硬件访问。
* `GFP_KERNEL` 允许睡眠，`GFP_ATOMIC` 用于中断上下文。
* 一般限制在 `order <= 10`（4MB）内。

---

### 2. `vmalloc` / `vzalloc`

```c
void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size); // 分配并清零
```

* 分配的是 **虚拟连续但物理不连续** 的内存。
* 内存不可用于 DMA。
* 用于大块内存，通常用于内核模块缓存等。

---

### 3. `alloc_pages` / `__get_free_pages`

```c
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
```

* 分配的内存单位是 **页**。
* `order` 是以 2 的幂计算的页面数（`order=2` 表示 4 页）。
* 适合页表、大页内存分配。

---

### 4. `kmap` / `kmap_atomic`

* 只用于高端内存（32-bit 系统为主）。
* `kmap()` 允许睡眠。
* `kmap_atomic()` 不允许睡眠，必须在临界区使用。

---

### 5. `kmem_cache_alloc`

```c
struct kmem_cache *kmem_cache_create(...);
void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);
```

* 适用于对象频繁分配释放的场景（如 slab cache）。
* 分配固定大小对象，效率高、碎片少。

---

## 内核中各种内存释放方法详解

---

### 1. `kfree`

```c
void *p = kmalloc(128, GFP_KERNEL);
// ...
kfree(p);
```

* 对应 `kmalloc`、`kzalloc`。
* 不能用于释放 `vmalloc` 的内存。

---

### 2. `vfree`

```c
void *p = vmalloc(1024 * 1024);
// ...
vfree(p);
```

* 对应 `vmalloc` / `vzalloc`。
* 不可用 `kfree` 来释放。

---

### 3. `__free_pages`

```c
struct page *p = alloc_pages(GFP_KERNEL, 2);
// ...
__free_pages(p, 2);
```

* 分配和释放页面的标准方式。

---

### 4. `free_pages`

```c
unsigned long addr = __get_free_pages(GFP_KERNEL, 1);
// ...
free_pages(addr, 1);
```

* 注意传入的是 `unsigned long` 虚拟地址。

---

### 5. `kmem_cache_free` + `kmem_cache_destroy`

```c
struct kmem_cache *cache = kmem_cache_create(...);
void *obj = kmem_cache_alloc(cache, GFP_KERNEL);
// ...
kmem_cache_free(cache, obj);
kmem_cache_destroy(cache);
```

* 通常在模块卸载时释放。

---

### 6. `kunmap` / `kunmap_atomic`

```c
void *vaddr = kmap(page);
// ...
kunmap(page);
```

```c
void *vaddr = kmap_atomic(page);
// ...
kunmap_atomic(vaddr);
```

* 映射高端页后一定要解映射，防止地址泄漏或错误。


## 各种申请方法适用场景详解

### 1. `kmalloc` / `kzalloc`

#### 功能：
* `kmalloc(size, flags)`：分配**物理连续**的内核内存。
* `kzalloc(size, flags)`：同 `kmalloc`，但分配后会将内存清零（zero）。

#### 特性：
| 特性             | 说明                       |
| ---------------- | -------------------------- |
| 分配粒度         | 任意小内存（推荐 <128KB）  |
| 物理地址连续     | ✅ 是                      |
| 虚拟地址连续     | ✅ 是                      |
| 可DMA            | ✅（GFP_DMA 时）           |
| 可用于中断上下文 | ✅（配合 GFP_ATOMIC）      |
| 是否清零         | `kzalloc` 是，`kmalloc` 否 |

#### 适用场景：
* 分配小型结构体、对象、缓存、链表节点
* 驱动中与硬件共享（DMA）的内存
* 内核对象或短生命周期数据
* **中断上下文使用（GFP_ATOMIC）**

---

### 2. `vmalloc` / `vzalloc`

#### 功能：
* `vmalloc(size)`：分配**虚拟地址连续**但**物理地址不连续**的内存。
* `vzalloc(size)`：和 `vmalloc` 类似，但自动清零。

#### 特性：
| 特性             | 说明                    |
| ---------------- | ----------------------- |
| 分配粒度         | 大内存块（推荐 >128KB） |
| 物理地址连续     | ❌ 否                   |
| 虚拟地址连续     | ✅ 是                   |
| 性能             | 访问速度比 `kmalloc` 慢 |
| 用户态 mmap 支持 | ✅ 可用于共享映射       |
| 睡眠上下文       | ✅（不可用于中断上下文）|

#### 适用场景：
* 大缓冲区、日志缓存、大数组等
* 内核中预处理图像、音频、文件缓存等
* 不需要物理连续地址，且不用于 DMA 的场合

---

### 3. `alloc_pages` / `__get_free_pages`

#### 功能：
* 分配**一个或多个页面**的连续物理内存（按页计量）
* 返回 `struct page *` 或 `void *`（需转换）

```c
struct page *p = alloc_pages(GFP_KERNEL, order);
void *vaddr = page_address(p); // 转换为虚拟地址
```

* `__get_free_pages()` 是更底层版本，直接返回虚拟地址

#### 特性：
| 特性           | 说明                             |
| -------------- | -------------------------------- |
| 分配粒度       | 以 2^order 页为单位（1页 = 4KB） |
| 最大支持       | 通常最大为 `order = 10` (\~4MB)  |
| 物理地址连续   | ✅ 是                            |
| 可用于 DMA     | ✅（GFP\_DMA）                   |
| 可映射高端内存 | ✅ 配合 `kmap()` 使用            |
| 不自动清零     | ❌（除非你手动 memset）          |

#### 适用场景：
* 分配多个连续页面用于大缓冲区或页面缓存
* 内核页缓存、slab 分配器后端
* 需要对齐到页边界的内存

---

### 4. `kmap` / `kmap_atomic`

#### 功能：
将高端内存（HighMem）页映射到内核虚拟地址空间中，以便访问。
* `kmap(struct page *p)`：**可睡眠**上下文中使用
* `kmap_atomic(struct page *p)`：**不可睡眠**，中断上下文安全（速度快）

#### 特性：

| 特性           | 说明                            |
| -------------- | ------------------------------- |
| 适用页         | 高端内存页（ZONE\_HIGHMEM）     |
| 可睡眠上下文   | `kmap` ✅，`kmap_atomic` ❌     |
| 映射后访问方式 | 得到虚拟地址用于 memcpy、读写等 |
| 使用方式       | 一般与 `alloc_pages()` 联用     |

#### 适用场景：
* 在 32bit 系统或嵌入式架构中访问高端内存页
* 映射大块物理内存页到可访问区域
* 处理页缓存、文件系统块等

---

### 5. `kmem_cache_alloc`

#### 功能：

使用 **slab 分配器** 分配对象。适合分配大量 **固定大小的对象**，效率比 `kmalloc` 更高。

#### 特性：

| 特性            | 说明                           |
| --------------- | ------------------------------ |
| 分配类型        | 结构体对象（定长）             |
| 支持构造函数    | ✅（构造+析构函数初始化对象）  |
| 重复分配效率高  | ✅                             |
| 清零            | ❌，但可以搭配 `GFP_ZERO` 使用 |

#### 适用场景：
* 网络 buffer、inode、task\_struct、sk\_buff 等对象池
* 频繁创建/销毁的内核结构体
* 自定义缓存池（`kmem_cache_create()`）

---

### 总结对比表

| 接口                | 适合分配       | 物理连续 | 虚拟连续 | 可睡眠 | 中断上下文      | 可清零       | 备注                        |
| ------------------- | -------------- | -------- | -------- | ------ | --------------- | ------------ | --------------------------- |
| `kmalloc`/`kzalloc` | 小对象/结构体  | ✅       | ✅       | ✅     | ✅ (GFP_ATOMIC) | `kzalloc` 是 | 常用于内核常规分配          |
| `vmalloc`/`vzalloc` | 大缓冲区       | ❌       | ✅       | ✅     | ❌              | `vzalloc` 是 | 不适合 DMA 或中断中使用     |
| `alloc_pages`       | 多页连续内存   | ✅       | ✅       | ✅     | ✅              | ❌           | 适合缓存、大块内存          |
| `kmap`              | 高端内存映射   | ❌       | ✅       | ✅     | ❌              | N/A          | 映射高端页，非直接分配      |
| `kmap_atomic`       | 高端内存映射   | ❌       | ✅       | ❌     | ✅              | N/A          | 中断上下文安全              |
| `kmem_cache_alloc`  | 固定大小对象池 | ✅       | ✅       | ✅     | ✅              | ❌           | 用于高频率分配的结构体缓存  |

---

### 使用建议总结

| 使用需求                        | 推荐接口               |
| ------------------------------- | ---------------------- |
| 分配小对象（几百字节）          | `kzalloc` / `kmalloc`  |
| 分配大内存缓冲区（>128KB）      | `vzalloc` / `vmalloc`  |
| 分配多页物理连续内存            | `alloc_pages`          |
| 自定义结构体池、高性能对象分配  | `kmem_cache_alloc`     |
| 映射高端内存页                  | `kmap` / `kmap_atomic` |

