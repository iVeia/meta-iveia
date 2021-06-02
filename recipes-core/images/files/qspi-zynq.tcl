#
# Boot to U-Boot/Linux for ivinstall
#
# Some parts from Xilinx's AR# 68657
#
connect

# Set jtag freq to a super high number (it will be adjusted to max)
jtag targets 1
jtag frequency 10000000000

# Reset A53, load and run FSBL
targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}
rst -processor
dow elf/fsbl.elf
bpadd LoadBootImage
con

# Zynq-7000 doesn't support restart in JTAG mode (Zynqmp does).  So we hardcode
# a magic number in memory to force U-Boot to act as if it is booting in JTAG
# mode.
mwr 0x500000 {0x1dbf7679 0xc9ffdfa0 0x5da792eb 0xeb3051b9}

# Load Images
#
# These image locations must match those used by U-Boot and uEnv.txt.
# Note: *.bin files are images modified with a 3*64-bit header, and must be
# placed shifted earlier in mem by the header amount.  See the ivinstall script
# for more info.
dow -data uEnv.bin 0x5fffe8
dow -data boot.bin.bin 0x6fffe8

# Other SW...
dow elf/u-boot.elf
con

