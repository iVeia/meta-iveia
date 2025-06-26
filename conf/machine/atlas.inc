UBOOT_MACHINE = "xilinx_${SOC_FAMILY}_virt_defconfig"

ERROR_QA:remove = "version-going-backwards"

# This is an 'XSCT' based BSP
XILINX_XSCT_VERSION = "2024.2"
XILINX_WITH_ESW = "xsct"

# Add system XSA
HDF_URI = "file://system.xsa"

# Yocto PMUFW variables
YAML_SERIAL_CONSOLE_STDIN:pn-pmu-firmware = "psu_uart_1"
YAML_SERIAL_CONSOLE_STDOUT:pn-pmu-firmware = "psu_uart_1"
YAML_SERIAL_CONSOLE_STDIN:pn-pmu-firmware:00126 = "psu_uart_0"
YAML_SERIAL_CONSOLE_STDOUT:pn-pmu-firmware:00126 = "psu_uart_0"

# Yocto FSBL variables
YAML_SERIAL_CONSOLE_STDIN:pn-fsbl-firmware = "psu_uart_1"
YAML_SERIAL_CONSOLE_STDOUT:pn-fsbl-firmware = "psu_uart_1"
YAML_SERIAL_CONSOLE_STDIN:pn-fsbl-firmware:00126 = "psu_uart_0"
YAML_SERIAL_CONSOLE_STDOUT:pn-fsbl-firmware:00126 = "psu_uart_0"

# Yocto arm-trusted-firmware(TF-A) variables
ATF_CONSOLE = "cadence1"
ATF_CONSOLE:00126 = "cadence0"

# Serial Console Settings
#SERIAL_CONSOLES ?= "115200;ttyPS0"
#YAML_SERIAL_CONSOLE_BAUDRATE ?= "115200"
