# meta-iveia

The meta-iveia Yocto BSP layer provides support for iVeia Atlas SoMs and
IO boards.  meta-iveia uses the Yocto layered approach on top of the meta-xilinx
layer, which itself is built on top of the Poky reference design.

The Yocto build process is self contained, therefore Xilinx build tools do not
need to be installed.

# Supported Boards/Machines

iVeia Mainboard SoM part numbers (`MACHINE`) supported by this layer:
- 00068 (Atlas-II Z7x)
- 00090 (Atlas-II Z8)
- 00104 (Atlas-III Z8)
- 00114 (Atlas-III Z8e)
- 00122 (Atlas-II Z8 HP)
- 00126 (Helios Z8)
- 00127 (Atlas-II Z8ev)
- 00130 (Atlas-II Z8p HD PS)

iVeia IO boards (`IVIO`) supported by this layer:
- io-captiva (PN 00074)
- a3io-sierra (PN 00087)
- a2io-blackwing (PN 00093)
- a3io-rogue (PN 00096)
- a3io-aurora (PN 00100)

# Quick start

This section details the fastest way to get started.

## Requirements

iVeia follows the Xilinx tools in regards to supported OS versions, currently
including Ubuntu 16.04 and 18.04.

meta-iveia is currently compatible with **Xilinx tools version: "2021.2"**.

## Download & Build

For convenience, this layer includes a script to download all dependent Xilinx
files.  For more information, see the [BUILD](BUILD.md) doc.

To download this repo and the rest of the Xilinx sources (this step only needs
to be run once):
```
git clone https://github.com/iVeia/meta-iveia.git meta-iveia
meta-iveia/download
```
Note: the **download** script will move the meta-iveia layer directory into the
sources directory, which is where Xilinx stores layer directories.

The source hierarchy will be moved into a directory named
**meta-iveia-project**.  To build a single installable image:
```
cd meta-iveia-project
source setupsdk
MACHINE=<CHOOSE FROM SUPPORTED MACHINES ABOVE> bitbake ivinstall-full
```
Alternatively, 'full' may be replaced by 'minimal'.  See [BUILD](BUILD.md) and
[INSTALL](INSTALL.md).

Note: the setupsdk script will end up changing the current working directory to
**build**.

Once completed, an image will be created in
**tmp/deploy/images/${MACHINE}/ivinstall-full-${MACHINE}**.

## Installation

To install the complete set of binaries (bootloaders, Linux OS, Rootfs, etc)
use the installer image (ivinstall-full-${MACHINE}) created above.  For more
information, see the [INSTALL](INSTALL.md) doc.

To install to an SD card mounted on a Linux host machine, run:
```
./ivinstall-full-${MACHINE} -fc /dev/sdX
```
Note: the **MACHINE** should reflect the one used during the build process.
Also, the device name **/dev/sdX** should correspond to the SD card on your
host machine.  This is often **/dev/sdb** but will vary - use **lsblk** or a
similar utility to find out the device name of your SD card.

When installation finishes, safely eject your SD card.

## Boot

Insert the SD card into the iVeia IO board, and boot the device.  The serial
console should display something similar to:

```
Xilinx Zynq MP First Stage Boot Loader
Release 20xx.x   Feb  1 2021  -  21:34:26
Machine:     iVeia atlas-ii-z8ev
Src commit:  xilinx-v20xx.x-0-ge8db5fb(tainted)
Meta commit: 20xx.x-1.5-16-g5df9c39-dirty
iVeia seq boot mode (0x00000001): QSPI24, NORM
Searching SD0 for valid boot image...
Good image found, resetting...

�Xilinx Zynq MP First Stage Boot Loader
Release 20xx.x   Feb  2 2021  -  16:23:31
Machine:     iVeia atlas-ii-z8ev
Src commit:  xilinx-v20xx.x-0-ge8db5fb
Meta commit: 20xx.x-1.5-12-gd3a39a3

IVEIA SEQ BOOT MODE (0x00003103): SD0, ALT
PMU Firmware 20xx.x     Feb  2 2021   16:24:47
PMU_ROM Version: xpbr-v8.1.0-0

...

U-Boot 20xx.01 (Feb 02 2021 - 16:23:28 +0000)

Model: Atlas-II Z8
Board: Xilinx ZynqMP
DRAM:  4 GiB

...

Starting kernel ...

[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x410fd034]
[    0.000000] Linux version 4.19.0-xilinx-v20xx.x (oe-user@oe-host) (gcc version 8.2.0 (GCC)) #1 SMP Tue Feb 2 16:06:40 UTC 2021
[    0.000000] Src commit:  xlnx_rebase_v4.19_20xx.x-0-gb983d5f
[    0.000000] Meta commit: 20xx.x-1.5-12-gd3a39a3

...

root@atlas-ii-z8ev:~#

```

Note: Version numbers in the above output have been modified to make them
generic.

# Package support

The default package management system is opkg.  Packges will be created in
tmp/deploy/ipk.

To build and add a package:
- `MACHINE=<machine-name> bitbake <package-name>`
- Copy .ipk from tmp/deploy/ipk to target
- Install on target with "opkg install --force-depends <ipk>"

Available package recipes can be found at the OpenEmbedded Layer Index at:

> https://layers.openembedded.org/

# Other information

Other information can be found in the docs in this repo, e.g.:
- [INSTALL](INSTALL.md)
- [BUILD](BUILD.md)
- [BOOTGEN](BOOTGEN.md)

# Maintainers

Please submit any patches against the meta-iveia layer to the maintainers:

* Brian Silverman <bsilverman@iveia.com>

