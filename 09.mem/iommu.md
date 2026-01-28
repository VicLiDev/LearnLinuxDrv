# iommu

## iommu组和iommu域

在 Linux IOMMU（输入输出内存管理单元）子系统中，**IOMMU 组（IOMMU Group）** 和
**IOMMU 域（IOMMU Domain）** 是两个关键概念，主要用于管理设备的地址映射和访问权限。

---

### IOMMU 域（IOMMU Domain）

**IOMMU 域**（`struct iommu_domain`）是 **IOMMU 进行地址转换的核心单位**，它定义
了一个独立的 **地址空间**，IOMMU 通过它来管理设备的虚拟地址到物理地址的映射。

#### 类比虚拟内存管理

* CPU 进程：
    * 每个进程有一个 mm_struct，里面挂着自己的页表树（PGD → PUD → PMD → PTE）。
    * 不同进程之间的页表隔离。
* IOMMU domain：
    * 每个 domain 对应一个 iommu_domain，里面挂着自己的页表树（可能是两级或三级，硬件相关）。
    * 不同 domain 之间隔离。

更精确的说法
* 在 Linux IOMMU 框架里：
    * struct iommu_domain 就代表了一个 虚拟地址空间。
    * 每个 domain 拥有自己的 页表根节点（类似 CPU 里的 pgd）。
    * 页表里的 PTE (Page Table Entry) 记录 IOVA→PA 的映射。

因此可以说：
    * domain = 页表集合 + 地址空间属性 + 访问权限
    * PTE = domain 中的基本元素

也就是说，domain 是管理一批 PTE 的单位，但它不只是“容器”，它还定义了访问策略和
隔离边界。


#### IOMMU 域的作用
- 控制 **设备的内存访问权限**。
- 维护 **虚拟地址（IOVA）到物理地址（PA）的映射关系**。
- **不同的域拥有不同的页表**，可以隔离设备的 DMA 访问。
- 在 **虚拟化（VFIO、PCI 直通）** 场景中，每个虚拟机通常会对应一个独立的 IOMMU 域。

#### IOMMU 域的类型
IOMMU 域的类型通常由底层硬件和驱动决定，常见类型包括：
- **IOMMU_DOMAIN_DMA**（通用 DMA 映射模式）
  - 用于 Linux 通用 DMA 映射 API (`dma_map_*()`)。
  - 适用于一般设备，设备的 DMA 地址会经过 IOMMU 转换。

- **IOMMU_DOMAIN_IDENTITY**（直通模式）
  - IOMMU **不会修改地址映射**，即设备的物理地址和 CPU 看到的一致（1:1 映射）。
  - 主要用于性能敏感的场景，如直通 PCI 设备。

- **IOMMU_DOMAIN_UNMANAGED**（手动管理）
  - 由 **驱动程序或用户态** 自行管理映射关系。
  - 适用于 VFIO（虚拟化设备直通）。

- **IOMMU_DOMAIN_BLOCKED**（禁止访问）
  - 设备无法访问任何内存。
  - 主要用于安全性，防止未授权设备进行 DMA 操作。

#### 关键 API
```c
// 分配 IOMMU 域
struct iommu_domain *domain = iommu_domain_alloc(&platform_bus_type);

// 在 IOMMU 域中创建映射
iommu_map(domain, iova, phys_addr, size, prot);

// 解除 IOMMU 映射
iommu_unmap(domain, iova, size);

// 释放 IOMMU 域
iommu_domain_free(domain);
```

---

### IOMMU 组（IOMMU Group）

**IOMMU 组**（`struct iommu_group`）是 IOMMU 进行 **设备管理** 的基本单位，**一个
IOMMU 组内的所有设备共享同一个 IOMMU 域**。

#### 为什么需要 IOMMU 组？
- **某些设备必须共享 IOMMU 映射**
  - 例如 **PCIe 设备和它的 PCIe 交换桥（PCI Switch）** 可能必须位于同一组，因为
    它们共享相同的 IOMMU 访问权限。
  - **某些 SoC 设备**（比如 GPU 和其相关 DMA 控制器）也可能被分配到同一个 IOMMU 组。

- **安全性（特别是虚拟化场景）**
  - VFIO（虚拟机设备直通）要求 **整个 IOMMU 组必须一起分配给同一个虚拟机**。
  - 如果一个 IOMMU 组内包含多个设备，那么这些设备必须一起分配，不能分割。

#### 如何创建和获取 IOMMU 组？
设备注册时，IOMMU 子系统会为它们创建或分配到 IOMMU 组：
```c
// 获取设备所属的 IOMMU 组
struct iommu_group *group = iommu_group_get(dev);
if (!group) {
    pr_err("Failed to get IOMMU group\n");
    return -EINVAL;
}

// 释放 IOMMU 组
iommu_group_put(group);
```

