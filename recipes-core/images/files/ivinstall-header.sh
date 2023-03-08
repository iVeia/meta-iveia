#!/bin/bash
#
# ininstall utility - install iVeia images via JTAG, SD, SSH
#
# Copyright (c) 2020 iVeia
#

# __INSERT_YOCTO_VARS_HERE_DO_NOT_REMOVE_THIS_LINE__

CMD="$0"

endmsg()
{
    if [[ -n "$ENDMSG" ]]; then
        echo "$ENDMSG"
    fi
}

warn()
{
    echo "WARNING: $1"
}

info()
{
    echo "INFO: $1"
}

error()
{
    sync
    endmsg
    echo "ERROR: $1"
    echo "Run ""'""$CMD -h""'"" for usage"
    exit 1
}

verify()
{
    for c in $*; do
        command -v $c &> /dev/null || \
            error "script requires external command '"$c"' which could not be found"
    done
}

# Must test for bash shell early, as from here on out we use bash.
[[ -n "$BASH_VERSION" ]] || error "script requires bash shell"

#
# Create USAGE String
#
read -r -d '' USAGE <<EOF
# __INSERT_YOCTO_DOC_HERE_DO_NOT_REMOVE_THIS_LINE__
EOF

help()
{
    echo "$USAGE"
    exit 0
}

# getfilesize in a POSIX-compatible way
verify awk
getfilesize()
{
    wc -c "$1" | awk '{print $1}'
}

#
# Process args
#
# Note: use bash internal getopts as this must work on vanilla Linux or macos.
# However, long opts not supported, SAD!
#
SAVEARGS="$*"
unset DO_COPY DO_EXTRACT DO_FORMAT DO_QSPI DO_QSPI_ONLY DO_VERSION ENDMSG FORCE_SD_MODE IOBOARD
unset JTAG_REMOTE MODE SKIP_ROOTFS SSH_TARGET USE_INITRD USER_FAT_SIZE USER_LABEL USER_ROOTFS_SIZE
unset ONLY_BOOT EXTRACT_DIR DO_ASSEMBLE REPLACE REPLACE_STARTUP_SCRIPT REPLACE_EXTRA_IMAGE
SD_MODE=0
SSH_MODE=1
JTAG_MODE=2
MODE=$SD_MODE
while getopts "a:A:B:b:cCde:fhi:jJ:kn:oqQr:R:s:vxX:zZ" opt; do
    case "${opt}" in
        a) JTAG_ADAPTER="$OPTARG" ;;
        A) DO_ASSEMBLE=1; EXTRACT_DIR="$OPTARG"; ;;
        b) USER_FAT_SIZE="$OPTARG"; ;;
        B) USER_ROOTFS_SIZE="$OPTARG"; ;;
        c) DO_COPY=1; ;;
        C) DO_COPY=1; DO_COPY_SELF=1; ;;
        d) DO_COPY=1; USE_INITRD=1 ;;
        e) ENDMSG="$OPTARG"; ;;
        f) DO_FORMAT=1; ;;
        h) help ;;
        i) DO_COPY=1; IOBOARD="$OPTARG"; ;;
        j) MODE=$JTAG_MODE ;;
        J) JTAG_REMOTE="$OPTARG" ;;
        k) DO_COPY=1; SKIP_ROOTFS=1 ;;
        n) DO_FORMAT=1; USER_LABEL="$OPTARG"; ;;
        o) ONLY_BOOT=1 ;;
        q) DO_QSPI=1 ;;
        Q) DO_QSPI_ONLY=1 ;;
        r) REPLACE=1; REPLACE_STARTUP_SCRIPT="$OPTARG"; ;;
        R) REPLACE=1; REPLACE_EXTRA_IMAGE="$OPTARG"; ;;
        s) MODE=$SSH_MODE; SSH_TARGET="$OPTARG" ;;
        v) DO_VERSION=1 ;;
        x) DO_EXTRACT=1 ;;
        X) DO_EXTRACT=1; EXTRACT_DIR="$OPTARG"; ;;
        z) MODE=$SD_MODE ;;
        Z) FORCE_SD_MODE=1 ;;   # Undocumented option
        \?) error "Invalid option: -$OPTARG" 1>&2; ;;
        :) error "Invalid option: -$OPTARG requires an argument" 1>&2; ;;
    esac
