FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://zap-add-driver.patch \
    file://iveia-version-banner.patch \
    file://net-macb-Allow-eth-d-ordering-according-to-DTS-ether.patch \
    "

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

#
# Zap not currently buildable as an OOT module.  Copy drivers and build as
# built-in (requires patches above).  Future: build as module.
#
ZAP_SRC_DIR := "${THISDIR}/files/zap/"
ZAP_DEST_DIR := "${S}/drivers/char/zap"

do_compile[prefuncs] += "do_pre_compile_copy"
do_pre_compile_copy () {
    cp -r --no-target-directory ${ZAP_SRC_DIR} ${ZAP_DEST_DIR}
}


