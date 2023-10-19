FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://iveia-version-banner.patch \
    file://net-macb-Allow-eth-d-ordering-by-DTS-index-prop.patch \
    file://mtd-fix-xilinx-qspi-mtd-driver-error-call-trace.patch \
    file://usbserial.cfg \
    file://export-setup-dma-ops.patch \
    file://rtc.cfg \
    file://hwmon.cfg \
    file://usb-gadget.cfg \
    "


inherit iveia-version-header
IVEIA_VERSION_HEADER_FILE = "${S}/include/iveia_version.h"

#
# I2C-based MDIO driver
#
SRC_URI += " \
    file://mdio-i2c-gen-add-driver-to-build.patch \
    file://mdio-i2c-gen-change-probe-order-of-i2c-before-net.patch \
    file://mdio-i2c-gen.cfg \
    file://drivers/net/mdio/mdio-i2c-gen.c;subdir=git \
    file://Documentation/devicetree/bindings/net/mdio-i2c-gen.txt;subdir=git \
    "

SRC_URI_append_00068 = " \
    file://pca953x.cfg \
    "