done
shift $((OPTIND -1))

DEVICE="$1"
shift

# Environ
PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin:"$PATH"
SSH_OPTS="-o ConnectTimeout=5"

# Find tarball start line at end of script
verify awk
ARCHIVE_START=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$CMD")

# We're running on the target if we can find a specfic /sys file.
IS_ZYNQMP_TARGET=$([[ ! -d /sys/firmware/zynqmp ]]; echo $?)
IS_ZYNQ_TARGET=$([[ ! -d /sys/devices/platform/cpuidle-zynq.0 ]]; echo $?)
IS_TARGET=$((IS_ZYNQMP_TARGET || IS_ZYNQ_TARGET))

#
# Validate arguments
#
[[ -z "$@" ]] || error "Extra invalid options were supplied"
(($OPTIND <= 1)) && help
[[ -n "$MACHINE" ]] || error "INTERNAL ERROR: mainboard not available"
if [[ -n "$IOBOARD" ]]; then
    grep -q "\<$IOBOARD\>" <<<"$IOBOARDS" || error 'invalid ioboard "'$IOBOARD'"'
fi

#
# FORCE_SD_MODE can be easily prepended to the arg list to use SD mode,
# overiding user mode choice.
#
if ((FORCE_SD_MODE)); then
    MODE=$SD_MODE
fi

extract_archive_to_TMPDIR()
{
    TMPDIR=$(mktemp -d)
    on_exit() { rm -rf $TMPDIR; }
    trap on_exit EXIT
    tail -n+$ARCHIVE_START "$CMD" | tar -xz -C $TMPDIR || error "untar archive failed"
}

#
# If requested, display version and md5sums
#
if ((DO_VERSION)); then
    [[ -n "$IVEIA_META_BUILD_HASH" || -n "$IVEIA_BUILD_DATE" ]] || \
        error "INTERNAL ERROR: version not available"
    echo "Meta commit: ${IVEIA_META_BUILD_HASH}"
    echo "Build date: ${IVEIA_BUILD_DATE}"
    echo

    exit 0
fi

if ((DO_EXTRACT)); then
    info "Extracting Image Archive..."
    if [[ -n "$EXTRACT_DIR" ]]; then
        TMPDIR="$EXTRACT_DIR"
        mkdir "$TMPDIR" || error "Unable to create dir \"$EXTRACT_DIR\""
    else
        TMPDIR=$(mktemp -d)
    fi
    tail -n+$ARCHIVE_START "$CMD" | tar -xz -C "$TMPDIR" || error "Untar failed"
    head -n+$((ARCHIVE_START-1)) "$CMD" > "$TMPDIR"/.header || error "Header extract failed"

    echo "Contents extracted to:"
    echo "    $TMPDIR"
    exit 0
fi

if ((DO_ASSEMBLE)); then
    # stdout intended to be redirected to a file, so all messages
    # must go to stderr
    info "Reassemlble Image Archive from \"$EXTRACT_DIR\"..." 1>&2
    cd "$EXTRACT_DIR"
    cat .header || error "header not found" 1>&2
    tar cvzf - * || error "tar failed" 1>&2
    exit 0
fi

