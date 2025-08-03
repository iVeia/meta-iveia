#include "ivfru_common.h"
#include "ivfru_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <gpiod.h>

#define IPMI_DT_PATH "/proc/device-tree/iv_ipmi/"
#define BOARD_PREFIX "iv_"
#define ZYNQMP_GPIO_OFNODE "/axi/gpio@ff0a0000"
#define MAX_QSPI_PART_LABEL_LEN 100
#define MAX_MTD_FILE_PATH 256

static void *image_buffer;
static int buffer_size = 0;
static int image_size = 0;

/*
 * gpio_open - Open a GPIO line for output
 *
 * gpiochip: GPIO chip number (e.g., 0 for gpiochip0)
 * pin: GPIO pin number on the chip
 * active_low: 1 if GPIO is active low, 0 if active high
 *
 * Returns GPIO line handle or NULL on failure
 */
static struct gpiod_line *gpio_open(int gpiochip, int pin, int active_low)
{
	struct gpiod_chip *chip;
	struct gpiod_line *line;

	chip = gpiod_chip_open_by_number(gpiochip);
	if (!chip) {
		return NULL;
	}

	line = gpiod_chip_get_line(chip, pin);
	if (!line) {
		gpiod_chip_close(chip);
		return NULL;
	}

	int flags = active_low ? GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW : 0;
	if (gpiod_line_request_output_flags(line, "ivfru", flags, 1) < 0) {
		gpiod_chip_close(chip);
		return NULL;
	}

	return line;
}

/*
 * gpio_close - Close a GPIO line
 *
 * line: GPIO line handle to close
 */
static void gpio_close(struct gpiod_line *line)
{
	if (line) {
		struct gpiod_chip *chip = gpiod_line_get_chip(line);
		gpiod_line_release(line);
		if (chip) {
			gpiod_chip_close(chip);
		}
	}
}

/*
 * get_i2c_info_from_dt - Extracts the IPMI I2C EEPROM info from the device tree.
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 */
static int get_i2c_info_from_dt(enum ivfru_board board, uint32_t *bus, uint32_t *addr, uint32_t *ipmi_offset)
{
	*bus = -1;
	*addr = -1;
	*ipmi_offset = -1;

	char *board_str = ivfru_board2str(board);
	if(board_str == NULL) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	const int path_len = strlen(IPMI_DT_PATH) + strlen(BOARD_PREFIX)
		+ strlen(board_str) + 1 /* slash */ + 4 /* qspi/i2c */
		+ 1 /* null term */;
	char path[path_len];
	strcpy(path, IPMI_DT_PATH);
	strcat(path, BOARD_PREFIX);
	strcat(path, board_str);
	strcat(path, "/i2c");

	if(access(path, R_OK) != 0)
		return IVFRU_RET_INVALID_DEVICE_TREE;

	int fd = open(path, O_RDONLY);
	if(fd < 0)
		return IVFRU_RET_IO_ERROR;

	int rlen = 0;
	rlen += read(fd, bus, sizeof(*bus));
	rlen += read(fd, addr, sizeof(*addr));
	rlen += read(fd, ipmi_offset, sizeof(*ipmi_offset));
	if(rlen != sizeof(*bus) + sizeof(*addr) + sizeof(*ipmi_offset))
		return IVFRU_RET_IO_ERROR;

	*bus = ntohl(*bus);
	*addr = ntohl(*addr);
	*ipmi_offset = ntohl(*ipmi_offset);

	return IVFRU_RET_SUCCESS;
}

/*
 * get_wp_info_from_dt - Extracts the IPMI I2C EEPROM write-protect GPIO pin
 * info from the device tree for use with libgpiod.
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 * Sets chip to the GPIO chip number, gpio to the pin number, and active to the polarity.
 */
