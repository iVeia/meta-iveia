# SPDX-License-Identifier: GPL-2.0+

obj-y	+= board.o
obj-$(CONFIG_ARCH_ZYNQMP) += zynqmp.o
obj-$(CONFIG_ARCH_ZYNQMP) += cpu-info.o
obj-$(CONFIG_ARCH_ZYNQ) += zynq.o
obj-y	+= iveia-init.o
obj-$(CONFIG_EEPROM_IPMI_ALTERNATE_FOR_00130) += eeprom-ipmi-00130.o

# iVeia IPMI FRU info utility
obj-y += ivfru.o
obj-y += ivfru_common.o
obj-y += ivfru_plat.o


