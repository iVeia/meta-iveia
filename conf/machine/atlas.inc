require conf/machine/include/machine-xilinx-default.inc

MACHINE_FEATURES = "rtc ext2 ext3 vfat usbhost"
EXTRA_IMAGE_FEATURES += "fpga-manager"

UBOOT_MACHINE = "xilinx_${SOC_FAMILY}_virt_config"

SERIAL_CONSOLE = "115200 ttyPS0"
SERIAL_CONSOLES_CHECK = "${SERIAL_CONSOLES}"

ERROR_QA:remove = "version-going-backwards"