static int get_wp_info_from_dt(enum ivfru_board board, int *chip, int *pin, int *active_low)
{
	char *board_str = ivfru_board2str(board);
	if(board_str == NULL) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	const int path_len = strlen(IPMI_DT_PATH) + strlen(BOARD_PREFIX)
		+ strlen(board_str) + 1 /* slash */ + 9 /* wp-gpios */
		+ 1 /* null term */;
	char path_gpios[path_len];
	strcpy(path_gpios, IPMI_DT_PATH);
	strcat(path_gpios, BOARD_PREFIX);
	strcat(path_gpios, board_str);
	strcat(path_gpios, "/wp-gpios");

	if(access(path_gpios, R_OK) == 0) {
		int fd = open(path_gpios, O_RDONLY);
		if(fd < 0)
			return IVFRU_RET_IO_ERROR;

		// Standard GPIO reference format: <phandle pin flags>
		uint32_t phandle, arg;
		if (read(fd, &arg, sizeof(arg)) != sizeof(arg)) return IVFRU_RET_IO_ERROR;
		phandle = ntohl(arg);
		if (read(fd, &arg, sizeof(arg)) != sizeof(arg)) return IVFRU_RET_IO_ERROR;
		*pin = ntohl(arg);
		if (read(fd, &arg, sizeof(arg)) != sizeof(arg)) return IVFRU_RET_IO_ERROR;
		*active_low = !!arg;
		close(fd);

		// Find the GPIO chip name by searching /sys/bus/gpio/devices/ for matching phandle
		DIR *gpio_dir = opendir("/sys/bus/gpio/devices");
		if (!gpio_dir) {
			return IVFRU_RET_IO_ERROR;
		}

		int found = 0;
		struct dirent *entry;
		while ((entry = readdir(gpio_dir)) != NULL && !found) {
			if (strncmp(entry->d_name, "gpiochip", 8) != 0)
				continue;

			// Check the phandle for this gpiochip
			char phandle_path[300];
			snprintf(phandle_path, sizeof(phandle_path),
					"/sys/bus/gpio/devices/%s/of_node/phandle", entry->d_name);

			int phandle_fd = open(phandle_path, O_RDONLY);
			if (phandle_fd >= 0) {
				uint32_t chip_phandle;
				if (read(phandle_fd, &chip_phandle, sizeof(chip_phandle)) == sizeof(chip_phandle)) {
					chip_phandle = ntohl(chip_phandle);
					if (chip_phandle == phandle) {
						// Found matching GPIO chip - extract chip number
						// entry->d_name is like "gpiochip0", extract the number
						char *end;
						*chip = strtol(entry->d_name + 8, &end, 10);
						if (end > entry->d_name + 8 && *end == '\0') {
							found = 1;
						} else {
							*chip = -1;
						}
					}
				}
				close(phandle_fd);
			}
		}
		closedir(gpio_dir);

		if (!found) {
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}

		return IVFRU_RET_SUCCESS;
	}

	return IVFRU_RET_INVALID_DEVICE_TREE;
}

/*
 * get_qspi_info_from_dt - Extracts the IPMI qspi partition label
 * from the device tree.
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 */
static int get_qspi_info_from_dt(enum ivfru_board board, char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1], uint32_t *qspi_offset)
{
	char *board_str = ivfru_board2str(board);
	if(board_str == NULL) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	const int path_len = strlen(IPMI_DT_PATH) + strlen(BOARD_PREFIX)
		+ strlen(board_str) + 1 /* slash */ + 11 /* qspi_offset */
		+ 1 /* null term */;
	char path[path_len];
	strcpy(path, IPMI_DT_PATH);
	strcat(path, BOARD_PREFIX);
	strcat(path, board_str);
	strcat(path, "/qspi");

	if(access(path, R_OK) != 0)
		return IVFRU_RET_INVALID_DEVICE_TREE;

	int fd = open(path, O_RDONLY);
	if(fd < 0)
		return IVFRU_RET_IO_ERROR;

	struct stat st;
	int result = fstat(fd, &st);
	if (result < 0)
		return IVFRU_RET_IO_ERROR;
	int len = st.st_size;
	if(len > MAX_QSPI_PART_LABEL_LEN)
		return IVFRU_RET_INVALID_DEVICE_TREE;
	if(read(fd, ipmi_label, len) != len)
		return IVFRU_RET_IO_ERROR;
	ipmi_label[len] = '\0';

	close(fd);

	// Read optional property qspi_offset
	strcat(path, "_offset");
	if(access(path, R_OK) != 0) {
		*qspi_offset = 0;
		return IVFRU_RET_SUCCESS;
	}

	fd = open(path, O_RDONLY);
	if(fd < 0)
		return IVFRU_RET_IO_ERROR;

	const int rlen = sizeof(*qspi_offset);
	if(read(fd, qspi_offset, rlen) != rlen)
		return IVFRU_RET_IO_ERROR;
	*qspi_offset = ntohl(*qspi_offset);

	close(fd);

	return IVFRU_RET_SUCCESS;
}

