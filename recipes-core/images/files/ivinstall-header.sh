#!/bin/bash
#
# ininstall utility - install iVeia images via JTAG, SD, SSH
#
# Copyright (c) 2020 iVeia
#

# __INSERT_YOCTO_VARS_HERE_DO_NOT_REMOVE_THIS_LINE__

CMD="$0"

info()
{
    echo "INFO: $1"
}

error()
{
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
USAGE: ivinstall [MODE-OPTION] [OPTIONS] DEVICE

DESCRIPTION

    ivinstall is a self-extracting shell archive that installs Yocto-built
    Linux images (bootloaders, kernel, rootfs) onto iVeia target hardware via
    one of multiple methods (SD, JTAG, SSH).  The script is self-contained: all
    of the required boot images are baked into this script, and therefore it
    can be shared and/or transferred to other users/machines.

    Images are installed onto DEVICE, which is a top-level block device.  The
    DEVICE refers to the actual block device name, so this will vary on the
    host.  On the target, as a convenience an alias can be used for the DEVICE:
        - /dev/mmcblk0 or sd0 or sd refer to the io-board SD card.
        - /dev/mmcblk1 or sd1 or emmc refer to the SoM eMMC.
    See MODE OPTIONS for more details.

    It runs on either a Linux host or the Linux target.  A subset of features
    are supported on other Unixes (e.g. macos).

    The files installed and used in the boot process are:
        - Xilinx bootloaders and firmware (e.g. FSBL)
        - U-Boot and its environment
        - Linux kernel and device tree files
        - Rootfs

    All of the above, except the rootfs, are stored on the first partition of
    the SD or eMMC.  The rootfs can be stored in one of two places:
        - As an initramfs (historically named initrd (init ramdisk)) it can be
          stored as a single file named "initrd" along with the other boot
          files.  In this case, the rootfs is loaded into memory, and runtime
          changes to the rootfs are not stored on disk.  On boot, initrd
          supercedes ext4, i.e.  if initrd exists, it will be used.
        - As an ext3 formatted partition on the SD or eMMC.  In this case,
          runtime rootfs changes are saved to disk.  However, the rootfs cannot
          be fully replaced with an updated rootfs at runtime.  To update the
          rootfs, a reboot or alternate method is required.  When used, the
          ext3 image is required in partition 3.

MODE OPTIONS

    -z
        SD MODE

        The default install option.  Install images to the given DEVICE.
        Optionally partition and format device.  SD mode is fast when
        formatting is not required, but does require manually
        ejecting/inserting SD cards.

        The SD card will contain 4 partitions:
            1) FAT: boot files
            2) raw: small partition for optional data
            3) EXT: Linux root file system
            4) EXT: Linux user data partition

        SD mode must run as root, e.g., "sudo ivinstall ..." to perform format
        or ext4 rootfs install.

        For copying without ext4 (i.e. -k or -d options), sudo is not required.
        In this case, the DEVICE can refer to a block device (which must be
        mounted) or a directory to install the boot files.  In addition,
        macos is supported.

        Can be run on a Linux host or target, in both cases DEVICE refers to
        the block device or directory attached to the machine running this
        utility.

    -s SSH_TARGET
        SSH MODE

        Install via ssh to SD card on target.  SSH_TARGET is of the same form
        used by ssh, i.e. [user@]ip_address.  ssh is the fastest method, but
        requires a running target.  Also, ssh cannot update a mounted ext4
        rootfs while the target is running.

        Can only be run on a host.  The DEVICE refers to the device name on the
        target (see DESCRIPTION).

    -j
        JTAG MODE

        Install via JTAG to SD card on target.  Requires installed Vivado
        command line tool xsdb.  This is by far the slowest method, however it
        requires no user intervention, and can completely reformat a target
        even if it is bricked or the network is not available.  JTAG mode does
        not use DEVICE.

        Can only be run on a host.  The DEVICE refers to the device name on the
        target (see DESCRIPTION).

