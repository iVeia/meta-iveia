SUMMARY = "Miscellaneous extra utility files for the base system"
LICENSE = "CLOSED"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append += "\
    file://overlay-mgr \
    file://overlay_mgr_completion \
    file://pl-mgr \
    file://pl_mgr_completion \
    file://iio \
    file://iio-completion \
    file://init.d_completion \
    file://dtc-converter \
    file://bashrc_dev.sh \
    file://ldd \
    file://sysctl.conf \
    "

RDEPENDS_${PN} += " bash"

FILES_${PN}_append = " /lib/firmware/dt-overlays"

do_install_append () {
        install -Dm 0755 ${WORKDIR}/overlay-mgr ${D}/usr/bin/overlay-mgr
        install -Dm 0755 ${WORKDIR}/overlay_mgr_completion ${D}/etc/bash_completion.d/overlay_mgr_completion
        install -Dm 0755 ${WORKDIR}/pl-mgr ${D}/usr/bin/pl-mgr
        install -Dm 0755 ${WORKDIR}/pl_mgr_completion ${D}/etc/bash_completion.d/pl_mgr_completion
        install -Dm 0755 ${WORKDIR}/iio ${D}/usr/bin/iio
        install -Dm 0755 ${WORKDIR}/iio-completion ${D}/etc/bash_completion.d/iio-completion
        install -Dm 0755 ${WORKDIR}/init.d_completion ${D}/etc/bash_completion.d/init.d_completion
        install -Dm 0755 ${WORKDIR}/dtc-converter ${D}/usr/bin/dtc-converter
        install -Dm 0644 ${WORKDIR}/bashrc_dev.sh ${D}/etc/profile.d/bashrc_dev.sh
        install -Dm 0755 ${WORKDIR}/ldd ${D}/usr/bin/ldd
        install -d ${D}/lib/firmware/dt-overlays
        install -Dm 0644 ${WORKDIR}/sysctl.conf ${D}/etc/sysctl.conf
}

