FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append:zynqmp = "\
    file://iveia-version-banner.patch \
    file://QSPI32-Workaround-for-unfinished-QSPI-transfer.patch \
    file://force-XSDPS_DEFAULT_SPEED_MODE-for-EMMC-boot-target.patch \
    "

SRC_URI:append:zynq = "\
    file://iveia-version-banner-zynq.patch \
    file://weaken-fsbl-hooks-zynq.patch \
    file://ps7_init-pre-and-post-hooks.patch \
    file://qspi-jtag-mode-zynq.patch \
    "

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
PSU_INIT_PATCH          := "${THISDIR}/files/psu_init_zynqmp.patch"
PSU_INIT_PATCH:atlas-z8 := "${THISDIR}/files/psu_init_atlas-z8.patch"
PSU_INIT_PATCH:00126    := "${THISDIR}/files/psu_init_00126.patch"
PSU_INIT_PATCH:00146    := "${THISDIR}/files/psu_init_00146.patch"
POST_CONFIGURE_PATCHES:zynqmp := "\
    ${THISDIR}/files/add_sequence_boot.patch \
    ${PSU_INIT_PATCH} \
    "

# helios-z8-mods must be applied before psu_init_00126 patch
POST_CONFIGURE_PATCHES:prepend:00126 := "\
    ${THISDIR}/files/helios-z8-mods.patch \
    "

FSBL_DIR := "${THISDIR}"

FSBL_SRCS:zynqmp := "\
    ${THISDIR}/files/iv_z8_init.c \
    ${THISDIR}/files/clkinit_*.h \
    ${THISDIR}/files/iv_z8_sequence_boot.c \
    ${THISDIR}/files/iv_z8_sequence_boot.h \
    ${THISDIR}/files/iv_z8_user_boot_sequence.c \
    ${THISDIR}/../shared/files/iveia-ipmi.h \
    "
FSBL_SRCS:zynq := "\
    ${THISDIR}/files/zynq-hooks.c \
    "
FSBL_SRCS:append:atlas-z8 := " ${THISDIR}/files/atlas-z8.c"
FSBL_SRCS:append:00104 := " ${THISDIR}/files/00104.c"
FSBL_SRCS:append:00104-fs001 := " ${THISDIR}/files/00104.c"
FSBL_SRCS:append:00104-fs002 := " ${THISDIR}/files/00104.c"
FSBL_SRCS:append:00108 := " ${THISDIR}/files/00108.c"
FSBL_SRCS:append:00114 := " ${THISDIR}/files/00114.c"
FSBL_SRCS:append:00126 := " ${THISDIR}/files/00126.c"
FSBL_SRCS:append:00127 := " ${THISDIR}/files/00127.c"
FSBL_SRCS:append:00131 := " ${THISDIR}/files/00131.c"
FSBL_SRCS:append:00133 := " ${THISDIR}/files/00133.c"
FSBL_SRCS:append:00146 := " ${THISDIR}/files/00104.c"
FSBL_SRCS:append:00172 := " ${THISDIR}/files/00172.c"
FSBL_SRCS:append:00172-fs001 := " ${THISDIR}/files/00172.c"
FSBL_SRCS:append:00172-fs002 := " ${THISDIR}/files/00172.c"
FSBL_SRCS:append:00172-fs003 := " ${THISDIR}/files/00172.c"

YAML_COMPILER_FLAGS:append:00049 = " -DNO_CLKGEN"
YAML_COMPILER_FLAGS:append:00102 = " -DNO_I2C"
YAML_COMPILER_FLAGS:append:atlas-z8 = " -DFORCE_BOOT_BY_PN"

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${B}/fsbl-firmware/iveia_version.h"

#
# Create variants of FSBL that have alternate iVeia boot sequences.  See
# files/iv_z8_user_boot_sequence.c
#
do_compile:prepend:zynqmp() {
    echo "" > ${B}/flags.mak
}

do_compile:append:zynqmp() {
    cd ${B}/${XSCTH_PROJ}
    cp executable.elf ${B}/executable-default.elf

    echo "include ${B}/flags.mak" >> Makefile

    echo 'CC_FLAGS += -DFORCE_QSPI32_BOOT_ONLY' > ${B}/flags.mak
    make clean && oe_runmake
    cp executable.elf ${B}/executable-qspi32.elf

    echo 'CC_FLAGS += -DFORCE_QSPI24_BOOT_ONLY' > ${B}/flags.mak
    make clean && oe_runmake
    cp executable.elf ${B}/executable-qspi24.elf

    echo "" > ${B}/flags.mak
    cp ${B}/executable-default.elf executable.elf
}

do_deploy:append:zynqmp() {
    install -Dm 0644 ${B}/executable-qspi32.elf ${DEPLOYDIR}/fsbl-${MACHINE}-qspi32.elf
    install -Dm 0644 ${B}/executable-qspi24.elf ${DEPLOYDIR}/fsbl-${MACHINE}-qspi24.elf
}

#YAML_COMPILER_FLAGS:append = " -DFSBL_DEBUG -DFSBL_DEBUG_INFO"
