SUMMARY = "init.d script linked to startup.sh on SD card"
LICENSE = "CLOSED"

inherit deploy
inherit update-rc.d

INITSCRIPT_NAME = "ivstartup.sh"
INITSCRIPT_PARAMS = "start 999 S ."

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = "\
    file://ivstartup.sh \
    file://startup.sh \
    "

do_install() {
    install -Dm 0755 ${WORKDIR}/ivstartup.sh ${D}${sysconfdir}/init.d/ivstartup.sh
	install -Dm 0755 ${WORKDIR}/startup.sh ${D}/boot/startup.sh
}

FILES_${PN} = "${sysconfdir}/init.d/ivstartup.sh"
FILES_${PN} += "/boot/startup.sh"

do_deploy() {
	install -Dm 0755 ${WORKDIR}/startup.sh ${DEPLOYDIR}/startup.sh
}
addtask do_deploy after do_compile before do_build

