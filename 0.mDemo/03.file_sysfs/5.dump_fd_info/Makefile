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

MYMOD := kDemo

ifneq ($(KERNELRELEASE),)

DEMO_GIT_VERSION := \
	$(shell cd $(PWD); git log -1 --no-decorate --date=short \
	--pretty=format:"%h author: %<|(30)%an %cd %s" -- $(src) \
	|| echo -n "unknown git version info, pwd:"`pwd`)

$(info "======> git version"$(DEMO_GIT_VERSION))

CFLAGS_$(MYMOD).o += -DDEMO_GIT_VERSION="\"$(DEMO_GIT_VERSION)\""

obj-m := $(MYMOD).o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

.PHONY: clean
clean:
	@echo "======> build <======"
	rm -rf *.o *~ core .depend .*.cmd .*.o.d *.mod *.ko *.mod.c .tmp_versions Module* modules*

.PHONY: init
init:
	@echo "======> init <======"
	@#==> 安装模块
	sudo insmod ./$(MYMOD).ko init_desc="init_desc_from_cmd_line" exit_desc="exit_desc_from_cmd_line"
	@#==> modprobe 命令比 insmod 命令更强大，他在加载某模块时会同时加载该模块所依赖的其他模块
	@#    使用modprobe命令加载的模块如果使用 modprobe -r <fileName> 的方式卸载，将同时卸载其
	@#    依赖的模块
	@#==> 模块间的依赖关系存放在根文件系统的 /lib/modules/<kernel_version>/modules.dep文件中
	@#    也可以使用modinfo <模块名>命令查看模块信息
	@#sudo modprobe ./$(MYMOD).ko
	@#==>  lsmod 可以获得系统中已加载的所有模块以及模块间的依赖关系
	sudo lsmod | grep -E "$(MYMOD)"

.PHONY: exit
exit:
	@echo "======> exit <======"
	sudo rmmod $(MYMOD)
	@#sudo modprobe -r ./$(MYMOD).ko

test:
	@echo "======> test <======"
	./uDemo -b

endif
