#include "ivfru_common.h"
#include "ivfru_plat.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MIN_YEAR 1996

const struct ivfru_common_header CH_DEFAULT = {
	.format_version = 0x01,
	.board_info_area = 0x01,
	.checksum = 0xFE,
};

static int fru_make_tl_byte(const char *data, enum ivfru_tlb_type type, unsigned char *tl)
{
	if(type != IVFRU_TLB_TYPE_11)
		return IVFRU_RET_INVALID_ARGUMENT;

	*tl = 0xC0;
	int length = strlen(data);
	if(length > 63)
		return IVFRU_RET_INVALID_ARGUMENT;
	if(length == 1)
		return IVFRU_RET_INVALID_ARGUMENT;

	*tl |= length;
	return IVFRU_RET_SUCCESS;
}

static int fru_tl_get_length(const char *address)
{
	if(*address == 0xC1)
		return 0;

	return (*address) & 0x3F;
}

static int fru_area_format_field(unsigned char *address, const char *data, unsigned char **next)
{
	unsigned char tl;

	if(fru_make_tl_byte(data, IVFRU_TLB_TYPE_11, &tl) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;

	if(data == NULL) {
		*address = 0xC1;
		*next = address + 1;
	} else {
		*address = tl;
		*next = address + 1 + strlen(data);
		memcpy(address + 1, data, strlen(data));
	}

	return IVFRU_RET_SUCCESS;
}

static int fru_area_get_field_at_address(unsigned char *address, char *data, unsigned char **next)
{
	int length = fru_tl_get_length(address);
	memcpy(data, address + 1, length);
	data[length] = 0;
	*next = address + 1 + length;
	return IVFRU_RET_SUCCESS;
}

static int fru_calculate_checksum(unsigned char *address, int size, unsigned char *checksum)
{
	int sum = 0;
	while(size--) {
		sum = (sum + *address) % 0xFF;
		address++;
	}

	sum = 0x100 - sum;
	*checksum = sum;

	return IVFRU_RET_SUCCESS;
}

static int ipmi_time_to_date(unsigned char *time, int *day, int *month, int *year)
{
	int days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	uint32_t minutes = 0;
	minutes += (*(time + 2));
	minutes <<= 8;
	minutes += (*(time + 1));
	minutes <<= 8;
	minutes += *time;
	uint32_t total_days = minutes / 1440;

	*year = 1996;
	*month = 1;
	*day = 1;
	int is_leap_year = (*year % 400 == 0 || (*year % 4 == 0 && *year % 100 != 0));

	while (total_days >= days_per_month[*month] + (is_leap_year && *month == 2)) {
		total_days -= days_per_month[*month] + (is_leap_year && *month == 2);
		(*month)++;
		if (*month > 12) {
			(*year)++;
			*month = 1;
			is_leap_year = (*year % 400 == 0 || (*year % 4 == 0 && *year % 100 != 0));
		}
	}

	*day += total_days;

	return IVFRU_RET_SUCCESS;
}

static int date_to_ipmi_time(int day, int month, int year, unsigned char *time)
{
#define MINUTES_PER_DAY 1440
	const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	uint32_t minutes = 0;

	if(year < 1996)
		return IVFRU_RET_INVALID_ARGUMENT;

	int isLeapYear = 1;
	for (int i = 1996; i < year;) {
		minutes += (isLeapYear ? 366 : 365) * MINUTES_PER_DAY;
		i++;
		isLeapYear = (i % 4 == 0 && (i % 100 != 0 || i % 400 == 0));
	}

	for (int i = 1; i < month; i++) {
		minutes += days_in_month[i] * MINUTES_PER_DAY;
	}

	if(month > 2 && isLeapYear)
		minutes += MINUTES_PER_DAY;

	minutes += (day - 1) * MINUTES_PER_DAY;

	if((minutes & 0xFFFFFF) == 0)
		return IVFRU_RET_INVALID_ARGUMENT;

	*time = minutes & 0xFF;
	time++;
	minutes >>= 8;
	*time = minutes & 0xFF;
	time++;
	minutes >>= 8;
	*time = minutes & 0xFF;

	return IVFRU_RET_SUCCESS;
}

static int get_total_length(void *location)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	int length = 0;

	if(ch->format_version != 0x01)
		return length;
	length += 8;

	if(ch->board_info_area != 0x00)
		length += bia->length * 8;

	return length;
}

