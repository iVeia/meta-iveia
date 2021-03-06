# Boot binaries (boot.bin and uEnv.txt)
IMAGE_INSTALL += "u-boot-uenv"
IMAGE_INSTALL += "xilinx-bootbin"

# iVeia boot
IMAGE_INSTALL += "ivstartup sdmount"

# Add on packages
IMAGE_INSTALL += "rsync parted coreutils"
IMAGE_INSTALL += "e2fsprogs-e2fsck e2fsprogs-mke2fs e2fsprogs-tune2fs e2fsprogs-badblocks"
IMAGE_INSTALL += "i2c-tools"
IMAGE_INSTALL += "strace"
IMAGE_INSTALL += "lmsensors-sensors"
IMAGE_INSTALL += "lmsensors-libsensors"
IMAGE_INSTALL += "bdevmem"
IMAGE_INSTALL += "util-linux"

IMAGE_INSTALL += "opkg"
IMAGE_REMOVE += "rpm"

IMAGE_INSTALL += "kernel-module-ocp"

IMAGE_CLASSES_remove = "image-types-xilinx-qemu qemuboot-xilinx"

IMAGE_FSTYPES_remove = " \
    cpio.gz cpio.bz2 cpio.gz \
    ext3 ext3.bz2 \
    tar.gz tar.bz2 \
    jffs2 \
    wic.qemu-sd \
    "
IMAGE_FSTYPES_append = " ext4 tar.xz"
