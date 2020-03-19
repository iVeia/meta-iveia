FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += "file://iveia-version-banner.patch"

# These patches must be applied after configure because configure dynamically
# creates some sources (based off of the XSA hardware def), and these patches
# are applied to them.
POST_CONFIGURE_PATCHES := "\
    ${THISDIR}/files/add-iveia-init-hook.patch \
    ${THISDIR}/files/use-uart1.patch \
    "
FSBL_SRCS := "\
    ${THISDIR}/files/iv_z8_init.c \
    "

RECIPE_DIR := "${THISDIR}"
FSBL_SRC_DIR = "${B}/fsbl"

# Dynamcally created header file that includes defines for build versions,
# dates, etc.
IVEIA_VERSION_HEADER_FILE = "${B}/fsbl/iveia_version.h"

append_gitrev_version_macro() {
    GIT_DIR=$1
    MACRO_NAME=$2
    COMMIT=$3
    GIT_REV=`cd ${GIT_DIR} && git describe --long --tags ${COMMIT}`
    echo '#define '${MACRO_NAME}' "'${GIT_REV}'"' >> ${IVEIA_VERSION_HEADER_FILE}
}

do_configure[postfuncs] += "do_post_configure"
do_post_configure () {
    rm -f ${IVEIA_VERSION_HEADER_FILE}
    append_gitrev_version_macro ${S}            FSBL_BUILD_HASH         ${SRCREV}
    append_gitrev_version_macro ${RECIPE_DIR}   FSBL_META_BUILD_HASH
    echo '#define FSBL_MACHINE "'${MACHINE}'"' >> ${IVEIA_VERSION_HEADER_FILE}

    # Apply patches, but skip if it has already been applied correctly (which we
    # test with a reversed dry-run).
    for p in ${POST_CONFIGURE_PATCHES}; do
        if ! patch -p1 -N -d ${B} --reverse --dry-run < ${p}; then
            patch -p1 -N -d ${B} < ${p}
        fi
    done

    cp ${FSBL_SRCS} ${FSBL_SRC_DIR}
}

