FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

do_install_append () {
   install -d ${D}/media/sd0
   install -d ${D}/media/sd1
}

FILES_${PN}_append = " /media/sd0 /media/sd1"