/*
 * get_mtd_info_from_label - Gets the MTD dev path from partition label
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 */
static int get_mtd_info_from_label(const char *ipmi_label, char mtd[MAX_MTD_FILE_PATH])
{
	DIR *dir;
	struct dirent *entry;
	int err = IVFRU_RET_INVALID_DEVICE_TREE;

	dir = opendir("/sys/class/mtd/");
	if (dir == NULL)
		return IVFRU_RET_IO_ERROR;

	while ((entry = readdir(dir)) != NULL) {
		char path[MAX_MTD_FILE_PATH];
		snprintf(path, sizeof(path), "/sys/class/mtd/%s/name", entry->d_name);

		FILE *name_file = fopen(path, "r");
		if (name_file != NULL) {
			char name[MAX_MTD_FILE_PATH];
			if (fgets(name, sizeof(name), name_file) != NULL) {
				name[strcspn(name, "\r\n")] = 0;
				if (strcmp(name, ipmi_label) == 0) {
					strcpy(mtd, "/dev/");
					strcat(mtd, entry->d_name);
					err = IVFRU_RET_SUCCESS;
					fclose(name_file);
					goto end;
				}
			}
			fclose(name_file);
		}
	}

end:
	closedir(dir);
	return err;
}

/*
 * ivfru_read_i2c - Reads data from I2C EEPROM given offset and size
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
static int ivfru_read_i2c(int bus, int addr, int offset, void *data, int size)
{
	char i2c_bus[20];
	snprintf(i2c_bus, sizeof(i2c_bus) - 1, "/dev/i2c-%d", bus);

	char cmd[100];
	snprintf(cmd, sizeof(cmd), "eeprog %s 0x%02x -q -f -16 -r %d:%d", i2c_bus, addr, offset, size);

	FILE* pipe = popen(cmd, "r");
	if (pipe == NULL) {
		printf("Error: failed to execute eeprog!\n");
		return IVFRU_RET_IO_ERROR;
	}
	int read = 0;
	while(read < size) {
		int ret = fread(data + read, 1, size - read, pipe);
		if(ret < 0) {
			printf("Error: failed to read eeprog output!\n");
			return IVFRU_RET_IO_ERROR;
		}
		read += ret;
	}
	pclose(pipe);

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_write_i2c - Writes data to I2C EEPROM at given offset and size
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
static int ivfru_write_i2c(int bus, int addr, int offset, void *data, int size)
{
	char i2c_bus[20];
	snprintf(i2c_bus, sizeof(i2c_bus) - 1, "/dev/i2c-%d", bus);

	char cmd[100];
	snprintf(cmd, sizeof(cmd), "eeprog %s 0x%02x -q -f -16 -w %d", i2c_bus, addr, offset);

	FILE* pipe = popen(cmd, "w");
	if (pipe == NULL) {
		printf("Error: failed to execute eeprog!\n");
		return IVFRU_RET_IO_ERROR;
	}
	if(fwrite(data, 1, size, pipe) != size) {
		printf("Error: failed to write eeprog input!\n");
		return IVFRU_RET_IO_ERROR;
	}
	pclose(pipe);

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_read_qspi - Reads data from QSPI MTD partition given offset and size.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
static int ivfru_read_qspi(char mtd_path[MAX_MTD_FILE_PATH], int offset, void *data, int size)
{
	int fd = open(mtd_path, O_RDONLY);
	if(fd < 0) {
		printf("Error: failed to open '%s'!\n", mtd_path);
		return IVFRU_RET_IO_ERROR;
	}

	if(lseek(fd, offset, SEEK_SET) != offset) {
		printf("Error: failed to seek to offset %d in '%s'!\n", offset, mtd_path);
		return IVFRU_RET_IO_ERROR;
	}

	if(read(fd, data, size) != size) {
		printf("Error: failed to read from '%s'!\n", mtd_path);
		return IVFRU_RET_IO_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_write_qspi - Writes data to a QSPI MTD partition given size
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
static int ivfru_write_qspi(const char *mtd_path, uint32_t qspi_offset, void *data, int size)
{

	// Make and write the data to a temp file
	char temp_file[] = "/tmp/temp_XXXXXX";
	int fd = mkstemp(temp_file);
	if(fd < 0) {
		printf("Error: failed to create temp file for flashcp!\n");
		return IVFRU_RET_IO_ERROR;
	}

	if(qspi_offset > 0) {
		// We cannot directly write to the middle of an MTD partition using
		// flashcp. We first have to read the existing data in the partition
		// upto the write offset and then append it with the actual data that is
		// to be written.
		char temp[qspi_offset];
		int ret = ivfru_read_qspi(mtd_path, 0, temp, qspi_offset);
		if(ret != IVFRU_RET_SUCCESS) {
			printf("Error: failed to read existing data in QSPI flash for partial block update!\n");
			return ret;
		}
		if(write(fd, temp, qspi_offset) != qspi_offset) {
			printf("Error: failed to write data to temp file for partial block update!\n");
			close(fd);
			return IVFRU_RET_IO_ERROR;
		}
	}

	if(write(fd, data, size) != size) {
		printf("Error: failed to write to temp file used for flashcp!\n");
		close(fd);
		return IVFRU_RET_IO_ERROR;
	}
	close(fd);

	// Construct and execute the flashcp command to write the data
	char cmd[100];
	snprintf(cmd, sizeof(cmd), "flashcp -v %s %s", temp_file, mtd_path);
	if (system(cmd) != 0) {
		printf("Error: failed to execute flashcp command to write to QSPI flash!\n");
		return IVFRU_RET_IO_ERROR;
	}

	if(remove(temp_file) != 0) {
		printf("Warning: failed to remove temp file %s.\n", temp_file);
	}

	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_read_from_location - Reads data from location (file path) to
 * the static buffer.
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
int ivfru_plat_read_from_location(void *location)
{
	char *path = (char *)location;
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		printf("Error: Failed to open the file: %s\n", path);
		return IVFRU_RET_IO_ERROR;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(ivfru_plat_reserve_memory(size) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to allocate memory.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	void *mem = ivfru_plat_get_buffer();
	if(!mem)
		return IVFRU_RET_MEM_ERROR;

	if(fread(mem, size, 1, file) != 1) {
		printf("Error: Failed to read data from the file: %s\n", path);
		fclose(file);
		return IVFRU_RET_IO_ERROR;
	}

	fclose(file);
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_write_to_location - Writes buffer data to location (file path)
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
int ivfru_plat_write_to_location(void *location)
{
	char *path = (char *)location;
	FILE *file = fopen(path, "wb");

	if (file == NULL) {
		printf("Error: Failed to open the file: %s\n", path);
		return IVFRU_RET_IO_ERROR;
	}

	void *mem = ivfru_plat_get_buffer();
	if(!mem)
		return IVFRU_RET_MEM_ERROR;

	int size = ivfru_plat_get_image_size();
	if(fwrite(mem, 1, size, file) != size) {
		printf("Error: Failed to write data to the file: %s\n", path);
		fclose(file);
		return IVFRU_RET_IO_ERROR;
	}

	fclose(file);
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_read_from_board - Reads IPMI data of given board ID to memory
 *
 * Returns IVFRU_RET_SUCCESS iff the data was read successfully.
 */
