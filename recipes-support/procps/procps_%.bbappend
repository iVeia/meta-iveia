FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://iveia-sysctl.conf"

do_install:append() {
    # Append iVeia-specific sysctl settings to the procps sysctl.conf
    cat ${WORKDIR}/iveia-sysctl.conf >> ${D}${sysconfdir}/sysctl.conf
}