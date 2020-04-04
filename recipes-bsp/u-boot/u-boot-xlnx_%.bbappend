FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://iveia-init.c;subdir=git/board/xilinx/common \
    file://Makefile;subdir=git/board/xilinx/common \
    file://iveia-z8.h;subdir=git/include/configs \
    file://add-iveia-z8-config-overriding-xilinx-zynqmp.patch \
    file://fragment.cfg \
    "
