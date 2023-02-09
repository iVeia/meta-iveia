iv-zap-objs := zap.o pool.o dma.o
iv-zap-objs += $(if $(CONFIG_ARCH_ZYNQ),dma_zynq.o)
iv-zap-objs += $(if $(CONFIG_ARCH_ZYNQMP),dma_zynq.o)
obj-m := iv-zap.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