OPTIONS

    -b SIZE
        Size in MB of FAT boot partition, default 4GB (or 512MB for SD cards
        less than 16GB).

    -B SIZE
        Size in MB of rootfs partition, default 1024MB.

    -c
        Copy the necessary boot files to the SD card.  Copies rootfs as ext4 to
        partition 3.

    -d
        Copies the initrd instead of ext4 rootfs.  Implies -c (copy).

    -f
        Partition and format the SD card.

    -J JTAG_REMOTE
        Run JTAG commands from a remote host that is connected via JTAG to a
        target.  JTAG_REMOTE is of the same form used by ssh, i.e.
        [user@]ip_address.  JTAG_REMOTE machine can be Linux or Windows.  Only
        valid in JTAG MODE.

    -k
        Skip copying rootfs for faster install.  Implies -c (copy).

    -h
        Show this help message and exit.

    -i IOBOARD
        Target io-board for which the SD card will be configured.  This will
        setup boot to use the correct device tree overlay file, which will
        correctly configure all io-board interfaces.  Implies -c.  See
        SUPPORTED BOARDS for allowed IOBOARDs.

    -n NAME
        Optional volume name (label) for boot partition; default is main-board
        name up to the first hyphen.  Implies -f.

    -q
        Reflash the QSPI.  Valid only for SSH and JTAG modes, as it must be run
        on the target.  Runs the command "ivflash <boot.bin>" on the target to
        do the reflash, where the boot.bin is the binary included with this
        installer.  Implies -c, as the boot.bin is expected on the target.
        Alternatively, "ivflash" can be run manually on the target.

    -v
        Report version from which images were built.

    -x
        Extract only.  Extracts contents of this installer to temp directory
        and exits.

SUPPORTED BOARDS

    Mainboard supported:
        $MACHINE

    IO boards supported:
$(echo $IOBOARDS | xargs -n1 | sed 's/^/        /')

EXAMPLES

    sudo ivinstall -f -c -n MY_CARD -b 256 /dev/sdb
        Partition & format the device "/dev/sdb".  Name the volume "MY_CARD"
        and copy over the necessary boot files.  The FAT boot partition will be
        256 MB.

    ivinstall -jJ user@192.168.1.123 -c -q emmc
        Install from host 192.168.1.123 which is connected to the target via
        JTAG.  Do not format eMMC, but copy all bootfiles to it, and reflash
        QSPI as well.
EOF

#
# Process args
#
# Note: use bash internal getopts as this must work on vanilla Linux or macos.
# However, long opts not supported, SAD!
#
SAVEARGS="$*"
unset USER_FAT_SIZE USER_ROOTFS_SIZE DO_COPY DO_COPY DO_FORMAT JTAG_REMOTE
unset DO_COPY IOBOARD USER_LABEL DO_QSPI DO_VERSION DO_EXTRACT
MODE=sd
while getopts "B:b:cdfhi:jJ:k:n:qs:vxzZ" opt; do
    case ${opt} in
        b) USER_FAT_SIZE=$OPTARG; ;;
        B) USER_ROOTFS_SIZE=$OPTARG; ;;
        c) DO_COPY=1; ;;
        d) DO_COPY=1; USE_INITRD=1 ;;
        f) DO_FORMAT=1; ;;
        h) echo "$USAGE"; exit 0; ;;
        i) DO_COPY=1; IOBOARD=$OPTARG; ;;
        j) MODE=jtag ;;
        J) JTAG_REMOTE=$OPTARG ;;
        k) DO_COPY=1; SKIP_ROOTFS=1 ;;
        n) DO_FORMAT=1; USER_LABEL=$OPTARG; ;;
        q) DO_COPY=1; DO_QSPI=1 ;;
        s) MODE=ssh; SSH_TARGET=$OPTARG ;;
        v) DO_VERSION=1 ;;
        x) DO_EXTRACT=1 ;;
        z) MODE=sd ;;
        Z) FORCE_SD_MODE=1 ;;   # Undocumented option
        \?) error "Invalid option: -$OPTARG" 1>&2; ;;
        :) error "Invalid option: -$OPTARG requires an argument" 1>&2; ;;
    esac
done
shift $((OPTIND -1))

DEVICE="$1"
shift

# Environ
PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Find tarball start line at end of script
verify awk
ARCHIVE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$CMD")

# We're running on the target if we can find a specfic /sys file.
IS_TARGET=$([[ ! -d /sys/firmware/zynqmp ]]; echo $?)

#
# Handle simple args first
#
((DO_QSPI)) && error "QSPI reflash currently unimplemented"
if ((FORCE_SD_MODE)); then
    # FORCE_SD_MODE can be easily prepended to the arg list to use SD mode,
    # overiding user mode choice.
    MODE=sd
