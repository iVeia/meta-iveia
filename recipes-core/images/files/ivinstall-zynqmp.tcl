#
# Boot to U-Boot/Linux for ivinstall
#
# Some parts from Xilinx's AR# 68657
#
connect

# If given, first arg is the jtag_cable_serial wildcard string.  Use to select
# the specific JTAG adapter.
if {$argc > 0} {
    set jtag_cable_serial [lindex $argv 0]
} else {
    set jtag_cable_serial "*"
}

# Set jtag freq to a super high number (it will be adjusted to max)
jtag targets 1
jtag frequency 10000000000

# Restart in JTAG mode, required as iVeia devices are fixed to QSPI bootstrap
# See also: ZynqMP BOOT_MODE_USER (CRL_APB) Register
targets -set -filter {jtag_cable_serial =~ "$jtag_cable_serial" && name =~ "PSU"}
mwr  0xff5e0200 0x0100
rst -system

# Disable Security gates to view PMU MB target
# By default, JTAG security gates are enabled
# This disables security gates for DAP, PLTAP and PMU.
# See also: ZynqMP jtag_sec (CSU) Register
mwr 0xffca0038 0x1ff
after 500

# Load and run PMU FW
targets -set -filter {jtag_cable_serial =~ "$jtag_cable_serial" && name =~ "MicroBlaze PMU"}
dow pmu.elf
con
after 500

# Reset A53, load and run FSBL
targets -set -filter {jtag_cable_serial =~ "$jtag_cable_serial" && name =~ "Cortex-A53 #0"}
rst -processor
dow fsbl.elf
con

# Give FSBL time to run
after 5000
stop

# Load Images - see ivinstall script for details on image locations
dow -data uEnv.txt.bin 0x005ffff4
dow -data system.dtb 0x00700000
dow -data Image 0x00800000
dow -data initrd 0x04000000
dow -data tarball.tgz.bin 0x17fffff4

# Other SW...
dow u-boot.elf
dow atf.elf
con
