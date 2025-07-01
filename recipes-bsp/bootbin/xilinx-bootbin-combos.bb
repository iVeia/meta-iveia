SUMMARY = "Generate combo boot*_combo.bin files using bootgen tool"
DESCRIPTION = "Combo boot*_combo.bin files mirror the primary boot.bin, but \
have a device tree DTB file that is a combination of the MACHINE and IO board \
device trees.  One boot*_combo.bin is built for each MACHINE/IO combo."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit deploy

DEPENDS += "bootgen-native"

#
# This recipe depends on other recipes to have finished putting their files in
# the deploy dir.
#
do_compile[depends] += "xilinx-bootbin:do_deploy"
do_compile[depends] += "device-tree:do_deploy"

do_compile() {
    rm -rf ${B}/bootbin
    mkdir ${B}/bootbin

    #
    # For each combo, use bootgen.bif from xilinx-bootbin recipe, and modify
    # *.dtb to be a *_combo.dtb.  Then rebuild with bootgen.
    #
    # There may not be ANY combos that match the shell wildcard.  We use the idiom 
    #   "set -- ...*...; if [ -e "$1" ]; then"
    # as a portable way to test if the glob macthes or not.
    #
    set -- ${DEPLOY_DIR_IMAGE}/devicetree/*_combo.dtb
    if [ -e "$1" ]; then
        for DTB in "$@"; do
            BIN_BASENAME=$(basename ${DTB})
            sed "s/\/${MACHINE}.dtb$/\/${BIN_BASENAME}/" "${DEPLOY_DIR_IMAGE}/bootgen.bif" \
                > ${B}/bootgen.bif
            BIN_BASENAME_NO_EXTENSION=${BIN_BASENAME%.*}
            BIN=boot_${BIN_BASENAME_NO_EXTENSION}.bin
            bootgen -image ${B}/bootgen.bif -arch ${SOC_FAMILY} -w -o ${B}/bootbin/${BIN}
            if [ ! -e ${B}/bootbin/${BIN} ]; then
                bbfatal "bootgen failed. See log"
            fi
        done
    fi
}

do_deploy() {
    # Portably test if wildcard glob matches
    install -d ${DEPLOYDIR}/bootbin/
    set -- ${B}/bootbin/*
    if [ -e "$1" ]; then
        install -Dm 0644 "$@" ${DEPLOYDIR}/bootbin/
    fi
}
addtask do_deploy after do_compile before do_build

