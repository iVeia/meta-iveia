do_configure[postfuncs] += "do_post_configure"
do_post_configure () {
    # Apply patches, but skip if it has already been applied correctly (which we
    # test with a reversed dry-run).
    for p in ${POST_CONFIGURE_PATCHES}; do
        if ! patch -p1 -N -d ${B} --reverse --dry-run < ${p}; then
            patch -p1 -N -d ${B} < ${p}
        fi
    done
}
