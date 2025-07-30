FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://iveia-version-banner.patch \
    file://net-macb-Allow-eth-d-ordering-by-DTS-index-prop.patch \
    file://usbserial.cfg \
    file://rtc.cfg \
    file://hwmon.cfg \
    file://usb-gadget.cfg \
    file://gpio-sysfs.cfg \
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

SRC_URI:append:00068 = " \
    file://pca953x.cfg \
    "

SRC_URI:append:00049 = " \
    file://pca953x.cfg \
    file://usb-net.cfg \
    file://usbnet-rename-to-eth.patch \
    "

# load cdc_mbim before ax88170_178a
# https://github.com/FreddyXin/ax88179_178a/issues/6
# https://unix.stackexchange.com/a/713046
#
KERNEL_MODULE_PROBECONF:append:00049 = "ax88179_178a"
module_conf_ax88179_178a = "softdep ax88179_178a pre: cdc_mbim"