#### IOMMU 组的绑定
在 IOMMU 体系下，**所有设备必须通过 IOMMU 组绑定到 IOMMU 域**，这样才能正确访问
DMA 资源：
```c
// 绑定 IOMMU 组到 IOMMU 域
iommu_attach_group(domain, group);

// 解除绑定
iommu_detach_group(domain, group);
```

---

### IOMMU 组与 IOMMU 域的关系
- **一个 IOMMU 组只能绑定到一个 IOMMU 域**。
- **一个 IOMMU 域可以管理多个 IOMMU 组**（但通常每个域只用于一个 IOMMU 组）。
- **所有属于同一个 IOMMU 组的设备必须共享相同的地址映射**。

它们的关系可以归纳如下：
1. IOMMU 组是设备的管理单位，一个 IOMMU 组内的所有设备共享相同的 IOMMU 配置。
2. IOMMU 域是地址映射的管理单位，负责维护虚拟地址（IOVA）到物理地址（PA）的转换
   规则。
3. IOMMU 组必须绑定到一个 IOMMU 域，才能正确使用 IOMMU 进行地址映射。
4. 一个 IOMMU 域可以绑定多个 IOMMU 组，但通常不会这么做，而是每个 IOMMU 组绑定到
   一个独立的 IOMMU 域。

简单绘图表示：
```
        IOMMU组1 ------ <绑定到> ------> IOMMU域
           |                                ^
  +--------+----+---+                       |
  |        |    |   |                   <绑定到>
  |        |    |   |                       |
  |        |    |   |                       |
设备1    设备2 ... 设备n                IOMMU组2
                                            |
                                   +--------+----+---+
                                   |        |    |   |
                                   |        |    |   |
                                   |        |    |   |
                                 设备1    设备2 ... 设备n
```

示例关系

| IOMMU 组   | 包含的设备                 | 绑定的 IOMMU 域 |
| ---------- | -------------------------- | --------------- |
| IOMMU 组 0 | `PCIe 设备 A, PCIe 设备 B` | IOMMU 域 1      |
| IOMMU 组 1 | `SATA 控制器`              | IOMMU 域 2      |
| IOMMU 组 2 | `GPU, GPU DMA 控制器`      | IOMMU 域 3      |

---

### 真实场景：VFIO（虚拟化设备直通）
在 **KVM/QEMU** 环境中，给虚拟机直通 PCI 设备时，IOMMU 组的概念至关重要：
- **所有属于同一 IOMMU 组的设备必须同时直通**，否则会带来安全风险。
- **如果一个 IOMMU 组包含多个 PCI 设备，用户必须分配整个组**，不能只直通其中一个设备。

例如：
```bash
# 查看 IOMMU 组信息（对于 PCI 设备 0000:00:1f.0）
find /sys/kernel/iommu_groups/ -type l | grep 0000:00:1f.0
```

如果 `0000:00:1f.0` 设备的 IOMMU 组包含多个设备，则必须直通整个组。

---

### 总结

| **概念**                     | **作用**                                      |
| ---------------------------- | --------------------------------------------- |
| **IOMMU 域（IOMMU Domain）** | 定义一个地址映射空间，管理设备的 DMA 地址转换 |
| **IOMMU 组（IOMMU Group）**  | 设备的管理单位，组内设备共享 IOMMU 访问权限   |

- **IOMMU 组用于组织设备**，确保 **必须共享 DMA 访问的设备** 在一个组里。
- **IOMMU 域用于管理地址映射**，多个 IOMMU 组可以共享一个 IOMMU 域。

这些概念主要用于：
- **内存隔离（防止 DMA 攻击）**
- **虚拟化（VFIO 直通 PCI 设备）**
- **地址转换（物理地址和设备地址映射）**


## iommu组/域和dma的关系

IOMMU 域（IOMMU Domain）/ IOMMU 组（IOMMU Group） 和 DMA（直接内存访问）紧密相连，
但它们的作用不同：

- **IOMMU 负责管理和保护设备的 DMA 访问**，它提供 **地址映射（IOVA -> 物理地址）**
  和 **访问控制**。
- **DMA 是设备访问内存的方式**，当 IOMMU 使能时，设备的 DMA 请求会经过 IOMMU 进行
  地址转换和权限检查。

---

### DMA 在无 IOMMU 和有 IOMMU 时的区别
#### 无 IOMMU（直接物理地址访问）
当 IOMMU **未启用** 时，设备的 DMA 直接访问 **物理地址**：
```plaintext
设备 DMA 地址 == 物理地址（PA）
```
- 设备直接访问物理内存，容易造成 **内存安全问题（DMA 攻击）**。
- 在嵌入式系统、某些简单 SoC 设备上，这种模式较为常见。

