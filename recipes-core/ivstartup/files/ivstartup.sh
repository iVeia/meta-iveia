if [ -f /proc/device-tree/chosen/startup ]; then
    # Run script in device-tree.  Can't run directly as it ends with NULL.
    TMP=$(mktemp)
    tr -d \\0 < /proc/device-tree/chosen/startup > $TMP
    echo "Running iVeia device-tree/chosen/startup"
    bash $TMP &
elif [ -e /media/sd/startup.sh ];  then
    echo "Running iVeia startup.sh"
    /media/sd/startup.sh &
else
    echo "iVeia startup not found.  Skipping."
fi
