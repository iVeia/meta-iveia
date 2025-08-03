Location of IPMI FRU Information is denoted by the following device-tree nodes

Below iv_ipmi node should be specified at the root of the device-tree.  It is
parsed in uboot to set /proc/device-tree/chosen nodes and ethernet mac
addresses.


Example:

    iv_ipmi: iv_ipmi {
        compatible = "iv,ipmi";
        #address-cells = <0x01>;
        #size-cells = <0x01>;

        // below can be modified as needed in machine dtsi's

        iv_ipmi_mb: iv_mb {
            i2c = < 1 0x52 0 >;  // < bus_num addr offset >
            wp-gpios = <&gpio_expander 6 GPIO_ACTIVE_HIGH>;
        };

        iv_ipmi_io: iv_io {
            i2c = < 0 0x53 0 >;  // < bus_num addr offset >
            wp-gpios = <&tca6424_io 8 GPIO_ACTIVE_LOW>;
        };
    };

Board subnodes (optional, used to specify ipmi location for each board type):
    iv_mb: MainBoard (SoM)
    iv_io: carrier IO board
    iv_bp: BackPlane board

        FRU location properties:
            Either I2C-based EEPROM:
                i2c = < bus_num addr offset >
            Or QSPI-based flash:
                # "partition-name" must be defined within &qspi node.
                qspi = "partition-name"
                # <offset> denotes the offset within the chosen partition.
                qspi_offset = < offset >

        Optional write protect property, used to specify write protect GPIO):
            wp-gpios = <&gpio_controller pin_number flags>
            Where:
                gpio_controller: phandle to GPIO controller (e.g., &tca6424_io)
                pin_number: GPIO pin number on the controller
                flags: GPIO_ACTIVE_HIGH or GPIO_ACTIVE_LOW
