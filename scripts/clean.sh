#!/bin/sh
[ -n "$MACHINE" ] || { echo "ERROR: MACHINE not set"; exit 1; }
bitbake -c cleansstate fsbl fsbl-firmware u-boot-xlnx linux-xlnx xilinx-bootbin ivinstall-full
