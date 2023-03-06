SOC_VARIANT ?= "7z"

require conf/machine/include/soc-zynq.inc
require conf/machine/include/machine-xilinx-default.inc

MACHINE_FEATURES = "rtc ext2 ext3 vfat usbhost"

#
# Atlas Z7 is based off of ZC702 (with mods to the u-boot recipe)
#
UBOOT_MACHINE = "zynq_zc702_config"

SERIAL_CONSOLE = "115200 ttyPS0"
SERIAL_CONSOLES_CHECK = "${SERIAL_CONSOLES}"