#
# For loading files via JTAG, add a simple binary header that includes:
#   - 32-bit magic number (little endian)
#   - 32-bit filesize (little endian)
#   - 32-bit CRC32 (big endian)
# iVeia U-Boot environ vars understand the above format, and can verify and
# extract the file.
#
# Note: Zynq targets are little endian.  However, U-Boot crc32 function
# generates crc in big-endian format, even on little-endian machines.
#
# Usage:
#   add_header <input_file> <output_file>
#
add_header()
{
    verify python3 crc32
    MAGIC=0x4a544147
    CRC32=0x$(crc32 <(cat "$1")) || error "add_header($1, $2) failure 1"
    FILESIZE=$(printf "0x%08X" $(getfilesize "$1")) || error "add_header($1, $2) failure 2"
    PACK_PREFIX="import sys,struct;sys.stdout.buffer.write(struct.pack"
    python3 -c "$PACK_PREFIX('<L',$MAGIC))" > "$2" || error "add_header($1, $2) failure 3"
    python3 -c "$PACK_PREFIX('<L',$FILESIZE))" >> "$2" || error "add_header($1, $2) failure 4"
    python3 -c "$PACK_PREFIX('>L',$CRC32))" >> "$2" || error "add_header($1, $2) failure 5"
    cat "$1" >> "$2" || error "add_header($1, $2) failure 6"
}

#
# Call xsdb from Windows
#
# On Windows, if a TCL script is run via ssh and immediately exits,
# the JTAG connection can become hung (process is halted).  As a
# workaround, we run a short sleep after running the script.  The
# sleep must be run AFTER xsdb exits, but before ssh exits.
#
# In addition, "call" must be used for xsdb to run in a batch file
# without exiting right after it runs.
#
# This MUST be run as a batch file, as opposed to one liner, because
# %errorlevel% substituion occurs before the command is run.
#
run_remote_tcl_in_windows()
{
    BAT_TMP=$(mktemp)
    TCL="$1"
    ADAPTER="$2"
    cat <<-EOF >${BAT_TMP}
		cd iv_staging
		call xsdb $TCL $ADAPTER
		set err=%errorlevel%
		call xsdb -eval "after 1000"
		exit %err%
		EOF
    scp ${SSH_OPTS} ${BAT_TMP} ${JTAG_REMOTE}:winxsdb.bat || error "scp $TCL to JTAG_REMOTE failed"
    ssh ${SSH_OPTS} ${JTAG_REMOTE} winxsdb.bat || error "xsdb $TCL TCL/JTAG failure"
}

#
# Setup JTAG
#
setup_jtag()
{
    if [[ -n "$JTAG_REMOTE" ]]; then
        ssh ${SSH_OPTS} $JTAG_REMOTE cd &> /dev/null || "Cannot ssh to JTAG_REMOTE ($JTAG_REMOTE)"

        JTAG_REMOTE_IS_UNIX=0
        if ssh ${SSH_OPTS} $JTAG_REMOTE uname &> /dev/null; then
            JTAG_REMOTE_IS_UNIX=1
        fi

        if ((JTAG_REMOTE_IS_UNIX)); then
            ssh ${SSH_OPTS} $JTAG_REMOTE rm -rf iv_staging
        else
            ssh ${SSH_OPTS} $JTAG_REMOTE rmdir /q /s iv_staging
        fi
        ssh ${SSH_OPTS} $JTAG_REMOTE mkdir iv_staging || error "Cannot create iv_staging"
    fi

    #
    # WORKAROUND: Often, when trying to install on a target that has just
    # been powercycled, JTAG loads can fail.  The issue appears related to
    # letting the target run fully into Linux (but this is not always
    # true).  Because the extract_archive_to_TMPDIR function takes a while
    # to run, the actual load doesn't happen until far along into the boot.
    # The fix is to run a script right NOW that halts the processor, before
    # Linux has a chance to boot.
    #
    info "Attempting to halt target..."
    HALT_TCL_ABSPATH=$(mktemp -d)/halt.tcl
    cat <<-'EOF' > ${HALT_TCL_ABSPATH}
		connect
		if {$argc > 0} {
			set jtag_cable_serial [lindex $argv 0]
		} else {
			set jtag_cable_serial "*"
		}
		targets -set -filter {jtag_cable_serial =~ "$jtag_cable_serial" && name =~ "PSU"}
		stop
		EOF
    run_jtag_tcl halt.tcl ${HALT_TCL_ABSPATH}
}

