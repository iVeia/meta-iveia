FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append += "\
    file://ams.hwmon \
    file://atlas-ii.hwmon \
    file://issue \
    "

do_install_append () {
   install -d ${D}/media
	install -Dm 0755 ${WORKDIR}/ams.hwmon ${D}/etc/sensors.d/ams.hwmon
	install -Dm 0755 ${WORKDIR}/atlas-ii.hwmon ${D}/etc/sensors.d/atlas-ii.hwmon
}

FILES_${PN}_append = " /media/sd0 /media/sd1"

do_install_basefilesissue () {
	install -m 644 ${WORKDIR}/issue*  ${D}${sysconfdir}
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue
	install -Dm 0644 ${WORKDIR}/issue ${D}${sysconfdir}/issue.net
}

