#
# Boot to U-Boot/Linux for ivinstall
#
# Some parts from Xilinx's AR# 68657
#
connect

# Set jtag freq to a super high number (it will be adjusted to max)
jtag targets 1
jtag frequency 10000000000

# Restart in JTAG mode, required as iVeia devices are fixed to QSPI bootstrap
# See also: ZynqMP BOOT_MODE_USER (CRL_APB) Register
targets -set -filter {name =~ "PSU"}
mwr  0xff5e0200 0x0100
rst -system

# Disable Security gates to view PMU MB target
# By default, JTAG security gates are enabled
# This disables security gates for DAP, PLTAP and PMU.
# See also: ZynqMP jtag_sec (CSU) Register
mwr 0xffca0038 0x1ff
after 500

# Load and run PMU FW
targets -set -filter {name =~ "MicroBlaze PMU"}
dow elf/pmu.elf
con
after 500

# Reset A53, load and run FSBL
targets -set -filter {name =~ "Cortex-A53 #0"}
rst -processor
dow elf/fsbl.elf
con

# Give FSBL time to run
after 5000
stop

# Load Images - see ivinstall script for details on image locations
dow -data uEnv.bin 0x5ffff4
dow -data boot.bin.bin 0x6ffff4

# Other SW...
dow elf/u-boot.elf
dow elf/atf.elf
con

