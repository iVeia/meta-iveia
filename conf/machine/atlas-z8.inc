#### Preamble
MACHINEOVERRIDES =. "${@['', '00122:']['00122' !='${MACHINE}']}"
#### Regular settings follow

# Required generic machine inclusion
require conf/machine/zynqmp-generic.conf
require conf/machine/atlas.inc

# Yocto MACHINE_FEATURES Variable
MACHINE_FEATURES += "mali400"

# We manage uEnv (u-boot-uenv) and scripts ourselves
EXTRA_IMAGEDEPENDS:remove = "u-boot-xlnx-uenv u-boot-xlnx-scr"

#### No additional settings should be after the Postamble
#### Postamble
PACKAGE_EXTRA_ARCHS:append = "${@['', ' 00122']['00122' != '${MACHINE}']}"
