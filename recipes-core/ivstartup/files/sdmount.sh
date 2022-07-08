#
# Symlink /media/sd to /media/sd[01], whichever one is the booting device.
#
# If not booting from SD, or otherwise can't determine the boot device, default
# to sd0.
#
SDBOOTDEV=0
BOOTDEV=

for mmc_dev in $(ls /dev/mmcblk[0-1]); do
  mkdir /media/sd${mmc_dev: -1}
  mount ${mmc_dev}p1 /media/sd${mmc_dev: -1}
done

DT_BOOTDEV=/proc/device-tree/chosen/iv_boot
if [ -e ${DT_BOOTDEV} ]; then
    BOOTDEV=$(tr -d '\0' <${DT_BOOTDEV})
fi

DT_MMC=/proc/device-tree/chosen/iv_mmc
if [ -e ${DT_MMC} ]; then
    SDBOOTDEV=$(tr -d '\0' <${DT_MMC})
	ln -sf /media/sd${SDBOOTDEV} /media/sd
fi

if [ -b "/dev/mmcblk${SDBOOTDEV}p4" ]; then
    mkdir /media/data
    mount /dev/mmcblk${SDBOOTDEV}p4 /media/data
fi

