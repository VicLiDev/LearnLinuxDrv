#==============================================================================
# make 定义了很多默认变量，${MAKE} 就是预设的 make 这个命令的名称（或者路径）。
# make -p 可以查看所有预定义的变量的当前值。
#
#==============================================================================
# $(MAKE) -C $(KERNELDIR) M=$(PWD) modules
# Use make M=dir to specify directory of external module to build
#
#==============================================================================
# 最关键的几个变量:CC,obj-m,KERNELDIR
# 例如:
#   CC=arm-none-linux-gnueabi-gcc
#   obj-m:=smodule.o
#   KERNELDIR?=/lib/modules/$(shell uname -r)/build;
# 其中:
#   CC是编译器;
#   obj-m为需要编译的目标模块;
#   KERNELDIR 指的是内核库文件的路径，你的代码中使用的是内核提供的函数，而这些
#   函数也是有具体实现的，在连接成一个内核模块时要说明这些库文件在哪里，方便链接
#   程序把它们连接成一个完成的模块。“?=”表示如果变量值为空则进行赋值
# 
# 注意在编写可加载模块前要有一个内核代码目录树.KERNEL的内核版本必须与运行的内核
# 版本一致,否则编译出的模块往往无法加载.
#
#==============================================================================
# 模块中的makefile脚本运行分析：
#
# KERNELRELEASE 是在内核源码的顶层 Makefile 中定义的一个变量，是一个字符串，用于
# 构建安装目录的名字(一般使用版本号来区分)或者显示当前的版本号。
# 默认情况下，模块会被安装到$(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)中，
# 默认INSTALL_MOD_PATH不会被指定，所以会被安装到/lib/modules/$(KERNELRELEASE)中。
#
# 在第一次读取执行模块中的Makefile时，KERNELRELEASE 没有被定义，所以make将读取
# else之后的内容。如果make的目标是clean，直接执行clean操作，然后结束。当make的
# 目标为all时，-C $(KERNELDIR)指明跳转到内核源码目录下读取那里的Makefile,
# M=$(PWD)表明顶层makfile会调用模块中的makefile，即返回到当前目录继续读入、执行
# 模块中的的Makefile。进行模块中Makefile文件的第二次调用。
#
# 当第二次调用模块中的Makefile时，KERNELRELEASE已被定义，kbuild也被启动去解析
# kbuild语法的语句，make将继续读取else之前的内容。else之前的内容为kbuild语法的
# 语句，指明模块源码中各文件的依赖关系，以及要生成的目标模块名。
#
# param-objs := file1.o file2.o 表示param.o由file1.o与file2.o 连接生成
# obj-m := param.o表示编译连接后将生成param.o模块。
#
#==============================================================================

USERDEMO := "userDemoBase.c"
USERDEMO_EXE := "uDemo"

MYMOD := kDemo
MYMOD_2 := kDemo_2

# 编译模式选择: linux 或 android
BUILD_MODE ?= android

# Android kernel 配置
ANDROID_KERNELDIR ?= ${HOME}/Projects/kernel
ANDROID_TOOLCHAIN_PATH ?= ${HOME}/Projects/prebuilts/toolchains/linux-x86_rk/clang-r487747c/bin
ANDROID_CROSS_COMPILE ?= aarch64-linux-gnu-

# Linux kernel 配置
LINUX_KERNELDIR ?= ${HOME}/Projects/kernel
LINUX_TOOLCHAIN_PATH ?= ${HOME}/Projects/prebuilts/toolchains/aarch64/aarch64-rockchip1240-linux-gnu/bin
LINUX_CROSS_COMPILE ?= aarch64-rockchip1240-linux-gnu-

