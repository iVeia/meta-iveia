IV_MB_DTSI = "${MACHINE}.dtsi"
IV_IO_DTSI = "a3io-aurora.dtsi"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://${IV_MB_DTSI} \
    file://ivio/ \
    "

do_configure_append() {
    cat "${WORKDIR}/${IV_MB_DTSI}" >> ${B}/device-tree/system-top.dts
    cat "${WORKDIR}/ivio/${IV_IO_DTSI}" >> ${B}/device-tree/system-top.dts
}
