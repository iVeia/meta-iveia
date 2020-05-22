# Workaround for required dependency on xsct that is not met.  The dependency
# can be faked by depending on an arbitrary target that DOES depend on xsct.
DEPENDS += "bitstream-extraction"