#### 有 IOMMU（IOMMU 进行地址转换）
当 IOMMU **启用** 后，设备的 DMA 请求先经过 IOMMU：
```plaintext
设备 DMA 地址（IOVA）  ->  IOMMU  ->  物理地址（PA）
```
- 设备只能访问 **IOMMU 允许的地址范围**，从而实现 **隔离和保护**。
- 设备看到的是 **IOMMU 分配的虚拟地址（IOVA, I/O 虚拟地址）**，而不是物理地址。

---

### IOMMU 域（IOMMU Domain）与 DMA
#### IOMMU 域的作用
**IOMMU 域决定了 DMA 地址如何被映射**，它可以：
- **提供一个虚拟地址空间（IOVA），并映射到物理地址**，设备使用 IOVA 进行 DMA 操作。
- **提供权限控制**，防止 DMA 访问未经授权的地址。

#### IOMMU 域与 DMA 关系
当设备进行 DMA 时，需要使用 IOMMU 提供的映射：
```c
struct iommu_domain *domain = iommu_domain_alloc(&platform_bus_type);

// 设备分配 IOVA
dma_addr_t iova = dma_map_single(dev, cpu_addr, size, DMA_TO_DEVICE);

// IOMMU 建立映射（IOVA -> PA）
iommu_map(domain, iova, phys_addr, size, prot);
```
- `dma_map_single()` 会 **分配一个 IOVA 地址** 并映射到物理地址。
- `iommu_map()` 负责 **维护 IOMMU 的地址映射表**。

---

### IOMMU 组（IOMMU Group）与 DMA
#### 为什么 IOMMU 组会影响 DMA？
- **IOMMU 组内的设备必须共享相同的地址映射**。
- **如果 DMA 设备和其控制器在同一 IOMMU 组内，它们必须共用 IOMMU 配置**。

#### IOMMU 组如何影响 DMA ？
当 IOMMU 组绑定到 IOMMU 域时，设备的 DMA 访问受 IOMMU 限制：
```c
struct iommu_group *group = iommu_group_get(dev);
iommu_attach_group(domain, group);
```
这意味着：
1. 组内 **所有设备** 的 DMA **必须共享相同的地址转换规则**。
2. **多个设备共享一个 IOVA 地址空间**，这对于 **PCIe 设备直通** 或 **多个 DMA 设备协作** 很重要。

---

### DMA 和 IOMMU 典型使用场景
#### 普通设备 DMA（使用 IOMMU 进行地址转换）
适用于：
- **嵌入式 SoC、服务器**
- **NVMe、GPU、NIC**
- 设备使用 `dma_map_single()` 分配 IOVA 地址。

流程：
```plaintext
设备发起 DMA（IOVA 地址） -> IOMMU 转换（IOVA -> 物理地址） -> 访问内存
```

#### VFIO（PCI 直通）
适用于：
- **虚拟化（QEMU/KVM）**
- **GPU 直通、网卡直通**
- **整个 IOMMU 组直通给虚拟机**

流程：
```plaintext
VM 设备（IOVA） -> IOMMU 转换（IOVA -> 物理地址） -> 物理内存
```
所有 **IOMMU 组内的设备必须被一起直通**，不能拆分。

#### IOMMU 直通模式（IOMMU_DOMAIN_IDENTITY）
适用于：
- **高性能计算（HPC）**
- **IOMMU 存在但不做地址转换**
- **IOMMU 仅用于访问权限控制**

流程：
```plaintext
设备发起 DMA（物理地址） -> IOMMU 允许直接访问
```

---

### 结论
| **概念**         | **IOMMU 组**                          | **IOMMU 域**                           | **DMA**                    |
| ---------------- | ------------------------------------- | -------------------------------------- | -------------------------- |
| **作用**         | 设备管理单位，组内设备共享 IOMMU 规则 | 地址映射管理，提供 IOVA 到物理地址转换 | 设备访问内存的方式         |
| **是否影响 DMA** | 是，组内设备共享 DMA 规则             | 是，IOMMU 域决定 DMA 地址映射          | 是，设备通过 DMA 访问 IOVA |
| **典型使用场景** | PCIe 设备管理、VFIO 直通              | 地址转换、内存保护                     | 外设访问内存               |

#### **IOMMU 域、IOMMU 组、DMA 是如何连接的？**
1. **IOMMU 组** 定义了 **哪些设备共享相同的 IOMMU 规则**。
2. **IOMMU 域** 定义了 **设备的地址转换规则（IOVA -> 物理地址）**。
3. **DMA 通过 IOMMU 进行地址转换**，使设备能够访问正确的内存区域。

#### 绘图表示

