SUMMARY = "Create SD Card Self Extracting Image Script"
LICENSE = "CLOSED"

inherit deploy

PACKAGE_ARCH = "${MACHINE_ARCH}"

# ivinstall depends on the final iveia image - but it can't use DEPENDS, as it
# depends on do_populate_sysroot, which ${IVIMG} doesn't have.
IVIMG := "iveia-image-${IMAGE_TYPE}"
do_deploy[depends] = "${IVIMG}:do_build"
DEPENDS_append = " fsbl u-boot-xlnx u-boot-uenv xilinx-bootbin device-tree linux-xlnx ivstartup"
DEPENDS_append_zynqmp = " pmu-firmware arm-trusted-firmware"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
HEADER_DOC = "ivinstall-doc"
HEADER_ORIG = "ivinstall-header.sh"
HEADER = "ivinstall-header-final.sh"
SRC_URI = "\
    file://${HEADER_ORIG} \
    file://${HEADER_DOC} \
    file://halt.tcl \
    "

S = "${WORKDIR}"
B = "${S}"

VARS_INSERT_STR = "__INSERT_YOCTO_VARS_HERE_DO_NOT_REMOVE_THIS_LINE__"
DOC_INSERT_STR = "__INSERT_YOCTO_DOC_HERE_DO_NOT_REMOVE_THIS_LINE__"
HALT_INSERT_STR = "__INSERT_YOCTO_HALT_HERE_DO_NOT_REMOVE_THIS_LINE__"

#
# Create IV_TAR_FILE_MAP.  Format:
#   TAR_FILE_MAP="PATH1:ARCNAME1 PATH2:ARCNAME2 ... PATHn:ARCNAMEn"
# where
#   PATHn is the full path of the file or directory to include in the archive
#   ARCNAMEn is the relative path in the archive of the file or directory
#
# whitespace is allowed before and after the colon for readability.
#
IV_TAR_FILE_MAP = "\
    ${DEPLOY_DIR_IMAGE}/fsbl-${MACHINE}.elf                 :   elf/fsbl.elf \
    ${DEPLOY_DIR_IMAGE}/u-boot.elf                          :   elf/u-boot.elf \
    ${DEPLOY_DIR_IMAGE}/boot.bin                            :   boot/boot.bin \
    ${DEPLOY_DIR_IMAGE}/uEnv.txt                            :   boot/uEnv.txt \
    ${DEPLOY_DIR_IMAGE}/devicetree                          :   devicetree \
    ${DEPLOY_DIR_IMAGE}/${IVIMG}-${MACHINE}.cpio.gz.u-boot  :   rootfs/initrd \
    ${DEPLOY_DIR_IMAGE}/${IVIMG}-${MACHINE}.ext4            :   rootfs/rootfs.ext4 \
    ${DEPLOY_DIR_IMAGE}/startup.sh                          :   boot/startup.sh \
    ${THISDIR}/files/uEnv.ivinstall.txt                     :   jtag/uEnv.ivinstall.txt \
    ${THISDIR}/files/uEnv.boot.txt                          :   jtag/uEnv.boot.txt \
    ${THISDIR}/files/uEnv.qspi.txt                          :   jtag/uEnv.qspi.txt \
    "

IV_TAR_FILE_MAP_append_zynqmp = "\
    ${DEPLOY_DIR_IMAGE}/Image                               :   boot/Image \
    ${THISDIR}/files/qspi-zynqmp.tcl                        :   jtag/qspi.tcl \
    ${THISDIR}/files/ivinstall-zynqmp.tcl                   :   jtag/ivinstall.tcl \
    ${DEPLOY_DIR_IMAGE}/pmu-firmware-${MACHINE}.elf         :   elf/pmu.elf \
    ${DEPLOY_DIR_IMAGE}/arm-trusted-firmware.elf            :   elf/atf.elf \
    "

IV_TAR_FILE_MAP_append_zynq = "\
    ${DEPLOY_DIR_IMAGE}/uImage                              :   boot/uImage \
    ${THISDIR}/files/qspi-zynq.tcl                          :   jtag/qspi.tcl \
    ${THISDIR}/files/ivinstall-zynq.tcl                     :   jtag/ivinstall.tcl \
    "

inherit iveia-version-header

