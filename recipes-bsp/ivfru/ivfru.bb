SUMMARY = "Utility for accessing and modifying IPMI FRU data"
LICENSE = "CLOSED"

RDEPENDS_${PN} += "bash"

FILESEXTRAPATHS_prepend := "${THISDIR}/../shared/files:"

SRC_URI = ""
SRC_URI += "file://ivfru.c"
SRC_URI += "file://ivfru_common.c"
SRC_URI += "file://ivfru_common.h"
SRC_URI += "file://ivfru_plat.c"
SRC_URI += "file://ivfru_plat.h"
SRC_URI += "file://iveeprom"
S = "${WORKDIR}"

do_compile() {
    ${CC} -o ${PN} ivfru.c ivfru_common.c ivfru_plat.c ${CFLAGS} ${LDFLAGS}
}

do_install() {
    install -d ${D}${bindir}
    install -m0755 ${PN} ${D}${bindir}
    install -Dm 0755 ${WORKDIR}/iveeprom ${D}/usr/bin/iveeprom
}
