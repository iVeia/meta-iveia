FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append += "\
    file://issue \
    "

SRC_URI_append_zynq += "\
    file://zynq.hwmon \
    "

SRC_URI_append_zynqmp += "\
    file://zynqmp.hwmon \
    "

SRC_URI_append_atlas-z8 += "\
    file://atlas-z8.hwmon \
    "

do_install_append () {
	install -d ${D}/media
	install -m 0755 -d ${D}/etc/sensors.d/
	cp ${WORKDIR}/*.hwmon ${D}/etc/sensors.d/
	chmod 0755 ${D}/etc/sensors.d/*
}

SRC_URI_append_00102 += "\
    file://brcm-firmware/ \
   "

DEPENDS = "rsync-native"

do_install_append_00102 () {
	install -d ${D}/lib/firmware/brcm
	rsync -lr ${B}/brcm-firmware/ ${D}/lib/firmware/brcm
}

FILES_${PN}_append = " /media/sd0 /media/sd1"

do_install_basefilesissue () {
	install -m 644 ${WORKDIR}/issue*  ${D}${sysconfdir}
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue.net
}

