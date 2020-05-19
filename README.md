# meta-iveia

The meta-iveia Yocto BSP layer provides support for iVeia Atlas SoMs and carriers.

# Supported Boards/Machines

iVeia Mainboard SoMs (`MACHINE`) supported by this layer:
- atlas-ii-z8-hp (PN 00122)

iVeia IO boards (`IVIO`) supported by this layer:
- a3i0-aurora (PN 00100)

# Building

The meta-iveia layer depends on the meta-xilinx layer:

> https://github.com/Xilinx/meta-xilinx

and follows a similar approach for downloading and building sources.  To build
the full suite of software requires:
1. Using the `repo` tool to download a specific revision of Xilinx sources
2. `git clone` the meta-iveia layer (this repo) and add it as a bitbake layer.
3. Use bitbake to build iVeia specific targets

See [Xilinx's instructions](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841862/Install+and+Build+with+Xilinx+Yocto) for more information about the meta-xilinx layer and build process.

Tagged iVeia versions are based off of a specific Xilinx version included in
the tag followed by an iVeia version number, e.g. `2019.2-1.0`.

iVeia provides two primary targets `iveia-boot` and `iveia-image-*`, described
in the next section.

iVeia follows the Xilinx tools in regards to supported OS versions, currently
including Ubuntu 16.04 and 18.04.

The first time download and build steps are as follows:
```
repo init -u git://github.com/Xilinx/yocto-manifests.git -b rel-v2019.2          # Xilinx step
repo sync                                                                        # Xilinx step
git clone -b 2019.2-1.0 git://github.com/iVeia/meta-iveia.git sources/meta-iveia # clone meta-iveia
source setupsdk                                                                  # Xilinx step
bitbake-layers add-layer ../sources/meta-iveia                                   # add meta-iveia
MACHINE=atlas-ii-z8-hp bitbake iveia-boot iveia-image-minimal                    # build iveia targets
```

Note: the above versions for Xilinx and iVeia (defined in the `repo init` and
`git clone` lines, respectively), and the `MACHINE` may de different for your
board.

After the first run, all future runs require `source setupsdk` to setup
environment in a new shell, and then building with the last line above.

# Build targets and products

Final build products are located under Yocto's build directory at:
`build/tmp/deploy/images/${MACHINE}`. That directory includes both the final
build products and some intermediates, in different formats. The important
final build products for each targets are:

- For `iveia-boot` target:
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
- For `iveia-image-minimal` (or full):
    - **`Image`**: Linux kernel image
    - Root FS, in different formats:
        - **`iveia-image-minimal-${MACHINE}.cpio.gz.u-boot`**: U-Boot initrd
          format - uEnv.txt supports booting from this Root FS when renamed
          `initrd` on device (or remotely downloaded, e.g. tftp)
        - **`iveia-image-minimal-${MACHINE}.ext4`**: Could be useful to
          manually flash to SD partition
        - **`iveia-image-minimal-${MACHINE}.tar.xz`**: Format used by
          iVeia's `sdformat` utility to populate SD card.

It is not necessary to build both targets - they are independent.

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

Xilinx bootgen supports a boot method where the FSBL can start in the primary
boot device (QSPI) but them immediately jump to a boot.bin on a secondary boot
device. The `boot2sd.bin` uses this method - it contains an FSBL only, and then
jumps to boot.bin on SD0 as the secondary boot device.

When the above method is used, only the first FSBL (in `boot2bin.sd`) is run.
The FSBL in `boot.bin` is skipped.

Therefore, there are two choices for boot:
- Program `boot2sd.bin` to QSPI and put boot.bin on SD0.
- Program `boot.bin` directly to QSPI.

The above model also allows for other secondary devices, such as onboard eMMC.

## Programming Flash

QSPI flash can be programmed using the Xilinx `program_flash` utility with:

```
program_flash -f $FSBL_BIN -offset 0 -flash_type qspi_single -fsbl $FSBL_ELF -cable type xilinx_tcf url TCP:127.0.0.1:3121
```

where `FSBL_BIN` is the file to be flashed (can be either `boot2sd.bin` or
`boot.bin)` and `FSBL_ELF` is the FSBL program in elf format
(`fsbl-${MACHINE}.elf`).

# Maintainers

Please submit any patches against the meta-iveia layer to the maintainer:

> Brian Silverman <brian.silverman@iveia.com>





