#
# Boot to U-Boot/Linux for ivinstall
#
connect

# Set jtag freq to a super high number (it will be adjusted to max)
jtag targets 1
jtag frequency 10000000000

# Reset ARM, load and run FSBL
targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}
rst -system
dow fsbl.elf
bpremove -all
bpadd LoadBootImage
con

# Zynq-7000 doesn't support restart in JTAG mode (Zynqmp does).  So we hardcode
# a magic number in memory to force U-Boot to act as if it is booting in JTAG
# mode.  (Even if this happened to match, it's doesn't affect boot unless the
# uEnv.txt file/crc matches).
mwr 0x500000 0x4a544147

# Load Images - see ivinstall script for details on image locations
dow -data uEnv.ivinstall.txt.bin 0x5ffff4
dow -data system.dtb 0x700000
dow -data uImage 0x800000
dow -data initrd 0x8000000
dow -data startup.sh.bin 0x10000000
dow -data ivinstall.bin 0x10100000

# Other SW...
dow u-boot.elf
con