```
        IOMMU组1 ------ <绑定到> ------> IOMMU域
           |                                ^
  +--------+----+---+                       |
  |        |    |   |                   <绑定到>
  |        |    |   |                       |
  |        |    |   |                       |
设备1    设备2 ... 设备n                IOMMU组2
                                            |
                                   +--------+-------+------+----+---+
                                   |        |       |      |    |   |
                                   |        |       |      |    |   |
                                   |        |       |      |    |   |
                                 设备1    设备2  DMA设备 设备4 ... 设备n
```

## IOMMU/DMA 驱动注册和使用流程

### 1. 硬件和设备树描述

在 **设备树 (DT)** 里，IOMMU 硬件节点和 client 设备会这样描述：
```dts
rkvdec0_mmu: iommu@fdc38700 {
    compatible = "rockchip,iommu-v2";
    #iommu-cells = <0>;
    power-domains = <&power RK3588_PD_RKVDEC0>;
};

rkvdec0: video-codec@fdc00000 {
    compatible = "rockchip,rkvdec";
    iommus = <&rkvdec0_mmu>;
};
```
- `iommu@...` 定义了 IOMMU 硬件。
- `rkvdec0` 设备通过 `iommus = <&rkvdec0_mmu>` 表示需要使用这个 IOMMU。

### 2. IOMMU 驱动注册

在内核启动时，IOMMU 驱动（比如 `rockchip-iommu.c`）会：
1. 调用 `bus_set_iommu(&platform_bus_type, &rockchip_iommu_ops)`  
    → 把自己挂到 **platform bus 的 IOMMU ops** 上。
2. 提供 `struct iommu_ops`，包括：
    - `.domain_alloc / .domain_free`
    - `.attach_dev / .detach_dev`
    - `.map / .unmap`
    - `.probe_device / .release_device`
    - `.pgsize_bitmap` 等

这样，IOMMU core 就知道 **这个 bus 上的设备如果有 iommus 属性，就要用 rockchip iommu 驱动**。

### 3. 设备 probe 阶段：IOMMU 绑定

当 `rkvdec0` 驱动 probe 时：
1. 内核会调用 `iommu_probe_device(dev)`：
    - 找到 `iommus` 属性，匹配到 `rockchip_iommu_ops`。
    - 调用驱动的 `.probe_device(dev)`。
    - 建立 `dev->iommu_group`，把设备加到 **IOMMU group**。

2. 查找或新建 iommu_group
    - 调用 iommu_group_get_for_dev(dev)：
        - 如果这个设备已经属于某个 group，就直接返回。
        - 如果没有 group，就会 新建一个 group（iommu_group_alloc()），然后把设备挂进去。

    🔑 这里的规则是：
    - 一些 IOMMU 驱动可能会在 .probe_device 里决定 group 的粒度（比如同一硬件共享
      context 的设备必须放在一个 group）。
    - Rockchip IOMMU 通常是 每个设备一个 group，除非硬件本身多个设备共用 MMU。

3. default domain 创建
    - IOMMU core 会为 group 创建一个默认的 domain：
        ```c
        iommu_group_alloc_default_domain(group, bus, dev);
        ```
    - domain 的类型一般是 **DMA domain**（支持 DMA API）。
    - 如果内核设置了 `iommu.passthrough=1`，则创建 passthrough domain。
4. 调用 `.attach_dev(domain, dev)`
    - 把设备的 IOMMU 硬件 attach 到这个 domain。
    - 之后设备访问内存时，就会经过 domain 的页表。

所以在驱动里你 `iommu_get_domain_for_dev(dev)` 就能拿到 domain ——因为它在 probe 时
已经建好了。

### 4. DMA API 和 IOMMU 的关系

用户驱动里通常用 DMA API，比如：
```c
dma_addr_t dma = dma_map_single(dev, cpu_buf, size, DMA_TO_DEVICE);
```
流程是这样的：
1. **DMA API → IOMMU ops**
    - `dma_map_single()` 调用 `iommu_map()`，把 `cpu_buf` 的物理地址映射到 domain
      里的一个 IOVA。
    - 返回给你的是这个 IOVA，也就是 `dma_addr_t`。
2. **设备访问时**
    - 设备发出的 DMA 请求带着 IOVA。
    - IOMMU 硬件查 domain 页表，把它翻译成物理地址。
    - 最终访问内存。
3. **释放**
    ```c
    dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);
    ```
    - 对应调用 `iommu_unmap()`，解除 IOVA→PA 映射。

### 5. 生命周期总结
- **domain 的创建与释放**
    - **default domain**：内核在设备 probe 时自动分配，不能手动 free。
    - **自建 domain**：用 `iommu_domain_alloc()` → `iommu_attach_device()`，用完
      再 `iommu_detach_device()` + `iommu_domain_free()`。
- **DMA 映射**
    - 驱动只需要用 `dma_map_*()` / `dma_unmap_*()`。
    - 底层会透过 IOMMU domain 做页表操作。