ifeq ($(BUILD_MODE),android)
    KERNELDIR := $(ANDROID_KERNELDIR)
    # 即使使用 Clang 编译，CROSS_COMPILE 仍然有重要作用，仍然需要设置 CROSS_COMPILE
    # 1. Clang 需要知道目标架构
    #    Clang 不是通过 CROSS_COMPILE 前缀来找编译器，而是通过 --target 参数来
    #    交叉编译。内核 Makefile 会利用 CROSS_COMPILE 来构造这个 target：
    #    # 内部会转换成类似：
    #    clang --target=aarch64-linux-gnu ...
    # 2. 其他工具仍然需要 CROSS_COMPILE
    #    编译内核模块不仅需要 C 编译器，还需要其他工具，这些工具没有 LLVM 替代品
    #    或者仍使用 GNU 版本：
    #    ┌─────────┬────────────────────┬──────────┐
    #    │  工具   │        作用        │ 仍需前缀 │
    #    ├─────────┼────────────────────┼──────────┤
    #    │ objcopy │ 复制并转换目标文件 │ ✓        │
    #    ├─────────┼────────────────────┼──────────┤
    #    │ objdump │ 显示目标文件信息   │ ✓        │
    #    ├─────────┼────────────────────┼──────────┤
    #    │ nm      │ 显示符号表         │ ✓        │
    #    ├─────────┼────────────────────┼──────────┤
    #    │ strip   │ 去除符号           │ ✓        │
    #    └─────────┴────────────────────┴──────────┘
    #    例如最终调用的命令可能是：
    #    clang --target=aarch64-linux-gnu -c xxx.c
    #    aarch64-linux-gnu-objcopy -O binary ...
    CROSS_COMPILE := $(ANDROID_CROSS_COMPILE)
    ARCH := arm64
    # 1. LLVM := 1
    #    作用：告诉内核构建系统使用 LLVM/Clang 工具链，而不是 GCC
    #    原因：Android 内核从 Android 10（Kernel 4.14/4.19）开始强制使用 Clang 编译。
    #          Google 要求 Android 内核必须用 Clang 编译，以确保与 Android 系统
    #          其他组件的二进制兼容性
    # 2. LLVM_IAS := 1
    #   作用：启用 LLVM 的集成汇编器
    #   IAS = Integrated Assembler
    #   原因：Clang 的集成汇编器对某些新的汇编语法支持更好。启用后，汇编代码会由
    #         Clang 的内部汇编器处理，而不是调用外部的 as 命令
    # 3. CC := clang
    #   作用：显式指定 C 编译器为 clang
    #   原因：覆盖内核 Makefile 中默认的 $(CROSS_COMPILE)gcc，强制使用 Clang
    LLVM := 1
    LLVM_IAS := 1
    CC := clang
else
    KERNELDIR := $(LINUX_KERNELDIR)
    # Linux 内核通常仍使用 GCC 工具链（如 aarch64-linux-gnu-gcc），所以不需要
    # 这些 LLVM 相关参数。
    CROSS_COMPILE := $(LINUX_CROSS_COMPILE)
    ARCH := arm64
endif


ifneq ($(KERNELRELEASE),)

DEMO_GIT_VERSION := \
	$(shell cd $(PWD); git log -1 --no-decorate --date=short \
	--pretty=format:"%h author: %<|(30)%an %cd %s" -- $(src) \
	|| echo -n "unknown git version info, pwd:"`pwd`)

$(info "======> git version "$(DEMO_GIT_VERSION))

CFLAGS_$(MYMOD).o += -DDEMO_GIT_VERSION="\"$(DEMO_GIT_VERSION)\""

obj-m := $(MYMOD).o
obj-m += $(MYMOD_2).o

else

PWD := $(shell pwd)

ifeq ($(BUILD_MODE),android)
    export PATH := $(ANDROID_TOOLCHAIN_PATH):${PATH}
else
    export PATH := $(LINUX_TOOLCHAIN_PATH):${PATH}
endif

.PHONY: m_modules
m_modules:
ifeq ($(BUILD_MODE),android)
	@echo "======> Android kernel build mode"
	@echo "======> TOOLCHAIN: $(ANDROID_TOOLCHAIN_PATH)"
	@echo "======> KERNELDIR: $(KERNELDIR)"
	@echo "======> CROSS_COMPILE: $(CROSS_COMPILE)"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		LLVM=$(LLVM) LLVM_IAS=$(LLVM_IAS) CC=$(CC) modules
	@echo "======> Building user demo with clang <======"
	clang --target=$(ANDROID_CROSS_COMPILE)aarch64-linux-android -o $(USERDEMO_EXE) $(USERDEMO) -static
