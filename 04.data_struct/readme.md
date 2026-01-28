# Linux 内核数据结构

## 概述

Linux 内核使用了一套精心设计的数据结构，这些结构高效、通用，是内核各个子系统的基础。
掌握这些数据结构是深入理解内核的关键。

## 数据结构一览

### 基础容器

| 数据结构        | 文件                   | 典型用途               | 时间复杂度     | 特点                   |
|-----------------|------------------------|------------------------|----------------|------------------------|
| **list_head**   | 1.list.md              | 进程链表、设备链表     | O(n) 遍历      | 双向循环链表，侵入式   |
| **hlist_head**  | 1.list.md              | 哈希表桶               | O(1) 插入/删除 | 双向单向链表，节省内存 |
| **哈希表**      | 2.hash.md              | 进程查找、dentry cache | O(1) 查找      | 快速查找键值对         |
| **红黑树**      | 3.tree.md              | 虚拟内存区、进程调度   | O(log n)       | 自平衡二叉搜索树       |
| **Radix Tree**  | 3.tree.md              | 页表、内存管理         | O(key bits)    | 基数树，快速前缀查找   |
| **XArray**      | 3.tree.md              | 页缓存、inode cache    | O(1) ~ O(n)    | Radix Tree 现代替代    |
| **scatterlist** | 4.scatterlist.md       | DMA 传输               | -              | 管理不连续物理内存     |
| **atomic_t**    | 5.atomic_and_bitmap.md | 原子计数、引用计数     | O(1)           | 原子操作，无锁         |
| **bitmap**      | 5.atomic_and_bitmap.md | CPU 掩码、内存位图     | O(1)           | 位图操作               |
| **IDR**         | 6.idr.md               | PID 分配、设备 ID      | O(1)           | 整数 ID 到指针映射     |

### 驱动模型与同步

| 数据结构           | 文件               | 典型用途                    | 特点                   |
|--------------------|--------------------|-----------------------------|------------------------|
| **kobject**        | 7.kobject.md       | sysfs 目录、设备管理        | 驱动模型核心           |
| **kset**           | 7.kobject.md       | kobject 集合                | 热插拔事件             |
| **kref**           | 7.kobject.md       | 引用计数                    | 原子引用计数           |
| **completion**     | 8.sync.md          | 等待一次性事件              | DMA 完成、模块加载     |
| **wait_queue**     | 8.sync.md          | 等待事件/资源               | 可重复事件、poll 支持  |
| **RCU**            | 9.rcu.md           | 读多写少场景                | 读者零开销             |
| **per-CPU 变量**   | 10.percpu.md       | 每个 CPU 独立数据           | 避免伪共享             |
| **gen_pool**       | 11.gen_pool.md     | 固定大小内存池              | DMA pool、mempool      |
| **seq_file**       | 12.seq_file.md     | /proc、debugfs 输出         | 格式化大量数据         |
| **notifier**       | 13.notifier.md     | 事件通知机制                | 发布-订阅模式          |

## 选择数据结构的决策树

```
需要存储什么类型的数据？
│
├─ 键值对查找？
│  ├─ 需要有序遍历？ → 红黑树 (mm_struct, vma)
│  └─ 快速查找即可 → 哈希表 (pid_hash, dentry)
│
├─ 多个对象串联？
│  ├─ 需要频繁双向遍历？ → list_head (task list)
│  └─ 哈希表桶？ → hlist_head (hash bucket)
│
├─ DMA 相关？
│  └─ 物理内存不连续 → scatterlist (DMA SG)
│
├─ 简单计数？
│  ├─ 需要原子性？ → atomic_t (refcount)
│  └─ 集合标志位？ → bitmap (cpumask)
│
├─ 整数 ID 管理？
│  └─ ID 分配和查找 → IDR (PID allocation)
│
├─ 同步/等待？
│  ├─ 一次性事件 → completion
│  └─ 可重复事件 → wait_queue
│
├─ 读多写少？
│  └─ RCU (读者零开销)
│
└─ sysfs 接口？
   └─ kobject/kset
```

## 各子系统使用的核心数据结构

```
进程调度 (sched)
  ├── task_struct (list_head)
  ├── rb_tree (运行队列)
  └── plist (优先级队列)

内存管理 (mm)
  ├── list_head (页面链表)
  ├── radix tree / xarray (页表缓存)
  ├── rb_tree (VMA 管理)
  └── per-CPU 变量

文件系统 (fs)
  ├── hlist (dentry cache)
  ├── hash (inode cache)
  ├── xarray (页缓存)
  └── list (超级块列表)

网络协议栈 (net)
  ├── hlist (连接哈希表)
  └── list (接口列表)

驱动模型 (driver)
  ├── kobject (sysfs 目录)
  ├── kset (设备集合)
  ├── kref (引用计数)
  ├── idr (设备 ID)
  └── notifier (事件通知)

块设备 (block)
  ├── scatterlist (DMA)
  ├── gen_pool (内存池)
  └── completion (I/O 完成)
```

## 参考资料

### 内核头文件
- `include/linux/list.h` - 链表 API
- `include/linux/hashtable.h` - 哈希表 API
- `include/linux/rbtree.h` - 红黑树 API
- `include/linux/xarray.h` - XArray API
- `include/linux/scatterlist.h` - scatterlist API
- `include/linux/atomic.h` - 原子操作
- `include/linux/kobject.h` - kobject API
- `include/linux/completion.h` - completion API
- `include/linux/wait.h` - wait_queue API
- `include/linux/rcupdate.h` - RCU API
- `include/linux/percpu.h` - per-CPU API
- `include/linux/notifier.h` - notifier API

### 内核文档
- [Linux Kernel Data Structures](https://www.kernel.org/doc/html/latest/core-api/kernel-api.html)
- [RCU documentation](https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html)
- [Driver Model](https://www.kernel.org/doc/html/latest/driver-api/driver-model/)