### 6. 简化时序图
```
设备树
   ↓
设备 probe
   ↓
iommu_probe_device()
   ↓
iommu_group_alloc_default_domain()
   ↓
iommu_domain_alloc() [DMA domain]
   ↓
iommu_attach_device()
   ↓
iommu_get_domain_for_dev() 可获取
   ↓
驱动用 dma_map_*()
   ↓
iommu_map() / unmap()
   ↓
设备发起DMA → IOMMU页表翻译 → 内存
```

✅ 总结一句话：
**domain 是在 probe 时自动分配的 default domain，DMA API 通过这个 domain 做 IOVA
映射，驱动层只管 `dma_map_*()`，不用自己维护页表。**


## IOMMU + DMA 数据流

### 1. 场景

在驱动里做了这样一个操作：
```c
cpu_buf = kzalloc(size, GFP_KERNEL);     // CPU可见的物理内存
dma_addr = dma_map_single(dev, cpu_buf, size, DMA_TO_DEVICE);
```

### 2. 流程图（抽象）

```
[CPU虚拟地址]  ---->  [物理地址]
     |                     |
     | (dma_map_single)    |
     v                     v
   ┌───────────────────────────┐
   │       IOMMU domain        │
   │   (IOVA -> 物理地址表)    │
   └───────────────────────────┘
              ^
              | (返回IOVA)
        dma_map_single()
              |
        dma_addr_t (IOVA)
              |
              v
     ┌──────────────────┐
     │   设备 (VDEC)    │
     │ 发起DMA访问IOVA  │
     └──────────────────┘
              |
              v
   IOMMU硬件查domain页表
              |
              v
         内存控制器
              |
              v
         [物理内存数据]
```

### 3. 数据流解释

1. **CPU 分配 buffer**
   * `kzalloc()` 得到一块物理连续的内存。
   * CPU 通过 **虚拟地址 → 物理地址** 直接访问。

2. **DMA API 建立映射**
   * `dma_map_single()` 调用 IOMMU core → `iommu_map()`。
   * 在当前 device 的 **domain 页表**里，分配一个 IOVA（IO虚拟地址）。
   * 建立 IOVA → 物理地址的映射。
   * 返回这个 IOVA 给驱动 (`dma_addr_t`)，驱动将这个地址配置给设备。

3. **设备访问**
   * 设备发 DMA 请求时，用的就是 IOVA (`dma_addr`)。
   * IOMMU 硬件拦截，查页表，翻译成真正的物理地址。
   * 内存控制器完成访问。

4. **解除映射**
   * `dma_unmap_single()` → `iommu_unmap()`，回收 IOVA，清理映射。

### 4. 补充说明

* **default domain** 是自动在 probe 时建立好的，所以 `dma_map_single()` 直接能用。
* 如果你手动 `iommu_domain_alloc()`，就要负责 attach/detach。
* **DMA API 统一封装**了 cache flush / sync / sg list / iommu map，驱动基本不用关心细节。


✅ 总结：
* 驱动里调用 `dma_map_*()` 得到 IOVA（domain里分配的地址）。
* 设备用 IOVA，IOMMU 翻译成物理地址。
* 整个映射关系存在 domain 里，domain 在 probe 阶段就自动准备好了。


## 多个设备共享 IOMMU group/domain

### 1. 背景
* 在 Linux IOMMU 框架里，设备是以 `iommu_group` 为单位管理的。
* 同一个 group 内的设备 **必须共享一个 domain**。
* 为什么？因为这些设备的 DMA 请求在硬件层面无法隔离，它们被视为一个安全单元。

### 2. 典型情况

设备树里可能是这样的：
```dts
rkvdec0: video-codec@fdc00000 {
    iommus = <&rkvdec_mmu>;
};

rkvdec1: video-codec@fdd00000 {
    iommus = <&rkvdec_mmu>;
};
```
* `rkvdec0` 和 `rkvdec1` 都指向同一个 `rkvdec_mmu`。
* 内核在 probe 时，会把它们放进 **同一个 `iommu_group`**。
* `iommu_group` 只有一个 default domain。
* 所以它们共享同一张 IOVA→PA 页表。

### 3. 数据流示意图
```
         ┌──────────────────────┐
         │    iommu_group       │
         │  (包含 rkvdec0,1)    │
         └─────────┬────────────┘
                   │
                   v
         ┌──────────────────────┐
         │   IOMMU domain       │
         │ (IOVA -> 物理地址表) │
         └─────────┬────────────┘
                   │
     ┌─────────────┴─────────────┐
     v                           v
┌─────────────┐            ┌─────────────┐
│   VDEC0     │            │   VDEC1     │
│ DMA req:IOVA│            │ DMA req:IOVA│
└─────────────┘            └─────────────┘
                   │
                   v
           [IOMMU 硬件翻译]
                   │
                   v
           [系统物理内存]
```

