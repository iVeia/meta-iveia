#!/bin/sh
[ -n "$MACHINE" ] || { echo "ERROR: MACHINE not set"; exit 1; }
TGTS="\
    fsbl \
    fsbl-firmware \
    u-boot-xlnx \
    linux-xlnx \
    xilinx-bootbin \
    ivinstall-full \
    "
if [ "$(bitbake-getvar --value SOC_FAMILY)" = "zynqmp" ]; then
    TGTS="$TGTS pmu-firmware"
fi
bitbake -c cleansstate $TGTS