int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size, int quiet)
{
	uint32_t bus = -1;
	uint32_t addr = -1;
	uint32_t ipmi_offset = -1;
	uint32_t qspi_offset = 0;
	char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1];

	if (get_i2c_info_from_dt(board, &bus, &addr, &ipmi_offset) == IVFRU_RET_SUCCESS) {
		if(ivfru_read_i2c(bus, addr, ipmi_offset + offset, (void *) mem, size) != IVFRU_RET_SUCCESS)
			return IVFRU_RET_IO_ERROR;
	} else if (get_qspi_info_from_dt(board, ipmi_label, &qspi_offset) == IVFRU_RET_SUCCESS) {
		char mtd_path[MAX_MTD_FILE_PATH];
		if(get_mtd_info_from_label(ipmi_label, mtd_path) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_read_qspi(mtd_path, qspi_offset + offset, (void *) mem, size) != IVFRU_RET_SUCCESS) {
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
 * ivfru_plat_write_to_board - Writes IPMI data to given board ID from memory
 *
 * Returns IVFRU_RET_SUCCESS iff the data was written successfully.
 */
int ivfru_plat_write_to_board(enum ivfru_board board, void *mem, int size)
{
	uint32_t bus = -1;
	uint32_t addr = -1;
	uint32_t ipmi_offset = -1;
	uint32_t qspi_offset = 0;
	char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1];

	if (get_i2c_info_from_dt(board, &bus, &addr, &ipmi_offset) == IVFRU_RET_SUCCESS) {
		// Deassert write-protect gpio
		int chip, pin, active_low;
		int gpioret = get_wp_info_from_dt(board, &chip, &pin, &active_low);
		struct gpiod_line *wp_line = NULL;

		if (gpioret == IVFRU_RET_SUCCESS) {
			// Open GPIO line for write protection control
			wp_line = gpio_open(chip, pin, active_low);
			if (wp_line) {
				// Disable write protection: set opposite of active state
				gpiod_line_set_value(wp_line, 1);
			}
		}

		int ret = ivfru_write_i2c(bus, addr, ipmi_offset, (void *) mem, size);

		// Re-assert write protection and cleanup
		if (wp_line) {
			// Re-enable write protection: set to active state
			gpiod_line_set_value(wp_line, 0);
			gpio_close(wp_line);
		}

		if(ret != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to write to i2c storage!\n");
			return IVFRU_RET_IO_ERROR;
		}
	} else if (get_qspi_info_from_dt(board, ipmi_label, &qspi_offset) == IVFRU_RET_SUCCESS) {
		char mtd_path[MAX_MTD_FILE_PATH];
		if(get_mtd_info_from_label(ipmi_label, mtd_path) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_write_qspi(mtd_path, qspi_offset, (void *) mem, size) != IVFRU_RET_SUCCESS) {
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
	if(buffer_size == 0)
		return NULL;
	return image_buffer;
}

/*
 * ivfru_plat_reserve_memory - Reserves memory space for the static buffer.
 *
 * Returns IVFRU_RET_SUCCESS if memory was allocated successfully.
 */
int ivfru_plat_reserve_memory(int size)
{
	if(image_buffer == NULL) {
		image_buffer = malloc(size);
		if(!image_buffer)
			return IVFRU_RET_MEM_ERROR;
	}
	else {
		void *new_buffer = realloc(image_buffer, size);
		if(!new_buffer)
			return IVFRU_RET_MEM_ERROR;
		image_buffer = new_buffer;
	}

	buffer_size = size;
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_set_image_size - Sets the size of the IPMI FRU image in the static
 * buffer.
 *
 * Returns IVFRU_RET_SUCCESS.
 */
int ivfru_plat_set_image_size(int size)
{
	image_size = size;
	return IVFRU_RET_SUCCESS;
}

/*
 * ivfru_plat_get_image_size - Gets the size of the IPMI FRU image in the static
 * buffer.
 *
 * Returns the size of the image data.
 */
int ivfru_plat_get_image_size()
{
	return image_size;
}
