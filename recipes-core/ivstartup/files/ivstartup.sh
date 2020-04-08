if [ -e /media/sd/startup.sh ];  then
    echo "Running iVeia startup.sh"
    /media/sd/startup.sh &
else
    echo "iVeia startup.sh not found.  Skipping."
fi
