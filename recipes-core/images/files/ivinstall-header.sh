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
unset ONLY_BOOT
SD_MODE=0
SSH_MODE=1
JTAG_MODE=2
MODE=$SD_MODE
while getopts "B:b:cde:fhi:jJ:kn:oqQs:vxzZ" opt; do
    case "${opt}" in
        b) USER_FAT_SIZE="$OPTARG"; ;;
        B) USER_ROOTFS_SIZE="$OPTARG"; ;;
        c) DO_COPY=1; ;;
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
        s) MODE=$SSH_MODE; SSH_TARGET="$OPTARG" ;;
        v) DO_VERSION=1 ;;
        x) DO_EXTRACT=1 ;;
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
ARCHIVE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$CMD")

# We're running on the target if we can find a specfic /sys file.
IS_ZYNQMP_TARGET=$([[ ! -d /sys/firmware/zynqmp ]]; echo $?)
IS_ZYNQ_TARGET=$([[ ! -d /sys/devices/platform/cpuidle-zynq.0 ]]; echo $?)
IS_TARGET=$((IS_ZYNQMP_TARGET || IS_ZYNQ_TARGET))

#
# Validate arguments
#
[[ -z "$@" ]] || error "Extra invalid options were supplied"
(($OPTIND <= 1)) && { echo "$USAGE"; exit 0; }
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
    tail -n+$ARCHIVE "$CMD" | tar -xz -C $TMPDIR || error "untar archive failed"
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

    echo "Archive MD5SUMs:"
    extract_archive_to_TMPDIR
    (
        cd $TMPDIR
        if [[ $(uname) == Darwin ]]; then
            verify md5 xargs
            find . -depth +1 -type f | xargs -n1 md5 | \
                awk -F"[() =]*" '{print $3 " " $2}' | sort -k 2 > md5sums
        else
            verify md5sum xargs
            find . -type f | xargs -n1 md5sum | sort -k 2 > md5sums
        fi
        while read ARCH_MD5 ARCH_FILE; do
            echo "   ${ARCH_MD5} ${ARCH_FILE}"
        done < md5sums
    )
    exit 0
fi

if ((DO_EXTRACT)); then
    info "Extracting Image Archive..."
    TMPDIR=$(mktemp -d)
    tail -n+$ARCHIVE "$CMD" | tar -xz -C $TMPDIR

    echo "Contents extracted to:"
    echo "    $TMPDIR"
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
    verify xsdb
    info "Extracting archive..."
    extract_archive_to_TMPDIR
    (
        cd $TMPDIR
        add_header jtag/uEnv.qspi.txt uEnv.qspi.txt.bin || exit 1
        add_header boot/boot.bin boot.bin.bin || exit 1
        xsdb jtag/qspi.tcl || exit 1
    ) || error "Failed running TCL script to program QSPI flash"

    info "JTAG load complete, device will now reflash QSPI."
    info "Watch the serial console to confirm reflash completion."
    echo

    exit 0
fi

#
# From here on out, we're copying/formatting, and the device is required
# (unless -q alone)
#
# Another exception: if ONLY_BOOT, then we don't care about other options
# (excpet that it is JTAG mode)
#
if ((DO_FORMAT || DO_COPY)); then
    [[ -n "$DEVICE" ]] || error "DEVICE was not specified"
