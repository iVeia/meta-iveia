FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://iveia-version-banner.patch \
    file://net-macb-Allow-eth-d-ordering-by-DTS-index-prop.patch \
    file://rtc.cfg \
    "

inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

#
# Zap not currently buildable as an OOT module.  Future: build as module.
#
SRC_URI += " \
    file://zap-add-driver.patch \
    file://drivers/char/zap/;subdir=git \
    "

