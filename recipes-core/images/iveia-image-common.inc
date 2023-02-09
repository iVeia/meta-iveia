IMAGE_FSTYPES_remove = " \
    cpio.gz cpio.bz2 \
    ext3 ext3.bz2 \
    tar.gz tar.bz2 \
    jffs2 \
    wic.qemu-sd \
    "
IMAGE_FSTYPES_append = " ext4 tar.xz"

# Boot binaries (boot.bin and uEnv.txt)
IMAGE_INSTALL += "u-boot-uenv"
IMAGE_INSTALL += "xilinx-bootbin"

# iVeia boot
IMAGE_INSTALL += "ivstartup sdmount"

IMAGE_INSTALL += "opkg"
IMAGE_REMOVE += "rpm"

IMAGE_REMOVE += "python3"

# iVeia specific
IMAGE_INSTALL += "bdevmem"
IMAGE_INSTALL += "busyloop"
IMAGE_INSTALL += "kernel-module-iv-ocp"

IMAGE_CLASSES_remove = "image-types-xilinx-qemu qemuboot-xilinx"


# create /etc/os-release with build timestamp and git hash for meta-iveia 
META_IVEIA_VER_DIR := "${THISDIR}"  
gen_os_release() {
        echo "date: $(date)" >> ${IMAGE_ROOTFS}/etc/os-release
        META_IVEIA_COMMIT=`git -C ${META_IVEIA_VER_DIR} describe --dirty --match=NeVeRmAtCh --always`
        echo "meta-iveia: ${META_IVEIA_COMMIT}" >> ${IMAGE_ROOTFS}/etc/os-release
}
IMAGE_PREPROCESS_COMMAND += "gen_os_release; "