fi
if ((DO_VERSION)); then
    [[ -n "$VERSION" ]] || error "INTERNAL ERROR: version not available"
    echo $VERSION
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
# Validate arguments
#
[[ -n "$DEVICE" ]] || error "DEVICE was not specified"
[[ -z "$@" ]] || error "Extra invalid options were supplied"
((!DO_FORMAT && !DO_COPY)) && error "Either -f or -c (or both) required"
(($OPTIND <= 1)) && { echo "$USAGE"; exit 0; }

[[ -n "$MACHINE" ]] || error "INTERNAL ERROR: mainboard not available"
if [[ -n "$IOBOARD" ]]; then
    grep -q "\<$IOBOARD\>" <<<"$IOBOARDS" || error 'invalid ioboard "'$IOBOARD'"'
fi

#
# For loading files via jtag, add a simple binary header that includes:
#   - 64-bit magic number
#   - 64-bit filesize
#   - 64-bit CRC32 (big endian)
# iVeia U-Boot environ vars understand the above format, and can verify and
# extract the file.
#
# Note: U-Boot crc32 function generates crc in big-endian format (even on
# little-endian machines).  In addition, U-Boot's setexpr can't handle reading
# in 32-bit numbers (it's broken), so we have to swap the longs in the 64bit
# number.  So we fix that here for easy matching during U-Boot.
#
# Usage:
#   add_header <input_file> <output_file>
#
add_header()
{
    MAGIC=0x5d8b1a08c74bf3d7
    CRC32=0x$(crc32 <(cat "$1")) || error "add_header($1, $2) failure 1"
    FILESIZE=$(printf "0x%016X" $(stat -f %z "$1")) || error "add_header($1, $2) failure 2"
    python3 -c "import sys,struct;\
        sys.stdout.buffer.write(struct.pack('<QQ',$MAGIC,$FILESIZE))" > "$2" || \
            error "add_header($1, $2) failure 3"
    python3 -c "import sys,struct;\
        sys.stdout.buffer.write(struct.pack('>LL',$CRC32,0))" >> "$2" || \
            error "add_header($1, $2) failure 4"
    cat "$1" >> "$2" || error "add_header($1, $2) failure 5"
}

