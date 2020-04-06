FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " file://iveia-version-banner.patch"

# These patches must be applied after configure because configure dynamically
# creates some sources (based off of the XSA hardware def), and these patches
# are applied to them.
inherit post-configure-patches
POST_CONFIGURE_PATCHES := "\
    ${THISDIR}/files/add-iveia-init-hook.patch \
    ${THISDIR}/files/set-secondary-boot-mode-register.patch \
    "
FSBL_SRCS := "\
    ${THISDIR}/files/iv_z8_init.c \
    "

inherit switch-uart
XPARAMETERS_H = "${B}/fsbl/zynqmp_fsbl_bsp/psu_cortexa53_0/include/xparameters.h"

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${B}/fsbl/iveia_version.h"

do_configure[postfuncs] += "do_post_configure_copy"
do_post_configure_copy () {
    cp ${FSBL_SRCS} ${B}/fsbl
}

