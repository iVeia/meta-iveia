#include "ivfru_common.h"
#include "ivfru_plat.h"
#include <i2c.h>
#include <spi.h>
#include <spi_flash.h>

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

	err = dm_i2c_write(dev, offset, (uchar *) data, size);
	if (err != 0)
		return IVFRU_RET_IO_ERROR;

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
	return IVFRU_RET_IO_ERROR;
}

int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size)
{
	int ipmi_node;
	int class_node;
	int bus = -1;
	int addr = 0;
	int cs  = -1;
	int ipmi_offset = 0;
	const fdt32_t *val;
	const void *fdt = gd->fdt_blob;

	ipmi_node = fdt_node_offset_by_compatible(fdt, 0, "iv,ipmi");
	if ( ipmi_node < 0 ) {
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
	const fdt32_t *val;
	const void *fdt = gd->fdt_blob;

	ipmi_node = fdt_node_offset_by_compatible(fdt, 0, "iv,ipmi");
	if ( ipmi_node < 0 ) {
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
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	if ( (val = fdt_getprop(fdt, class_node, "i2c", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		addr = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		if(ivfru_write_i2c(bus, addr, ipmi_offset, (void *) mem, size) == IVFRU_RET_SUCCESS)
			return IVFRU_RET_SUCCESS;
	} else if ( (val = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
		bus = fdt32_to_cpu(*(val + 0));
		cs = fdt32_to_cpu(*(val + 1));
		ipmi_offset = fdt32_to_cpu(*(val + 2));

		if(ivfru_write_qspi(bus, cs, ipmi_offset, (void *) mem, size) == IVFRU_RET_SUCCESS)
			return IVFRU_RET_SUCCESS;
	} else {
		return IVFRU_RET_INVALID_DEVICE_TREE;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_plat_set_memory_size(int size)
{
	return IVFRU_RET_SUCCESS;
}
