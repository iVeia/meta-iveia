SUMMARY = "iVeia OCP driver"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d478fe1a3fa276aa27da65644e60b9fc"

inherit module

FILESEXTRAPATHS_prepend := "${THISDIR}/:"
SRC_URI = "file://src/"

S = "${WORKDIR}/src"

RPROVIDES_${PN} += "kernel-module-ocp"

KERNEL_MODULE_AUTOLOAD += "ocp"
