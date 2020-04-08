# Dynamcally created header file that includes defines for build versions
#
# Parameters: IVEIA_VERSION_HEADER_FILE: location of C header file in builddir
# (${B}), created by this class.

META_DIR := "${THISDIR}"

do_compile[prefuncs] += "do_pre_compile_version"
do_pre_compile_version () {
    rm -f ${IVEIA_VERSION_HEADER_FILE}

    #
    # Recipe source may be patched, in which case git describe's "dirty" would
    # always report true.  Instead, to show that the recipe has been mucked
    # with, we check for tainted forced builds vis Yocto, and if found we
    # append a tainted label to the revision
    #
    TAINTED=""
    ls ${STAMP}.*.taint && TAINTED="(tainted)"
    GIT_REV="`cd ${S} && git describe --long --tags`${TAINTED}"
    echo '#define IVEIA_SRC_BUILD_HASH "'${GIT_REV}$'"' >> ${IVEIA_VERSION_HEADER_FILE}

    #
    # For meta-iveia layer itself, we can correctly show git describe's dirty flag
    #
    GIT_REV=`cd ${META_DIR} && git describe --long --tags --dirty`
    echo '#define IVEIA_META_BUILD_HASH "'${GIT_REV}'"' >> ${IVEIA_VERSION_HEADER_FILE}

    echo '#define IVEIA_MACHINE "'${MACHINE}'"' >> ${IVEIA_VERSION_HEADER_FILE}
}

