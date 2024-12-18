HDF_BASE = "file://"
HDF_PATH = "${HDF_NAME}"
FILESEXTRAPATHS_prepend := "${THISDIR}:"

# atlas-z7/8 are "fake" MACHINEs, used solely for building SDKs.  They don't
# have their own HDF file, and instead can use an HDF file from any machine
# from the same family.
FILESEXTRAPATHS_prepend_atlas-z7 := "${THISDIR}/00068:"
FILESEXTRAPATHS_prepend_atlas-z8 := "${THISDIR}/00104:"
