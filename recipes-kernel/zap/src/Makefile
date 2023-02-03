zap-objs := main.o pool.o dma.o
zap-objs += $(if $(CONFIG_ARCH_ZYNQ),dma_zynq.o)
zap-objs += $(if $(CONFIG_ARCH_ZYNQMP),dma_zynq.o)
obj-m := zap.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
