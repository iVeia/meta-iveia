SUMMARY = "Mount /media/sd to booting SD card on startup"
LICENSE = "CLOSED"

inherit update-rc.d

INITSCRIPT_NAME = "sdmount.sh"
INITSCRIPT_PARAMS = "start 50 S ."

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "\
    file://sdmount.sh \
    "

do_install() {
    install -Dm 0755 ${WORKDIR}/sdmount.sh ${D}${sysconfdir}/init.d/sdmount.sh
}

FILES:${PN} = "${sysconfdir}/init.d/sdmount.sh"

