FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
 
SYSTEM_USER_DTSI ?= "system-user.dtsi"
 
SRC_URI_append = " file://${SYSTEM_USER_DTSI}"

YAML_DT_BOARD_FLAGS = "{BOARD zcu102-rev1.0}"
 
do_configure_append() {
        cp ${WORKDIR}/${SYSTEM_USER_DTSI} ${B}/device-tree
        echo "/include/ \"${SYSTEM_USER_DTSI}\"" >> ${B}/device-tree/system-top.dts
}
