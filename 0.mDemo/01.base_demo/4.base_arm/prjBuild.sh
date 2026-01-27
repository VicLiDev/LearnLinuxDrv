#!/bin/bash
#########################################################################
# File Name: prjBuild.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Tue 14 May 2024 02:18:06 PM CST
#########################################################################

# 编译模式选择: linux 或 android
BUILD_MODE=""

###################################
# compile tools select
###################################
ker_sel=""
tc_sel=""
cmp_sel=""
# kernel
source ${HOME}/bin/select_node.sh
ker_paths_possible=(
    ${HOME}/Projects/kernel
    ${HOME}/Projects/kernel2
    ${HOME}/Projects/kernel3
    ${HOME}/Projects/kernel4
    )
# android toolchains
a_tc_possible=(
    ${HOME}/Projects/prebuilts/toolchains/linux-x86_rk/clang-r487747c/bin
    )
# android compilers
a_cmp=(
    aarch64-linux-gnu-
    )
# linux toolchains
l_tc_possible=(
    ${HOME}/Projects/prebuilts/toolchains/aarch64/aarch64-rockchip1240-linux-gnu/bin
    )
# linux compilers
l_cmp=(
    aarch64-rockchip1240-linux-gnu-
    )


# 编译操作选择: build, init, exit, test
OP_TYPE="build"

# 显示帮助信息
function show_help()
{
    echo "Usage: $0 [linux|android] [build|init|exit|test]"
    echo ""
    echo "Examples:"
    echo "  $0 linux         # build Linux kernel modules"
    echo "  $0 android       # build Android kernel modules"
    echo "  $0 linux init    # init modules on device"
    echo "  $0 linux exit    # exit modules on device"
    echo "  $0 linux test    # test modules on device"
    echo ""
    echo "config info:"
    echo "  Linux kernel:"
    echo "    toolchain: aarch64-rockchip1240-linux-gnu-"
    echo "    kerneldir: /home/lhj/Projects/rk/kernel2"
    echo ""
    echo "  Android kernel:"
    echo "    toolchain: clang (LLVM)"
    echo "    kerneldir: /home/lhj/Projects/rk/kernel"
}

function proc_paras()
{
    BUILD_MODE="${1:-android}"
    OP_TYPE="${2:-build}"

    if [ "${BUILD_MODE}" = "-h" ] || [ "${BUILD_MODE}" = "--help" ]; then
        show_help
        exit 0
    fi

    if [ "${BUILD_MODE}" != "linux" ] && [ "${BUILD_MODE}" != "android" ]; then
        echo "error: unknown build mode '${BUILD_MODE}'"
        echo ""
        show_help
        exit 1
    fi

    if [ "${OP_TYPE}" != "build" ] && [ "${OP_TYPE}" != "init" ] && \
        [ "${OP_TYPE}" != "exit" ] && [ "${OP_TYPE}" != "test" ]; then
        echo "error: unknown op type '${OP_TYPE}'"
        echo ""
        show_help
        exit 1
    fi

    echo "======> build mode: ${BUILD_MODE}"
    echo "======> op type: ${OP_TYPE}"
}

function select_ker()
{
    kernel_paths=()

    for cur_var in "${ker_paths_possible[@]}"
    do
        [ -d "${cur_var}" ] && kernel_paths+=("${cur_var}")
    done

    select_node "arm_ko_kernel_dir: " "kernel_paths" "ker_sel" "select kernel dir"
}

function select_tools()
{
    tc_paths=()

    if [ "${BUILD_MODE}" = "android" ]; then
        for cur_var in "${a_tc_possible[@]}"
        do
            [ -d "${cur_var}" ] && tc_paths+=("${cur_var}")
        done
        select_node "arm_ko_tc_dir: " "tc_paths" "tc_sel" "select toolchain"

        select_node "arm_ko_cmp_dir: " "a_cmp" "cmp_sel" "select compiler"
    else
        for cur_var in "${l_tc_possible[@]}"
        do
            [ -d "${cur_var}" ] && tc_paths+=("${cur_var}")
        done
        select_node "arm_ko_tc_dir: " "tc_paths" "tc_sel" "select toolchain"

        select_node "arm_ko_cmp_dir: " "l_cmp" "cmp_sel" "select compiler"
    fi

}

function exe_opt()
{
    if [ "${OP_TYPE}" = "build" ]; then
        echo "======> clean up old compile files"
        make clean

        if [ "${BUILD_MODE}" = "android" ]; then
            echo "======> build Android kernel ko"
            make modules_android \
                ANDROID_KERNELDIR=${ker_sel} \
                ANDROID_TOOLCHAIN_PATH=${tc_sel} \
                ANDROID_CROSS_COMPILE=${cmp_sel}
        else
            echo "======> build Linux kernel ko"
            make modules_linux \
                LINUX_KERNELDIR=${ker_sel} \
                LINUX_TOOLCHAIN_PATH=${tc_sel} \
                LINUX_CROSS_COMPILE=${cmp_sel}
        fi

        if [ $? -eq 0 ]; then
            echo "======> build success!"
            echo ""
            echo "generated module files:"
            ls -lh *.ko 2>/dev/null || echo "  can't find .ko file"
            echo ""
        else
            echo "======> build failed!"
            exit 1
        fi
    elif [ "${OP_TYPE}" = "init" ]; then
        echo "======> init modules on device"
        make BUILD_MODE=${BUILD_MODE} init
    elif [ "${OP_TYPE}" = "exit" ]; then
        echo "======> exit modules on device"
        make BUILD_MODE=${BUILD_MODE} exit
    elif [ "${OP_TYPE}" = "test" ]; then
        echo "======> test modules on device"
        make BUILD_MODE=${BUILD_MODE} test
    fi
}

function main()
{
    proc_paras $@

    # build 操作需要选择内核和工具链，其他操作不需要
    if [ "${OP_TYPE}" = "build" ]; then
        select_ker
        select_tools
    fi

    exe_opt
}

main $@
