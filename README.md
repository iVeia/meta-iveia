# meta-iveia

The meta-iveia Yocto BSP layer provides support for iVeia Atlas SoMs and
IO boards.  meta-iveia uses the Yocto layered approach on top of the meta-xilinx
layer, which itself is built on top of the Poky reference design.

The Yocto build process is self contained, therefore Xilinx build tools do not
need to be installed.

# Supported Boards/Machines

iVeia Mainboard SoMs (`MACHINE`) supported by this layer:
- atlas-iii-z8 (PN 00104)
- atlas-ii-z8-hp (PN 00122)
- atlas-ii-z8ev (PN 00127)

iVeia IO boards (`IVIO`) supported by this layer:
- io-captiva (PN 00074)
- a2io-blackwing (PN 00093)
- a3io-aurora (PN 00100)

# Building

The meta-iveia layer depends on the meta-xilinx layer:

> https://github.com/Xilinx/meta-xilinx

and follows a similar approach for downloading and building sources.  To build
the full suite of software requires:
1. Using the `repo` tool to download a specific revision of Xilinx sources
2. `git clone` the meta-iveia layer (this repo) and add it as a bitbake layer.
3. Use bitbake to build iVeia specific targets

See [Xilinx's instructions](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841862/Install+and+Build+with+Xilinx+Yocto) for more information about the meta-xilinx layer and build process.

iVeia provides the primary target `iveia-image-*`, described in the next section.

iVeia follows the Xilinx tools in regards to supported OS versions, currently
including Ubuntu 16.04 and 18.04.

Tagged iVeia versions are based off of a specific Xilinx version included in
the tag followed by an iVeia version number, e.g. `2019.2-1.0`.

The first time download and build steps are as follows:
```
IVEIA_TAG=<CHOOSE THE LATEST TAGGED VERSION, I.E. 2019.2-M.N>
repo init -u git://github.com/Xilinx/yocto-manifests.git -b rel-v2019.2          # Xilinx step
repo sync                                                                        # Xilinx step
git clone -b $IVEIA_TAG git://github.com/iVeia/meta-iveia.git sources/meta-iveia # clone meta-iveia
source setupsdk                                                                  # Xilinx step
bitbake-layers add-layer ../sources/meta-iveia                                   # add meta-iveia
MACHINE=<CHOOSE FROM SUPPORTED MACHINES ABOVE> bitbake iveia-image-minimal       # build iveia target
```

After the first run, all future runs require `source setupsdk` to setup
environment in a new shell, and then building with the last line above.

# Build targets and products

Final build products are located under Yocto's build directory at:
`build/tmp/deploy/images/${MACHINE}`. That directory includes both the final
build products and some intermediates, in different formats. The important
final build products are:
- **`boot2sd.bin`**: QSPI burnable FSBL-only, that jumps to secondary
  boot device on SD0.  See "Booting" section.
- **`fsbl-${MACHINE}.elf`**: Needed for QSPI programming using Xilinx's
  `program_flash` tool.
- **`boot.bin`**: Contains FSBL, ATF, PMUFW and U-Boot. Can sit on SD, or
  be burned to QSPI.  See "Booting" section.
- **`uEnv.txt`**: REQUIRED boot script for U-Boot boot. Supports SD/eMMC
  or tftp boot.
- **`devicetree/`**: directory containing devicetree files
    - **`${MACHINE}.dtb`**: Mainboard (i.e. MACHINE) DTB
    - **`${IVIO}_overlay.dtbo`**: IO board DT Overlays for all supported
      IO boards
    - **`${MACHINE}_{IVIO}_combo.dtb`**: Combined DT file that includes
      the DT for both the Mainboard and IO board - only built when the
      variable `IVIO` is set.
- **`Image`**: Linux kernel image
- Root FS, in different formats:
    - **`iveia-image-minimal-${MACHINE}.cpio.gz.u-boot`**: U-Boot initrd
      format - uEnv.txt supports booting from this Root FS when renamed
      `initrd` on device (or remotely downloaded, e.g. tftp)
    - **`iveia-image-minimal-${MACHINE}.ext4`**: Could be useful to
      manually flash to SD partition
    - **`iveia-image-minimal-${MACHINE}.tar.xz`**: Format used by
      iVeia's `sdformat` utility to populate SD card.
- **`startup.sh`**: User initialization shell script.  Empty by default.
  Allows user to easily add startup apps that run at the end of the Linux
  boot process.

The `MACHINE` used above maps directly to an iVeia SoM. For example, the
Atlas-II-Z8 HP (board 00122), with the canonical name "atlas-ii-z8-hp", is also
used as the `MACHINE`.

The `IVIO` variable is optional, and can be specified as a command line
environment variable or a configuration setting.  It defines an iVeia IO board
to build.  When set, two things change:
- The **`devicetree/${MACHINE}_{IVIO}_combo.dtb`** will be built (in addition
  to the other DTB files).
- U-Boot will use the above DTB, instead of the default **`${MACHINE}.dtb`**.
  This allows U-Boot, which does not support using internal DT overlay files,
  to include both mainboard *and* IO board specific features.

# Booting

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

When using the `boot2sd.bin` as defined above, the rest of the boot (excluding
the FSBL), run from the SD card (this process can also be used with eMMC, but
it is not built by default).

The boot process from SD requires the following files on the first partition of
the SD card (FAT32 formatted).  The files are listed in the order that they are
loaded:
- **`boot.bin`** (REQUIRED): Boot's bootloaders up to U-Boot.
- **`uEnv.txt`** (REQUIRED): boot script loaded by U-Boot.
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

## Programming Flash

QSPI flash can be programmed using the Xilinx `program_flash` utility with:

```
program_flash -f $FSBL_BIN -offset 0 -flash_type qspi_single -fsbl $FSBL_ELF -cable type xilinx_tcf url TCP:127.0.0.1:3121
```

where `FSBL_BIN` is the file to be flashed (can be either `boot2sd.bin` or
`boot.bin)` and `FSBL_ELF` is the FSBL program in elf format
(`fsbl-${MACHINE}.elf`).

# Package support

The default package management system is opkg.  Packges will be created in
tmp/deploy/ipk.

To build and add a package:
- `MACHINE=<machine-name> bitbake <package-name>`
- Copy .ipk from tmp/deploy/ipk to target
- Install on target with "opkg install --force-depends <ipk>"

Available package recipes can be found at the OpenEmbedded Layer Index at:

> https://layers.openembedded.org/

# Maintainers

Please submit any patches against the meta-iveia layer to the maintainer:

> Brian Silverman <brian.silverman@iveia.com>





