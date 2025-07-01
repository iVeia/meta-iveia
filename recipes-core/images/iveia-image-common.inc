# Remove uneeded packages included by Xilinx.
# Note: meson depends on python3, which is huge!
IMAGE_INSTALL:remove = "\
    bridge-utils \
    can-utils \
    htop \
    iperf3 \
    meson \
    "

IMAGE_FSTYPES:remove = " \
    cpio.gz cpio.bz2 \
    ext3 ext3.bz2 \
    tar.gz tar.bz2 \
    jffs2 \
    wic.qemu-sd \
    "
IMAGE_FSTYPES:append = " ext4 tar.xz"

# iVeia boot
IMAGE_INSTALL += "xilinx-bootbin"
IMAGE_INSTALL += "ivstartup sdmount"

IMAGE_INSTALL += "opkg"

# iVeia specific
IMAGE_INSTALL += "bdevmem"
IMAGE_INSTALL += "busyloop"
IMAGE_INSTALL += "kernel-module-iv-ocp"
IMAGE_INSTALL += "kernel-module-iv-zap"
IMAGE_INSTALL += "ivfru"

IMAGE_CLASSES:remove = "image-types-xilinx-qemu qemuboot-xilinx"

# Required for ivinstall:
IMAGE_INSTALL += "parted"
IMAGE_INSTALL += "e2fsprogs-e2fsck e2fsprogs-mke2fs e2fsprogs-tune2fs"
IMAGE_INSTALL += "e2fsprogs-badblocks e2fsprogs-resize2fs"
IMAGE_INSTALL += "util-linux"

# create /etc/os-release with build timestamp and git hash for meta-iveia 
gen_os_release() {
     cp ${META_IVEIA_VERSIONS_FILE} ${IMAGE_ROOTFS}/etc/os-release
}
IMAGE_PREPROCESS_COMMAND += "gen_os_release; "

