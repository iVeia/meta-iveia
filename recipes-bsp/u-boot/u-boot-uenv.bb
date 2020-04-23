SUMMARY = "iVeia U-Boot uEnv.txt for SD boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

INHIBIT_DEFAULT_DEPS = "1"

inherit deploy

UENV_PATH := "${THISDIR}/files"
UENV_FILE = "uEnv.txt"

do_compile() {
    cat ${UENV_PATH}/${UENV_FILE} > ${B}/${UENV_FILE}

    # Extra vars to append to UENV_FILE
    echo "machine=${MACHINE}" >> ${B}/${UENV_FILE}
}

do_deploy() {
    install -Dm 0644 ${B}/${UENV_FILE} ${DEPLOYDIR}/${UENV_FILE}
}
addtask do_deploy after do_compile before do_build

