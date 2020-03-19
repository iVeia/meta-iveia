SUMMARY = "Generates boot.bin using bootgen tool for booting to SD"
DESCRIPTION = "Creates a boot.bin that uses the boot_device attribute \
for booting from another device - in this case, sd0."

LICENSE = "CLOSED"

#
# Based off of recipes-bsp/bootbin/xilinx-bootbin_1.0.bb - however, that recipe
# does not support handling images in a BIF file that are not actual files.
# The BIF file uses "[boot_device] sd0" for booting to the secondary device
# sd0, and "sd0" is not a actual file.
#

inherit deploy

DEPENDS += "virtual/fsbl"
DEPENDS += "bootgen-native"

do_configure() {
    echo "the_ROM_image:{" > "${BIF_FILE_PATH}"
	echo "  [bootloader] ${DEPLOY_DIR_IMAGE}/fsbl-${MACHINE}.elf" >> "${BIF_FILE_PATH}"
	echo "  [boot_device] sd0" >> "${BIF_FILE_PATH}"
	echo "}" >> "${BIF_FILE_PATH}"
}

BIF_FILE_PATH ?= "${B}/bootgen.bif"
BOOTBIN_FILE_NAME ?= "boot2sd.bin"

BOOTGEN_EXTRA_ARGS ?= ""

do_fetch[noexec] = "1"
do_unpack[noexec] = "1"
do_patch[noexec] = "1"

do_compile[depends] += "virtual/fsbl:do_deploy"
do_compile() {
    cd ${WORKDIR}
    rm -f ${B}/${BOOTBIN_FILE_NAME}
    bootgen -image ${BIF_FILE_PATH} -arch ${SOC_FAMILY} ${BOOTGEN_EXTRA_ARGS} \
        -w -o ${B}/${BOOTBIN_FILE_NAME}
    if [ ! -e ${B}/${BOOTBIN_FILE_NAME} ]; then
        bbfatal "bootgen failed. See log"
    fi
}

do_install() {
    install -d ${D}/boot
    install -m 0644 ${B}/${BOOTBIN_FILE_NAME} ${D}/boot/${BOOTBIN_FILE_NAME}
}

do_deploy() {
    install -d ${DEPLOYDIR}
    install -m 0644 ${B}/${BOOTBIN_FILE_NAME} ${DEPLOYDIR}/${BOOTBIN_FILE_NAME}
}

FILES_${PN} += "/boot/${BOOTBIN_FILE_NAME}"
SYSROOT_DIRS += "/boot"

addtask do_deploy before do_build after do_compile

