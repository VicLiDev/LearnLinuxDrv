#==============================================================================
# DMA Demo Makefile
#
# DMA driver demo - demonstrates various Linux kernel DMA API usages
#
#==============================================================================

USERDEMO := userDemoBase.c
USERDEMO_EXE := uDemo

MYMOD := kDemo

ifneq ($(KERNELRELEASE),)

# Git version info - simplified to avoid build errors
DEMO_GIT_VERSION := \
	$(shell git log -1 --no-decorate --date=short \
	--pretty=format:"%h author: %<|(30)%an %cd %s" 2>/dev/null \
	|| echo "unknown")

CFLAGS_$(MYMOD).o += -DDEMO_GIT_VERSION="\"$(DEMO_GIT_VERSION)\""

obj-m := $(MYMOD).o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: modules
modules:
	@echo "======> build DMA module <======"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	gcc -o $(USERDEMO_EXE) $(USERDEMO)

.PHONY: clean
clean:
	@echo "======> clean <======"
	rm -rf *.o *~ core .depend .*.cmd .*.o.d *.mod *.ko *.mod.c .tmp_versions Module* modules*
	rm -f $(USERDEMO_EXE)

.PHONY: init
init:
	@echo "======> init <======"
	sudo insmod ./$(MYMOD).ko init_desc="DMA demo init" exit_desc="DMA demo exit"
	sudo lsmod | grep -E "$(MYMOD)"
	@echo ""
	@echo "DMA module loaded, device nodes: /dev/m_chrdev_0, /dev/m_chrdev_1"
	@echo "Use 'make test' to run tests"
	@echo "Use 'make log' to view kernel logs"

.PHONY: exit
exit:
	@echo "======> exit <======"
	sudo rmmod $(MYMOD)

.PHONY: test
test:
	@echo "======> test <======"
	./uDemo -h

.PHONY: test-all
test-all:
	@echo "======> Running all DMA tests <======"
	./uDemo -a

.PHONY: test-coherent
test-coherent:
	@echo "======> Testing Coherent DMA <======"
	./uDemo -c

.PHONY: test-single
test-single:
	@echo "======> Testing Streaming DMA Single Mapping <======"
	./uDemo -s

.PHONY: test-sg
test-sg:
	@echo "======> Testing Scatter-Gather DMA <======"
	./uDemo -g

.PHONY: test-pool
test-pool:
	@echo "======> Testing DMA Pool <======"
	./uDemo -p

.PHONY: test-info
test-info:
	@echo "======> Getting DMA Information <======"
	./uDemo -i

.PHONY: log
log:
	@echo "======> kernel log <======"
	sudo dmesg -w | grep -E "DMA:| dma:"

.PHONY: log-show
log-show:
	@echo "======> showing kernel log <======"
	sudo dmesg | grep -E "DMA:| dma:" | tail -50

.PHONY: help
help:
	@echo "DMA Demo Makefile targets:"
	@echo "  make modules   - Build kernel module and userspace test program"
	@echo "  make clean     - Clean build files"
	@echo "  make init      - Load kernel module"
	@echo "  make exit      - Unload kernel module"
	@echo "  make test      - Show test program help"
	@echo "  make test-all  - Run all DMA tests"
	@echo "  make test-coherent  - Test coherent DMA"
	@echo "  make test-single    - Test streaming DMA single mapping"
	@echo "  make test-sg        - Test scatter-gather DMA"
	@echo "  make test-pool      - Test DMA pool"
	@echo "  make test-info      - Get DMA information"
	@echo "  make log        - Watch kernel DMA logs in real-time"
	@echo "  make log-show   - Show recent DMA logs"
	@echo "  make help       - Show this help message"

endif
