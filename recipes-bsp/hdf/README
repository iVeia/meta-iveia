# XSA files (formerly HDF)

Each subdirectory corresponds to a MACHINE (iVeia mainboard) that contains a
system.xsa file.  The system.xsa is the hardware description file exported from
Vivado.  Yocto uses this to generate device-tree and bootloader source.

You can modify the XSA by recreating a Vivado project from the XSA file, and 
editing the block design in Vivado.

To recreate the Vivado project XPR file:

- Extract the TCL block design with:

      unzip system.xsa "design*bd.tcl"

- Recreate the XPR file with:

      vivado -mode batch -source design*bd.tcl

- The resulting project is located at: "vivado myproj/project_1.xpr"

To modify setting from Vivado:

- Open the project:

      vivado myproj/project_1.xpr

- Flow Navigator -> IP INTEGRATOR -> Open Block Design
- Double-click the Zynq/ZynqMP block to edit settings

To save your changes:

- Create an HDL wrapper for the block design:
    - From the Sources window, under "Design Sources"
    - Right click "design_1" -> Create HDL Wrapper...
- Flow Navigator -> IP INTEGRATOR -> Generate Block Design
- Export XSA with:
    - File -> Export -> Export Hardware...
- Copy the exported files on top of the existing XSA:
    - Copy myproj/design_1_wrapper.xsa to system.xsa


