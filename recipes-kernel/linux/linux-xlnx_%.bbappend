FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://iveia-version-banner.patch \
    file://net-macb-Allow-eth-d-ordering-by-DTS-index-prop.patch \
    file://mtd-fix-xilinx-qspi-mtd-driver-error-call-trace.patch \
    file://rtc.cfg \
    file://hwmon.cfg \
    file://usb-gadget.cfg \
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

#
# I2C-based MDIO driver
#
SRC_URI += " \
    file://mdio-i2c-gen-add-driver-to-build.patch \
    file://mdio-i2c-gen-change-probe-order-of-i2c-before-net.patch \
    file://mdio-i2c-gen.cfg \
    file://drivers/net/phy/mdio-i2c-gen.c;subdir=git \
    file://Documentation/devicetree/bindings/net/mdio-i2c-gen.txt;subdir=git \
    "

