SUMMARY = "Busyloop count utility for speed testing"
LICENSE = "CLOSED"

SRC_URI = "file://busyloop.c"
S = "${WORKDIR}"

# WARNING: Force off compiler optimization
CFLAGS+=" -O0"

do_compile() {
    ${CC} -o busyloop busyloop.c ${CFLAGS} ${LDFLAGS}
}

do_install() {
    install -d ${D}${bindir}
    install busyloop ${D}${bindir}
}
