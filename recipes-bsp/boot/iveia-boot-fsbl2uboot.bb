SUMMARY = "Generates boot.bin using bootgen tool"

LICENSE = "CLOSED"

require recipes-bsp/bootbin/xilinx-bootbin_1.0.bb
PROVIDES_remove = "virtual/boot-bin"

BIF_PARTITION_ATTR_remove = "bitstream"

