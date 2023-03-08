# meta-iveia version stampimg

The meta-iveia layer stamps versions into the binaries for the FSBL, U-Boot,
Linux and the Root FS.  The versions are displayed at boot time on the console.
In addition, the Root FS version is stored in the file /etc/os-release.

For each binary, several versions are stored:
- The git version of the fetched source.  Not applicable for the Root FS, as it
  has many sources.
- The git version of the meta-iveia layer.
- The git version of any layer of the form meta-iveia-*
- A build date

The git version is generated with `git describe`, and uses "long" form to
always include the git commit.

The code that generates the versions can handle any number of meta-iveia-*
layers.  However, the code (FSBL, U-Boot, Linux) that displays the versions in
the banner is limited to 3 versions, and will fail to build if addition
meta-iveia-* layers are used.

## Different versions

The version displayed by `ivinstall` can be different from the versions stamped
on the binaries within `ivinstall` (e.g. Linux).  A clean build of all targets
will use a identical version and date timestamps on all binaries.

Yocto maintains a state cache for building target binaries.  If the Yocto
sources are modified and the final image (e.g. ivinstall-minimal) is rebuilt,
not all binaries will necessarily be rebuilt.  This can cause the displayed
versions for different binaries to be inconsistent.

For example, if U-Boot is rebuilt (e.g. with `bitbake -fC compile`
u-boot-xlnx), then U-Boot's binary will be updated with a new timestamp.
However, when running `bitbake ivinstal-minimal`, the final target image will
contain a U-Boot with an update timestamp, but other binaries (e.g. Linux) will
retain an old timestamp as they will have been included from the Yocto state
cache.

## Version files

The versions script generate an version file (META_IVEIA_VERSIONS_FILE) on
EVERY build.  The location is defined as:
    META_IVEIA_VERSIONS_FILE := "${DEPLOY_DIR_IMAGE}/versions.txt"

An example of the file is:
    IVEIA_MACHINE="atlas-ii-z8ev"
    IVEIA_BUILD_DATE="2023-03-07_19:39:25"
    IVEIA_NUM_LAYERS="2"
    IVEIA_META_1_LAYER="meta-iveia"
    IVEIA_META_1_BUILD_HASH="2019.2-1.7-61-gf582fed-dirty"
    IVEIA_META_2_LAYER="meta-iveia-test"
    IVEIA_META_2_BUILD_HASH="v1.2.3-2-g163826c"

The file is shell script sourceable, and is sourced within the Yocto recipes.

The file is directly copied to the Root FS at /etc/os-release.

Specific recipes additionally use another version, the IVEIA_SRC_BUILD_HASH,
which is the git describe of the fetched source.  For example, for the FSBL
project, this will be the `git describe` of the fetched FSBL source, excluding
patches.  This is displayed in boot banners as "Src commit: <version>"


