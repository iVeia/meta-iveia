FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI_append_zynqmp = "\
    file://banner-added-additional-CRLF.patch \
    file://iveia-version-banner.patch \
    file://QSPI32-Workaround-for-unfinished-QSPI-transfer.patch \
    file://force-XSDPS_DEFAULT_SPEED_MODE-for-EMMC-boot-target.patch \
    "
SRC_URI_append_zynq = "\
    file://iveia-version-banner-zynq.patch \
    file://weaken-fsbl-hooks-zynq.patch \
    file://ps7_init-pre-and-post-hooks.patch \
    "

SRC_URI_append_00108 = " file://eMMC-HS200-speed-workaround.patch"

# These patches must be applied after configure because configure dynamically
# creates some sources (based off of the XSA hardware def), and these patches
# are applied to them.
do_configure[postfuncs] += "do_post_configure_copy"
do_post_configure_copy () {
    if [ -n "${FSBL_SRCS}" ]; then
        cp ${FSBL_SRCS} ${B}/fsbl-firmware
    fi
}
inherit post-configure-patches
POST_CONFIGURE_PATCHES_zynqmp := "\
    ${THISDIR}/files/add-iveia-init-hook.patch \
    ${THISDIR}/files/add_sequence_boot.patch \
    ${THISDIR}/files/ddr-dots.patch \
    "

# Remove patch ddr-dots which fails on 00146
POST_CONFIGURE_PATCHES_remove_00146 := "\
    ${THISDIR}/files/ddr-dots.patch \
    "

# Remove patch ddr-dots which conflicts with helios-z8-mods
POST_CONFIGURE_PATCHES_remove_00126 := "\
    ${THISDIR}/files/ddr-dots.patch \
    "
POST_CONFIGURE_PATCHES_append_00126 := "\
    ${THISDIR}/files/helios-z8-mods.patch \
    "

FSBL_DIR := "${THISDIR}"

FSBL_SRCS_zynqmp := "\
    ${THISDIR}/files/iv_z8_init.c \
    ${THISDIR}/files/*.h \
    ${THISDIR}/files/iv_z8_sequence_boot.c \
    ${THISDIR}/files/iv_z8_sequence_boot.h \
    ${THISDIR}/files/iv_z8_user_boot_sequence.c \
    ${THISDIR}/../shared/files/iveia-ipmi.h \
    "
FSBL_SRCS_zynq := "\
    ${THISDIR}/files/zynq-hooks.c \
    "
FSBL_SRCS_append_00104 := " ${THISDIR}/files/00104.c"
FSBL_SRCS_append_00104-fs001 := " ${THISDIR}/files/00104.c"
FSBL_SRCS_append_00104-fs002 := " ${THISDIR}/files/00104.c"
FSBL_SRCS_append_00108 := " ${THISDIR}/files/00108.c"
FSBL_SRCS_append_00114 := " ${THISDIR}/files/00114.c"
FSBL_SRCS_append_00126 := " ${THISDIR}/files/00126.c"
FSBL_SRCS_append_00127 := " ${THISDIR}/files/00127.c"
FSBL_SRCS_append_00146 := " ${THISDIR}/files/00104.c"

YAML_COMPILER_FLAGS_append_00049 = " -DNO_CLKGEN"
YAML_COMPILER_FLAGS_append_00102 = " -DNO_I2C"

inherit switch-uart
XPARAMETERS_H = "${B}/fsbl-firmware/zynqmp_fsbl_bsp/psu_cortexa53_0/include/xparameters.h"
SWITCH_UART_00126 := "0"

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${B}/fsbl-firmware/iveia_version.h"

#YAML_COMPILER_FLAGS_append = " -DFSBL_DEBUG -DFSBL_DEBUG_INFO"