static int date2dmy(const char *date, int *day, int *month, int *year)
{
	*day = 0;
	*month = 0;
	*year = 0;

	while(*date && *date >= '0' && *date <= '9') {
		*day = *day * 10 + (*date - '0');
		date++;
	}

	if(*date != '-')
		return IVFRU_RET_INVALID_ARGUMENT;
	date++;

	if(*day < 1 || *day > 31)
		return IVFRU_RET_INVALID_ARGUMENT;

	while(*date && *date >= '0' && *date <= '9') {
		*month = *month * 10 + (*date - '0');
		date++;
	}

	if(*date != '-')
		return IVFRU_RET_INVALID_ARGUMENT;
	date++;

	if(*month < 1 || *month > 12)
		return IVFRU_RET_INVALID_ARGUMENT;

	while(*date && *date >= '0' && *date <= '9') {
		*year = *year * 10 + (*date - '0');
		date++;
	}

	if(*year < MIN_YEAR)
		return IVFRU_RET_INVALID_ARGUMENT;

	int days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if(*month != 2) {
		if(*day > days_per_month[*month])
			return IVFRU_RET_INVALID_ARGUMENT;
	} else {
		int is_leap_year = (*year % 400 == 0 || (*year % 4 == 0 && *year % 100 != 0));
		if(is_leap_year && *day > 29)
			return IVFRU_RET_INVALID_ARGUMENT;
		else if(!is_leap_year && *day > 28)
			return IVFRU_RET_INVALID_ARGUMENT;
	}
	return IVFRU_RET_SUCCESS;
}

static char *ivfru_board_str[] = {
	"mb",
	"io",
	"bp",
};
_Static_assert(sizeof(ivfru_board_str) / sizeof(ivfru_board_str[0]) == MAX_IVFRU_BOARD,
			   "Board string should be defined for all boards.");

char *ivfru_board2str(enum ivfru_board board)
{
	if(board >= 0 && board < sizeof(ivfru_board_str) / sizeof(ivfru_board_str[0]))
		return ivfru_board_str[board];

	return NULL;
}

enum ivfru_board ivfru_str2board(char *str)
{
	for(int i = 0; i < MAX_IVFRU_BOARD; i++)
		if(strcmp(str, ivfru_board_str[i]) == 0)
			return i;
	return MAX_IVFRU_BOARD;
}

int ivfru_read(enum ivfru_board board, void *location)
{
	int err = IVFRU_RET_SUCCESS;
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	// Read CH + fixed length BIA fields
	err = ivfru_plat_read_from_board(board, location, 0, sizeof(*ch) + sizeof(*bia));
	printf("%d: err: %d\n", __LINE__, err);
	if(err != IVFRU_RET_SUCCESS)
		return IVFRU_RET_IO_ERROR;

	// Read remaining BIA bytes (if any)
	int rem_offset = sizeof(*ch) + sizeof(*bia);

	int rem_size = bia->length;
	// Convert stored length to size in bytes
	// (except in the case of Old Format where length is already a multiple of 8)
	if(rem_size != 0x40)
		rem_size *= 8;
	rem_size -= sizeof(*bia);

	err = ivfru_plat_read_from_board(board, location + rem_offset, rem_offset, rem_size);
	printf("%d: err: %d\n", __LINE__, err);
	if(err != IVFRU_RET_SUCCESS)
		return IVFRU_RET_IO_ERROR;

	return IVFRU_RET_SUCCESS;
}

int ivfru_write(enum ivfru_board board, void *location)
{
	return ivfru_plat_write_to_board(board, location, get_total_length(location));
}

