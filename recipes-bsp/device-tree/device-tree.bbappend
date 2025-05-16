#
# Create several DTB files:
#   ${MACHINE}.dtb
#       iVeia SoM DTB.  Created by combining the Xilinx DTG generated
#       system-top.dts with the iVeia SoM DTS file added as a "#include"
#       at the end.
#   *_overlay.dtbo
#       DT overlays for each ioboard from DTS source (files/ivio/{zynqmp,zynq}/*_overlay.dts)
#   ${MACHINE}_*_combo.dtb
#       Combination of SoM DTS and ioboard DTS, for each ioboard.
#
IV_MB_DTSI = "machine/${MACHINE}.dtsi"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
IVIO_DIR_zynqmp = "ivio/zynqmp"
IVIO_DIR_zynq = "ivio/zynq"
SRC_URI += " \
    file://atlas-z7-inc.dtsi \
    file://atlas-z8-inc.dtsi \
    file://machine/${MACHINE}.dtsi \
    file://${IVIO_DIR}/ \
    "

DTC_BFLAGS = "-p ${DT_PADDING_SIZE} -@ -H epapr"

do_configure_append() {
    # Create DTS for ${MACHINE}.dtb
    echo "#include <${IV_MB_DTSI}>" >> ${DT_FILES_PATH}/system-top.dts
    mv ${DT_FILES_PATH}/system-top.dts ${DT_FILES_PATH}/${MACHINE}.dts

    # Combo DTBs, one for each overlay
    OVERLAY_SUFFIX="_overlay.dts"
    for IVIO in ${WORKDIR}/${IVIO_DIR}/*${OVERLAY_SUFFIX}; do
        IVIO=$(basename ${IVIO})
        IVIO=${IVIO%$OVERLAY_SUFFIX}

        COMBO_DTS="${DT_FILES_PATH}/${MACHINE}_${IVIO}_combo.dts"
        cp ${DT_FILES_PATH}/${MACHINE}.dts ${COMBO_DTS}
        IVIO_DTS=${WORKDIR}/${IVIO_DIR}/${IVIO}${OVERLAY_SUFFIX}
        if [ ! -f ${IVIO_DTS} ]; then
            bbfatal "Invalid IVIO board (${IVIO})."
        fi
        grep -v "/plugin/\|/dts-v1/" ${IVIO_DTS} > ${WORKDIR}/${IVIO}.dtsi
        echo "#include <${IVIO}.dtsi>" >> ${COMBO_DTS}
    done

    # DTS overlays for each ioboard
    cp ${WORKDIR}/${IVIO_DIR}/*${OVERLAY_SUFFIX} ${DT_FILES_PATH}
}

do_deploy() {
    # Xilinx appends to devicetree.bbclass's deploy, and we can't use their
    # mods (which are based on system-top.dtb, which we've renamed).
    # So, we deploy the DTBs ourselves.
    for DTB_FILE in `ls *.dtb *.dtbo`; do
        install -Dm 0644 ${B}/${DTB_FILE} ${DEPLOYDIR}/devicetree/${DTB_FILE}
    done
}
