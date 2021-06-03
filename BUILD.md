# Building

The meta-iveia layer depends on the meta-xilinx layer:

> https://github.com/Xilinx/meta-xilinx

and follows a similar approach for downloading and building sources.  To build
the full suite of software requires:
1. Using the `repo` tool to download a specific revision of Xilinx sources
2. `git clone` the meta-iveia layer (this repo) and add it as a bitbake layer.
3. Use bitbake to build iVeia specific targets

See [Xilinx's instructions](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841862/Install+and+Build+with+Xilinx+Yocto) for more information about the meta-xilinx layer and build process.

iVeia provides the primary target `iveia-image-*`, described in the next
section.  In addition is provides an `ivinstall` target that creates a single
installable image.

Tagged iVeia versions are based off of a specific Xilinx version included in
the tag followed by an iVeia version number, e.g. `2019.2-1.0`.

The first time download and build steps are as shown below.  For convenience, a
download script is avaiable to simplify the process - see
[README.md](README.md).
```
IVEIA_TAG=<CHOOSE THE LATEST TAGGED VERSION, I.E. 2019.2-M.N>
repo init -u git://github.com/Xilinx/yocto-manifests.git -b rel-v2019.2          # Xilinx step
repo sync                                                                        # Xilinx step
git clone -b $IVEIA_TAG git://github.com/iVeia/meta-iveia.git sources/meta-iveia # clone meta-iveia
source setupsdk                                                                  # Xilinx step
bitbake-layers add-layer ../sources/meta-iveia                                   # add meta-iveia
MACHINE=<CHOOSE FROM SUPPORTED MACHINES ABOVE> bitbake ivinstalll                # build iveia target
```

After the first run, all future runs require `source setupsdk` to setup
environment in a new shell, and then building with the last line above.

# Build targets and products

Final build products are located under Yocto's build directory at:
`build/tmp/deploy/images/${MACHINE}`. That directory includes both the final
build products and some intermediates, in different formats. The important
final build products are:
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
- **`Image`**: Linux kernel image (**`uImage`** on Zynq-7000 platforms)
- Root FS, in different formats:
    - **`iveia-image-minimal-${MACHINE}.cpio.gz.u-boot`**: U-Boot initrd
      format - uEnv.txt supports booting from this Root FS when renamed
      `initrd` on device (or remotely downloaded, e.g. tftp)
    - **`iveia-image-minimal-${MACHINE}.ext4`**: Used by ivinstall to
      install Root FS to SD partition
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

