# source branch renamed from 'master' to 'main'; specify the branch, else bitbake defaults to 'master'
# (this bbappend can be removed when the core/meta layer is updated)
SRC_URI_remove  = " git://gitlab.gnome.org/GNOME/mobile-broadband-provider-info.git;protocol=https "
SRC_URI_prepend = " git://gitlab.gnome.org/GNOME/mobile-broadband-provider-info.git;protocol=https;branch=main "
