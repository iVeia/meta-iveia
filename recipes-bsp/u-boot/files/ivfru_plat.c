#include "ivfru_common.h"
#include "ivfru_plat.h"
#include <env.h>
#include <i2c.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <command.h>
#include <dm/uclass.h>

#define IPMI_NODE_STR "iv,ipmi"

DECLARE_GLOBAL_DATA_PTR;

static void *image_buffer;

/*
 * get_qspi_info_from_label - Extracts the IPMI QSPI info from the device tree.
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 */
static int get_qspi_info_from_label(const char *ipmi_label, int *bus, int *cs, int *part_offset)
{
	const void *fdt = gd->fdt_blob;
	struct uclass *sfuc;
	struct udevice *flashdev;
	const fdt32_t *val;

	int ret = uclass_get(UCLASS_SPI_FLASH, &sfuc);
	if(ret < 0)
		return IVFRU_RET_INVALID_DEVICE_TREE;

	*bus = -1;
	*cs = -1;
	*part_offset = -1;

	uclass_foreach_dev(flashdev, sfuc) {
		int flash_ofnode = dev_of_offset(flashdev);
		int part_ofnode;
		fdt_for_each_subnode(part_ofnode, fdt, flash_ofnode) {
			if(strncmp(fdt_get_name(fdt, part_ofnode, NULL), "partition", 9) != 0)
				continue;
			char *label = fdt_getprop(fdt, part_ofnode, "label", NULL);
			if(!label)
				continue;
			if(strcmp(ipmi_label, label) != 0)
				continue;

			// Get SPI bus
			struct udevice *spidev = flashdev->parent;
			*bus = spidev->seq;
			if(*bus < 0)
				*bus = spidev->req_seq;
			if(*bus < 0)
				continue;

			// Get CS of SPI flash
			val = fdt_getprop(fdt, flash_ofnode, "reg", NULL);
			if(!val)
				continue;
			*cs = fdt32_to_cpu(*val);

			// Get offset of FRU image in the flash
			val = fdt_getprop(fdt, part_ofnode, "reg", NULL);
			if(!val)
				continue;
			*part_offset = fdt32_to_cpu(*val);
		}
	}
	if(*part_offset < 0)
		return IVFRU_RET_INVALID_DEVICE_TREE;
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_read_i2c - Reads data from I2C EEPROM given offset and size
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
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

/*
 * ivfru_write_i2c - Writes data to I2C EEPROM at given offset and size
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
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

/*
 * ivfru_read_qspi - Reads data from QSPI flash given bus, cs, offset and size.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
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

/*
 * ivfru_write_qspi - Writes data to the QSPI flash given bus, cs, offset and size.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
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

/*
 * ivfru_plat_read_from_location - Reads data from location (memory address) to
 * the static buffer.
 *
 * In the case of U-Boot implementation, it is a noop. This function is added
 * for compatibility with linux implementation.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
int ivfru_plat_read_from_location(void *location)
{
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_write_to_location - Writes buffer data to location (memory address)
 *
 * In the case of U-Boot implementation, it is a noop. This function is added
 * for compatibility with linux implementation.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
int ivfru_plat_write_to_location(void *location)
{
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_read_from_board - Reads IPMI data of given board ID to memory
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size, int quiet)
{
	int ipmi_node;
	int class_node;
	int bus = -1;
	int addr = -1;
	int cs  = -1;
	int ipmi_offset = -1;
	const fdt32_t *val;
	char *ipmi_label = "";
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

		if(ivfru_read_i2c(bus, addr, ipmi_offset + offset, (void *) mem, size) != IVFRU_RET_SUCCESS)
			return IVFRU_RET_IO_ERROR;
	} else if ( (ipmi_label = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
		if(get_qspi_info_from_label(ipmi_label, &bus, &cs, &ipmi_offset) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_read_qspi(bus, cs, ipmi_offset + offset, (void *) mem, size) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to read from qspi storage!\n");
			return IVFRU_RET_IO_ERROR;
		}
	} else {
		if(!quiet)
			printf("Error: Failed to find valid storage info (i2c/qspi) from the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_read_from_board - Writes IPMI data to given board ID from memory
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
int ivfru_plat_write_to_board(enum ivfru_board board, void *mem, int size)
{
	int ipmi_node;
	int class_node;
	int bus = -1;
	int addr = -1;
	int cs  = -1;
	int ipmi_offset = -1;
	char *ipmi_label = "";
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
	} else if ( (ipmi_label = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
		if(get_qspi_info_from_label(ipmi_label, &bus, &cs, &ipmi_offset) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_write_qspi(bus, cs, ipmi_offset, (void *) mem, size) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to write to qspi storage!\n");
			return IVFRU_RET_IO_ERROR;
		}
	} else {
		printf("Error: Failed to find valid storage info (i2c/qspi) from the device tree!\n");
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_set_buffer - Set the static buffer pointer.
 *
 * Returns IVFRU_RET_SUCCESS.
 */
int ivfru_plat_set_buffer(void *buffer)
{
	image_buffer = buffer;
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_get_buffer - Gets the static buffer pointer.
 *
 * Returns the buffer pointer (NULL if not set).
 */
void *ivfru_plat_get_buffer()
{
	return image_buffer;
}

/*
 * ivfru_plat_reserve_memory - Reserves memory space for the static buffer.
 *
 * In the case of U-Boot implementation, it is a noop because we use the same
 * memory location provided by the user for the output and it is unmanaged.
 *
 * Returns IVFRU_RET_SUCCESS.
 */
int ivfru_plat_reserve_memory(int size)
{
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_set_image_size - Sets the size of the IPMI FRU image in the static
 * buffer.
 *
 * Sets the 'filesize' U-Boot environment variable with this value.
 *
 * Returns IVFRU_RET_SUCCESS.
 */
int ivfru_plat_set_image_size(int size)
{
	char sizestr[20];
	sprintf(sizestr, "%d", size);
	env_set("filesize", sizestr);

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_get_image_size - Gets the size of the IPMI FRU image in the static
 * buffer.
 *
 * Not implemented/required in the U-Boot implementation.
 *
 * Returns 0.
 */
int ivfru_plat_get_image_size()
{
	return 0;
}