else
	@echo "======> Linux kernel build mode"
	@echo "======> TOOLCHAIN: $(LINUX_TOOLCHAIN_PATH)"
	@echo "======> KERNELDIR: $(KERNELDIR)"
	@echo "======> CROSS_COMPILE: $(CROSS_COMPILE)"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	@echo "======> Building user demo with cross compiler <======"
	$(CROSS_COMPILE)gcc -o $(USERDEMO_EXE) $(USERDEMO) -static
endif

.PHONY: help
help:
	@echo "Usage: make [target] [BUILD_MODE=android|linux]"
	@echo ""
	@echo "Targets:"
	@echo "  help              Show this help message"
	@echo "  modules_android   Build Android kernel modules"
	@echo "  modules_linux     Build Linux kernel modules"
	@echo "  clean             Clean build artifacts"
	@echo "  init              Push and load modules to device"
	@echo "  exit              Unload modules from device"
	@echo "  test              Run test on device"
	@echo ""
	@echo "Examples:"
	@echo "  make modules_android          # Build Android kernel modules"
	@echo "  make modules_linux            # Build Linux kernel modules"
	@echo "  make BUILD_MODE=android init  # Init modules on Android device"
	@echo "  make BUILD_MODE=linux exit    # Exit modules on Linux device"
	@echo "  make BUILD_MODE=linux test    # Run test on Linux device"
	@echo ""
	@echo "Default BUILD_MODE: android"

.PHONY: modules_android
modules_android:
	$(MAKE) BUILD_MODE=android m_modules

.PHONY: modules_linux
modules_linux:
	$(MAKE) BUILD_MODE=linux m_modules

.PHONY: clean
clean:
	@echo "======> clean build artifacts <======"
	rm -rf *.o *~ core .depend .*.cmd .*.o.d *.mod *.ko *.mod.c .tmp_versions Module* modules*
	rm -f $(USERDEMO_EXE)

.PHONY: init
init:
	@echo "======> init <======"
	@adbCmd=$$(adbs); \
	if [ "$(BUILD_MODE)" = "android" ]; then \
		demo_path="/vendor/bin"; \
	else \
		demo_path="/usr/bin"; \
	fi; \
	echo "======> BUILD_MODE: $(BUILD_MODE) <======"; \
	echo "======> ADB command: $$adbCmd <======"; \
	echo "======> Demo path: $$demo_path <======"; \
	echo "======> Pushing modules to device <======"; \
	$$adbCmd push $(MYMOD).ko /data/; \
	$$adbCmd push $(MYMOD_2).ko /data/; \
	$$adbCmd push $(USERDEMO_EXE) $$demo_path/; \
	$$adbCmd shell chmod 755 $$demo_path/$(USERDEMO_EXE); \
	echo "======> Loading modules <======"; \
	$$adbCmd shell insmod /data/$(MYMOD).ko; \
	$$adbCmd shell insmod /data/$(MYMOD_2).ko; \
	echo "======> Verify loaded modules <======"; \
	$$adbCmd shell lsmod | grep -E "$(MYMOD)|$(MYMOD_2)"

.PHONY: exit
exit:
	@echo "======> exit <======"
	@adbCmd=$$(adbs); \
	echo "======> ADB command: $$adbCmd <======"; \
	echo "======> Unloading modules <======"; \
	$$adbCmd shell rmmod $(MYMOD_2); \
	$$adbCmd shell rmmod $(MYMOD)

test:
	@echo "======> test <======"
	@adbCmd=$$(adbs); \
	if [ "$(BUILD_MODE)" = "android" ]; then \
		demo_path="/vendor/bin"; \
	else \
		demo_path="/usr/bin"; \
	fi; \
	echo "======> ADB command: $$adbCmd <======"; \
	echo "======> Running test on device <======"; \
	$$adbCmd shell $$demo_path/$(USERDEMO_EXE) -b

endif
