BIF_PARTITION_ATTR_remove = "bitstream"

#
# Select the correct DTB to add boot.bin - either the DTB for this MACHINE, or
# a combined DTB of the MACHINE and the DT overlay for an iVeia IO board
# (IVIO).
#
# See also the device-tree recipe.
#
DEVICETREE_DIR = "${RECIPE_SYSROOT}/boot/devicetree"
do_configure[prefuncs] += "do_pre_configure_ivio"
do_pre_configure_ivio[vardepsexclude] += "BB_ORIGENV"
python do_pre_configure_ivio() {
    import glob
    ivio_env = d.getVar("BB_ORIGENV").getVar("IVIO")
    ivio = ivio_env if ivio_env else d.getVar("IVIO")

    machine = d.getVar("MACHINE")
    filename = f"{machine}_{ivio}_combo.dtb" if ivio else f"{machine}.dtb"
    dtb = os.path.join(d.getVar("DEVICETREE_DIR"), filename)
    if not os.path.isfile(dtb):
        bb.fatal("Cannot find DTB ({%s})".format(dtb))
    d.setVarFlag("BIF_PARTITION_IMAGE", "device-tree", dtb)
}
