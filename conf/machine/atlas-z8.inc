SOC_VARIANT ?= "eg"

#require conf/machine/include/xilinx-soc-family.inc
#require conf/machine/include/machine-xilinx-default.inc
require conf/machine/include/soc-zynqmp.inc
require conf/machine/include/machine-xilinx-default.inc

MACHINE_FEATURES = "rtc ext2 ext3 vfat usbhost"
MACHINEOVERRIDES =. "atlas-z8:"
EXTRA_IMAGE_FEATURES += "fpga-manager"

#
# Atlas Z8 is based off of ZCU102 (with mods to the u-boot recipe)
#
UBOOT_MACHINE = "xilinx_zynqmp_virt_defconfig"

SERIAL_CONSOLE = "115200 ttyPS0"
SERIAL_CONSOLES_CHECK = "${SERIAL_CONSOLES}"

PREFERRED_PROVIDER_virtual/kernel ?= "linux-xlnx"
PREFERRED_PROVIDER_virtual/bootloader ?= "u-boot-xlnx"


