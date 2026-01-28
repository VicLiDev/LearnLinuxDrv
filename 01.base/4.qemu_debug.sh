#!/usr/bin/env bash
#########################################################################
# File Name: 4.qemu_debug.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Sat 08 Mar 2025 09:17:12 AM CST
#########################################################################

# ref: https://zhuanlan.zhihu.com/p/521196386

rootDir=`pwd`
atfDir="arm-trusted-firmware"
rootfsDir="buildroot"
kernelDir="linux"
ubootDir="u-boot"
mkImgDir="mkimg"
imgName="sdcard.img"
logNmae="debug.log"

toochainDir="${HOME}/Projects/prebuilts/toolchains"

# aarch
toochainSel="gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu"
m_toolchains="${toochainDir}/aarch64/${toochainSel}/bin"
m_arch="arm64"
m_cpl_prefix="aarch64-none-linux-gnu-"

# # arm
# toochainSel="gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf"
# m_toolchains="${toochainDir}/arm/${toochainSel}/bin"
# m_arch="arm"
# m_cpl_prefix="arm-linux-gnueabihf-"

export PATH=${m_toolchains}:$PATH


echo "toolchain dir: ${m_toolchains}"
echo "arch:          ${m_arch}"
echo "cross prefix:  ${m_cpl_prefix}"

qemu_dtb=${rootDir}/${kernelDir}/arch/arm64/boot/dts/arm/corstone1000-fvp.dtb
qemu_dtb=${rootDir}/${kernelDir}/arch/arm64/boot/dts/arm/foundation-v8.dtb
qemu_u_boot=${rootDir}/${ubootDir}/u-boot.bin
qemu_img_uboot=${rootDir}/${ubootDir}/u-boot
qemu_kernel=${rootDir}/${kernelDir}/arch/arm64/boot/Image
qemu_rootfs=${rootDir}/${rootfsDir}/output/images/rootfs.ext4
qemu_img=${rootDir}/${mkImgDir}/${imgName}
qemu_img_log=${rootDir}/${mkImgDir}/${logNmae}

create_dir()
{
    if [ ! -d $1 ]; then echo "create dir $1" >&2; mkdir -p $1; fi
}

build_atf()
{
    # download atf
    # # git clone https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git
    # git clone https://github.com/ARM-software/arm-trusted-firmware.git
    cd ${rootDir}/${atfDir}
    # 由于atf的qemu配置中默认使用GICv2，若需要配置系统使用GICv3，
    # 则修改plat/qemu/qemu/platform.mk中的如下行：
    # QEMU_USE_GIC_DRIVER    := QEMU_GICV2
    # 修改为：
    # QEMU_USE_GIC_DRIVER    := QEMU_GICV3
    make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} PLAT=qemu BL33=${rootDir}/${ubootDir}/u-boot.bin all fip DEBUG=1
}

build_uboot()
{
    # download uboot
    # git clone https://github.com/u-boot/u-boot.git
    cd ${rootDir}/${ubootDir}
    # 若uboot需用semihosting方式加载内核和dtb，则在configs/qemu_arm64_defconfig
    # 中添加以下配置：
    # CONFIG_SEMIHOSTING=y
    # 在编译 U-Boot 时，通常情况下不需要显式指定 ARCH=arm64，因为 U-Boot 的构建
    # 系统会根据你选择的 defconfig 自动推断出正确的架构
    make CROSS_COMPILE=${m_cpl_prefix} qemu_arm64_defconfig
    # sed -i "s/CONFIG_TEXT_BASE=0x00000000/CONFIG_TEXT_BASE=0x200000/g" .config
    # sed -i "s/CONFIG_TEXT_BASE=0x00000000/CONFIG_TEXT_BASE=0x40080000/g" .config
    make CROSS_COMPILE=${m_cpl_prefix} -j 10
}

build_kernel()
{
    # download kernel
    # # git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
    # git clone https://github.com/torvalds/linux.git
    cd ${rootDir}/${kernelDir}
    make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} defconfig
    # make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} -j 20
    make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} -j 20

    # QEMU 默认使用 virt 机器，内核会自动启用它的设备树，不需要额外提供 .dtb 文件。
    # 但如果你要自己编译：
    # make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} dtbs
    # 生成的设备树位于：
    # arch/arm64/boot/dts/qemu/qemu-virt.dtb
}

build_rootfs()
{
    # download buildroot
    # git clone https://gitlab.com/buildroot.org/buildroot.git

    cd ${rootDir}/${rootfsDir}
    make ARCH=${m_arch} CROSS_COMPILE=${m_cpl_prefix} qemu_aarch64_virt_defconfig
    make -j20
}

