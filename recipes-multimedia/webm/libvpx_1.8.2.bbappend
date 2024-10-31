# source branch renamed from 'master' to 'main'; specify the branch, else it defaults to 'master'
# (this bbappend can be removed when the meta-openembedded/meta-oe layer is updated)
SRC_URI_remove  = " git://chromium.googlesource.com/webm/libvpx;protocol=https "
SRC_URI_prepend = " git://chromium.googlesource.com/webm/libvpx;protocol=https;branch=main "
