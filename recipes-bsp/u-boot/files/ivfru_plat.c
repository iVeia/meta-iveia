#include "ivfru_common.h"
#include "ivfru_plat.h"
#include <env.h>
#include <i2c.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <command.h>

#define IPMI_NODE_STR "iv,ipmi"

DECLARE_GLOBAL_DATA_PTR;

static int ivfru_read_i2c(int bus, int addr, int offset, void *data, int size)
{
	int err = IVFRU_RET_SUCCESS;
	struct udevice *dev;

	err = i2c_get_chip_for_busnum(bus, addr, 2, &dev);
	if (err != 0)
		return IVFRU_RET_IO_ERROR;

	err = dm_i2c_read(dev, offset, (uchar *) data, size);
	if (err != 0)
		return IVFRU_RET_IO_ERROR;

	return err;
}

static int ivfru_write_i2c(int bus, int addr, int offset, void *data, int size)
{
	int err = IVFRU_RET_SUCCESS;
	struct udevice *dev;

	err = i2c_get_chip_for_busnum(bus, addr, 2, &dev);
	if (err != 0)
		return IVFRU_RET_IO_ERROR;

	while(size--) {
		err = dm_i2c_write(dev, offset, (uchar *) data, 1);
		if (err != 0)
			return IVFRU_RET_IO_ERROR;
		offset++;
		data++;
		// Write delay of 10ms with margin (same as cmd/i2c.c)
		udelay(11000);
	}

	return err;
}

static int ivfru_read_qspi(int bus, int cs, int offset, void *data, int size)
{
	int err = IVFRU_RET_SUCCESS;
	struct spi_flash *board_flash;
	struct udevice *dev;

	/* speed and mode will be read from DT */
	err = spi_flash_probe_bus_cs(bus, cs, 3000000, 0, &dev);
	if (err)
		return IVFRU_RET_IO_ERROR;

	board_flash = dev_get_uclass_priv(dev);

	err = spi_flash_read(board_flash, offset, size, (uchar *) data);
	if (err)
		return IVFRU_RET_IO_ERROR;

	return err;
}

static int ivfru_write_qspi(int bus, int cs, int offset, void *data, int size)
{
	char cmd[100];
	int err;

	sprintf(cmd, "sf probe %d:%d", bus, cs);
	err = run_command(cmd, 0);
	if(err != 0)
		return IVFRU_RET_IO_ERROR;

	sprintf(cmd, "sf update 0x%p 0x%x 0x%x", data, offset, size);
	err = run_command(cmd, 0);
	if(err != 0)
		return IVFRU_RET_IO_ERROR;

	return IVFRU_RET_SUCCESS;
}

