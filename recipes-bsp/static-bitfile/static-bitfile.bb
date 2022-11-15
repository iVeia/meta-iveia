SUMMARY = "static bitfile"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

INHIBIT_DEFAULT_DEPS = "1"

inherit deploy

BITFILE_PATH := "${THISDIR}/files"
BITFILE_FILE = "xilinx.bit"

do_compile() {
    cat ${BITFILE_PATH}/${BITFILE_FILE} > ${B}/${BITFILE_FILE}

    # Extra vars to append to BITFILE_FILE
    echo "machine=${MACHINE}" >> ${B}/${BITFILE_FILE}
}

do_deploy() {
    install -Dm 0644 ${B}/${BITFILE_FILE} ${DEPLOYDIR}/${BITFILE_FILE}
}
addtask do_deploy after do_compile before do_build

