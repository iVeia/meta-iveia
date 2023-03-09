FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append += "\
    file://ams.hwmon \
    file://atlas-ii.hwmon \
    "

do_install:append () {
   install -d ${D}/media
	install -Dm 0755 ${WORKDIR}/ams.hwmon ${D}/etc/sensors.d/ams.hwmon
	install -Dm 0755 ${WORKDIR}/atlas-ii.hwmon ${D}/etc/sensors.d/atlas-ii.hwmon
}

FILES:${PN}:append = " /media/sd0 /media/sd1"