#
# JTAG is used to fully load Linux and the run the ivinstall script.
# The process in brief:
#   - Using xsdb (via uboot.tcl) load and run images to RAM:
#       - A startup.sh script that runs ivinstall with user's arguments ($*)
#       - ivinstall script (with a header added) in memory above Linux (via mem=xxx)
#       - A special uEnv.txt (with a header added)
#       - Linux images (Image, DTB, initrd)
#       - bootloader elf files (fsbl, ..., u-boot)
#   - xsdb will get boot running up to U-Boot:
#       - runs the default boot command
#       - runs loadenv_jtag, which finds/validates special uEnv.txt in RAM via JTAG
#       - runs uEnv.txt's bootenv_cmd
#       - insert a special startup script into device-tree/chosen
#       - boot Linux from images already loaded in RAM via JTAG
#   - Linux boots:
#       - runs ivstartup init.d script which finds chosen/startup
#       - chosen/startup loads/validates startup
#       - chosen/startup loads/validates ivinstall
#       - run startup, which ivinstall with user's args
#
#
if [[ $MODE = jtag ]]; then
    [[ -z "$JTAG_REMOTE" ]] && error "JTAG mode currently requires using remote (-J)"
    TMPDIR=$(mktemp -d)
    on_exit() { rm -rf $TMPDIR; }
    trap on_exit EXIT

    info "Extracting Image Archive for JTAG mode..."
    cp "$CMD" $TMPDIR
    (
        cd $TMPDIR
        BASECMD=$(basename "$CMD")
        tail -n+$ARCHIVE "$BASECMD" | tar -xz || error "untar archive failed"
        echo "bash /tmp/ivinstall -Z $SAVEARGS" > startup.sh
        add_header startup.sh startup.bin
        add_header jtag/uEnv.txt uEnv.bin
        add_header $CMD ivinstall.bin
        cp devicetree/$MACHINE.dtb system.dtb
        scp startup.bin uEnv.bin ivinstall.bin jtag/uboot.tcl \
            elf/* boot/Image rootfs/initrd system.dtb \
            $JTAG_REMOTE: || error "scp to JTAG_REMOTE failed"
        ssh $JTAG_REMOTE xsdb uboot.tcl
    )

elif [[ $MODE = ssh ]]; then
    verify ssh scp
    scp $CMD ${SSH_TARGET}:/tmp || error "scp failed"
    # Don't error() on ssh failure - let the error come from the remote
    ssh ${SSH_TARGET} bash /tmp/$(basename ${CMD}) -Z $SAVEARGS

elif [[ $MODE = sd ]]; then
    if ((IS_TARGET)); then
        if [[ "$DEVICE" = sd0 || "$DEVICE" = sd ]]; then
            DEVICE=/dev/mmcblk0
        fi
        if [[ "$DEVICE" = sd1 || "$DEVICE" = emmc ]]; then
            DEVICE=/dev/mmcblk1
        fi
    fi
    [[ -b "$DEVICE" || -d "$DEVICE" ]] || error "DEVICE must be a block device or directory"
    if ((DO_FORMAT || (DO_COPY && (!SKIP_ROOTFS && !USE_INITRD)))); then
        (("$(id -u)" == 0)) || error "must be run as root (e.g. from sudo)"
        [[ $(uname) = Darwin ]] && error "macos only supports copy without ext4 rootfs"
    fi

    verify lsblk awk
    if ((DO_FORMAT)); then
        verify mount umount parted wipefs mkfs.vfat mkfs.ext3

        BLOCKDEV_BYTES=$(blockdev --getsize64 "$DEVICE")
        BLOCKDEV_MB=$((BLOCKDEV_BYTES/1024/1024))
        [[ -n "$BLOCKDEV_MB" ]] || error "could not determine size of block device"
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
            LABEL=$USER_LABEL
        else
            LABEL=$(cut -d- -f1 <<<$MACHINE | tr a-z A-Z)
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
        wipefs -a "$DEVICE" >/dev/null || error "wipefs failed.  Is device in use?"
        parted -s "$DEVICE" mklabel msdos || error "mklabel failed"
        MKPART="parted -s -a optimal "$DEVICE" mkpart primary"
        ${MKPART} fat32 0%              $((P1_END + 1)) || error "failed to create partition 1"
        ${MKPART} fat32 $((P1_END + 1)) $((P2_END + 1)) || error "failed to create partition 2"
        ${MKPART} ext2  $((P2_END + 1)) $((P3_END + 1)) || error "failed to create partition 3"
        ${MKPART} ext2  $((P3_END + 1)) 100%            || error "failed to create partition 4"
        parted -s "$DEVICE" set 1 boot on || error "could not make partition 1 bootable"
    fi

    # Get the partition list as an array (we want this even if we're not
    # formatting).  Give up to a few seconds for partitions to appear.
    SECS=5
    for ((i = 0; i < SECS; i++)); do
        (($(lsblk -n "$DEVICE" | wc -l) == 5)) && break;
        sleep 1
    done
    ((i == SECS)) && error "failed to create all partitions"

    PARTS=($(lsblk -nrx NAME "$DEVICE" | awk '{print $1}'))
    ((${#PARTS[*]} == 5)) || error "INTERNAL ERROR: failed to create all partitions"

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
        mkfs.vfat -F 32 -n "$LABEL" /dev/${PARTS[1]} || error "failed to format partition 1"
        info "Formatting partition 2 as raw"
        info "Formatting partition 3 as ext3"
        mkfs.ext3 -q -F -L "ROOTFS" /dev/${PARTS[3]}   || error "failed to format partition 3"
        info "Formatting partition 4 as ext3"
        mkfs.ext3 -q -F -L "SDHOME" /dev/${PARTS[4]} || error "failed to format partition 4"
    fi

    if ((DO_COPY)); then
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

        TMPDIR=$(mktemp -d)
        info "Extracting Image Archive..."
        tail -n+$ARCHIVE "$CMD" | tar -xz -C $TMPDIR || error "untar archive failed"
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
            info "Copying ext4 rootfs to DEVICE ($DEVICE) partition 3"
            ((${#PARTS[*]} > 3)) || error "partition 3 of $DEVICE does not exist"
            # NOTE: Can't use "bs=1m" - not cross-platform!
            dd if=$TMPDIR/rootfs/rootfs.ext4 of=/dev/${PARTS[3]} bs=$((1024*1024)) status=none || \
                error "dd ext4 rootfs failed"
        fi
    fi

    # Sync just in case user does unsafe eject
    sync

    info "Done"
fi

exit 0

__ARCHIVE_BELOW__