int ivfru_display(void *location)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));
	int day, month, year;

	printf("BIA Length: 0x%02x\n", bia->length);
	printf("BIA Version: 0x%02x\n", bia->format_version);
	printf("Lang code: 0x%02x\n", bia->language_code);
	ipmi_time_to_date((unsigned char *)bia->mfg_date_time, &day, &month, &year);
	printf("Mfg Date: %02d-%02d-%04d\n", day, month, year);

	const char *fixed[] = {
		"Manufacturer",
		"Part Name",
		"Serial Number",
		"Part Number",
		"File ID Len",
	};
	unsigned char *address = (unsigned char *)bia->variable;
	for(int i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
	{
		int length = fru_tl_get_length(address);
		char data[length + 1];
		fru_area_get_field_at_address(address, data, &address);
		printf("%s:\t%s\n", fixed[i], data);
	}

	for(int i = 1; *address != 0xC1; i++)
	{
		int length = fru_tl_get_length(address);
		char data[length + 1];
		fru_area_get_field_at_address(address, data, &address);
		printf("Field %d:\t%s\n", fixed[i], data);
	}

	int bia_data_size = bia->length * 8 - 1;
	const char *checksum = ((char *) bia) + bia_data_size;
	char calc_checksum;
	fru_calculate_checksum((unsigned char *)bia, bia_data_size, &calc_checksum);
	if(*checksum == calc_checksum)
		printf("BIA is valid.\n");
	else
		printf("BIA is invalid.\n");

	if(ch->internal_use_area)
		printf("Internal Use Area exists!\n");
	if(ch->chassis_info_area)
		printf("Chassis Info Area exists!\n");
	if(ch->product_info_area)
		printf("Product Info Area exists!\n");
	if(ch->multirecord_area)
		printf("MultiRecord Area exists!\n");

	return IVFRU_RET_SUCCESS;
}

int ivfru_fix(void *location)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	if(ch->board_info_area == 0x08) {
		ch->board_info_area = 0x01;
		bia->length /= 0x08;
	} else {
		return IVFRU_RET_FRU_ALREADY_FIXED;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_create(void *location, const char *mfgdate, const char *product, const char *sn, const char *pn, const char *mfr)
{
	if (!mfgdate || !product || !sn || !pn) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	int day, month, year;
	if (date2dmy(mfgdate, &day, &month, &year) != IVFRU_RET_SUCCESS) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));
	const char *mfr_default = "iVeia";

	if(mfr == NULL)
		mfr = mfr_default;

	int total_size = sizeof(*ch);
	total_size += sizeof(*bia);
	total_size += 1 + strlen(mfr);
	total_size += 1 + strlen(product);
	total_size += 1 + strlen(sn);
	total_size += 1 + strlen(pn);
	total_size += 1; /* FRU File ID */
	total_size += 1; /* End-of-fields marker */
	total_size += 1; /* Checksum */
	int pad_len = total_size;

	/* Pad to next multiple of 8 */
	total_size = (((total_size + 7) / 8) * 8);
	pad_len = total_size - pad_len;

	if(ivfru_plat_set_memory_size(total_size) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_MEM_ERROR;

	memcpy(location, &CH_DEFAULT, sizeof(CH_DEFAULT));
	int bia_size = total_size - sizeof(*ch);
	bia->format_version = 0x01;
	bia->length = bia_size / 8;
	bia->language_code = 0x00;
	date_to_ipmi_time(day, month, year, (unsigned char *)bia->mfg_date_time);

	unsigned char *next;
	unsigned char *address = (unsigned char *)bia->variable;

	if(fru_area_format_field(address, mfr, &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	if(fru_area_format_field(address, product, &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	if(fru_area_format_field(address, sn, &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	if(fru_area_format_field(address, pn, &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	// FRU File ID
	if(fru_area_format_field(address, "", &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	// End marker
	if(fru_area_format_field(address, NULL, &next) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;
	address = next;

	memset(address, 0, pad_len);
	address += pad_len;

	fru_calculate_checksum((unsigned char *)bia, bia_size - 1, address);

	return IVFRU_RET_SUCCESS;
}

int ivfru_add(void *location, int index, char *hex_string)
{
	if (!hex_string) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_rm(void *location, int index)
{
	if (index < 0) {
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	return IVFRU_RET_SUCCESS;
}
