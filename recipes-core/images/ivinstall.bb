SUMMARY = "Create SD Card Self Extracting Image Script"
LICENSE = "CLOSED"

inherit deploy

# ivinstall depends on the final iveia image - but it can't use DEPENDS, as it
# depends on do_populate_sysroot, which ${IVIMG} doesn't have.
IVIMG := "iveia-image-minimal"
do_deploy[depends] = "${IVIMG}:do_build"

LOCAL_FILES := "${THISDIR}/files"
FILESEXTRAPATHS_prepend := "${LOCAL_FILES}:"
HEADER_ORIG = "ivinstall-header.sh"
HEADER = "ivinstall-header-final.sh"
SRC_URI = "file://${HEADER_ORIG}"
S = "${WORKDIR}"
B = "${S}"

INSERTION_STRING = "__INSERT_YOCTO_VARS_HERE_DO_NOT_REMOVE_THIS_LINE__"

inherit iveia-version-header
IVEIA_VERSION_META = "${S}/version"

do_compile() {
    INSERT_VARS="MACHINE='${MACHINE}'\n"
    IOBOARDS=""
    for i in ${DEPLOY_DIR_IMAGE}/devicetree/*.dtbo; do
        IOBOARD=$(basename "$i")
        IOBOARD="${IOBOARD%.*}"
        IOBOARDS+="${IOBOARD} "
    done
    INSERT_VARS+="IOBOARDS='${IOBOARDS}'\n"
    INSERT_VARS+="VERSION='"$(echo $(cat version))"'\n"

    sed "s/.*${INSERTION_STRING}.*/${INSERT_VARS}/" ${B}/${HEADER_ORIG} > ${B}/${HEADER}
}

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
        dep_dir("pmu-" + machine + ".elf") :       {"arcname" : "elf/pmu.elf"},
        dep_dir("arm-trusted-firmware.elf") :      {"arcname" : "elf/atf.elf"},
        dep_dir("u-boot.elf") :                    {"arcname" : "elf/u-boot.elf"},
        dep_dir("boot.bin") :                      {"arcname" : "boot/boot.bin"},
        dep_dir("uEnv.txt") :                      {"arcname" : "boot/uEnv.txt"},
        dep_dir("devicetree") :                    {"arcname" : "devicetree"},  # recursive dir
        dep_dir("Image") :                         {"arcname" : "boot/Image"},
        dep_dir(rootfs_base + ".cpio.gz.u-boot") : {"arcname" : "rootfs/initrd"},
        dep_dir(rootfs_base + ".ext4") :           {"arcname" : "rootfs/rootfs.ext4"},
        dep_dir("startup.sh") :                    {"arcname" : "boot/startup.sh"},
        loc_dir("uboot.tcl") :                     {"arcname" : "jtag/uboot.tcl"},
        loc_dir("uEnv.txt") :                      {"arcname" : "jtag/uEnv.txt"},
    }

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

