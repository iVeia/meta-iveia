# Dynamcally created header file that includes defines for build versions, dates, etc.
#
# Parameters: IVEIA_VERSION_HEADER_FILE: location of C header file in builddir
# (${B}), created by this class.

append_gitrev_version_macro() {
    GIT_DIR=$1
    MACRO_NAME=$2
    CHECK_FOR_TAINT=$3
    GIT_REV=`cd ${GIT_DIR} && git describe --long --tags`
    TAINTED=""
    TAINT_FILES=${STAMP}.*.taint
    if [ ${CHECK_FOR_TAINT} -ne 0 -a -n "${TAINT_FILES}" ]; then
        TAINTED="(tainted)"
    fi
    echo '#define '${MACRO_NAME}' "'${GIT_REV}${TAINTED}'"' >> ${IVEIA_VERSION_HEADER_FILE}
}

RECIPE_DIR := "${THISDIR}"

do_configure[postfuncs] += "do_post_configure_version"
do_post_configure_version () {
    rm -f ${IVEIA_VERSION_HEADER_FILE}
    append_gitrev_version_macro ${S}            IVEIA_SRC_BUILD_HASH    1
    append_gitrev_version_macro ${RECIPE_DIR}   IVEIA_META_BUILD_HASH   0
    echo '#define IVEIA_MACHINE "'${MACHINE}'"' >> ${IVEIA_VERSION_HEADER_FILE}
}

