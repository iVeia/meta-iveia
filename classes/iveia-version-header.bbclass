# Dynamcally created header file that includes defines for build versions
#
# Creates two files:
#       1. a C header file if specified by the variable
#           IVEIA_VERSION_HEADER_FILE (as an absolute path).
#       2. a shell script sourceable versions file IVEIA_VERSIONS_FILE, at
#           location defined below.

META_DIR := "${THISDIR}"
IVEIA_VERSIONS_FILE = "${B}/${PN}.versions"

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
    if (cd "${S}"; git describe --tags); then
        GIT_REV=$(cd "${S}" && git describe --long --tags)
    else
        GIT_REV="UNKNOWN"
    fi
    echo 'SRC_BUILD_HASH="'"${GIT_REV}"'"' > ${WORKDIR}/src_build_hash
}

do_compile[prefuncs] += "do_pre_compile_version"
do_pre_compile_version () {
    META_BUILD_HASH=$(cd ${META_DIR} && git describe --long --tags --dirty)
    BUILD_DATE=$(date -u "+%F_%T")

    if [ -f ${WORKDIR}/src_build_hash ]; then
        . ${WORKDIR}/src_build_hash
    fi
    if [ -z "${SRC_BUILD_HASH}" ]; then
        SRC_BUILD_HASH="NA"
    fi

    TAINTED=""
    ls ${STAMP}.*.taint 2>/dev/null && TAINTED="(tainted)"
    SRC_BUILD_HASH="${SRC_BUILD_HASH}${TAINTED}"

    #
    # If IVEIA_VERSION_HEADER_FILE is given, create version header file.
    #
    # Additionally embed version string into the binaries, into an easily
    # greppable format.  To avoid any name conflict, string identifier is given
    # a arbitrary unique name.
    #
    # To grep name from binary, you might use:
    #   strings MY_BINARY | awk -F= '/^GREPPABLE_SRC_BUILD_HASH/{print $2}'
    #
    if [ -n "${IVEIA_VERSION_HEADER_FILE}" ]; then
        DECL='static char * __attribute__((used)) UID_734176a5a68b860afbe2293ae7ee22fd'
        GREPPABLE_SRC_BUILD_HASH="GREPPABLE_SRC_BUILD_HASH=${SRC_BUILD_HASH}"
        GREPPABLE_META_BUILD_HASH="GREPPABLE_META_BUILD_HASH=${META_BUILD_HASH}"
        GREPPABLE_BUILD_DATE="GREPPABLE_BUILD_DATE=${BUILD_DATE}"
        GREPPABLE_MACHINE="GREPPABLE_MACHINE=${MACHINE}"
        cat <<-EOF | sed 's/^ *//' > "${IVEIA_VERSION_HEADER_FILE}"
            ${DECL}_1 = "${GREPPABLE_SRC_BUILD_HASH}";
            ${DECL}_2 = "${GREPPABLE_META_BUILD_HASH}";
            ${DECL}_3 = "${GREPPABLE_BUILD_DATE}";
            ${DECL}_4 = "${GREPPABLE_MACHINE}";
            #define IVEIA_SRC_BUILD_HASH "${SRC_BUILD_HASH}"
            #define IVEIA_META_BUILD_HASH "${META_BUILD_HASH}"
            #define IVEIA_BUILD_DATE "${BUILD_DATE}"
            #define IVEIA_MACHINE "${MACHINE}"
			EOF
    fi

    # Shell-script sourceable versions file.
    cat <<-EOF | sed 's/^ *//' > "${IVEIA_VERSIONS_FILE}"
        IVEIA_SRC_BUILD_HASH="${SRC_BUILD_HASH}"
        IVEIA_META_BUILD_HASH="${META_BUILD_HASH}"
        IVEIA_BUILD_DATE="${BUILD_DATE}"
        IVEIA_MACHINE="${MACHINE}"
		EOF
}

inherit deploy

do_deploy[postfuncs] += "do_deploy_versions"
do_deploy_versions() {
    install -Dm 0644 ${IVEIA_VERSIONS_FILE} ${DEPLOYDIR}/versions/$(basename ${IVEIA_VERSIONS_FILE})
}

addtask do_deploy after do_compile before do_build

