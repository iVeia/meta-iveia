FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
FILESEXTRAPATHS_prepend_atlas-ii-z8ev := "${THISDIR}/files/atp-uut:"

RDEPENDS_${PN} += "bash"

SRC_URI_append_atlas-ii-z8ev += "\
    file://uut-gpio \
    file://uut_gpio_completion \
    file://temp_sensor \
    file://usb_overcurrent_test \
    file://sgmii_loopback \
    file://eth_test \
    file://slot_id \
    file://sd_rw \
    file://usb_test \
    file://gpio_expander \
   "

do_install_append_atlas-ii-z8ev () {
    install -Dm 0755 ${WORKDIR}/uut-gpio ${D}/usr/bin/uut-gpio
    install -Dm 0755 ${WORKDIR}/uut_gpio_completion ${D}/etc/bash_completion.d/uut_gpio_completion

    install -Dm 0755 ${WORKDIR}/temp_sensor ${D}/usr/bin/temp_sensor
    install -Dm 0755 ${WORKDIR}/usb_overcurrent_test ${D}/usr/bin/usb_overcurrent_test
    install -Dm 0755 ${WORKDIR}/sgmii_loopback ${D}/usr/bin/sgmii_loopback
    install -Dm 0755 ${WORKDIR}/eth_test ${D}/usr/bin/eth_test
    install -Dm 0755 ${WORKDIR}/slot_id ${D}/usr/bin/slot_id
    install -Dm 0755 ${WORKDIR}/sd_rw ${D}/usr/bin/sd_rw
    install -Dm 0755 ${WORKDIR}/usb_test ${D}/usr/bin/usb_test
    install -Dm 0755 ${WORKDIR}/gpio_expander ${D}/usr/bin/gpio_expander
}

