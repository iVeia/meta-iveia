SUMMARY = "Generate combo boot*_combo.bin files using bootgen tool"
DESCRIPTION = "Combo boot*_combo.bin files mirror the primary boot.bin, but \
have a device tree DTB file that is a combination of the MACHINE and IO board \
device trees.  One boot*_combo.bin is built for each MACHINE/IO combo."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit deploy

DEPENDS += "bootgen-native"
DEPENDS += "xilinx-bootbin"
DEPENDS += "device-tree"

BIF_FILE_PATH = "${STAGING_DATADIR}/bootgen.bif"

do_compile() {
    rm -rf ${B}/bootbin
    mkdir ${B}/bootbin

    #
    # For each combo, use bootgen.bif from xilinx-bootbin recipe, and modify
    # *.dtb to be a *_combo.dtb.  Then rebuild with bootgen.
    #
    for DTB in ${STAGING_DATADIR}/devicetree/*_combo.dtb; do
        sed "s/\/${MACHINE}.dtb$/\/$(basename ${DTB})/" ${BIF_FILE_PATH} > ${B}/bootgen.bif
        BIN_BASENAME=$(basename $DTB)
        BIN_BASENAME_NO_EXTENSION=${BIN_BASENAME%.*}
        BIN=boot_${BIN_BASENAME_NO_EXTENSION}.bin
        bootgen -image ${B}/bootgen.bif -arch ${SOC_FAMILY} -w -o ${B}/bootbin/${BIN}
        if [ ! -e ${B}/bootbin/${BIN} ]; then
            bbfatal "bootgen failed. See log"
        fi
    done
}

do_deploy() {
    install -d ${DEPLOYDIR}/bootbin/
    install -Dm 0644 ${B}/bootbin/* ${DEPLOYDIR}/bootbin/
}
addtask do_deploy after do_compile before do_build

