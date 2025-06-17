BIF_PARTITION_ATTR:remove = "bitstream"
BIF_PARTITION_IMAGE[device-tree] = "${RECIPE_SYSROOT}/boot/devicetree/${MACHINE}.dtb"

#
# Make bootgen.bif available to other recipes
#
do_deploy:append() {
    install -Dm644 ${BIF_FILE_PATH} ${DEPLOYDIR}/bootgen.bif
}
addtask deploy after do_compile before do_build