#
# Run local or remote JTAG TCL script
#
# Usage: run_jtag_tcl <TCL script> <JTAG FILES required by script ... >
#
# Requires setup_jtag() to be run beforehand.
#
run_jtag_tcl()
{
    TCL="$1"
    shift
    JTAG_FILES="$*"
    if [[ -n "$JTAG_REMOTE" ]]; then
        scp ${SSH_OPTS} $JTAG_FILES $JTAG_REMOTE:iv_staging || error "scp to JTAG_REMOTE failed"
        if ((JTAG_REMOTE_IS_UNIX)); then
            # This is Unix - NOTE UNTESTED
            ssh ${SSH_OPTS} ${JTAG_REMOTE} "cd iv_staging; xsdb \"$TCL\" \"$JTAG_ADAPTER\"" \
                || error "xsdb TCL/JTAG failure"
        else
            run_remote_tcl_in_windows "$TCL" "$JTAG_ADAPTER"
        fi
    else
        verify xsdb
        rm -rf iv_staging
        mkdir iv_staging
        cp $JTAG_FILES iv_staging/

        #
        # Must run from iv_staging dir, but must leave this function in
        # original dir - so run in subshell
        #
        # DO NOT USE double-quote for $JTAG_ADAPTER!  If it is blank, then xsdb
        # will still see it as a parameter, and the number of args will fail
        # (this mean JTAG_ADAPTER can't have spaces)
        (
            cd iv_staging
            xsdb "$TCL" $JTAG_ADAPTER
        ) || error "Failed running TCL script to program QSPI flash"
    fi
}


unmount_all()
{
    # DEVICE partitions may have been mounted multiple times.  Assume not
    # more than 5.  Assume each device has max 9 partitions.
    for i in {1..5}; do
        # sda style device partitions are of the form sda1
        umount "$DEVICE"[1-9] 2>/dev/null
        # mmcblk0 style device partitions are of the form mmcblk0p1
        umount "$DEVICE"p[1-9] 2>/dev/null
        sleep 1
    done
}


#
# Direct QSPI programming.  Replaces Xilinx's program_flash:
#   - Loads uEnv.qspi.txt into RAM for load via JTAG (similar to JTAG_MODE)
#   - Loads boot.bin to RAM
#   - Runs a QSPI script that boots to U-Boot
#   - Commands in uEnv.qspi.txt reflash QSPI and halt.
# Note: we use this process instead of program_flash because program_flash
# relies on the device being in JTAG mode, and can often fail if the device has
# booted into Linux.  iVeia boards all (mostly) boot from QSPI mode, so the TCL
# script resets the processor into JTAG mode.
#
if ((DO_QSPI_ONLY)); then
    setup_jtag

    info "Extracting archive..."
    extract_archive_to_TMPDIR
    cd $TMPDIR
    add_header jtag/uEnv.qspi.txt uEnv.qspi.txt.bin
    add_header boot/boot.bin boot.bin.bin

    JTAG_FILES="jtag/qspi.tcl boot.bin.bin uEnv.qspi.txt.bin elf/*"
    run_jtag_tcl qspi.tcl $JTAG_FILES

    echo
    info "JTAG load complete, device will now reflash QSPI."
    info "Watch the serial console to confirm reflash completion."
    echo

    exit 0
fi

#
# From here on out, we're copying/formatting, and the device is required
# (unless -q alone)
#
# Another exception: if ONLY_BOOT or REPLACE, then we don't care about other
# options (except that it is JTAG mode)
#
if ((DO_FORMAT || DO_COPY)); then
    [[ -n "$DEVICE" ]] || error "DEVICE was not specified"
