# Dynamcally created header file that includes defines for build versions
#
# Parameters: IVEIA_VERSION_HEADER_FILE: location of C header file in builddir
# (${B}), created by this class.

META_DIR := "${THISDIR}"

#
# Because a recipe may be patched and would therefore report the wrong "git
# describe", we get the git version just before patch time.  In addition, to
# show that the recipe has been mucked with, we check for tainted forced builds
# via Yocto, and if found we append a tainted label to the revision
#
# For the meta layer version this doesn't apply, so we get the version at
# compile time.
#
do_patch[prefuncs] += "do_post_patch_version"
do_post_patch_version() {
    GIT_REV=`cd ${S} && git describe --long --tags`
    echo '#define SRC_BUILD_HASH "'${GIT_REV}$'"' > ${WORKDIR}/iveia_src_version
}

do_compile[prefuncs] += "do_pre_compile_version"
do_pre_compile_version () {
    cp ${WORKDIR}/iveia_src_version ${IVEIA_VERSION_HEADER_FILE}

    TAINTED=""
    ls ${STAMP}.*.taint 2>/dev/null && TAINTED="(tainted)"
    echo '#define SRC_BUILD_TAINT "'${TAINTED}'"' >> ${IVEIA_VERSION_HEADER_FILE}
    echo '#define IVEIA_SRC_BUILD_HASH (SRC_BUILD_HASH SRC_BUILD_TAINT)' >> ${IVEIA_VERSION_HEADER_FILE}

    GIT_REV=`cd ${META_DIR} && git describe --long --tags --dirty`
    echo '#define IVEIA_META_BUILD_HASH "'${GIT_REV}'"' >> ${IVEIA_VERSION_HEADER_FILE}

    echo '#define IVEIA_MACHINE "'${MACHINE}'"' >> ${IVEIA_VERSION_HEADER_FILE}
}

