FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://iveia-init.c;subdir=git/board/xilinx/common \
    file://Makefile;subdir=git/board/xilinx/common \
    file://fragment.cfg \
    "
