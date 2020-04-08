FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://iveia-init.c;subdir=git/board/xilinx/common \
    file://Makefile;subdir=git/board/xilinx/common \
    file://iveia-z8.h;subdir=git/include/configs \
    file://add-iveia-z8-config-overriding-xilinx-zynqmp.patch \
    file://zynqmp-blank-out-repeated-boot_targets.patch \
    file://Fix-saveenv-causes-next-boot-to-skip-board_late_init.patch \
    file://fragment.cfg \
    "

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"
