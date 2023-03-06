require recipes-core/images/petalinux-image-full.bb
require iveia-image-common.inc

# cpio only needed for minimal images that can be used to boot via ramfs
IMAGE_FSTYPES:remove = "cpio cpio.gz.u-boot"
