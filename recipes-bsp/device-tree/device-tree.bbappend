IV_MB_DTSI = "${MACHINE}.dtsi"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://${IV_MB_DTSI} \
    file://ivio/ \
    "

do_configure_append() {
    echo "#include <${IV_MB_DTSI}>" >> ${DT_FILES_PATH}/system-top.dts
    cp ${WORKDIR}/ivio/*.dts ${DT_FILES_PATH}
}

python do_compile_append() {
    import shutil
    B = d.getVar("B")
    src = "{:s}/system-top.dtb".format(B)
    dest = "{:s}/{:s}.dtb".format(B, d.getVar("MACHINE"))
    shutil.move(src, dest)
}

do_deploy() {
    devicetree_do_deploy
}
