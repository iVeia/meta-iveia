BIF_PARTITION_ATTR_remove = "bitstream"
BIF_PARTITION_IMAGE[device-tree] = "${RECIPE_SYSROOT}/boot/devicetree/${MACHINE}.dtb"

#
# Make bootgen.bif available to other recipes
#
do_install_append() {
    install -Dm644 ${BIF_FILE_PATH} ${D}${datadir}/bootgen.bif
}
FILES_${PN} += "${datadir}/bootgen.bif"
