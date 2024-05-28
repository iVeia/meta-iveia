#
# Create several DTB files:
#   ${MACHINE}.dtb
#       iVeia mainboard DTB.  Created by combining the Xilinx DTG generated
#       system-top.dts with the iVeia mainboard DTS file added as a "#include"
#       at the end.
#   ${MACHINE}_${IVIO}_combo.dtb
#       Combination of mainboard and ioboard DTSs.  Like the above DTB, but
#       additionally with the user specified IVIO DTS tacked on to the end.
#       Currently required as the method to get the IVIO DTS info into U-Boot.
#   *_overlay.dtbo
#       DT overlays for each ioboard from DTS source (files/ivio/{zynqmp,zynq}/*_overlay.dts)
#
IV_MB_DTSI = "machine/${MACHINE}.dtsi"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
IVIO_DIR_zynqmp = "ivio/zynqmp"
IVIO_DIR_zynq = "ivio/zynq"
SRC_URI += " \
    file://atlas-z7-inc.dtsi \
    file://atlas-z8-inc.dtsi \
    file://machine/ \
    file://${IVIO_DIR}/ \
    "

DTC_BFLAGS = "-p ${DT_PADDING_SIZE} -@ -H epapr"

do_configure[prefuncs] += "do_pre_configure_ivio"
do_pre_configure_ivio[vardepsexclude] += "BB_ORIGENV"
python do_pre_configure_ivio() {
    ivio_env = d.getVar("BB_ORIGENV").getVar("IVIO")
    ivio = ivio_env if ivio_env else d.getVar("IVIO")
    d.setVar("IVIO", ivio)
}

do_configure_append() {
    # Create DTS for ${MACHINE}.dtb
    echo "#include <${IV_MB_DTSI}>" >> ${DT_FILES_PATH}/system-top.dts
    mv ${DT_FILES_PATH}/system-top.dts ${DT_FILES_PATH}/${MACHINE}.dts

    # Combo DTB.  Use IVIO from env var if available.
    if [ -n "${IVIO}" ]; then
        COMBO_DTS=${DT_FILES_PATH}/${MACHINE}_${IVIO}_combo.dts
        cp ${DT_FILES_PATH}/${MACHINE}.dts ${COMBO_DTS}
        IVIO_DTS=${WORKDIR}/${IVIO_DIR}/${IVIO}_overlay.dts
        if [ ! -f "${IVIO_DTS}" ]; then
            bbfatal "Invalid IVIO board (${IVIO})."
        fi
        grep -v "/plugin/\|/dts-v1/" ${IVIO_DTS} > ${WORKDIR}/${IVIO}.dtsi
        echo "#include <${IVIO}.dtsi>" >> ${COMBO_DTS}
    fi

    if [ -d ${WORKDIR}/${IVIO_DIR} ]; then
        # DTS overlays for each ioboard
        cp ${WORKDIR}/${IVIO_DIR}/*_overlay.dts ${DT_FILES_PATH}
    fi
}

do_deploy() {
    # Xilinx appends to devicetree.bbclass's deploy, and we can't use their
    # mods (which are based on system-top.dtb, which we've renamed).
    # So, we deploy the DTBs ourselves.
    for DTB_FILE in `ls *.dtb *.dtbo`; do
        install -Dm 0644 ${B}/${DTB_FILE} ${DEPLOYDIR}/devicetree/${DTB_FILE}
    done
}