* VDEC0、VDEC1 都用同一个 domain 的页表。
* `dma_map_single()` 分配的 IOVA 是在这个共享 domain 空间里。
* 硬件无论哪个设备发 IOVA，都能查到相同的映射。

### 4. 注意点

1. **IOVA 地址空间共享**
   * 同一 group 里的设备共享 IOVA 地址空间。
   * 如果你手动 `iommu_map()`，要避免地址冲突。

2. **安全性限制**
   * 如果一个 group 里有不可信设备，那就不能和其他安全设备共享，否则会被越权访问。

3. **detach/attach 的限制**
   * group 是最小操作单元，你不能只 detach group 里的某个设备，只能整体 detach。

### 5. DMA API 在多设备场景

* 对 driver 来说一样用 `dma_map_*()`。
* 内核保证映射是在共享 domain 中分配的。
* 所以 **多个设备可以访问同一块 buffer**，比如视频解码器解码 → 显示控制器取数据。


✅ 总结：
* **`iommu_group`** = 一组必须共享地址空间的设备。
* **`iommu_domain`** = 页表映射上下文，group attach 一个 domain。
* **多个设备共享同一 group/domain** → DMA API 在同一个 IOVA 空间里分配映射，
  设备之间天然可以共享 buffer。


## 接口介绍

下面把 **Linux IOMMU 相关接口**按使用者视角（驱动/子系统调用）和实现者视角
（IOMMU 驱动回调）系统性梳理一遍；并重点讲 **flush（IOTLB 刷新）** 等细节，
以及和 **DMA API** 的衔接。内容偏通用，放到 Rockchip/RK3588 也成立。


### 一、驱动可直接使用的 IOMMU 核心接口（linux/iommu.h）

#### 1) 域与组（domain/group）
* `struct iommu_domain *iommu_get_domain_for_dev(struct device *dev)`
  * 取设备当前 attach 的 **domain 指针**（通常是 default DMA domain）。
  * 只读指针，不要去 free。
* `struct iommu_domain *iommu_domain_alloc(struct bus_type *bus)`
* `void iommu_domain_free(struct iommu_domain *domain)`
  * 手工创建/释放自定义 domain（非默认域时才用）。
* `int iommu_attach_device(struct iommu_domain *domain, struct device *dev)`
* `void iommu_detach_device(struct iommu_domain *domain, struct device *dev)`
  * 将设备从原 domain 挂/摘到新的 domain（需要你自己管理生命周期）。
  * **注意**：组是最小单位（见下），同组内设备必须共享同一 domain。
* `struct iommu_group *iommu_group_get(struct device *dev)`
* `void iommu_group_put(struct iommu_group *group)`
* `int iommu_group_id(struct iommu_group *group)`
* `int iommu_group_for_each_dev(struct iommu_group *group, void *data,
  int (*fn)(struct device *dev, void *data))`
  * 组的查询/遍历等辅助接口。**一个 group 只 attach 一个 domain**，group 内所有
    设备共享该 domain 的 IOVA 空间。

> 小结：正常驱动不需要手动建 domain；使用 DMA API 时，core 会在 probe 阶段给 group
> 准备好 **default DMA domain**，你通过 `iommu_get_domain_for_dev()` 就能拿到。


#### 2) IOVA 映射（直接操作页表，适合“非 DMA API”场景）

> 大多数设备驱动**不需要**直接用这些接口；常用的是 DMA API（见后）。只有当要实现
> 自有地址空间/用户态共享 VA/SVM 等高级用法，或做特殊映射时才用。

* `int iommu_map(struct iommu_domain *domain, unsigned long iova,
  phys_addr_t paddr, size_t size, int prot)`
  * 在 domain 中建立 IOVA→物理 的映射；`prot` 组合如 `IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC`。
  * 映射建立后，**并不保证 IOTLB 立刻可见**，是否需要同步见下文的 “TLB 刷新”。
* `size_t iommu_unmap(struct iommu_domain *domain, unsigned long iova,
  size_t size, struct iommu_iotlb_gather *gather)`
  * 解除映射；返回真正解除的字节数。
  * 支持**延迟合并刷新**（通过 `gather`），提升大量 unmap 时的效率。
* `int iommu_map_sgtable(struct iommu_domain *domain, unsigned long iova,
  struct sg_table *sgt, unsigned int prot)`
  * 一次把 `sg_table` 映射到连续 IOVA；比逐段 `iommu_map()` 高效。

##### 2.1 IOTLB 刷新（flush table / flush TLB）

IOMMU 有自己的 **I/O TLB**（IOTLB），映射变更后需要让硬件可见。Linux 提供
**批量收集 + 一次性同步**的机制：
* `struct iommu_iotlb_gather`：收集要刷新的区间。
* `void iommu_iotlb_gather_init(struct iommu_iotlb_gather *gather)`
  * 初始化收集器。
