require recipes-core/images/iveia-image-minimal.bb

# cpio only needed for minimal images that can be used to boot via ramfs
#IMAGE_FSTYPES_remove = "cpio cpio.gz.u-boot"

# Use openssh, not dropbear
IMAGE_FEATURES += "ssh-server-openssh"
IMAGE_INSTALL += "packagegroup-core-ssh-openssh"

# Add on packages
IMAGE_INSTALL += "base-files-extras"
IMAGE_INSTALL += "rsync parted coreutils"
IMAGE_INSTALL += "e2fsprogs-e2fsck e2fsprogs-mke2fs e2fsprogs-tune2fs e2fsprogs-badblocks e2fsprogs-resize2fs"

IMAGE_INSTALL += "util-linux"
IMAGE_INSTALL += "i2c-tools"
IMAGE_INSTALL += "strace"
IMAGE_INSTALL += "lmsensors-sensors lmsensors-libsensors"
IMAGE_INSTALL += "bc"
IMAGE_INSTALL += "stress-ng"
IMAGE_INSTALL += "iperf2 iperf3"
IMAGE_INSTALL += "memtester"
IMAGE_INSTALL += "lrzsz"
IMAGE_INSTALL += "libgpiod"
IMAGE_INSTALL += "bash-completion"
IMAGE_INSTALL += "ethtool"
IMAGE_INSTALL += "htop"
IMAGE_INSTALL += "socat"
IMAGE_INSTALL += "tree"
IMAGE_INSTALL += "gdbserver"

#IMAGE_INSTALL += "python3"

