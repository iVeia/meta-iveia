#!/bin/sh
[ -n "$MACHINE" ] || { echo "ERROR: MACHINE not set"; exit 1; }
TGTS="\
    fsbl \
    fsbl-firmware \
    pmu-firmware \
    u-boot-xlnx \
    linux-xlnx \
    xilinx-bootbin \
    ivinstall-full \
    "
bitbake -c cleansstate $TGTS
