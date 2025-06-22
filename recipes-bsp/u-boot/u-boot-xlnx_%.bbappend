FILESEXTRAPATHS:prepend := "${THISDIR}/../shared/files:"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = "\
    file://atlas/;subdir=git/board/iveia \
    file://append-iv_board_late_init-to-chain.patch \
    file://Fix-saveenv-causes-next-boot-to-skip-board_late_init.patch \
    file://add-iv-ipmi-fdt-hooks.patch \
    file://uboot.cfg \
    "

# Shared ivfru files must be compied separately to atlas dir
SRC_URI:append = "\
    file://ivfru_common.h;subdir=git/board/iveia/atlas \
    file://ivfru_common.c;subdir=git/board/iveia/atlas \
    file://ivfru_plat.h;subdir=git/board/iveia/atlas \
    file://iveia-ipmi.h;subdir=git/board/iveia/atlas \
    "

# MACHINE specific patches
SRC_URI:append:00049 = "\
    file://uboot-00049.cfg \
    "
SRC_URI:append:00068 = "\
    file://uboot-00068.cfg \
    "
SRC_URI:append:00130 = "\
    file://uboot-00130.cfg \
    "

do_configure:append() {
    echo  "#undef CFG_EXTRA_ENV_SETTINGS" >> ${S}/include/configs/xilinx_zynqmp.h
    echo  'source "board/iveia/atlas/Kconfig"' >> ${S}/Kconfig
}

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

# U-Boot requires a DTB file that it includes in the image, even though it is
# not used (the one in boot.bin is used - see the xilinx-bootbin recipe).
# However, in the device-tree recipe, we've renamed it to ${MACHINE}.dtb, so it
# has to match here.
BASE_DTS = "${MACHINE}"
