#### Preamble
MACHINEOVERRIDES =. "${@['', '00122:']['00122' !='${MACHINE}']}"
#### Regular settings follow

# Yocto device-tree variables
YAML_CONSOLE_DEVICE_CONFIG:pn-device-tree ?= "psu_uart_0"
YAML_MAIN_MEMORY_CONFIG:pn-device-tree ?= "psu_ddr_0"
DT_PADDING_SIZE:pn-device-tree ?= "0x1000"
DTC_FLAGS:pn-device-tree ?= ""
YAML_DT_BOARD_FLAGS ?= "{BOARD zcu102-rev1.0}"

# Yocto u-boot-xlnx variables
UBOOT_MACHINE ?= "xilinx_zynqmp_virt_defconfig"

# Yocto PMUFW variables
YAML_COMPILER_FLAGS:append:pn-pmu-firmware = " "
YAML_SERIAL_CONSOLE_STDIN:pn-pmu-firmware ?= "psu_uart_0"
YAML_SERIAL_CONSOLE_STDOUT:pn-pmu-firmware ?= "psu_uart_0"

# Yocto FSBL variables
YAML_SERIAL_CONSOLE_STDIN:pn-fsbl-firmware ?= "psu_uart_0"
YAML_SERIAL_CONSOLE_STDOUT:pn-fsbl-firmware ?= "psu_uart_0"

# Serial Console Settings
SERIAL_CONSOLES ?= "115200;ttyPS0"
YAML_SERIAL_CONSOLE_BAUDRATE ?= "115200"

# Yocto arm-trusted-firmware(TF-A) variables
ATF_CONSOLE ?= "cadence"
TFA_BL33_LOAD ?= "0x8000000"

# Set DDR Base address for u-boot-xlnx-scr variables
DDR_BASEADDR ?= "0x0"
SKIP_APPEND_BASEADDR ?= "0"

# Yocto KERNEL Variables
UBOOT_ENTRYPOINT  ?= "0x200000"
UBOOT_LOADADDRESS ?= "0x200000"

# Yocto MACHINE_FEATURES Variable
MACHINE_FEATURES += "mali400"

# Required generic machine inclusion
require conf/machine/zynqmp-generic.conf

# This is an 'XSCT' based BSP
XILINX_XSCT_VERSION = "2024.2"
XILINX_WITH_ESW = "xsct"

# Add system XSA
HDF_URI = "file://system.xsa"
#HDF_URI[sha256sum] = "9c3b9f18b93bfead17758639edcf2ee464de4ec9bc3c5106d11acddcbf76fffe"

#### No additional settings should be after the Postamble
#### Postamble
PACKAGE_EXTRA_ARCHS:append = "${@['', ' 00122']['00122' != '${MACHINE}']}"