fi
if ((ONLY_BOOT || REPLACE)); then
    ((MODE==JTAG_MODE)) || error "The -o, -r and -R options can only be used in JTAG mode (-j)"
else
    ((DO_FORMAT || DO_COPY || DO_QSPI)) || error "Either -f, -c, or -q required (or a combination)"
fi
if ((MODE==SD_MODE)); then
    ((DO_QSPI && !IS_TARGET)) && error "QSPI mode can only run on target"
fi

#
# JTAG is used to fully load Linux and the run the ivinstall script.
#
# The concept is to load all images to RAM via JTAG, and then boot.  Images are
# then found during boot at the specific locations where they were stored.
# Some images need to encode their length, and so have a header added (via
# add_header()) that includes the image len and CRC.
#
# The process in brief:
#   - Using xsdb (via ivinstall.tcl) load and run images to RAM:
#       - A startup.sh script that runs ivinstall with user's arguments ($*)
#       - ivinstall script (with a header added) in memory above Linux (via mem=xxx)
#       - A special uEnv.txt (with a header added) (named uEnv.ivinstall.txt)
#       - Linux images (Image, DTB, initrd)
#       - bootloader elf files (fsbl, ..., u-boot)
#   - xsdb will get boot running up to U-Boot, which then:
#       - runs the default boot command
#       - runs loadenv_jtag, which finds/validates special uEnv.txt in RAM via JTAG
#       - runs uEnv.txt's bootenv_cmd
#       - insert a special startup script into device-tree/chosen
#       - boot Linux from images already loaded in RAM via JTAG
#   - Linux boots, and then:
#       - runs ivstartup init.d script which finds chosen/startup
#       - chosen/startup loads/validates startup
#       - chosen/startup loads/validates ivinstall
#       - run startup, which ivinstalls with user's args
#
# Rough mem layout for Zynq vs ZynqMP:
#   Zynq                    ZynqMP
#   MB      ADDR            MB      ADDR
#   0       0x00000000      "       "           Mem bottom
#   5       0x00500000      "       "           JTAG magic flag (zynq only)
#   6       0x00600000      "       "           uEnv.txt (with pre-header)
#   7       0x00700000      "       "           DTB
#   8       0x00800000      "       "           Kernel
#   64      0x04000000      128     0x08000000  U-Boot
#   128     0x80000000      256     0x10000000  initrd
#   <383    0x17f00000      NA      NA          Relocated initrd by U-Boot using fdt_high
#   <384    0x18000000      NA      NA          Relocated DTB by U-Boot using initrd_high
#   384     0x18000000      768     0x30000000  Top of mem allocated to Linux (via mem=xxx)
#   384     0x18000000      768     0x30000000  startup.sh (with header)
#   385     0x18100000      769     0x30100000  extra_image (with header)
#   ...
#   >=512   0x40000000      >=1024  0x80000000  Phys mem top (up to 4GB on some boards)
#
# On Zynq, our minimum memory is 512M, so layout is tighter and required
# relocation fdt/initrd.  This decreases the max initrd size available.
#
# Xilinx changed the default U-Boot location from 64M to 128MB when going from
# Zynq to ZynqMP, and we use that default.  If not for that change, we'd be
# able to use the same layout for both.
#
# The items with the pre-header above are shifted down by the header amount.
# See add_header().  Also, see the tcl scripts and uEnv.txt for the exact
# values used.
#
# The extra_image can be an ivinstall image, a tarball or anything.  It is
# extracted from above Linux mem to /tmp/extra_image
#
if ((MODE==JTAG_MODE)); then
    setup_jtag

    info "Extracting Image Archive for JTAG mode..."
    extract_archive_to_TMPDIR
    cp "$CMD" $TMPDIR || error "Cannot copy ivinstall image to TMPDIR"
    if [[ -n "$REPLACE_STARTUP_SCRIPT" ]]; then
        cp "$REPLACE_STARTUP_SCRIPT" $TMPDIR/startup.sh \
            || error "Cannot copy STARTUP_SCRIPT \"$REPLACE_STARTUP_SCRIPT\" to TMPDIR"
    fi
    if [[ -n "$REPLACE_EXTRA_IMAGE" ]]; then
        cp "$REPLACE_EXTRA_IMAGE" $TMPDIR/extra_image \
            || error "Cannot copy EXTRA_IMAGE \"$REPLACE_EXTRA_IMAGE\" to TMPDIR"
    fi
    cd $TMPDIR
    if ((ONLY_BOOT)); then
        echo > startup.sh
        echo > extra_image
    else
        if [[ -z "$REPLACE_STARTUP_SCRIPT" ]]; then
            echo "bash /tmp/extra_image -Z $SAVEARGS" > startup.sh
        fi
        if [[ -z "$REPLACE_EXTRA_IMAGE" ]]; then
            mv $(basename "$CMD") extra_image
        fi
    fi
    add_header startup.sh startup.sh.bin
    add_header extra_image extra_image.bin
    add_header jtag/uEnv.ivinstall.txt uEnv.ivinstall.txt.bin
    cp devicetree/$MACHINE.dtb system.dtb

    JTAG_FILES="startup.sh.bin uEnv.ivinstall.txt.bin extra_image.bin jtag/ivinstall.tcl"
    JTAG_FILES+=" elf/* boot/*Image rootfs/initrd system.dtb"
    run_jtag_tcl ivinstall.tcl $JTAG_FILES