fi
if ((ONLY_BOOT)); then
    ((MODE==JTAG_MODE)) || error "The -o option can only be used in JTAG mode (-j)"
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
# Rough mem layout (in MBs):
#   0       Mem bottom
#   5       JTAG magic flag (zynq only)
#   6       uEnv.txt (with pre-header)
#   7       DTB
#   8       Kernel
#   128     initrd
#   <255    Relocated initrd
#   <256    Relocated DTB
#   256     startup.sh (with header)
#   257     ivinstall script (with header)
#   ...
#   >=512   Mem top (at least 512MB, up to 4GB on some boards).
# The items with the pre-header above are shifted down by the header amount.
# See add_header().  Also, see the tcl scripts and uEnv.txt for the exact
# values used.
#
if ((MODE==JTAG_MODE)); then
    [[ -z "$JTAG_REMOTE" ]] && error "JTAG mode currently requires using remote (-J)"
    info "Extracting Image Archive for JTAG mode..."
    extract_archive_to_TMPDIR
    cp "$CMD" $TMPDIR
    (
        cd $TMPDIR
        if ((ONLY_BOOT)); then
            echo > empty_file
            add_header empty_file startup.sh.bin
            add_header empty_file ivinstall.bin
        else
            BASECMD=$(basename "$CMD")
            echo "bash /tmp/ivinstall -Z $SAVEARGS" > startup.sh
            add_header startup.sh startup.sh.bin
            add_header "$BASECMD" ivinstall.bin
        fi
        add_header jtag/uEnv.ivinstall.txt uEnv.ivinstall.txt.bin
        cp devicetree/$MACHINE.dtb system.dtb
        scp ${SSH_OPTS} startup.sh.bin uEnv.ivinstall.txt.bin ivinstall.bin jtag/ivinstall.tcl \
            elf/* boot/*Image rootfs/initrd system.dtb \
            $JTAG_REMOTE: || error "scp to JTAG_REMOTE failed"
        # On Windows, if the ivinstall.tcl script is run via ssh and
        # immediately exits, the JTAG connection becomes hung (process is
        # halted).  As a workaround, we test for Linux/Windows, and run a short
        # sleep after running the script.  The sleep must be run AFTER xsdb
        # exits, but before ssh exits.
        if ssh ${SSH_OPTS} $JTAG_REMOTE uname &> /dev/null; then
            # This is Unix - NOTE UNTESTED
            ssh ${SSH_OPTS} $JTAG_REMOTE xsdb ivinstall.tcl
        else
            # This is Windows - note '&' in Windows is equiv to ';' in Unix
            ssh ${SSH_OPTS} $JTAG_REMOTE 'xsdb ivinstall.tcl & xsdb -eval "after 1000"'
        fi
    )

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
        umount "$DEVICE"[1-9] 2>/dev/null
        umount "$DEVICE"p[1-9] 2>/dev/null
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
            PARTS=($(lsblk -nrx NAME "$DEVICE" | awk '{print $1}'))
            ((${#PARTS[*]} == 5)) || error "INTERNAL ERROR: failed to create all partitions"
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
        sleep 1 # Ensure parted done
        # Create array of partition names (including primary device first)
        info "Formatting partition 1 as FAT32"
        ((!MISSING_DEV)) || error "Some block devs did not complete partition"
        mkfs.vfat -F 32 -n "${LABEL}BOOT" /dev/${PARTS[1]} || error "failed to format partition 1"
        info "Formatting partition 2 as raw"
        dd status=none if=/dev/zero of=/dev/${PARTS[2]} 2>/dev/null
        info "Formatting partition 3 as ext4"
        mkfs.ext4 -q -F -L "${LABEL}ROOTFS" /dev/${PARTS[3]}   || error "failed to format partition 3"
        info "Formatting partition 4 as ext4"
        mkfs.ext4 -q -F -L "${LABEL}SDHOME" /dev/${PARTS[4]} || error "failed to format partition 4"
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
        tail -n+$ARCHIVE "$CMD" | tar -xz -C $TMPDIR || error "untar archive failed"
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
            info "Copying ext4 rootfs to DEVICE ($DEVICE) partition 3"
            ((${#PARTS[*]} > 3)) || error "partition 3 of $DEVICE does not exist"
            umount /dev/${PARTS[3]} 2>/dev/null
            findmnt /dev/${PARTS[3]} && error "partition 3 already mounted, cannot install rootfs"
            dd if=$TMPDIR/rootfs/rootfs.ext4 of=/dev/${PARTS[3]} bs=1M status=none || \
                error "dd ext4 rootfs failed"
            e2label /dev/${PARTS[3]} ${LABEL}ROOTFS || \
                (warn "ROOTFS label failed, disk label not set."; \
                warn "Label failure may be due to incompatible e2fsprogs tools.")
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
