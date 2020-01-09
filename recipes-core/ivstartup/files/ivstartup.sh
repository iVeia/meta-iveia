# Should be moved to fstab
mkdir -p /media/sd
mount /dev/mmcblk0p1 /media/sd

echo "Running iVeia startup.sh"
/media/sd/startup.sh &