mk_img()
{
    cd ${rootDir}
    create_dir ${mkImgDir}
    cd ${mkImgDir}
    # loopPnt=$(losetup -f)  # 获取空闲的 loop 设备
    # 使用 dd 生成一个磁盘映像：
    dd if=/dev/zero of=${imgName} bs=1M count=2048

    # 使用 parted 创建 GPT 分区表
    # 创建一个 GPT 分区表
    # sudo parted ${imgName} mklabel gpt
    # 创建一个 MBR 分区表
    sudo parted ${imgName} mklabel msdos
    # Boot 分区 1M起始 100M结束（避开 MBR/GPT 头部）
    sudo parted ${imgName} mkpart primary 1M 100M
    # RootFS 分区
    sudo parted ${imgName} mkpart primary 100M 2048M

    # 使用 losetup 绑定这个镜像到 loop 设备：
    # -f：自动查找一个空闲的 loop 设备（如 /dev/loop0）。
    # -P：强制 解析分区（partition），这样 /dev/loop0p1, /dev/loop0p2 等会自动出现（对应 disk.img 里的分区）。
    # --show：输出实际绑定的 loop 设备路径（如 /dev/loop0）。
    loopPnt=$(sudo losetup --show -fP ${imgName})

    # 如果你想知道分配到哪个 loop 设备，可以运行：
    # losetup -l
    # loopPnt=`losetup -l | grep sdcard | awk '{print $1}'`
    echo "loop mount point: ${loopPnt}"

    # 格式化
    sudo mkfs.vfat ${loopPnt}p1
    sudo mkfs.ext4 ${loopPnt}p2

    # 复制 rootfs 到 RootFs 分区
    sudo dd if=${qemu_rootfs} of=${loopPnt}p2 bs=1M

    # 复制 kernel 和 dtb 到 Boot 分区
    create_dir ${mkImgDir}/fs
    sudo mount ${loopPnt}p1 ${mkImgDir}/fs
    sudo cp ${qemu_kernel} ${mkImgDir}/fs
    # sudo cp ${qemu_dtb} ${mkImgDir}/fs
    sudo umount ${mkImgDir}/fs

    # U-Boot 需要写入磁盘的：
    # conv=notrunc → 确保不会截断 disk.img 的其他数据。
    # sudo dd if=${qemu_u_boot} of=${loopPnt} bs=1M seek=2 conv=notrunc
    sudo dd if=${qemu_u_boot} of=${qemu_img} bs=1M conv=notrunc

    echo "==> img part list"
    sudo fdisk -l ${loopPnt}

    # 解绑
    sudo losetup -d ${loopPnt}
}

boot_qemu()
{

    # boot uboot
    # qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 1G -bios ${qemu_u_boot} -nographic
    # # qemu-system-aarch64 -machine virt -cpu cortex-a57 -bios ${qemu_u_boot} -nographic
    # 某些 QEMU 版本可能不支持 -bios u-boot.bin，可以如下尝试，不过 -kernel 方式
    # 通常适用于直接加载 Linux 内核，而非 U-Boot。
    # qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 1G -kernel ${qemu_u_boot} -nographic
    # -machine virt：使用 QEMU 提供的 virt 机器（适用于 ARM64）。
    # -cpu cortex-a57：指定 CPU 类型。
    # -smp 2：指定 2 核 CPU。
    # -m 1G：分配 1GB 内存。
    # -nographic：禁用图形界面，使用串口输出。
    # -bios u-boot.bin：使用 u-boot.bin 作为固件（BIOS/引导程序）。
    #
    # 如果 U-Boot 启动成功，你会看到类似如下的输出：
    # U-Boot 2025.04-rc3-00052-g0fd7ee0306a8 (Mar 08 2025 - 14:35:07 +0800)
    # 
    # DRAM:  1 GiB
    # Core:  51 devices, 14 uclasses, devicetree: board
    # Flash: 64 MiB
    # Loading Environment from Flash... *** Warning - bad CRC, using default environment
    # 
    # In:    serial,usbkbd
    # Out:   serial,vidconsole
    # Err:   serial,vidconsole
    # No USB controllers found
    # Net:   eth0: virtio-net#32
    #
    # starting USB...
    # No USB controllers found
    # Hit any key to stop autoboot:  0

    # boot kernel
    #
	# qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 1G -nographic \
    # -kernel ${qemu_kernel} \
    # -append "console=ttyAMA0"
    # 其中：
    # -machine virt：使用 QEMU 提供的通用 ARM64 机器。
    # -cpu cortex-a57：指定 CPU 类型。
    # -smp 2：分配 2 核 CPU。
    # -m 1G：分配 1GB 内存。
    # -nographic：使用串口模式，无 GUI。
    # -kernel Image：加载编译好的 ARM64 内核。
    # -append "console=ttyAMA0"：传递启动参数，使内核使用串口输出。
    #
    # 运行后，内核会启动，但没有 RootFS，最终会 panic：
    # Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)
    #
    # 如果你有一个 rootfs.ext4（如 BusyBox 生成的），可以挂载它：
    # qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 1G -nographic \
    #     -kernel ${qemu_kernel} \
    #     -append "console=ttyAMA0 root=/dev/vda rw" \
    #     -drive file=${qemu_rootfs},format=raw,if=virtio
    # 如果 rootfs.ext4 需要手动加载 init：
    # -append "console=ttyAMA0 root=/dev/vda rw init=/bin/sh"
    # 这样可以进入 Shell 进行调试。
    #
    # 如果想使用 U-Boot 来引导 Linux，而不是直接用 -kernel，可以：
    # qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 1G -nographic \
    #     -bios ${qemu_u_boot} \
    #     -kernel ${qemu_kernel} \
    #     -append "console=ttyAMA0 root=/dev/vda rw" \
    #     -drive file=${qemu_rootfs},format=raw,if=virtio



    # boot img
    qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 2G -nographic \
        -drive file=${qemu_img},format=raw,if=virtio \
        -d in_asm,int,cpu_reset -D ${qemu_img_log}
}

# build_uboot
# build_atf
# build_kernel
# build_rootfs
mk_img
# boot_qemu