* `size_t iommu_unmap(..., struct iommu_iotlb_gather *gather)`
  * 每次 `unmap` 会把需刷新的范围加入 `gather`。
* `void iommu_tlb_sync(struct iommu_domain *domain,
  struct iommu_iotlb_gather *gather)`
  * **关键步骤**：把收集的范围一次性下发到 IOMMU 驱动去 **invalidate IOTLB**。
* （按需）`void iommu_flush_iotlb_all(struct iommu_domain *domain)`
  * 全域刷新（更重），通常只有在无法表达具体区间或做大范围变更时使用。

> **典型模式**（伪代码）：
>
> ```c
> struct iommu_iotlb_gather gather;
> iommu_iotlb_gather_init(&gather);
> size_t unmapped = iommu_unmap(domain, iova, size, &gather);
> iommu_tlb_sync(domain, &gather);  // 让硬件 IOTLB 生效
> ```
>
> * `map()` 侧若硬件要求“写 PTE 后需同步”才可用，可以由 core 调用 `iotlb_sync_map()`
> （详见下面“驱动回调”）或驱动内处理。


#### 3) Fault（翻译异常）处理

* `int iommu_set_fault_handler(struct iommu_domain *domain,
  iommu_fault_handler_t handler, void *data)`
  * 注册 domain 级 fault 回调；设备访问未映射/权限不符时会回调，便于 debug 或做
    容错（如上报、统计、触发复位）。
  * 回调原型一般携带 fault 地址、读写方向、设备信息等。


#### 4) Domain 属性（可调参数/查询）

* `int iommu_domain_get_attr(struct iommu_domain *domain,
  enum iommu_attr attr, void *data)`
* `int iommu_domain_set_attr(struct iommu_domain *domain,
  enum iommu_attr attr, void *data)`
  * 常见用途：
    * 查询 **支持的页大小** / IOVA 几何（page size bitmap、aperture）。
    * 某些平台特性开关（如脏页追踪、stall/fault model 等，取决于具体 IOMMU 驱动支持）。
  * 具体可用枚举以内核版本/平台为准（查看 `include/linux/iommu.h` 中的 `enum iommu_attr`）。


#### 5) 设备特性开关 / SVA（共享虚拟地址空间）

* `bool iommu_dev_has_feature(struct device *dev, enum iommu_dev_features feat)`
* `int iommu_dev_enable_feature(struct device *dev, enum iommu_dev_features feat)`
* `void iommu_dev_disable_feature(struct device *dev, enum iommu_dev_features feat)`
  * 典型 `feat`：`IOMMU_DEV_FEAT_SVA`（共享VA/ PASID）、`IOMMU_DEV_FEAT_AUX` 等。
* **SVA / PASID：**
  * `struct iommu_sva *iommu_sva_bind_device(struct device *dev,
    struct mm_struct *mm, unsigned long flags)`
  * `void iommu_sva_unbind_device(struct iommu_sva *handle)`
  * `u32 iommu_sva_get_pasid(struct iommu_sva *handle)`
  * 让设备和某个进程地址空间共享同一 VA（常用于 GPU、异构计算），IOMMU 以 PASID 区分上下文。


### 二、DMA API 与 IOMMU 的衔接（linux/dma-mapping.h）

> **大多数驱动首选 DMA API**；它对是否走 IOMMU 透明。若设备后端有 IOMMU，
> `dma_map_*()` 会自动在 **default DMA domain** 里做 IOVA 映射。

* **Streaming 映射：**
  * `dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
    size_t size, enum dma_data_direction dir)`
  * `void dma_unmap_single(struct device *dev, dma_addr_t dma_addr,
    size_t size, enum dma_data_direction dir)`
  * `int dma_map_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir, unsigned long attrs)`
  * `void dma_unmap_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir, unsigned long attrs)`
  * `dma_sync_single_for_cpu()` / `dma_sync_single_for_device()`（缓存一致性）
* **一致性（coherent）内存：**
  * `void *dma_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t gfp)`
  * `void dma_free_coherent(...)`
  * 可用 `dma_alloc_attrs()` / `DMA_ATTR_*` 做高级控制（跳过同步、写合并等）。
* **注意事项：**
  1. **方向（dir）**必须正确（TO/ FROM/ BIDIRECTIONAL），否则缓存/IOMMU 可见性会出问题。
  2. **unmap 之后** 设备不可再用旧的 IOVA。
  3. 多设备共享同一 group/domain 时，**IOVA 空间共享**，但 DMA API 会替你管理地址
     分配，避免冲突。


### 三、IOMMU 驱动需要实现的回调（`struct iommu_ops`，供了解 flush 细节）

