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
#       DT overlays for each ioboard from DTS source (files/ivio/*_overlay.dts)
#
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

IV_MB_DTSI = "machine/${MACHINE}.dtsi"
DTB_FILE_NAME = "${MACHINE}.dtb"

SRC_URI += " \
    file://atlas-z7.dtsi \
    file://atlas-z8.dtsi \
    file://machine/ \
    file://ivio/ \
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
        IVIO_DTS=${WORKDIR}/ivio/${IVIO}_overlay.dts
        if [ ! -f "${IVIO_DTS}" ]; then
            bbfatal "Invalid IVIO board (${IVIO})."
        fi
        grep -v "/plugin/\|/dts-v1/" ${IVIO_DTS} > ${WORKDIR}/${IVIO}.dtsi
        echo "#include <${IVIO}.dtsi>" >> ${COMBO_DTS}
    fi

	if [ -d ${WORKDIR}/iviio ]; then
    	# DTS overlays for each ioboard
    	cp ${WORKDIR}/ivio/*_overlay.dts ${DT_FILES_PATH}
	fi
}

do_deploy() {
    # Override Xilinx's bbappended deploy, and do the default
    # devicetree.bbclass's deploy
    devicetree_do_deploy
}
