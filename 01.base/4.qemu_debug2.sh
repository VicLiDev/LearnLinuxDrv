#!/usr/bin/env bash
#########################################################################
# File Name: 4.qemu_debug2.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Wed 20 Nov 2024 11:50:25 AM CST
#########################################################################

wk_dir=`pwd`
atf_dir="${wk_dir}/trusted-firmware-a"
u_boot_dir="${wk_dir}/u-boot"
ker_dir="${HOME}/Projects/linux"
buildroot_dir="${wk_dir}/buildroot"
log_dir="${wk_dir}/log"

build_atf()
{
    echo "<<<<<< build atf begin >>>>>>"
    cd ${wk_dir}
    if [ ! -e "${atf_dir}" ]; then
        git clone https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git
    fi
    cd ${atf_dir}

    export ARCH=arm64
    export CROSS_COMPILE=aarch64-linux-gnu-
    make PLAT=qemu BL33=${wk_dir}/u-boot/u-boot.bin all fip DEBUG=1
    echo "<<<<<< build atf done >>>>>>"
}

build_u_boot()
{
    echo "<<<<<< build u_boot begin >>>>>>"
    cd ${wk_dir}
    if [ ! -e "${u_boot_dir}" ]; then
        git clone https://source.denx.de/u-boot/u-boot.git
    fi
    cd ${u_boot_dir}

    export ARCH=arm64
    export CROSS_COMPILE=aarch64-linux-gnu-
    make qemu_arm64_defconfig
    make
    echo "<<<<<< build u_boot done >>>>>>"
}

build_ker()
{
    echo "<<<<<< build ker begin >>>>>>"
    cd ${wk_dir}
    if [ ! -e "${ker_dir}" ]; then
        echo "please git clone linux kernel source"
    fi
    cd ${ker_dir}

    export ARCH=arm64
    export CROSS_COMPILE=aarch64-linux-gnu-
    make defconfig
    make -j 10
    echo "<<<<<< build ker done >>>>>>"
}

build_buildroot()
{
    echo "<<<<<< build buildroot begin >>>>>>"
    cd ${wk_dir}
    if [ ! -e "${buildroot_dir}" ]; then
        git clone git://git.buildroot.net/buildroot
    fi
    cd ${buildroot_dir}

    export ARCH=arm64
    export CROSS_COMPILE=aarch64-linux-gnu-
    make defconfig
    make menuconfig
    echo "select output target format in menu: Filesystem images  --->"
    make BR2_TARGET_ROOTFS_EXT4=y BR2_ROOTFS_CPIO=y BR2_ROOTFS_CPIO_GZIP=y -j 10
    echo "<<<<<< build buildroot done >>>>>>"
}

# 可以直接安装
build_qemu()
{
    echo "<<<<<< build qemu begin >>>>>>"
    cd ${wk_dir}
    if [ ! -e "${qemu}" ]; then
        git clone https://gitlab.com/qemu-project/qemu.git
    fi

    cd ${qemu}
    git submodule init
    git submodule update --recursive
    # sudo apt-get install libcap-dev
    echo "<<<<<< build qemu done >>>>>>"
}

run_qemu()
{
    [ ! -e ${log_dir} ] && mkdir ${log_dir}
    cur_time=$(date +%Y-%m-%d-%H-%M-%S)
    qemu-system-aarch64 \
        -M virt,gic_version=3\
        -cpu cortex-a53 \
        -smp 2 \
        -m 2048M \
        -kernel ${ker_dir}/arch/arm64/boot/Image \
        -drive file=${buildroot_dir}/output/images/rootfs.ext4,if=none,id=blk1,format=raw \
        -device virtio-blk-device,drive=blk1 \
        -append "console=ttyAMA0 root=/dev/vda" \
        -nographic -d guest_errors,unimp -D log/$cur_time \
        -dtb ${ker_dir}/arch/arm64/boot/dts/qemu/test-board.dtb
}

run_qemu_uboot()
{
    [ ! -e ${log_dir} ] && mkdir ${log_dir}
    qemu-system-aarch64 \
        -M  virt \
        -cpu cortex-a53 \
        -smp 2 \
        -m 2048M \
        -kernel ${u_boot_dir}/u-boot \
        -nographic
}

run_qemu_atf()
{
    [ ! -e ${log_dir} ] && mkdir ${log_dir}
    cur_time=$(date +%Y-%m-%d-%H-%M-%S)
    qemu-system-aarch64 -nographic -machine virt,secure=on,gic_version=3 \
        -cpu cortex-a53 \
        -smp 2 -m 2048 \
        -d guest_errors,unimp -D log/$cur_time \
        -bios ${atf_dir}/build/qemu/debug/bl1.bin \
        -drive file=${buildroot_dir}/output/images/rootfs.ext4,if=none,id=blk1,format=raw \
        -device virtio-blk-device,drive=blk1 \
        -semihosting-config enable=on,target=native
}


main()
{
    # build_u_boot
    # build_atf
    # build_ker
    # build_buildroot
    # build_qemu
    run_qemu
    # run_qemu_uboot
    # run_qemu_atf
}

main $@
