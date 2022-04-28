SUMMARY = "Utility for reading an mmap()ped block from /dev/mem"
LICENSE = "CLOSED"

SRC_URI = "file://bdevmem.c"
S = "${WORKDIR}"

DEPENDS += "zlib"
CFLAGS += "-lz"

do_compile() {
    ${CC} -o bdevmem bdevmem.c ${CFLAGS} ${LDFLAGS}
}

do_install() {
    install -d ${D}${bindir}
    install bdevmem ${D}${bindir}
}