do_clean() {
    rm -rf ${DEPLOY_DIR_IMAGE}/ivinstall-${IMAGE_TYPE}*
}

do_compile() {
    INSERT_VARS="MACHINE='${MACHINE}'\n"

    # Extract IOBOARD names from devicetree/ *_overlay.dtbo files.
    IOBOARDS=""
    for i in $(find ${DEPLOY_DIR_IMAGE}/devicetree/ -name "*_overlay.dtbo" | sort); do
        IOBOARD=$(basename "$i")
        IOBOARD="${IOBOARD%_overlay.dtbo}"
        IOBOARDS="${IOBOARDS}${IOBOARD} "
    done
    if [ -n "${IOBOARDS}" ]; then
        INSERT_VARS="${INSERT_VARS}IOBOARDS='${IOBOARDS}'\n"
    else
        INSERT_VARS="${INSERT_VARS}IOBOARDS='none'\n"
    fi

    . "${META_IVEIA_VERSIONS_FILE}"
    . "${B}/${PN}.versions"
    INSERT_VARS="${INSERT_VARS}IVEIA_NUM_LAYERS='${IVEIA_NUM_LAYERS}'\n"
    for i in `seq $IVEIA_NUM_LAYERS`; do
        # Note: POSIX does not support bash/ksh variable indirect
        # expansion, so we use the more cumbersome `eval` below
        LAYER_VAR=IVEIA_META_${i}_LAYER
        LAYER=`eval "echo \\$$LAYER_VAR"`
        HASH_VAR=IVEIA_META_${i}_BUILD_HASH
        HASH=`eval "echo \\$$HASH_VAR"`
        INSERT_VARS="${INSERT_VARS}${LAYER_VAR}='${LAYER}'\n"
        INSERT_VARS="${INSERT_VARS}${HASH_VAR}='${HASH}'\n"
    done
    INSERT_VARS="${INSERT_VARS}IVEIA_BUILD_DATE='${IVEIA_BUILD_DATE}'\n"

    # Insert variables defs, halt script, and then the header doc into the
    # named lines of the ivinstall-header
    sed "s/.*${VARS_INSERT_STR}.*/${INSERT_VARS}/" ${B}/${HEADER_ORIG} > ${B}/header.tmp
    sed "s/^/\t\t/" ${B}/halt.tcl > ${B}/halt.tmp
    sed -i "/.*${HALT_INSERT_STR}/r ${B}/halt.tmp" ${B}/header.tmp
    sed -i "s/.*${HALT_INSERT_STR}//" ${B}/header.tmp
    sed "/${DOC_INSERT_STR}/r ${HEADER_DOC}" ${B}/header.tmp | grep -v "${DOC_INSERT_STR}" \
        > ${B}/${HEADER}
}

python do_build() {
    bb.plain("do_build")
    d.setVar("TAR_FILE_MAP", "dude frood")
}

python do_deploy() {
    from functools import partial
    import shutil
    import tarfile
    import stat
    import re

    # get necessary env vars
    build_dir = d.getVar("B")
    header = d.getVar("HEADER")
    machine = d.getVar("MACHINE")
    deploy_dir = d.getVar("DEPLOY_DIR_IMAGE")
    image_type = d.getVar("IMAGE_TYPE")

    tarball = os.path.join(deploy_dir, "ivinstall-%s-image.tgz" % image_type)
    ivinstall_header = os.path.join(build_dir, header)

    #
    # Convert IV_TAR_FILE_MAP into a list of filepath/archpath tuples.
    #
    tar_file_map = d.getVar("IV_TAR_FILE_MAP")
    tar_file_map = re.sub(" *: *", ":", tar_file_map)
    tar_file_map = (pair.split(':') for pair in tar_file_map.split())

    with tarfile.open(tarball, "w:gz", dereference=True) as tar:
        for filepath, archpath in tar_file_map:
            tar.add(filepath, arcname=archpath)

    out_script = os.path.join(deploy_dir, "ivinstall" + "-" + image_type + "-" + machine)
    with open(out_script, "wb") as fext:
        for f in (ivinstall_header, tarball):
            with open(f, "rb") as f:
                shutil.copyfileobj(f, fext)
    st = os.stat(out_script)
    os.chmod(out_script, st.st_mode | stat.S_IEXEC)
}

addtask do_clean before do_compile
addtask do_deploy after do_compile before do_build

