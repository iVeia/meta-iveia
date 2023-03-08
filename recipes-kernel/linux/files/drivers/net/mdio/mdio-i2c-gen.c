/*
 * I2C-based MDIO PHY driver.
 *
 * Copyright (c) 2020 iVeia
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/of_mdio.h>

#include "mdio-i2c.h"

static int mdio_i2c_gen_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *phy_node;
	struct mii_bus *bus;
	unsigned int phy_addr;
	unsigned int i2c_addr;
	int ret;

	bus = mdio_i2c_alloc(dev, client->adapter);
	if (IS_ERR(bus)){
		return PTR_ERR(bus);
	}
	bus->name = "I2C-based MDIO PHY";
	bus->parent = dev;

	/*
	 * Each I2C mdio node must have exactly one child phy whose phy address is
	 * a fixed offset from the I2C address.  In addition, phy addresses 0x10
	 * and 0x11 are not allowed.  These are restrictions of the mdio-i2c
	 * library for SFP support, which is uneeded here - however we use the
	 * mdio-i2c library for convenience.
	 */
	if (of_get_child_count(np) != 1) {
		dev_warn(dev, "MDIO node must have exactly one child PHY node\n");
		return  -EINVAL;
	}

	phy_node = of_get_next_child(np, NULL);
	if (phy_node == NULL)
		return -ENODEV;

	ret = of_property_read_u32(np, "reg", &i2c_addr);
	if (ret)
		return ret;
	ret = of_property_read_u32(phy_node, "reg", &phy_addr);
	if (ret)
		return ret;
	if (phy_addr + 0x40 != i2c_addr) {
		dev_warn(dev, "MDIO I2C addr must be PHY addr + 0x40\n");
		return  -EINVAL;
	}
	if (phy_addr == 0x10 || phy_addr == 0x11) {
		dev_warn(dev, "MDIO PHY addr must NOT be 0x10 or 0x11\n");
		return  -EINVAL;
	}

	return of_mdiobus_register(bus, np);
}

static int mdio_i2c_gen_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id mdio_i2c_gen_of_match[] = {
	{ .compatible = "mdio-i2c-gen", },
	{ }
};
MODULE_DEVICE_TABLE(of, mdio_i2c_gen_of_match);

static struct i2c_driver mdio_i2c_gen_driver = {
	.driver = {
		.name	= "mdio-i2c-gen",
		.of_match_table = of_match_ptr(mdio_i2c_gen_of_match),
	},
	.probe		= mdio_i2c_gen_probe,
	.remove	= mdio_i2c_gen_remove,
};

module_i2c_driver(mdio_i2c_gen_driver);

MODULE_AUTHOR("Brian Silverman <bsilverman@iveia.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic driver for MDIO bus over I2C");
