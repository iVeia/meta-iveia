# iVeia MACHINEs

## Overview

The Yocto `MACHINE` defines a target board and specific software features for that board.  For
iVeia SoMs, the `MACHINE` is generally the 5 digit numeric part number ordinal of the board.

On iVeia SoMs, the part number is written on the silkscreen and/or copper of the board, and is
in the form `20X-NNNNN`.  The `NNNNN` is the 5 digit numeric part number ordinal.  Additionally,
SoMs have a four digit variant/revision sitcker of the form `VVRR`.  The two character `VV`
indicates the BOM variant of the board, and the two character `RR` indicates the revision.

On boot, the full part number is displayed in the form `205-NNNNN-VV-RR`.

## Building

The `MACHINE` is used when building an image, for example:

```
MACHINE=00104 bitbake ivinstall-full
```

## Feature sets

Some SoMs will have additional features that can be enabled using an extended `MACHINE`
name.  For example, some variants have a higher speed grade part that is only enabled when
using the extended `MACHINE` name.

The format for the extended `MACHINE` is `NNNNN-fsFFF`, where FFF is a 3 digit code defining
the feature set.  If a `MACHINE` has a feature set it will be defined in the next section
and will include the applicable variants.  If a `MACHINE` has no feature sets, then the
`MACHINE` is useable for all variants of the SoM.

## Specific feature sets

| MACHINE     | Features                  | Variants (VV) |
|:------------|:--------------------------|:--------------|
| 00104       | NA                        | Any
| 00104-fs001 | Speed -2                  | 12

