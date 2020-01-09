IMAGE_INSTALL += " ivstartup "
IMAGE_INSTALL += " rsync parted coreutils "
IMAGE_INSTALL += " e2fsprogs-e2fsck e2fsprogs-mke2fs e2fsprogs-tune2fs e2fsprogs-badblocks "

IMAGE_FSTYPES_remove = "cpio cpio.gz.u-boot cpio.gz cpio.bz2 tar.gz cpio.gz ext3 ext3.bz2 tar.bz2 cpio.bz2 jffs2"
