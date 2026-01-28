# Tips

## 设备树里的中断号计算方法

1. GIC 中断编号分类

ARM GIC 将中断分为以下几类：
```
SGI (Software Generated Interrupts): 软件生成的中断，编号范围是 0-15。
PPI (Private Peripheral Interrupts): 私有外设中断，编号范围是 16-31。
SPI (Shared Peripheral Interrupts): 共享外设中断，编号范围是 32 及以上。
```

2. 设备树中的中断编号表示

设备树中描述外设中断通常使用的是 SPI 或 PPI 编号。由于 SPI 中断的硬件编号从 32
开始，设备树中采用的编号是基于 GIC 中断号的偏移表示法。

设备树中断编号计算公式:
```
interrupts = <type irq_number>;
```
* type: 表示中断类型，0 表示 PPI，1 表示 SPI。
* irq_number: 相对于类型起始编号的偏移量。


3. 示例分析

假设某外设的中断号为 45（硬件编号），它是一个 SPI 类型中断。
设备树中描述如下：
```
interrupts = <1 (45 - 32)>;
```
1: 表示 SPI 类型。
(45 - 32) = 13: 表示从 SPI 起始编号 32 偏移 13。
原因：设备树中的编号是基于 SPI 偏移编号表示，而不是直接使用硬件编号。
