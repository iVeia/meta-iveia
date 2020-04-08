#
# Symlink /media/sd to /media/sd[01], whichever one is the booting device.
#
# If not booting from SD, or otherwise can't determine the boot device, default
# to sd0.
#
SDBOOTDEV=0
BOOTDEV=

DT_BOOTDEV=/proc/device-tree/chosen/iv_boot
if [ -e ${DT_BOOTDEV} ]; then
    BOOTDEV=$(tr -d '\0' <${DT_BOOTDEV})
fi

DT_MMC=/proc/device-tree/chosen/iv_mmc
if [ -e ${DT_MMC} -a "$BOOTDEV" = sdboot ]; then
    SDBOOTDEV=$(tr -d '\0' <${DT_MMC})
fi
ln -sf /media/sd${SDBOOTDEV} /media/sd
