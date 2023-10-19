#include "ivfru_common.h"
#include "ivfru_plat.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MIN_YEAR 1996
#define FRU_END_MARKER 0xC1

const struct ivfru_common_header CH_DEFAULT = {
	.format_version = 0x01,
	.board_info_area = 0x01,
	.checksum = 0xFE,
};

const struct ivfru_common_header CH_OLD = {
	.format_version = 0x01,
	.board_info_area = 0x08,
	.checksum = 0xF7,
};

static int fru_make_tl_byte(const char *data, int length, enum ivfru_tlb_type type, char *tl)
{
	if(type != IVFRU_TLB_TYPE_11) {
		printf("Error: Invalid field type!\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	if(length > 63) {
		printf("Error: field length greater than maximum allowed(63)!\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	} else if(length == 1) {
		printf("Error: field length cannot be 1!\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	*tl = 0xC0;
	*tl |= length;
	return IVFRU_RET_SUCCESS;
}

static int fru_tl_get_length(const char *address)
{
	if(*address == FRU_END_MARKER)
		return 0;

	return (*address) & 0x3F;
}

static int fru_area_format_field(char *address, const char *data, int len, char **next)
{
	char tl;

	if(fru_make_tl_byte(data, len, IVFRU_TLB_TYPE_11, &tl) != IVFRU_RET_SUCCESS)
		return IVFRU_RET_INVALID_ARGUMENT;

	if(data == NULL) {
		*address = FRU_END_MARKER;
		len = 0;
	} else {
		*address = tl;
		memcpy(address + 1, data, len);
	}

	if(next)
		*next = address + 1 + len;

	return IVFRU_RET_SUCCESS;
}

static int fru_area_get_field_at_address(char *address, char *data, char **next)
{
	int length = fru_tl_get_length(address);
	if(data) {
		memcpy(data, address + 1, length);
		data[length] = 0;
	}
	*next = address + 1 + length;
	return IVFRU_RET_SUCCESS;
}

static int fru_calculate_checksum(char *address, int size, char *checksum)
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

static int ipmi_time_to_date(char *time, int *day, int *month, int *year)
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

static int date_to_ipmi_time(int day, int month, int year, char *time)
{
#define MINUTES_PER_DAY 1440
	const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	uint32_t minutes = 0;

	if(year < MIN_YEAR) {
		printf("Error: Invalid date. Year should be greater than or equal to 1996.\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

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

	if((minutes & 0xFFFFFF) == 0) {
		printf("Error: Invalid date/time. 01-01-1996 00:00 is unspecified in the IPMI time format.\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	*time = minutes & 0xFF;
	time++;
	minutes >>= 8;
	*time = minutes & 0xFF;
	time++;
	minutes >>= 8;
	*time = minutes & 0xFF;

	return IVFRU_RET_SUCCESS;
}

static int fru_get_total_length(void *location)
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

static int fru_is_old_format(void *location)
{
	if(memcmp(location, &CH_OLD, sizeof(CH_OLD)) != 0)
		return 0;

	return 1;
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

static void display_validation_errors(enum validation_result res)
{
	if(res & VAL_RES_CH_INVALID_FORMAT_VERSION)
		printf("Error: Invalid CH Format Version!\n");
	if(res & VAL_RES_CH_INVALID_BIA_OFFSET)
		printf("Error: Invalid BIA Offset! BIA should be placed directly after CH.\n");
	if(res & VAL_RES_CH_INVALID_CHECKSUM)
		printf("Error: Invalid CH Checksum!\n");

	if(res & VAL_RES_BIA_INVALID_FORMAT_VERSION)
		printf("Error: Invalid BIA Format Version!\n");
	if(res & VAL_RES_BIA_INVALID_LANGUAGE_CODE)
		printf("Error: Invalid BIA Language Code! Only English (0x00) is supported.\n");
	if(res & VAL_RES_BIA_PREDEF_FIELDS_MISSING)
		printf("Error: Missing pre-defined field(s) in the BIA!\n");
	if(res & VAL_RES_BIA_INVALID_FRU_FILE_ID)
		printf("Error: Invalid BIA FRU File ID field! FRU File ID field should be Empty/NULL\n");
	if(res & VAL_RES_BIA_MISSING_END_OF_FIELDS)
		printf("Error: End-of-fields marker is missing or length field is incorrect in the BIA!\n");
	if(res & VAL_RES_BIA_INVALID_PADDING)
		printf("Error: Invalid padding byte(s) in the BIA! Padding bytes should be 0x00.\n");
	if(res & VAL_RES_BIA_MISSING_CHECKSUM)
		printf("Error: BIA Checksum is missing or length field is incorrect!\n");
	if(res & VAL_RES_BIA_INCORRECT_CHECKSUM)
		printf("Error: BIA Checksum is incorrect!\n");
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

int ivfru_bia_validate(char *biaaddress, int old_format, enum validation_result *result, int ignore)
{
	enum validation_result res = VAL_RES_SUCCESS;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) biaaddress;
	int bia_length = bia->length;
	if(!old_format)
		bia_length *= 8;
	int rem_length = bia_length;

	if(rem_length < sizeof(*bia)) {
		res |= VAL_RES_BIA_PREDEF_FIELDS_MISSING;
		if(!ignore)
			goto end;
	}

	if(bia->format_version != 0x01) {
		res |= VAL_RES_BIA_INVALID_FORMAT_VERSION;
		if(!ignore)
			goto end;
	}

	if(bia->language_code != 0x00) {
		res |= VAL_RES_BIA_INVALID_LANGUAGE_CODE;
		if(!ignore)
			goto end;
	}

	char *address = bia->variable;
	int index = 0;
	rem_length -= sizeof(*bia);

	// Skip first 4 pre-defined fields
	while(*address != FRU_END_MARKER && index < 4 && rem_length > 0) {
		rem_length -= 1 + fru_tl_get_length(address);
		fru_area_get_field_at_address(address, NULL, &address);
		index++;
	}

	// Now at FRU File ID field
	if(index < 4 || rem_length < 3 || *address == FRU_END_MARKER) {
		res |= VAL_RES_BIA_PREDEF_FIELDS_MISSING;
		if(!ignore)
			goto end;
	}
	// FRU File ID length should be zero
	if(fru_tl_get_length(address) != 0) {
		res |= VAL_RES_BIA_INVALID_FRU_FILE_ID;
		if(!ignore)
			goto end;
	}

	// Custom fields
	while(*address != FRU_END_MARKER && rem_length > 0) {
		rem_length -= 1 + fru_tl_get_length(address);
		fru_area_get_field_at_address(address, NULL, &address);
	}

	if(*address != FRU_END_MARKER || rem_length < 1) {
		res |= VAL_RES_BIA_MISSING_END_OF_FIELDS;
		if(!ignore)
			goto end;
	}

	// Skip FRU_END_MARKER
	address++;
	rem_length--;

	// 0x00 padding till checksum
	while(*address == 0x00 && rem_length > 1) {
		address++;
		rem_length--;
	}
	if(rem_length > 1) {
		res |= VAL_RES_BIA_INVALID_PADDING;
		if(!ignore)
			goto end;
	}

	if(rem_length < 1) {
		res |= VAL_RES_BIA_MISSING_CHECKSUM;
		if(!ignore)
			goto end;
	}

	char checksum;
	fru_calculate_checksum((char *) bia, bia_length - 1, &checksum);
	if(checksum != *address) {
		res |= VAL_RES_BIA_INCORRECT_CHECKSUM;
		if(!ignore)
			goto end;
	}

end:
	if(result)
		*result |= res;
	if(res != VAL_RES_SUCCESS)
		return IVFRU_RET_INVALID_FRU_DATA;

	return IVFRU_RET_SUCCESS;
}

int ivfru_validate(char *location, enum validation_result *result, int ignore)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	enum validation_result res = VAL_RES_SUCCESS;
	int fru_old_format = fru_is_old_format(location);

	if(!fru_old_format) {
		if(ch->format_version != 0x01) {
			res |= VAL_RES_CH_INVALID_FORMAT_VERSION;
			if(!ignore)
				goto end;
		}

		char checksum;
		fru_calculate_checksum((char *)ch, 7, &checksum);
		if(checksum != ch->checksum) {
			res |= VAL_RES_CH_INVALID_CHECKSUM;
			if(!ignore)
				goto end;
		}

		if(ch->board_info_area != 0x01) {
			res |= VAL_RES_CH_INVALID_BIA_OFFSET;
			if(!ignore)
				goto end;
		}
	}

	if((res & VAL_RES_CH_INVALID_BIA_OFFSET) == 0)
		if(ivfru_bia_validate((char *)bia, fru_old_format, &res, ignore) != IVFRU_RET_SUCCESS && !ignore)
			goto end;

end:
	if(result)
		*result = res;
	if(res != VAL_RES_SUCCESS)
		return IVFRU_RET_INVALID_FRU_DATA;

	return IVFRU_RET_SUCCESS;
}

int ivfru_get_size_from_storage(enum ivfru_board board, int *size)
{
	int err;

	*size = 0;

	struct ivfru_common_header ch;
	err = ivfru_plat_read_from_board(board, &ch, 0, sizeof(ch), 1);
	if(err != IVFRU_RET_SUCCESS)
		return IVFRU_RET_IO_ERROR;

	*size += sizeof(ch);

	if(ch.board_info_area == 0x00)
		return IVFRU_RET_SUCCESS;

	struct ivfru_board_info_area bia;
	err = ivfru_plat_read_from_board(board, &bia, sizeof(ch), sizeof(bia), 1);
	if(err != IVFRU_RET_SUCCESS)
		return IVFRU_RET_IO_ERROR;

	*size += bia.length * 8;

	return IVFRU_RET_SUCCESS;
}

int ivfru_get_bia_predefined_fields(void *location, char **product, int *product_len, char **sn, int *sn_len, char **pn, int *pn_len)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	char *address = bia->variable;

	// Skip Board Manufacturer field
	fru_area_get_field_at_address(address, NULL, &address);

	// Product Name
	if(product && product_len) {
		*product_len = fru_tl_get_length(address);
		*product = address + 1;
	}
	fru_area_get_field_at_address(address, NULL, &address);

	// Serial Number
	if(sn && sn_len) {
		*sn_len = fru_tl_get_length(address);
		*sn = address + 1;
	}
	fru_area_get_field_at_address(address, NULL, &address);

	// Part Number
	if(pn && pn_len) {
		*pn_len = fru_tl_get_length(address);
		*pn = address + 1;
	}
	fru_area_get_field_at_address(address, NULL, &address);

	return IVFRU_RET_SUCCESS;
}

int ivfru_read(enum ivfru_board board, void *location, int quiet)
{
	int err = IVFRU_RET_SUCCESS;
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));
	int read_len = sizeof(*ch) + sizeof(*bia);

	if(ivfru_plat_reserve_memory(location, read_len) != IVFRU_RET_SUCCESS) {
		if(!quiet)
			printf("Error: Failed to reserve memory.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	// Read CH + fixed length BIA fields
	err = ivfru_plat_read_from_board(board, location, 0, read_len, quiet);
	if(err != IVFRU_RET_SUCCESS)
		goto error;

	// Read remaining BIA bytes (if any)
	int rem_offset = sizeof(*ch) + sizeof(*bia);

	int rem_size = bia->length;
	// Convert stored length to size in bytes
	// (except in the case of Old Format where length is already a multiple of 8)
	if(rem_size != 0x40)
		rem_size *= 8;
	rem_size -= sizeof(*bia);

	read_len += rem_size;
	if(ivfru_plat_reserve_memory(location, read_len) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to reserve memory.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	err = ivfru_plat_read_from_board(board, location + rem_offset, rem_offset, rem_size, quiet);
	if(err != IVFRU_RET_SUCCESS)
		goto error;

	if(!quiet) {
		if(ivfru_plat_set_image_size(location, read_len) != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to set image size.\n");
			return IVFRU_RET_MEM_ERROR;
		}
	}

	return IVFRU_RET_SUCCESS;

error:
	if(!quiet)
		printf("Error: Read failed!\n");
	return IVFRU_RET_IO_ERROR;
}

int ivfru_write(enum ivfru_board board, void *location)
{
	if(fru_is_old_format(location)) {
		printf("Error: The FRU image is in the old format. Please use the fix subcommand to convert to the new format and try again.\n");
		return IVFRU_RET_FRU_OLD_FORMAT;
	}

	if(ivfru_validate(location, NULL, 0) != IVFRU_RET_SUCCESS) {
		printf("Error: The FRU image is invalid. Please fix manually and try again.\n");
		return IVFRU_RET_INVALID_FRU_DATA;
	}

	int length = fru_get_total_length(location);
	int err = ivfru_plat_write_to_board(board, location, length);
	if(err != IVFRU_RET_SUCCESS) {
		printf("Error: Write failed!\n");
		return IVFRU_RET_IO_ERROR;
	}

	if(ivfru_plat_set_image_size(location, length) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_display(void *location)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));
	int day, month, year;

	enum validation_result valres = VAL_RES_SUCCESS;
	ivfru_validate(location, &valres, 1);
	display_validation_errors(valres);

	int length = bia->length;
	if(!fru_is_old_format(location))
		length *= 8;

	if(ivfru_plat_set_image_size(location, length) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
	}

	printf("BIA Length: 0x%02x\n", bia->length);
	printf("BIA Version: 0x%02x\n", bia->format_version);
	if(length > 2)
		printf("Lang code: 0x%02x\n", bia->language_code);
	if(length > 3) {
		ipmi_time_to_date(bia->mfg_date_time, &day, &month, &year);
		printf("Mfg Date: %02d-%02d-%04d\n", day, month, year);
	}

	length -= sizeof(*bia);

	const char *fixed[] = {
		"Manufacturer",
		"Part Name",
		"Serial Number",
		"Part Number",
	};
	char *address = bia->variable;
	for(int i = 0; length > 0 && i < sizeof(fixed) / sizeof(fixed[0]); i++)
	{
		int flen = 1 + fru_tl_get_length(address);
		length -= flen;
		char data[flen];
		fru_area_get_field_at_address(address, data, &address);
		printf("%s:\t%s\n", fixed[i], data);
	}

	if(length > 0)
	{
		int flen = fru_tl_get_length(address);
		length -= 1 + flen;
		printf("File ID Len:\t0x%02x\n", flen);
		fru_area_get_field_at_address(address, NULL, &address);
	}

	for(int i = 1; length > 0 && *address != FRU_END_MARKER; i++)
	{
		int flen = 1 + fru_tl_get_length(address);
		length -= flen;
		char data[flen];
		fru_area_get_field_at_address(address, data, &address);
		printf("Field %d:\t%s\n", i, data);
	}

	if((valres & VAL_RES_BIA_MASK) == VAL_RES_SUCCESS)
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
	if(!fru_is_old_format(location)) {
		printf("Error: The FRU image is already in the correct format. No need to fix.\n");
		return IVFRU_RET_FRU_NEW_FORMAT;
	}

	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	ch->board_info_area = 0x01;
	fru_calculate_checksum((char *)ch, sizeof(*ch) - 1, &ch->checksum);

	char *biaaddr = (char *) bia;
	int bia_len = bia->length;
	bia->length /= 0x08;
	fru_calculate_checksum(biaaddr, bia_len - 1, biaaddr + bia_len - 1);

	int length = bia_len + sizeof(*ch);
	if(ivfru_plat_set_image_size(location, length) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_create(void *location, const char *mfgdate, const char *product, const char *sn, const char *pn, const char *mfr)
{
	return ivfru_xcreate(location, mfgdate, product, 15, sn, 10, pn, 16, mfr, 10);
}

int ivfru_xcreate(void *location, const char *mfgdate, const char *product, int product_len, const char *sn, int sn_len, const char *pn, int pn_len, const char *mfr, int mfr_len)
{
	if (!mfgdate || !product || !sn || !pn) {
		printf("Error: Please provide all the arguments!\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	int day, month, year;
	if (date2dmy(mfgdate, &day, &month, &year) != IVFRU_RET_SUCCESS) {
		printf("Error: Invalid manufacturing date input!\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	const char *mfr_default = "iVeia";
	if(mfr == NULL) {
		mfr = mfr_default;
		mfr_len = 10;
	}

	// Pre-defined fields
	const char *const field_str[] = {
		mfr,
		product,
		sn,
		pn,
		"", /* FRU File ID */
		NULL, /* End marker */
	};

	// Provided lengths
	int field_len[] = {
		mfr_len,
		product_len,
		sn_len,
		pn_len,
		0,
		0,
	};
	const int num_fixed_fields = sizeof(field_str) / sizeof(field_str[0]);

	for(int i = 0; i < num_fixed_fields; i++) {
		if(field_str[i]) {
			int slen = strlen(field_str[i]);
			if(field_len[i] < 0) {
				field_len[i] = slen;
			} else if(slen > field_len[i]) {
				printf("Error: Length of field data greater is than expected length!\n");
				return IVFRU_RET_INVALID_ARGUMENT;
			}
		} else {
			field_len[i] = 0;
		}
	}

	int total_size = sizeof(*ch);
	total_size += sizeof(*bia);

	/* Fixed fields + End marker */
	for(int i = 0; i < num_fixed_fields; i++)
		total_size += 1 + field_len[i];

	total_size += 1; /* Checksum */

	/* Pad to next multiple of 8 */
	int pad_len = total_size;
	total_size = (((total_size + 7) / 8) * 8);
	pad_len = total_size - pad_len;

	if(ivfru_plat_reserve_memory(location, total_size) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to reserve memory.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	memcpy(location, &CH_DEFAULT, sizeof(CH_DEFAULT));
	int bia_len = total_size - sizeof(*ch);
	bia->format_version = 0x01;
	bia->length = bia_len / 8;
	bia->language_code = 0x00;
	date_to_ipmi_time(day, month, year, bia->mfg_date_time);

	char *address = bia->variable;

	for(int i = 0; i < num_fixed_fields; i++) {
		int ret;
		if(field_len[i] > 0) {
			char field[field_len[i]];
			memset(field, ' ', field_len[i]);
			memcpy(field, field_str[i], strlen(field_str[i]));
			ret = fru_area_format_field(address, field, field_len[i], &address);
		} else {
			ret = fru_area_format_field(address, field_str[i], 0, &address);
		}
		if(ret != IVFRU_RET_SUCCESS) {
			printf("Error: Failed to add field.\n");
			return IVFRU_RET_INVALID_ARGUMENT;
		}
	}

	memset(address, 0, pad_len);
	address += pad_len;

	fru_calculate_checksum((char *) bia, bia_len - 1, address);

	if(ivfru_plat_set_image_size(location, total_size) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_add(void *location, int index, char *data, int len)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	if(fru_is_old_format(location)) {
		printf("Error: The FRU image is in the old format. Please use the fix subcommand to convert to the new format and try again.\n");
		return IVFRU_RET_FRU_OLD_FORMAT;
	}

	if(ivfru_validate(location, NULL, 0) != IVFRU_RET_SUCCESS) {
		printf("Error: The FRU image is invalid. Please fix manually and try again.\n");
		return IVFRU_RET_INVALID_FRU_DATA;
	}

	// Format field in a temp variable.
	// Return without modifications on error.
	const int field_len = len + 1;
	char field[field_len];
	if(fru_area_format_field(field, data, len, NULL) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to add field.\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	char *address = bia->variable;

	// Get to the end of the 5 pre-defined fields.
	for(int i = 0; i < 5; i++)
		fru_area_get_field_at_address(address, NULL, &address);

	// Go to i-th custom field.
	while(*address != FRU_END_MARKER && index > 0) {
		fru_area_get_field_at_address(address, NULL, &address);
		index--;
	}

	// Find end.
	char *end = address;
	while(*end != FRU_END_MARKER)
		fru_area_get_field_at_address(end, NULL, &end);
	end++;

	// Set new memory size
	int bia_len = 0;
	bia_len += (end - ((char *)bia));
	bia_len += field_len;
	bia_len += 1; /* checksum */
	int pad_len = bia_len;
	bia_len = ((bia_len + 7) / 8) * 8;
	if(ivfru_plat_reserve_memory(location, sizeof(*ch) + bia_len) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to reserve memory.\n");
		return IVFRU_RET_MEM_ERROR;
	}
	pad_len = bia_len - pad_len;

	// Handle negative index.
	if(index < 0)
		address = end - 1;

	// Make room for the new field.
	char *temp = end - 1;
	while(temp >= address) {
		temp[field_len] = *temp;
		temp--;
	}

	// Copy new field bytes from the temp variable.
	memcpy(address, &field, field_len);

	// Fix padding, length, checksum
	end += field_len;
	memset(end, 0, pad_len);
	end += pad_len;
	bia->length = bia_len / 8;
	fru_calculate_checksum((char *) bia, bia_len - 1, end);

	int total_size = bia_len + sizeof(*ch);
	if(ivfru_plat_set_image_size(location, total_size) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}

int ivfru_rm(void *location, int index)
{
	struct ivfru_common_header *ch = (struct ivfru_common_header *) location;
	struct ivfru_board_info_area *bia = (struct ivfru_board_info_area *) (location + sizeof(*ch));

	if(fru_is_old_format(location)) {
		printf("Error: The FRU image is in the old format. Please use the fix subcommand to convert to the new format and try again.\n");
		return IVFRU_RET_FRU_OLD_FORMAT;
	}

	if(ivfru_validate(location, NULL, 0) != IVFRU_RET_SUCCESS) {
		printf("Error: The FRU image is invalid. Please fix manually and try again.\n");
		return IVFRU_RET_INVALID_FRU_DATA;
	}

	char *address = bia->variable;

	// Get to the end of the 5 pre-defined fields.
	for(int i = 0; i < 5; i++)
		fru_area_get_field_at_address(address, NULL, &address);

	// Go to i-th custom field.
	while(*address != FRU_END_MARKER && index > 0) {
		fru_area_get_field_at_address(address, NULL, &address);
		index--;
	}

	if(index != 0) {
		printf("Error: Failed get the field at the specified index.\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	if(*address == FRU_END_MARKER) {
		printf("Error: Failed get the field at the specified index.\n");
		return IVFRU_RET_INVALID_ARGUMENT;
	}

	int rlen = fru_tl_get_length(address) + 1;

	// Find end.
	char *end = address;
	while(*end != FRU_END_MARKER)
		fru_area_get_field_at_address(end, NULL, &end);
	end++;

	// Remove field.
	memcpy(address, address + rlen, end - address - rlen);
	end -= rlen;

	// Fix padding, length, checksum
	int bia_len = end - ((char *)bia);
	bia_len++;
	int pad_len = bia_len;
	bia_len = ((bia_len + 7) / 8) * 8;
	pad_len = bia_len - pad_len;
	memset(end, 0, pad_len);
	end += pad_len;
	bia->length = bia_len / 8;
	fru_calculate_checksum((char *) bia, bia_len - 1, end);

	int total_size = bia_len + sizeof(*ch);
	if(ivfru_plat_set_image_size(location, total_size) != IVFRU_RET_SUCCESS) {
		printf("Error: Failed to set image size.\n");
		return IVFRU_RET_MEM_ERROR;
	}

	return IVFRU_RET_SUCCESS;
}
