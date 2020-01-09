require recipes-core/images/petalinux-image-minimal.bb
require iveia-image-common.inc

# Minimal image can be used to boot via ramfs
IMAGE_FSTYPES_append = " cpio cpio.gz.u-boot"

