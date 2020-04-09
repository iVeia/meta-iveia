FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
 
SYSTEM_USER_DTSI ?= "system-user.dtsi"
 
SRC_URI_append = " file://${SYSTEM_USER_DTSI}"

do_configure_append() {
    cat "${WORKDIR}/${SYSTEM_USER_DTSI}" >> ${B}/device-tree/system-top.dts
}
