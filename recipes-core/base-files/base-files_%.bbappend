FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = "\
    file://issue \
    "

SRC_URI:append:zynq = "\
    file://zynq.hwmon \
    "

SRC_URI:append:zynqmp = "\
    file://zynqmp.hwmon \
    "

do_install:append () {
	install -d ${D}/media
	install -m 0755 -d ${D}/etc/sensors.d/
	cp ${WORKDIR}/*.hwmon ${D}/etc/sensors.d/
	chmod 0755 ${D}/etc/sensors.d/*
}

SRC_URI:append:00102 = "\
    file://brcm-firmware/ \
   "

DEPENDS = "rsync-native"

do_install:append:00102 () {
	install -d ${D}/lib/firmware/brcm
	rsync -lr ${B}/brcm-firmware/ ${D}/lib/firmware/brcm
}

FILES:${PN}:append = " /media/sd0 /media/sd1"

do_install_basefilesissue () {
	install -m 644 ${WORKDIR}/issue*  ${D}${sysconfdir}
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue.net
}

