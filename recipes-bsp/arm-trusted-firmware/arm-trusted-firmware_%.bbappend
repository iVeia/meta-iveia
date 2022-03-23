FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += "file://Force-use-of-UART1.patch"
SRC_URI_remove_helios-z8 += "file://Force-use-of-UART1.patch"
