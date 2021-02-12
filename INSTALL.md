## Target installation and boot

## Install

ivinstall is a bitbake build target that creates a single installable image
that includes all of the necessary images for the target.

The installer can install binaries to an SD card, over the network, or over
JTAG.  In addition, it can install to the eMMC on the SoM (if available), and
the bootloaders in the SoM's QSPI flash.

For more information, view the [help doc](recipes-core/images/files/ivinstall-doc).

# Boot

For almost all iVeia Atlas SoMs, boot starts from QSPI (the exceptions being
JTAG boot, or boards that support external bootmode pins).

The meta-iveia layer adds an iVeia Boot Sequencer to the boot process:
- On cold boot, the BootROM finds the boot image in QSPI, and loads the FSBL.
- The FSBL runs the Boot Sequencer which will look for a valid Xilinx boot
  image from a compiled-in list of devices.  The sequencer will continue
  through all devices until an image is found, if no image is found, boot will
  continue on from the current device (QSPI).
- The default list of devices is as follows (but can be changed via the
  `boot_seq` variable):
    - SD0 (external ejectable SD slot on the IO board)
    - SD1 (internal SoM eMMC)
- Once a device is found with a valid boot image, the sequencer will specify
  the device in the Zynq MP `CRL_APB_BOOT_MODE_USER` register.  This will
  enable a soft reboot to jump to that specific device.  Then, the sequencer
  will issue a soft reset.
- On reboot, the FSBL will be loaded from the specified device.  The FSBL will
  skip the Boot Sequencer, and instead continue the normal boot process.

Note: Once booted, a soft reboot will NOT run the Boot Sequencer, and will
instead continue to reboot from the same device.

## SD Card (or eMMC)

The boot process from SD/eMMC requires the following files on the first
partition of the SD/eMMC (FAT32 formatted).  The files are listed in the order
that they are loaded:
- **`boot.bin`** (REQUIRED): Boots bootloaders up to U-Boot.
- **`uEnv.txt`** (REQUIRED): Boot script loaded by U-Boot.
- **`xilinx.bit`** (OPTIONAL): FPGA bitfile.
- **`Image`** (REQUIRED): Linux kernel image
- **`${MACHINE}.dtb`** (REQUIRED): Device tree for the SoM.
- **`overlay.dtbo`** (OPTIONAL): Device tree for IO board specific features.
  This file must be renamed from **`${IVIO}_overlay.dtbo`** in the Yocto build
  directory (see Build targets and products section above).
- ROOTFS (REQUIRED): either of the following images found in the Yocto build
  directory (see Build targets and products section above):
    - **`initrd`**: Linux inital RAM disk, renamed from
      **`iveia-image-minimal-${MACHINE}.cpio.gz.u-boot`**.
    - Partition 3 of the SD card with the
      **`iveia-image-minimal-${MACHINE}.ext4`** image directly copied to it
      (`dd` recommended).
- **`startup.sh`** (OPTIONAL): User initialization shell script.

The above files can be installed manually, but using the ivinstall script is
recommended.

## Programming Flash

The ivinstall script can be used to program the QSPI flash on the SoM.
However, the Xilinx `program_flash` utility can be used to directly program the
flash via JTAG, and does not require a bootable target.

To use Xilinx's `program_flash` utility run:

```
program_flash -f $FSBL_BIN -offset 0 -flash_type qspi_single -fsbl $FSBL_ELF -cable type xilinx_tcf url TCP:127.0.0.1:3121
```

where `FSBL_BIN` is the file to be flashed (i.e.`boot.bin)` and `FSBL_ELF` is
the FSBL program in elf format (`fsbl-${MACHINE}.elf`).

