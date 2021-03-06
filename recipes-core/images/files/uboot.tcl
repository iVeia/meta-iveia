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
dow pmu.elf
con
after 500

# Reset A53, load and run FSBL
targets -set -filter {name =~ "Cortex-A53 #0"}
rst -processor
dow fsbl.elf
con

# Give FSBL time to run
after 5000
stop

# Load Images
#
# These image locations must match those used by U-Boot, uEnv.txt, and the
# Linux startup script.  Note: *.bin files are images modified with a 3*64-bit
# header, and must be placed shifted earlier in mem by the header amount.  See
# the ivinstall script for more info.
dow -data uEnv.bin 0x5fffe8
dow -data system.dtb 0x700000
dow -data Image 0x800000
dow -data initrd 0x10000000
dow -data startup.bin 0x20000000
dow -data ivinstall.bin 0x20100000

# Other SW...
dow u-boot.elf
dow atf.elf
con
