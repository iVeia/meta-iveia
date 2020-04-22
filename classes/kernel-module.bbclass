#
# Generic kernel module recipe
#
# Recipe PN (package name) must be of the form kernel-module-*
#
# All driver files and Makefile should sit in the recipe's subdir "src"
#
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d478fe1a3fa276aa27da65644e60b9fc"

inherit module

FILESEXTRAPATHS_prepend := "${THISDIR}/:"
SRC_URI = "file://src/"

S = "${WORKDIR}/src"

RPROVIDES_${PN} += "${PN}"

KERNEL_MODULE_AUTOLOAD += "${@d.getVar('PN').replace('kernel-module-', '')}"