int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size, int quiet)
{
	int ipmi_node;
	int class_node;
	int bus = -1;
	int addr = 0;
	int cs  = -1;
	int ipmi_offset = 0;
	const fdt32_t *val;
	const void *fdt = gd->fdt_blob;

	ipmi_node = fdt_node_offset_by_compatible(fdt, 0, IPMI_NODE_STR);
	if ( ipmi_node < 0 ) {
		if(!quiet)
			printf("Error: Failed to find '"IPMI_NODE_STR"' node in the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	char *board_str = ivfru_board2str(board);
	if(board_str == NULL) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	const char *dt_prefix = "iv_";
	char board_dtstr[strlen(board_str) + strlen(dt_prefix) + 1];
	strcpy(board_dtstr, dt_prefix);
	strcat(board_dtstr, board_str);
	class_node = fdt_subnode_offset(fdt, ipmi_node, board_dtstr);
	if (class_node < 0) {
		if(!quiet)
			printf("Error: Failed to find '%s' node in the device tree!\n", board_dtstr);
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	if ( (val = fdt_getprop(fdt, class_node, "i2c", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		addr = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		if(ivfru_read_i2c(bus, addr, ipmi_offset + offset, (void *) mem, size) == IVFRU_RET_SUCCESS)
			return IVFRU_RET_SUCCESS;
	} else if ( (val = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		cs = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		if(ivfru_read_qspi(bus, cs, ipmi_offset + offset, (void *) mem, size) == IVFRU_RET_SUCCESS)
			return IVFRU_RET_SUCCESS;
	} else {
		if(!quiet)
			printf("Error: Failed to find valid storage info (i2c/qspi) from the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_plat_write_to_board(enum ivfru_board board, void *mem, int size)
{
	int ipmi_node;
	int class_node;
	int bus = -1;
	int addr = 0;
	int cs  = -1;
	int ipmi_offset = 0;
	const fdt32_t *val, *val2;
	const void *fdt = gd->fdt_blob;

	ipmi_node = fdt_node_offset_by_compatible(fdt, 0, IPMI_NODE_STR);
	if ( ipmi_node < 0 ) {
		printf("Error: Failed to find '"IPMI_NODE_STR"' node in the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	char *board_str = ivfru_board2str(board);
	if(board_str == NULL)
		return IVFRU_RET_INVALID_ARGUMENT;

	const char *dt_prefix = "iv_";
	char board_dtstr[strlen(board_str) + strlen(dt_prefix) + 1];
	strcpy(board_dtstr, dt_prefix);
	strcat(board_dtstr, board_str);
	class_node = fdt_subnode_offset(fdt, ipmi_node, board_dtstr);
	if (class_node < 0) {
		printf("Error: Failed to find '%s' node in the device tree!\n", board_dtstr);
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	if ( (val = fdt_getprop(fdt, class_node, "i2c", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		addr = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		// Check for (optional) write-protect gpio info
		int wp_node = fdt_subnode_offset(fdt, class_node, "wp");
		int wp_bus = -1;
		int wp_addr = -1;
		int wp_gpio = -1;
		int wp_active = -1;
		struct gpio_desc wp_desc;
		unsigned int wp_gpio_global = -1;
		if (wp_node >= 0) {
			if ( (val2 = fdt_getprop(fdt, wp_node, "i2c_gpio", NULL) )) {
				wp_bus = fdt32_to_cpu(*(val2 + 0));
				wp_addr = fdt32_to_cpu(*(val2 + 1));
				wp_gpio = fdt32_to_cpu(*(val2 + 2));
				wp_active = fdt32_to_cpu(*(val2 + 3));

				struct udevice *dev;
				if(i2c_get_chip_for_busnum(wp_bus, wp_addr, 2, &dev) != 0) {
					printf("Error: Failed to get write-protect i2c GPIO device!\n");
					return IVFRU_RET_IO_ERROR;
				}
				wp_desc.dev = dev;
				wp_desc.flags = 0;
				wp_desc.offset = wp_gpio;
				wp_gpio_global = gpio_get_number(&wp_desc);
				if(wp_gpio_global < 0) {
					printf("Error: Failed to get write-protect i2c GPIO number!\n");
					return IVFRU_RET_IO_ERROR;
				}
			} else if ( (val2 = fdt_getprop(fdt, wp_node, "gpio", NULL) )) {
				wp_gpio = fdt32_to_cpu(*(val2 + 0));
				wp_active = fdt32_to_cpu(*(val2 + 1));
				char gpio_str[5];
				sprintf(gpio_str, "%d", wp_gpio);
				gpio_lookup_name(gpio_str, NULL, NULL, &wp_gpio_global);
				if(wp_gpio_global < 0) {
					printf("Error: Failed to get write-protect GPIO number!\n");
					return IVFRU_RET_IO_ERROR;
				}
			}
		}

		// Dessert WP (if used)
		if(wp_gpio >= 0) {
			if(gpio_request(wp_gpio_global, "iv_wp_gpio") != 0) {
				printf("Error: Failed to request write-protect GPIO!\n");
				return IVFRU_RET_IO_ERROR;
			}
			if(gpio_direction_output(wp_gpio_global, !wp_active) != 0) {
				printf("Error: Failed to deassert write-protect GPIO!\n");
				return IVFRU_RET_IO_ERROR;
			}
		}

		int err = ivfru_write_i2c(bus, addr, ipmi_offset, (void *) mem, size);

		// Assert WP
		if(wp_gpio >= 0) {
			if(gpio_set_value(wp_gpio_global, wp_active) != 0) {
				printf("Error: Failed to assert write-protect GPIO after write!\n");
				return IVFRU_RET_IO_ERROR;
			}
			if(gpio_free(wp_gpio_global) != 0) {
				printf("Error: Failed to free write-protect GPIO!\n");
				return IVFRU_RET_IO_ERROR;
			}
		}

		if(err != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to write to i2c storage!\n");
			return IVFRU_RET_IO_ERROR;
		}
	} else if ( (val = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		cs = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		if(ivfru_write_qspi(bus, cs, ipmi_offset, (void *) mem, size) == IVFRU_RET_SUCCESS) {
			printf("Error: Failed to write to qspi storage!\n");
			return IVFRU_RET_SUCCESS;
		}
	} else {
		printf("Error: Failed to find valid storage info (i2c/qspi) from the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_plat_reserve_memory(void *mem, int size)
{
	return IVFRU_RET_SUCCESS;
}

int ivfru_plat_set_image_size(void *mem, int size)
{
	char sizestr[20];
	sprintf(sizestr, "%d", size);
	env_set("filesize", sizestr);

	return IVFRU_RET_SUCCESS;
}
