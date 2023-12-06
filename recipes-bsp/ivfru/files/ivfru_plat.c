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

#define IPMI_DT_PATH "/proc/device-tree/iv_ipmi/"
#define BOARD_PREFIX "iv_"
#define ZYNQMP_GPIO_OFNODE "/axi/gpio@ff0a0000"
#define MAX_QSPI_PART_LABEL_LEN 100
#define MAX_MTD_FILE_PATH 256

static void *image_buffer;
static int buffer_size = 0;
static int image_size = 0;

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
 * info from the device tree.
 *
 * Returns IVFRU_RET_SUCCESS iff the info was extracted successfully.
 */
static int get_wp_info_from_dt(enum ivfru_board board, uint32_t *gpio, uint32_t *active)
{
	*gpio = -1;
	*active = -1;

	char *board_str = ivfru_board2str(board);
	if(board_str == NULL) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	const int path_len = strlen(IPMI_DT_PATH) + strlen(BOARD_PREFIX)
		+ strlen(board_str) + 1 /* slash */ + 2 /* wp */
		+ 1 /* slash */ + 8 /* i2c_gpio */
		+ 1 /* null term */;
	char path_i2c_gpio[path_len];
	char path_gpio[path_len];
	strcpy(path_gpio, IPMI_DT_PATH);
	strcat(path_gpio, BOARD_PREFIX);
	strcat(path_gpio, board_str);
	strcat(path_gpio, "/wp/");
	strcpy(path_i2c_gpio, path_gpio);
	strcat(path_gpio, "/gpio");
	strcat(path_i2c_gpio, "/i2c_gpio");

	if(access(path_i2c_gpio, R_OK) == 0) {
		int bus = -1;
		int addr = -1;
		int base = -1;

		int fd = open(path_i2c_gpio, O_RDONLY);
		if(fd < 0)
			return IVFRU_RET_IO_ERROR;

		int rlen = 0;
		rlen += read(fd, &bus, sizeof(bus));
		rlen += read(fd, &addr, sizeof(addr));
		rlen += read(fd, gpio, sizeof(*gpio));
		rlen += read(fd, active, sizeof(*active));
		if(rlen != sizeof(bus) + sizeof(addr) + sizeof(*gpio) + sizeof(*active))
			return IVFRU_RET_IO_ERROR;

		bus = ntohl(bus);
		addr = ntohl(addr);
		*gpio = ntohl(*gpio);
		*active = ntohl(*active);

		char temppath[100];
		snprintf(temppath, sizeof(temppath) - 1, "/sys/bus/i2c/devices/%d-00%02x/gpio/", bus, addr);
		DIR *dir = opendir(temppath);
		if (dir == NULL)
			return IVFRU_RET_IO_ERROR;
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			char *end;
			base = strtoul(entry->d_name + 8, &end, 10);
			if(end > entry->d_name + 8 && *end == '\0')
				break;
			base = -1;
		}
		if(base == -1)
			return IVFRU_RET_INVALID_DEVICE_TREE;

		*gpio += base;

		return IVFRU_RET_SUCCESS;
	} else if(access(path_gpio, R_OK) == 0) {
		int fd = open(path_gpio, O_RDONLY);
		if(fd < 0)
			return IVFRU_RET_IO_ERROR;

		int rlen = 0;
		rlen += read(fd, gpio, sizeof(*gpio));
		rlen += read(fd, active, sizeof(*active));
		if(rlen != sizeof(*gpio) + sizeof(*active))
			return IVFRU_RET_IO_ERROR;

		*gpio = ntohl(*gpio);
		*active = ntohl(*active);
		int base = -1;

		DIR *dir = opendir("/sys/class/gpio/");
		if (dir == NULL)
			return IVFRU_RET_IO_ERROR;
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if(strncmp(entry->d_name, "gpiochip", 8) != 0)
				continue;
			char temppath[100];
			char ofpath[1000];
			snprintf(temppath, sizeof(temppath) - 1, "/sys/class/gpio/%s/device/of_node", entry->d_name);
			int ofpathsz = readlink(temppath, ofpath, sizeof(ofpath));
			if(ofpathsz < 1)
				continue;
			ofpath[ofpathsz] = '\0';
			if(strncmp(ofpath + ofpathsz - strlen(ZYNQMP_GPIO_OFNODE), ZYNQMP_GPIO_OFNODE, strlen(ZYNQMP_GPIO_OFNODE)) != 0)
				continue;
			char *end;
			base = strtoul(entry->d_name + 8, &end, 10);
			if(end > entry->d_name + 8 && *end == '\0')
				break;
			base = -1;
		}
		if(base == -1)
			return IVFRU_RET_INVALID_DEVICE_TREE;

		*gpio += base;

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
static int get_qspi_info_from_dt(enum ivfru_board board, char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1])
{
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
static int ivfru_write_qspi(const char *mtd_path, void *data, int size)
{
	// Make and write the data to a temp file
	char temp_file[] = "/tmp/temp_XXXXXX";
	int fd = mkstemp(temp_file);
	if(fd < 0) {
		printf("Error: failed to create temp file for flashcp!\n");
		return IVFRU_RET_IO_ERROR;
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
	char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1];

	if (get_i2c_info_from_dt(board, &bus, &addr, &ipmi_offset) == IVFRU_RET_SUCCESS) {
		if(ivfru_read_i2c(bus, addr, ipmi_offset + offset, (void *) mem, size) != IVFRU_RET_SUCCESS)
			return IVFRU_RET_IO_ERROR;
	} else if (get_qspi_info_from_dt(board, ipmi_label) == IVFRU_RET_SUCCESS) {
		char mtd_path[MAX_MTD_FILE_PATH];
		if(get_mtd_info_from_label(ipmi_label, mtd_path) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_read_qspi(mtd_path, offset, (void *) mem, size) != IVFRU_RET_SUCCESS) {
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
	char ipmi_label[MAX_QSPI_PART_LABEL_LEN + 1];

	if (get_i2c_info_from_dt(board, &bus, &addr, &ipmi_offset) == IVFRU_RET_SUCCESS) {
		// Deassert write-protect gpio
		int gpio = -1;
		int active = -1;
		int gpioret = get_wp_info_from_dt(board, &gpio, &active);
		if(gpioret == IVFRU_RET_SUCCESS) {
			char cmd[100];
			sprintf(cmd, "echo %d > /sys/class/gpio/export", gpio);
			system(cmd);
			sprintf(cmd, "echo out > /sys/class/gpio/gpio%d/direction", gpio);
			system(cmd);
			sprintf(cmd, "echo %d > /sys/class/gpio/gpio%d/value", !active, gpio);
			system(cmd);
		}

		int ret = ivfru_write_i2c(bus, addr, ipmi_offset, (void *) mem, size);

		// Assert write-protect back after write
		if(gpioret == IVFRU_RET_SUCCESS) {
			char cmd[100];
			sprintf(cmd, "echo %d > /sys/class/gpio/gpio%d/value", active, gpio);
			system(cmd);
			sprintf(cmd, "echo %d > /sys/class/gpio/unexport", gpio);
			system(cmd);
		}

		if(ret != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to write to i2c storage!\n");
			return IVFRU_RET_IO_ERROR;
		}
	} else if (get_qspi_info_from_dt(board, ipmi_label) == IVFRU_RET_SUCCESS) {
		char mtd_path[MAX_MTD_FILE_PATH];
		if(get_mtd_info_from_label(ipmi_label, mtd_path) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to get a match for the IPMI qspi partition label!\n");
			return IVFRU_RET_INVALID_DEVICE_TREE;
		}
		if(ivfru_write_qspi(mtd_path, (void *) mem, size) != IVFRU_RET_SUCCESS) {
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
