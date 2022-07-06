FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI_append_zynqmp = "\
    file://banner-added-additional-CRLF.patch \
    file://iveia-version-banner.patch \
    file://QSPI32-Workaround-for-unfinished-QSPI-transfer.patch \
    file://force-XSDPS_DEFAULT_SPEED_MODE-for-EMMC-boot-target.patch \
    "
SRC_URI_append_zynq = "\
    file://iveia-version-banner-zynq.patch \
    "

SRC_URI_append_atlas-i-z8 = " file://eMMC-HS200-speed-workaround.patch"

# These patches must be applied after configure because configure dynamically
# creates some sources (based off of the XSA hardware def), and these patches
# are applied to them.
do_configure[postfuncs] += "do_post_configure_copy"
do_post_configure_copy () {
    if [ -n "${FSBL_SRCS}" ]; then
        cp ${FSBL_SRCS} ${B}/fsbl
    fi
}
inherit post-configure-patches
POST_CONFIGURE_PATCHES_zynqmp := "\
    ${THISDIR}/files/add-iveia-init-hook.patch \
    ${THISDIR}/files/set-secondary-boot-mode-register.patch \
    ${THISDIR}/files/add_sequence_boot.patch \
    "
FSBL_DIR := "${THISDIR}"

FSBL_SRCS_zynqmp := "\
    ${THISDIR}/files/iv_z8_init.c \
    ${THISDIR}/files/iv_z8_sequence_boot.c \
    ${THISDIR}/files/iv_z8_sequence_boot.h \
    ${THISDIR}/files/iv_z8_user_boot_sequence.c \
    ${THISDIR}/../shared/files/iveia-ipmi.h \
    "
FSBL_SRCS_append_atlas-iii-z8e := " ${THISDIR}/files/${MACHINE}.c"
FSBL_SRCS_append_atlas-iii-z8 := " ${THISDIR}/files/${MACHINE}.c"
FSBL_SRCS_append_atlas-iii-z8-var10 := " ${THISDIR}/files/atlas-iii-z8.c"
FSBL_SRCS_append_atlas-iii-z8-t2 := " ${THISDIR}/files/atlas-iii-z8.c"
FSBL_SRCS_append_atlas-ii-z8ev := " ${THISDIR}/files/${MACHINE}.c"
FSBL_SRCS_append_atlas-i-z8 := " ${THISDIR}/files/${MACHINE}.c"

inherit switch-uart
XPARAMETERS_H = "${B}/fsbl/zynqmp_fsbl_bsp/psu_cortexa53_0/include/xparameters.h"

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${B}/fsbl/iveia_version.h"
