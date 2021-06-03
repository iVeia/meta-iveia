SUMMARY = "Create SD Card Self Extracting Image Script"
LICENSE = "CLOSED"

inherit deploy

# ivinstall depends on the final iveia image - but it can't use DEPENDS, as it
# depends on do_populate_sysroot, which ${IVIMG} doesn't have.
IVIMG := "iveia-image-minimal"
do_deploy[depends] = "${IVIMG}:do_build"
DEPENDS_append = " fsbl u-boot-xlnx u-boot-uenv xilinx-bootbin device-tree linux-xlnx ivstartup"
DEPENDS_append_zynqmp = " pmu-firmware arm-trusted-firmware"

LOCAL_FILES := "${THISDIR}/files"
FILESEXTRAPATHS_prepend := "${LOCAL_FILES}:"
HEADER_DOC = "ivinstall-doc"
HEADER_ORIG = "ivinstall-header.sh"
HEADER = "ivinstall-header-final.sh"
SRC_URI = "\
    file://${HEADER_ORIG} \
    file://${HEADER_DOC} \
    "

S = "${WORKDIR}"
B = "${S}"

VARS_INSERT_STR = "__INSERT_YOCTO_VARS_HERE_DO_NOT_REMOVE_THIS_LINE__"
DOC_INSERT_STR = "__INSERT_YOCTO_DOC_HERE_DO_NOT_REMOVE_THIS_LINE__"

inherit iveia-version-header

do_compile() {
    INSERT_VARS="MACHINE='${MACHINE}'\n"

    # Extract IOBOARD names from devicetree/*_overlay.dtbo files.
    IOBOARDS=""
    for i in ${DEPLOY_DIR_IMAGE}/devicetree/*_overlay.dtbo; do
        IOBOARD=$(basename "$i")
        IOBOARD="${IOBOARD%_overlay.dtbo}"
        IOBOARDS+="${IOBOARD} "
    done
    INSERT_VARS+="IOBOARDS='${IOBOARDS}'\n"

    . "${B}/${PN}.versions"
    INSERT_VARS+="IVEIA_META_BUILD_HASH='${IVEIA_META_BUILD_HASH}'\n"
    INSERT_VARS+="IVEIA_BUILD_DATE='${IVEIA_BUILD_DATE}'\n"

    # Insert variables defs, and then the header doc into the named lines of
    # the ivinstall-header
    sed "s/.*${VARS_INSERT_STR}.*/${INSERT_VARS}/" ${B}/${HEADER_ORIG} > ${B}/header.tmp
    sed "/${DOC_INSERT_STR}/r ${HEADER_DOC}" ${B}/header.tmp | grep -v "${DOC_INSERT_STR}" \
        > ${B}/${HEADER}
}

IS_ZYNQMP = "0"
IS_ZYNQMP_zynqmp = "1"

python do_deploy() {
    from functools import partial
    import shutil
    import tarfile
    import stat

    # get necessary env vars
    build_dir = d.getVar("B")
    header = d.getVar("HEADER")
    machine = d.getVar("MACHINE")
    deploy_dir = d.getVar("DEPLOY_DIR_IMAGE")
    local_files_dir = d.getVar("LOCAL_FILES")
    ivimg = d.getVar("IVIMG")
    is_zynqmp = int(d.getVar("IS_ZYNQMP"))

    tarball = os.path.join(deploy_dir, "ivinstall-image.tgz")
    ivinstall_header = os.path.join(build_dir, header)
    rootfs_base = ivimg + "-" + machine

    #
    # Full list of filenames to be added to tarball, with src_dir and keyword
    # args to used for tarfile.add()
    #
    dep_dir = partial(os.path.join, deploy_dir)
    loc_dir = partial(os.path.join, local_files_dir)
    tar_files = {
        dep_dir("fsbl-" + machine + ".elf") :      {"arcname" : "elf/fsbl.elf"},
        dep_dir("u-boot.elf") :                    {"arcname" : "elf/u-boot.elf"},
        dep_dir("boot.bin") :                      {"arcname" : "boot/boot.bin"},
        dep_dir("uEnv.txt") :                      {"arcname" : "boot/uEnv.txt"},
        dep_dir("devicetree") :                    {"arcname" : "devicetree"},  # recursive dir
        dep_dir(rootfs_base + ".cpio.gz.u-boot") : {"arcname" : "rootfs/initrd"},
        dep_dir(rootfs_base + ".ext4") :           {"arcname" : "rootfs/rootfs.ext4"},
        dep_dir("startup.sh") :                    {"arcname" : "boot/startup.sh"},
        loc_dir("uboot.tcl") :                     {"arcname" : "jtag/uboot.tcl"},
        loc_dir("uEnv.txt") :                      {"arcname" : "jtag/uEnv.txt"},
        loc_dir("uEnv.qspi.txt") :                 {"arcname" : "jtag/uEnv.qspi.txt"},
    }

    if is_zynqmp:
        addtional_tar_files = {
            dep_dir("Image") :                     {"arcname" : "boot/Image"},
            loc_dir("qspi-zynqmp.tcl") :           {"arcname" : "jtag/qspi.tcl"},
            dep_dir("pmu-" + machine + ".elf") :   {"arcname" : "elf/pmu.elf"},
            dep_dir("arm-trusted-firmware.elf") :  {"arcname" : "elf/atf.elf"},
        }
        tar_files.update(addtional_tar_files)
    else:
        addtional_tar_files = {
            dep_dir("uImage") :                    {"arcname" : "boot/uImage"},
            loc_dir("qspi-zynq.tcl") :             {"arcname" : "jtag/qspi.tcl"},
        }
        tar_files.update(addtional_tar_files)

    with tarfile.open(tarball, "w:gz", dereference=True) as tar:
        for f in tar_files:
            tar.add(f, **tar_files[f])

    out_script = os.path.join(deploy_dir, "ivinstall-" + machine)
    with open(out_script, "wb") as fext:
        for f in (ivinstall_header, tarball):
            with open(f, "rb") as f:
                shutil.copyfileobj(f, fext)
    st = os.stat(out_script)
    os.chmod(out_script, st.st_mode | stat.S_IEXEC)
}

addtask do_deploy after do_compile before do_build

