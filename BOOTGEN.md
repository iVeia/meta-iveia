# Creating custom boot images with `bootgen`

The main boot image, `boot.bin`, is created automatically by the Yocto/bitbake
build process.  However, you may want to create your own `boot.bin` with custom
images.  This can be useful if you want to load a bitfile very early in the
boot process (before U-Boot).  Alternatively, you may not be using U-Boot and
want to boot with your own custom bootloader or application.

## Building the boot images

To build the `boot.bin`, you can directly use Xilinx's target `xilinx-bootbin`.
For example, after following the download & build instructions in the
[README](README.md) doc, you can build the `boot.bin` with:

```
MACHINE=<CHOOSE SUPPORTED MACHINE> bitbake xilinx-bootbin
```

The images will be located in the Yocto deploy directoy 
**build/tmp/deploy/images/${MACHINE}/**.

The `boot.bin` will contain the boot images from FSBL through U-Boot.  If the
`boot.bin` image is installed on the QSPI flash on a target device (see
[INSTALL](INSTALL.md)), then the device will boot to U-Boot.

When building the `xilinx-bootbin` target, all required binaries will also be
built in the deploy directory above.  This includes, for example, the FSBL and
PMUFW.  To rebuild these individually, you can use (to rebuild the FSBL):

```
MACHINE=<CHOOSE SUPPORTED MACHINE> bitbake fsbl
```

and to rebuild the PMUFW:

```
MACHINE=<CHOOSE SUPPORTED MACHINE> bitbake pmu-firmware
```

## Using `bootgen` to recreate the `boot.bin`

To manually recreate `boot.bin` with `bootgen`, you need to create a Xilinx BIF
file for `bootgen`.  An example BIF file (`image.bif`) is shown below that
defines all the bootloaders up to and including U-Boot:

```
the_ROM_image:{ 
	[bootloader] fsbl-atlas-iii-z8e.elf
	[destination_cpu=pmu] pmu-atlas-iii-z8e.elf
	[destination_cpu=a5x-0] arm-trusted-firmware.elf
	[destination_cpu=a53-0, exception_level = el-2] u-boot.elf
}
```

The above assumes that the current working directory is the deploy directory
previously defined, and that the `MACHINE` used is the `atlas-iii-z8e`.

To rebuild your own `boot.bin`, you can run `bootgen` with:
```
bootgen -arch zynqmp -image image.bif -w -o boot.bin
```

Note: that you will need to source the Xilinx tools to use `bootgen`.

## Customizing the `boot.bin` image

To create a `boot.bin` the same as above, except with a bitfile loaded after
the PMUFW, you could use:

```
the_ROM_image:{ 
	[bootloader] fsbl-atlas-iii-z8e.elf
	[destination_cpu=pmu] pmu-atlas-iii-z8e.elf
	[destination_device=pl] xilinx.bit
	[destination_cpu=a5x-0] arm-trusted-firmware.elf
	[destination_cpu=a53-0, exception_level = el-2] u-boot.elf
}
```

Other modifications will depend on the user application.  See Xilinx's Bootgen
User Guide for more information.
