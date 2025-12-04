HDF_BASE = "file://"
HDF_PATH = "${HDF_NAME}"
FILESEXTRAPATHS_prepend := "${THISDIR}:"

# atlas-z7 is a"fake" MACHINE, used solely for building SDKs.  It doesn't
# have its own HDF file, and instead can use an HDF file from any machine
# from the same family.
FILESEXTRAPATHS_prepend_atlas-z7 := "${THISDIR}/00068:"
