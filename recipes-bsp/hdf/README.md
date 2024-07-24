# XSA files (formerly HDF)

Each subdirectory corresponds to a MACHINE (iVeia mainboard) that contains a
system.xsa file.  The system.xsa is the hardware description file exported from
Vivado.  Yocto uses this to generate device-tree and bootloader source.  Vivado
creates system.xsa files using the "Export Hardware" feature.

For system.xsa files created with Vivado version 2019.2 and before, you can
create a Vivado project (XPR) from the XSA file.  After that version, you need
to save the a Block Design TCL script to alongside the XSA.  This TCL file will
ONLY be used to recreate the XSA, and is NOT used by Yocto.

You MUST recreate the XPR file using the same version of Xilinx tools with
which the XSA/TCL was created.  However, you may use any later version of the
Vivado tools to open the XPR project (the tools will upgrade the project).

To recreate the Vivado project XPR file:

- If the system.xsa was created with Vivado 2019.2 or before:
    - Extract the TCL Block Design with:

          unzip system.xsa "design*bd.tcl"

    - Recreate the XPR file with:

          vivado -mode batch -source design*bd.tcl

    - The resulting project is located at: "vivado myproj/project_1.xpr"
- Otherwise if the system.xsa was created with a Vivado version after 2019.2:
    - A TCL script for the Block Design (design.tcl) must have been previously
      saved alongside the system.xsa.
    - Recreate the XPR file with:

          vivado -mode batch -source design.tcl

To modify setting from Vivado:

- Open the project:

      vivado myproj/project_1.xpr

- An automatic upgrade of the project made be required when the tools are
  newer than the XSA file
- Flow Navigator -> IP INTEGRATOR -> Open Block Design
- Double-click the Zynq/ZynqMP block to edit settings

To save your changes:

- Create an HDL wrapper for the Block Design:
    - From the Sources window under "Design Sources"
    - Right click "design_1" -> Create HDL Wrapper...
- Flow Navigator -> IP INTEGRATOR -> Generate Block Design
- Export the XSA/TCL with:
    - File -> Export -> Export Hardware...
        - Choose the defaults, if asked (e.g "Fixed", "Pre-synthesis")
        - Choose Finish
    - [For > 2019.2 ONLY] File -> Export -> Export Block Design...
- Copy the exported files on top of the existing XSA/TCL:
    - Copy myproj/design_1_wrapper.xsa to system.xsa
    - [For > 2019.2 ONLY] Copy myproj/design_1.tcl to design.tcl