> 这些是 **IOMMU驱动**（如 `rockchip-iommu`、`arm-smmu`）在 `struct iommu_ops` 里
> 提供的回调；上层通过公共 API 调用它们。

* **域/设备管理：**
  * `.domain_alloc` / `.domain_free`
  * `.attach_dev` / `.detach_dev`
  * `.probe_device` / `.release_device`（绑定/解绑设备）
  * `.def_domain_type`（决定默认域类型：DMA / IDENTITY(passthrough) 等）
  * `.pgsize_bitmap`（支持的页大小集合）
* **映射：**
  * `.map(domain, iova, paddr, size, prot, gfp)`
  * `.unmap(domain, iova, size, gather)`
    * **不要直接在这里做重型 flush**；把需刷新范围加入 `gather`，交由 `.iotlb_sync`
      统一处理。
* **IOTLB 刷新（flush 关键）：**
  * `.flush_iotlb_all(domain)`：整体失效，最重。
  * `.iotlb_sync_map(domain, iova, size)`：**映射后**若硬件需要“map-side”同步
    （某些 IOMMU 修改 PTE 后需 poke），由 core 视情况调用。
  * `.iotlb_sync(domain, gather)`：**unmap/batch 结束**后统一把 `gather` 中的
    区间下发为硬件 IOTLB 失效（range-based 或全刷）。
  * 这样形成高效的 **批量收集（gather）→ 一次同步（sync）** 模式。
* **保留区域（RMRR/CSRs 等）：**
  * `.get_resv_regions(dev, &list)` / `.put_resv_regions(dev, &list)`
  * `.apply_resv_region(dev, domain, region)`：需要时自动在 domain 中为保留区建立
    identity 映射或隔离。
* **Fault/特性/SVA：**
  * `.domain_set_attr` / `.domain_get_attr`
  * `.dev_has_feat` / `.dev_enable_feat` / `.dev_disable_feat`
  * `.sva_bind` / `.sva_unbind` / `.sva_get_pasid`

> **Flush 小结**：
>
> * map：可能需要 `.iotlb_sync_map()`（平台相关）。
> * unmap：**一定**通过 `gather` + `.iotlb_sync()` 让失效生效。
> * 极端情况：`.flush_iotlb_all()`。

### 四、调试与观测

* **sysfs**：`/sys/kernel/iommu_groups/<id>/devices/` 观察每个 group 的设备成员。
* **debugfs / tracepoints**（视平台/内核配置而定）：可查看 map/unmap、fault 事件。
* **fault handler**：在回调里打印 IOVA、方向、设备名，定位“访问空洞/权限不符”。

### 五、实用“套路”与易错点

1. **批量 unmap 做 flush 合并：**
   * 不要每段都全刷；用 `gather` 合并区间，最后 `iommu_tlb_sync()` 一次性失效。
2. **延迟可见性：**
   * `iommu_map()` 并不等于“设备立刻可用”。若硬件要求，core/驱动会通过 `.iotlb_sync_map()`
     或 barrier 保障次序。
3. **组的粒度：**
   * 同组 = 同 domain = 共享 IOVA。**你不能只对组里某一个设备换 domain**，必须整体处理。
4. **DMA API 优先：**
   * 能用 `dma_map_*()` 就不要自己 `iommu_map()`；后者要自己处理页对齐、齐段、
     同步、回收等细节。
5. **方向与缓存：**
   * `dma_sync_*()` 别忘了；尤其是 CPU 与设备交替访问同一缓冲时。
6. **保留区/洞（reserved）**：
   * IOMMU core/驱动通常会自动解析并处理，不要把 IOVA 映射落在这些保留区上。


### 六、两个小示例

**A) 自己管理 IOVA 映射并做正确的 flush：**

```c
struct iommu_iotlb_gather gather;
iommu_iotlb_gather_init(&gather);

/* 建立映射 */
ret = iommu_map(domain, iova, phys, size, IOMMU_READ|IOMMU_WRITE);
if (ret)
    return ret;
/* 某些 IOMMU 可能需要 map-side 同步（由 core/驱动内部处理），
 * 这里通常不需要你手工做额外 flush。 */

/* 解除映射（可在退出/失败回滚时批量做） */
size_t unmapped = iommu_unmap(domain, iova, size, &gather);
/* 最终同步 IOTLB（关键！） */
iommu_tlb_sync(domain, &gather);
```

**B) 普通驱动用 DMA API（推荐）：**

```c
void *cpu = kzalloc(sz, GFP_KERNEL);
dma_addr_t dma = dma_map_single(dev, cpu, sz, DMA_TO_DEVICE);
if (dma_mapping_error(dev, dma))
    return -ENOMEM;

/* 设备用 dma (IOVA) 访问 ... */

/* 释放 */
dma_unmap_single(dev, dma, sz, DMA_TO_DEVICE);
kfree(cpu);
```
