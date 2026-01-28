# DMA Demo - ARM Version

这是一个完整的 Linux 内核 DMA 驱动演示程序的 ARM 平台版本，支持 Android 和 Linux 内核的交叉编译。

## 功能特性

本演示实现了以下 DMA 操作：

1. **Coherent DMA (一致性 DMA)**
   - 分配和释放一致性 DMA 缓冲区
   - 读写一致性缓冲区
   - 数据验证

2. **Streaming DMA Single Mapping (流式 DMA 单映射)**
   - 分配和映射单个缓冲区
   - DMA 同步操作（CPU 和设备方向）
   - 取消映射

3. **Scatter-Gather DMA (分散聚集 DMA)**
   - 分配多个物理不连续的页面
   - 映射 scatter-gather 列表
   - 同步操作
   - 取消映射

4. **DMA Pool (DMA 池)**
   - 创建固定大小的 DMA 内存池
   - 从池中分配/释放内存
   - 销毁内存池

5. **DMA Mask Configuration**
   - 设置 32/64 位 DMA 掩码
   - 查询 DMA 能力

## 目录结构

```
2.dma_base_arm/
├── kDemo.c           # 内核态 DMA 驱动程序
├── userDemoBase.c    # 用户态测试程序
├── Makefile          # ARM 交叉编译 Makefile
├── prjBuild.sh       # 项目构建脚本（可选）
└── ReadMe.md         # 本文档
```

## 编译模式

支持两种构建模式：

### Android 模式 (默认)
- 使用 Clang 编译 Android 内核模块
- 适用于 Android 设备

### Linux 模式
- 使用 GCC 编译 Linux 内核模块
- 适用于 Linux ARM 设备

## 使用方法

### 1. 配置路径

首先需要修改 Makefile 中的路径配置：

```makefile
# Android kernel 配置
ANDROID_KERNELDIR ?= ${HOME}/Projects/kernel3
ANDROID_TOOLCHAIN_PATH ?= ${HOME}/Projects/prebuilts/toolchains/linux-x86_rk/clang-r487747c/bin
ANDROID_CROSS_COMPILE ?= aarch64-linux-gnu-

# Linux kernel 配置
LINUX_KERNELDIR ?= ${HOME}/Projects/kernel
LINUX_TOOLCHAIN_PATH ?= ${HOME}/Projects/prebuilts/toolchains/aarch64/aarch64-rockchip1240-linux-gnu/bin
LINUX_CROSS_COMPILE ?= aarch64-rockchip1240-linux-gnu-
```

### 2. 编译模块

```bash
# 编译 Android 内核模块
make modules_android
# 或
make BUILD_MODE=android

# 编译 Linux 内核模块
make modules_linux
# 或
make BUILD_MODE=linux
```

### 3. 加载模块

```bash
# 加载到 Android 设备
make BUILD_MODE=android init

# 加载到 Linux 设备
make BUILD_MODE=linux init
```

### 4. 运行测试

```bash
# 显示帮助信息
./uDemo -h

# 运行所有测试
./uDemo -a

# 运行特定测试
./uDemo -c          # Coherent DMA 测试
./uDemo -s          # Streaming DMA 单映射测试
./uDemo -g          # Scatter-Gather DMA 测试
./uDemo -p          # DMA Pool 测试
./uDemo -i          # DMA 信息查询
./uDemo -m          # DMA Mask 配置测试

# 运行指定编号的测试
./uDemo -t 1        # 运行测试 1 (Coherent DMA)

# 详细输出模式
./uDemo -a -v
```

### 5. 卸载模块

```bash
# 从 Android 设备卸载
make BUILD_MODE=android exit

# 从 Linux 设备卸载
make BUILD_MODE=linux exit
```

### 6. 清理

```bash
make clean
```

## 设备节点

驱动会创建以下设备节点：
- `/dev/m_chrdev_0` - 主设备，用于 DMA 测试
- `/dev/m_chrdev_1` - 次设备

## 内核兼容性

本驱动针对不同内核版本做了兼容处理：

1. **class_create 函数**
   - 旧内核 (< 4.x): `class_create(name)`
   - 新内核 (>= 4.x): `class_create(owner, name)`

2. **uevent 回调**
   - 使用 `const struct device *` 签名

代码中已通过 `LINUX_VERSION_CODE` 宏检测并适配不同版本。

## IOCTL 命令

| 命令 | 功能 |
|------|------|
| DMA_IOCTL_ALLOC_COHERENT | 分配一致性 DMA 缓冲区 |
| DMA_IOCTL_FREE_COHERENT | 释放一致性 DMA 缓冲区 |
| DMA_IOCTL_READ_COHERENT | 从一致性缓冲区读取数据 |
| DMA_IOCTL_WRITE_COHERENT | 向一致性缓冲区写入数据 |
| DMA_IOCTL_MAP_SINGLE | 映射单个流式 DMA 缓冲区 |
| DMA_IOCTL_UNMAP_SINGLE | 取消映射单个缓冲区 |
| DMA_IOCTL_SYNC_SINGLE | 同步单个缓冲区 |
| DMA_IOCTL_MAP_SG | 映射 scatter-gather 列表 |
| DMA_IOCTL_UNMAP_SG | 取消映射 scatter-gather |
| DMA_IOCTL_SYNC_SG | 同步 scatter-gather |
| DMA_IOCTL_POOL_CREATE | 创建 DMA 池 |
| DMA_IOCTL_POOL_ALLOC | 从池中分配 |
| DMA_IOCTL_POOL_FREE | 释放到池中 |
| DMA_IOCTL_POOL_DESTROY | 销毁 DMA 池 |
| DMA_IOCTL_GET_INFO | 获取 DMA 信息 |
| DMA_IOCTL_SET_MASK | 设置 DMA 掩码 |

## 测试用例

| 编号 | 测试内容 |
|------|----------|
| 1 | Coherent DMA (一致性 DMA) |
| 2 | Streaming DMA Single Mapping (流式 DMA 单映射) |
| 3 | Scatter-Gather DMA (分散聚集 DMA) |
| 4 | DMA Pool (DMA 池) |
| 5 | DMA Information (DMA 信息查询) |
| 6 | DMA Mask Configuration (DMA 掩码配置) |

## 注意事项

1. 确保交叉编译工具链路径正确
2. 确保 ARM 设备通过 ADB 或串口连接
3. 加载模块前确保设备上没有同名模块
4. 测试完成后记得卸载模块
5. 查看内核日志可使用 `dmesg | grep DMA`

## 学习要点

通过本演示可以学习：

1. Linux 内核 DMA 子系统架构
2. 一致性 DMA 和流式 DMA 的区别
3. DMA 映射和同步的原理
4. Scatter-Gather 列表的使用
5. DMA 池的应用场景
6. 虚拟设备的 DMA 掩码设置
7. 字符设备驱动与 DMA 的结合
