# Dynamcally created header file that includes defines for build versions
#
# Creates two files:
#       1. a C header file if specified by the variable
#           IVEIA_VERSION_HEADER_FILE (as an absolute path).
#       2. a shell script sourceable versions file IVEIA_VERSIONS_FILE, at
#           location defined below.
#
# See also VERSIONS.md
#

IVEIA_VERSIONS_FILE = "${B}/${PN}.versions"

do_compile[prefuncs] += "do_pre_compile_version"
do_pre_compile_version () {
    . ${META_IVEIA_VERSIONS_FILE}
    
    TAINTED=""
    ls ${STAMP}.*.taint 2>/dev/null && TAINTED="(tainted)"
    SRC_BUILD_HASH="${PV}${TAINTED}"

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
        cat <<-EOF | sed 's/^ *//' > "${IVEIA_VERSION_HEADER_FILE}"
            ${DECL}_1001 = "GREPPABLE_SRC_BUILD_HASH=${SRC_BUILD_HASH},${PN}";
            ${DECL}_1002 = "GREPPABLE_BUILD_DATE=${IVEIA_BUILD_DATE},${PN}";
            ${DECL}_1003 = "GREPPABLE_MACHINE=${MACHINE},${PN}";
            #define IVEIA_SRC_BUILD_HASH "${SRC_BUILD_HASH}"
            #define IVEIA_BUILD_DATE "${IVEIA_BUILD_DATE}"
            #define IVEIA_MACHINE "${MACHINE}"
			EOF
        for i in `seq $IVEIA_NUM_LAYERS`; do
            # Note: POSIX does not support bash/ksh variable indirect
            # expansion, so we use the more cumbersome `eval` below
            H="${IVEIA_VERSION_HEADER_FILE}"
            LAYER_VAR=IVEIA_META_${i}_LAYER
            LAYER=`eval "echo \\$$LAYER_VAR"`
            HASH_VAR=IVEIA_META_${i}_BUILD_HASH
            HASH=`eval "echo \\$$HASH_VAR"`
            echo "${DECL}_${i}b = \"GREPPABLE_META_${i}_LAYER=${LAYER},${PN}\";" >> "${H}"
            echo "${DECL}_${i}a = \"GREPPABLE_META_${i}_BUILD_HASH=${HASH},${PN}\";" >> "${H}"
            echo "#define ${LAYER_VAR} \"${LAYER}\"" >> "${H}"
            echo "#define ${HASH_VAR} \"${HASH}\"" >> "${H}"
        done
    fi

    # Shell-script sourceable versions file.
    echo "IVEIA_SRC_BUILD_HASH=\"${SRC_BUILD_HASH}\"" > "${IVEIA_VERSIONS_FILE}"
    cat "${META_IVEIA_VERSIONS_FILE}" >> "${IVEIA_VERSIONS_FILE}"
}

inherit deploy

do_deploy[postfuncs] += "do_deploy_versions"
do_deploy_versions() {
    install -Dm 0644 ${IVEIA_VERSIONS_FILE} ${DEPLOYDIR}/versions/$(basename ${IVEIA_VERSIONS_FILE})
}

addtask do_deploy after do_compile before do_build

