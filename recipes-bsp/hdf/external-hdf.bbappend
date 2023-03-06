HDF_BASE = "file://"
HDF_PATH = "${HDF_NAME}"
FILESEXTRAPATHS:prepend := "${THISDIR}:"

# atlas-z7/8 are "fake" MACHINEs, used solely for building SDKs.  They don't
# have their own HDF file, and instead can use an HDF file from any machine
# from the same family.
FILESEXTRAPATHS:prepend:atlas-z7 := "${THISDIR}/atlas-i-z7e:"
FILESEXTRAPATHS:prepend:atlas-z8 := "${THISDIR}/atlas-iii-z8:"
