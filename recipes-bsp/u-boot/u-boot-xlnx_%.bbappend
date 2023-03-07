FILESEXTRAPATHS:prepend := "${THISDIR}/../shared/files:"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Kconfig
SRC_URI:append = "\
    file://include-iVeia-Kconfig.patch \
    file://Kconfig;subdir=git/board/iveia \
    "

# C sources
SRC_URI:append = "\
    file://iveia-init.c;subdir=git/board/xilinx/common \
    file://iveia-ipmi.h;subdir=git/board/xilinx/common \
    file://Makefile;subdir=git/board/xilinx/common \
    file://eeprom-ipmi-atlas-ii-z8p-hd.c;subdir=git/board/xilinx/common \
    file://iveia-config.h;subdir=git/include/configs \
    "

# Patches & CFG
SRC_URI:append = "\
    file://Fix-saveenv-causes-next-boot-to-skip-board_late_init.patch \
    file://uboot.cfg \
    "

# MACHINE specific patches
SRC_URI:append:atlas-ii-z8p-hd = "\
    file://uboot-atlas-ii-z8p-hd.cfg \
    "
SRC_URI:append:zynq = "\
    file://add-iveia-config-overriding-xilinx-zynq.patch \
    "
SRC_URI:append:zynqmp = "\
    file://add-iveia-config-overriding-xilinx-zynqmp.patch \
    "

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

DEVICETREE_DIR = "${STAGING_DIR_HOST}/boot/devicetree"

do_compile[prefuncs] += "do_pre_compile_ivio"
do_pre_compile_ivio[vardepsexclude] += "BB_ORIGENV"
python do_pre_compile_ivio() {
    import glob
    ivio_env = d.getVar("BB_ORIGENV").getVar("IVIO")
    ivio = ivio_env if ivio_env else d.getVar("IVIO")

    if ivio:
        combo_dtbs = glob.glob(os.path.join(d.getVar("DEVICETREE_DIR"), "*_combo.dtb"))
        if len(combo_dtbs) != 1:
            bb.fatal("Exactly one *_combo.dtb should have been built (found {:d})"
                .format(len(combo_dtbs)))
        dtb = combo_dtbs[0]
    else:
        dtb = os.path.join(d.getVar("DEVICETREE_DIR"), d.getVar("MACHINE") + ".dtb")
        if not os.path.isfile(dtb):
            bb.fatal("Cannot find DTB ({%s})".format(dtb))
    d.setVar("DTB_NAME", os.path.basename(dtb))
}
