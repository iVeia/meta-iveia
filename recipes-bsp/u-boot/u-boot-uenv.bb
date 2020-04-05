SUMMARY = "iVeia U-Boot uEnv.txt for SD boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

INHIBIT_DEFAULT_DEPS = "1"

inherit deploy

UENV_TYPES = "sd tftp"
UENV_PATH := "${THISDIR}/files"
UENV_FILE = "uEnv.txt"
UENV_SUBDIR = "uenv"

do_compile() {
    for u in ${UENV_TYPES}; do
        SED_EXPR=
        SED_EXPR+="s/__MACHINE__/${MACHINE}/;"
        mkdir -p ${B}/${UENV_SUBDIR}/${u}
        sed "${SED_EXPR}" ${UENV_PATH}/${u}/${UENV_FILE} > ${B}/${UENV_SUBDIR}/${u}/${UENV_FILE}
    done
}

do_deploy() {
    for u in ${UENV_TYPES}; do
        install -Dm 0644 ${B}/${UENV_SUBDIR}/${u}/${UENV_FILE} \
            ${DEPLOYDIR}/${UENV_SUBDIR}/${u}/${UENV_FILE}
    done
}
addtask do_deploy after do_compile before do_build

