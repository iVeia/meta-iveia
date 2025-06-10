FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://Force-use-of-UART1.patch"
SRC_URI:remove:00126 = "file://Force-use-of-UART1.patch"
