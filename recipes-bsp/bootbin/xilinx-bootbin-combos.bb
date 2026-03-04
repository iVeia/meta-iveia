SUMMARY = "Generate combo boot*_combo.bin files using bootgen tool"
DESCRIPTION = "Combo boot*_combo.bin files mirror the primary boot.bin, but \
have a device tree DTB file that is a combination of the MACHINE and IO board \
device trees.  One boot*_combo.bin is built for each MACHINE/IO combo."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit deploy

PACKAGE_ARCH = "${MACHINE_ARCH}"

DEPENDS += "bootgen-native"
DEPENDS += "xilinx-bootbin"
DEPENDS += "device-tree"

BIF_FILE_PATH = "${STAGING_DATADIR}/bootgen.bif"

do_compile[depends] += "fsbl-firmware:do_deploy"

run_bootgen() {
    BIN=$1
    bootgen -image ${B}/bootgen.bif -arch ${SOC_FAMILY} -w -o ${B}/bootbin/${BIN}
    if [ ! -e ${B}/bootbin/${BIN} ]; then
        bbfatal "bootgen failed for ${BIN}. See log"
    fi
}

do_compile() {
    rm -rf ${B}/bootbin
    mkdir ${B}/bootbin

    #
    # For each combo, use bootgen.bif from xilinx-bootbin recipe, and modify
    # *.dtb to be a *_combo.dtb.  Then rebuild with bootgen.
    #
    # In addition, build a qspi24 based combo.  Therefore two bins are created:
    #   - bootbin/boot_${MACHINE}_${IVIO}.bin
    #       A boot.bin that has the combo (machine and IVIO overlay) DTB file
    #       built in.
    #   - bootbin/boot_${MACHINE}_qspi24_${IVIO}.bin
    #       Same as above, but with FORCE_QSPI24_ONLY_BOOT set in FSBL.  This
    #       is useful if you want to always boot from QSPI even if there is a
    #       boot.bin on SD0/1.
    #
    #
    # There may not be ANY combos that match the shell wildcard.  We use the idiom
    #   "set -- ...*...; if [ -e "$1" ]; then"
    # as a portable way to test if the glob macthes or not.
    #
    #
    set -- ${STAGING_DATADIR}/devicetree/*_combo.dtb
    if [ -e "$1" ]; then
        for DTB in "$@"; do
            BIN_SUFFIX=$(basename $DTB)
            BIN_SUFFIX=${BIN_SUFFIX%.*}
            BIN_SUFFIX=${BIN_SUFFIX#${MACHINE}_}

            sed "s/\/${MACHINE}.dtb$/\/$(basename ${DTB})/" ${BIF_FILE_PATH} > ${B}/bootgen.bif
            run_bootgen boot_${MACHINE}_${BIN_SUFFIX}.bin

            if [ "${SOC_FAMILY}" = "zynqmp" ]; then
                FSBL_ELF="${DEPLOY_DIR_IMAGE}/fsbl-${MACHINE}-qspi24.elf"
                sed -i "s| [^ ]*/fsbl\.elf| ${FSBL_ELF}|" ${B}/bootgen.bif
                run_bootgen boot_${MACHINE}_qspi24_${BIN_SUFFIX}.bin
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