elif ((MODE==SSH_MODE)); then
    verify ssh scp
    scp ${SSH_OPTS} $CMD ${SSH_TARGET}:/tmp || error "scp failed"
    # Don't error() on ssh failure - let the error come from the remote
    ssh ${SSH_OPTS} ${SSH_TARGET} bash /tmp/$(basename ${CMD}) -Z $SAVEARGS

elif ((MODE==SD_MODE)); then
    if ((DO_FORMAT || DO_COPY)); then
        if ((IS_TARGET)); then
            if [[ "$DEVICE" = sd0 || "$DEVICE" = sd ]]; then
                DEVICE=/dev/mmcblk0
            fi
            if [[ "$DEVICE" = sd1 || "$DEVICE" = emmc ]]; then
                DEVICE=/dev/mmcblk1
            fi
        fi
        [[ -b "$DEVICE" || -d "$DEVICE" ]] || error "DEVICE must be a block device or directory"
        if [[ -d "$DEVICE" ]]; then
            ((!DO_FORMAT)) || error "DEVICE as directory not compatble with format (-f)"
            SKIP_ROOTFS=1
        fi
        if ((DO_FORMAT || (DO_COPY && (!SKIP_ROOTFS && !USE_INITRD)))); then
            (("$(id -u)" == 0)) || error "must be run as root (e.g. from sudo)"
            [[ $(uname) = Darwin ]] && error "macos only supports copy without ext4 rootfs"
        fi
    fi

    if ((DO_FORMAT)); then
        verify mount umount parted mkfs.vfat mkfs.ext4 blockdev

        BLOCKDEV_BYTES=$(blockdev --getsize64 "$DEVICE")
        BLOCKDEV_MB=$((BLOCKDEV_BYTES/1024/1024))
        if ((USER_FAT_SIZE)); then
            FAT_SIZE=$USER_FAT_SIZE
        elif ((BLOCKDEV_MB > 12000)); then
            FAT_SIZE=$((4*1024))
        else
            FAT_SIZE=512
        fi
        if ((USER_ROOTFS_SIZE)); then
            ROOTFS_SIZE=$USER_ROOTFS_SIZE
        else
            ROOTFS_SIZE=1024
        fi
        if [[ -n "$USER_LABEL" ]]; then
            LABEL="$USER_LABEL-"
        else
            LABEL="IVEIA-"
        fi

        #
        # Create partitions
        #
        info "Creating partitions"
        P1_END=$FAT_SIZE
        P2_END=$((P1_END + 1))
        P3_END=$((P2_END + ROOTFS_SIZE))
        unmount_all
        if command -v wipefs &> /dev/null; then
            wipefs -a "$DEVICE" >/dev/null || error "wipefs failed.  Is device in use?"
        else
            # Use poor man's wipefs
            dd if=/dev/zero of="$DEVICE" bs=1M count=16 status=none || \
                error "Unable to wipe disk.  Is device in use?"
        fi
        parted -s "$DEVICE" mklabel msdos || error "mklabel failed"
        MKPART="parted -s -a optimal "$DEVICE" mkpart primary"
        ${MKPART} fat32 0%              $((P1_END + 1)) || error "failed to create partition 1"
        ${MKPART} fat32 $((P1_END + 1)) $((P2_END + 1)) || error "failed to create partition 2"
        ${MKPART} ext2  $((P2_END + 1)) $((P3_END + 1)) || error "failed to create partition 3"
        ${MKPART} ext2  $((P3_END + 1)) 100%            || error "failed to create partition 4"
        parted -s "$DEVICE" set 1 boot on || error "could not make partition 1 bootable"
    fi

    # Get the partition list as an array.  Give up to a few seconds for
    # partitions to appear.
    #
    # We need this info if we're formatting or copying to block device (i.e. we
    # don't need it if this is only for QSPI, in which case it could fail if
    # the card isn't formatted).
    if [[ -b "$DEVICE" ]]; then
        if ((DO_FORMAT || DO_COPY)); then
            verify lsblk

            # Wait for partitions.  However, even when we get conformation
            # (via lsblk) that the partitions are available, it is possible to
            # have lsblk fail on a second call.  Therefore, we always sleep
            # and try again even on success.
            SECS=5
            for ((i = 0; i < 2; i++)); do
                for ((j = 0; j < SECS; j++)); do
                    (($(lsblk -n "$DEVICE" 2>/dev/null | wc -l) == 5)) && break;
                    sleep 1
                done
                ((j == SECS)) && error "all partitions were not created/exist on device"
                sleep 1
            done
            PARTS=($(lsblk -nr "$DEVICE" | awk '{print $1}' | sort))
            ((${#PARTS[*]} == 5)) || error "all partitions do not exist"
        fi
    fi

    if ((DO_FORMAT)); then
        #
        # Format partitions
        #
        # Unfortunately, mkfs.vfat can fail if run directly after parted, with the
        # error "No such file or directory", EVEN IF the file has been verified to
        # exist.  This issue has been reported online for years, but does not seem
        # to have a fix except for a sleep.
        #
        # In addition, parted has a known issue that it may remount a
        # filesystem after creating the partition if the old filesystem data is
        # still there.  Only fix is to wipe the full block device (way slow) or
        # unmount again - so we unmount again.
        #
        sleep 1 # Ensure parted done
        unmount_all
        # Create array of partition names (including primary device first)
        info "Formatting partition 1 as FAT32"
        mkfs.vfat -F 32 -n "${LABEL}BOOT" /dev/${PARTS[1]} || error "failed to format partition 1"
        info "Formatting partition 2 as raw"
        dd status=none if=/dev/zero of=/dev/${PARTS[2]} 2>/dev/null
        info "Formatting partition 3 as ext4"
        mkfs.ext4 -q -F -L "${LABEL}ROOTFS" /dev/${PARTS[3]}   || error "failed to format partition 3"
        info "Formatting partition 4 as ext4"
        mkfs.ext4 -q -F -L "${LABEL}DATA" /dev/${PARTS[4]} || error "failed to format partition 4"
    fi

    if ((DO_COPY || DO_QSPI)); then
        unset TMPMNT TMPDIR
        on_exit() {
            if [[ -n "$TMPMNT" ]]; then
                umount "${TMPMNT}" 2>/dev/null
                rmdir "$MNT"
            fi
            if [[ -n "$TMPDIR" ]]; then
                rm -rf "$TMPDIR"
            fi
        }
        trap on_exit EXIT

        TMPDIR=$(mktemp -d)
        info "Extracting Image Archive..."
        tail -n+$ARCHIVE_START "$CMD" | tar -xz -C $TMPDIR || error "untar archive failed"
    fi

    if ((DO_COPY)); then
        if [[ -b "$DEVICE" ]]; then
            verify mount
            [[ $(uname) = Darwin ]] && error "block devices not supported on macos"
            TMPMNT=$(mktemp -d)
            MNT="$TMPMNT"

            ((${#PARTS[*]} > 1)) || error "partition 1 of $DEVICE does not exist"
            mount /dev/${PARTS[1]} "$MNT" || error "could not mount partition 1 for copy"
        else
            MNT="$DEVICE"
        fi

        info "Copying boot files to DEVICE ($DEVICE)"
        cp -v $TMPDIR/boot/* "$MNT" || error "copy boot files failed"
        cp -v $TMPDIR/devicetree/$MACHINE.dtb "$MNT" || error "copy dtb failed"
        if [[ -n "$IOBOARD" ]]; then
            cp -v $TMPDIR/devicetree/${IOBOARD}_overlay.dtbo "$MNT/overlay.dtbo" || \
                error "copy dtbo failed"
        fi

        if ((USE_INITRD)); then
            info "Copying initrd rootfs to DEVICE ($DEVICE)"
            cp -v $TMPDIR/rootfs/initrd "$MNT" || error "copy initrd failed"
        elif ((!SKIP_ROOTFS)); then
            verify e2label findmnt
   			if [ -n "$DO_COPY_SELF" ]; then
            	info "Copying ivinstall script to ext4 rootfs"
				ext4_sz=$(stat --printf="%s" $TMPDIR/rootfs/rootfs.ext4)
				ivinstall_sz=$(stat --printf="%s" $0)
				resize2fs $TMPDIR/rootfs/rootfs.ext4 $((($ext4_sz + $ivinstall_sz)/1024))K
				mkdir $TMPDIR/rootfs/mnt
				mount -o loop $TMPDIR/rootfs/rootfs.ext4 $TMPDIR/rootfs/mnt
				mkdir -p $TMPDIR/rootfs/mnt/home/root
				cp $0 $TMPDIR/rootfs/mnt/home/root
				sync
				umount $TMPDIR/rootfs/mnt
			fi
            info "Copying ext4 rootfs to DEVICE ($DEVICE) partition 3"
            ((${#PARTS[*]} > 3)) || error "partition 3 of $DEVICE does not exist"
            umount /dev/${PARTS[3]} 2>/dev/null
            findmnt /dev/${PARTS[3]} && error "partition 3 already mounted, cannot install rootfs"
            dd if=$TMPDIR/rootfs/rootfs.ext4 of=/dev/${PARTS[3]} bs=1M status=none || \
                error "dd ext4 rootfs failed"
            e2label /dev/${PARTS[3]} ${LABEL}ROOTFS || \
                (warn "ROOTFS label failed, disk label not set."; \
                warn "Label failure may be due to incompatible e2fsprogs tools.")
            resize2fs -f /dev/${PARTS[3]}
        fi
    fi

    if ((DO_QSPI)); then
        [[ -f $TMPDIR/boot/boot.bin ]] || error "boot.bin missing from archive"
        flash_erase /dev/mtd0 0 0 || error "flash_erase failed"
        flashcp $TMPDIR/boot/boot.bin /dev/mtd0 || error "flashcp failed"
    fi

    # Sync just in case user does unsafe eject
    sync

    endmsg
    info "Done"
fi

exit 0

__ARCHIVE_BELOW__
