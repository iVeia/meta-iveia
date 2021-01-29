FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append += "\
    file://ams.hwmon \
    file://atlas-ii.hwmon \
    "

do_install_append () {
   install -d ${D}/media/sd0
   install -d ${D}/media/sd1
	install -Dm 0755 ${WORKDIR}/ams.hwmon ${D}/etc/sensors.d/ams.hwmon
	install -Dm 0755 ${WORKDIR}/atlas-ii.hwmon ${D}/etc/sensors.d/atlas-ii.hwmon
}

FILES_${PN}_append = " /media/sd0 /media/sd1"
