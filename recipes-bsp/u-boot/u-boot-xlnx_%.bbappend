FILESEXTRAPATHS_prepend := "${THISDIR}/../shared/files:"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

# Kconfig
SRC_URI_append = "\
    file://include-iVeia-Kconfig.patch \
    file://Kconfig;subdir=git/board/iveia \
    "

# C sources
SRC_URI_append = "\
    file://iveia-init.c;subdir=git/board/xilinx/common \
    file://iveia-ipmi.h;subdir=git/board/xilinx/common \
    file://eeprom-ipmi-00130.c;subdir=git/board/xilinx/common \
    file://iveia-config.h;subdir=git/include/configs \
    "

# Patches & CFG
SRC_URI_append = "\
    file://xilinx-board-common.patch \    
    file://setexpr-Add-explicit-support-for-32-and-64-bit-ints.patch \
    file://Fix-saveenv-causes-next-boot-to-skip-board_late_init.patch \
    file://add-iv-ipmi-fdt-hooks.patch \
    file://uboot.cfg \
    file://include-ivfru-command.patch \
    file://ivfru.c;subdir=git/cmd \
    file://ivfru_common.h;subdir=git/include \
    file://ivfru_common.c;subdir=git/cmd \
    file://ivfru_plat.h;subdir=git/cmd \
    file://ivfru_plat.c;subdir=git/cmd \
    "

# MACHINE specific patches
SRC_URI_append_00049 = "\
    file://uboot-00049.cfg \
    "
SRC_URI_append_00068 = "\
    file://uboot-00068.cfg \
    "
SRC_URI_append_00130 = "\
    file://uboot-00130.cfg \
    "
SRC_URI_append_zynq = "\
    file://add-iveia-config-overriding-xilinx-zynq.patch \
    "
SRC_URI_append_zynqmp = "\
    file://add-iveia-config-overriding-xilinx-zynqmp.patch \
    "

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

# U-Boot requires a DTB file that it includes in the image, even though it is
# not used (the one in boot.bin is used - see the xilinx-bootbin recipe).
# However, in the device-tree recipe, we've renamed it to ${MACHINE}.dtb, so it
# has to match here.
BASE_DTS = "${MACHINE}"
