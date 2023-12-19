#ifndef __IVFRU_COMMON_H__
#define __IVFRU_COMMON_H__

enum ivfru_ret {
	IVFRU_RET_SUCCESS,
	IVFRU_RET_INVALID_ARGUMENT,
	IVFRU_RET_IO_ERROR,
	IVFRU_RET_MEM_ERROR,
	IVFRU_RET_INVALID_DEVICE_TREE,
	IVFRU_RET_INVALID_FRU_DATA,
	IVFRU_RET_FRU_OLD_FORMAT,
	IVFRU_RET_FRU_NEW_FORMAT,
};

enum ivfru_board {
	IVFRU_BOARD_MB,
	IVFRU_BOARD_IO,
	IVFRU_BOARD_BP,
	MAX_IVFRU_BOARD,
};

enum ivfru_tlb_type {
	IVFRU_TLB_TYPE_UNSPECIFIED,
	IVFRU_TLB_TYPE_BCD_PLUS,
	IVFRU_TLB_TYPE_6BIT_ASCII_PACKED,
	IVFRU_TLB_TYPE_11,
	MAX_TLB_TYPE,
};

enum validation_result {
	VAL_RES_SUCCESS = 0x0000,

	VAL_RES_CH_INVALID_FORMAT_VERSION = 0x0001,
	VAL_RES_CH_INVALID_BIA_OFFSET = 0x0002,
	VAL_RES_CH_INVALID_CHECKSUM = 0x0004,
	VAL_RES_CH_MASK = 0x0007,

	VAL_RES_BIA_INVALID_FORMAT_VERSION = 0x0008,
	VAL_RES_BIA_INVALID_LANGUAGE_CODE = 0x0010,
	VAL_RES_BIA_PREDEF_FIELDS_MISSING = 0x0020,
	VAL_RES_BIA_INVALID_FRU_FILE_ID = 0x0040,
	VAL_RES_BIA_MISSING_END_OF_FIELDS = 0x0080,
	VAL_RES_BIA_INVALID_PADDING = 0x0100,
	VAL_RES_BIA_MISSING_CHECKSUM = 0x0200,
	VAL_RES_BIA_INCORRECT_CHECKSUM = 0x0400,
	VAL_RES_BIA_MASK = 0x4F8,
};

struct ivfru_common_header {
	char format_version;
	char internal_use_area;
	char chassis_info_area;
	char board_info_area;
	char product_info_area;
	char multirecord_area;
	char pad;
	char checksum;
};

struct ivfru_board_info_area {
	char format_version;
	char length;
	char language_code;
	char mfg_date_time[3];
	char variable[];
};

char *ivfru_board2str(enum ivfru_board board);
enum ivfru_board ivfru_str2board(char *str);

int ivfru_validate(char *location, enum validation_result *result, int ignore);
int ivfru_get_size_from_storage(enum ivfru_board board, int *size);
int ivfru_get_bia_predefined_fields(void *location, char **product, int *product_len, char **sn, int *sn_len, char **pn, int *pn_len);

int ivfru_read(enum ivfru_board board, void *location, int quiet);
int ivfru_write(enum ivfru_board board, void *location);
int ivfru_display(void *location);
int ivfru_fix(void *location);
int ivfru_create(void *location, const char *mfgdate, const char *product, const char *sn, const char *pn, const char *mfr);
int ivfru_xcreate(void *location, const char *mfgdate, const char *product, int product_len, const char *sn, int sn_len, const char *pn, int pn_len, const char *mfr, int mfr_len);
int ivfru_add(void *location, int index, char *data, int len);
int ivfru_rm(void *location, int index);

#define IVFRU_HELP_TEXT \
	"read <board> <location>\n" \
	"    Read FRU image from the given board storage into <location>\n" \
	"ivfru write <board> <location>\n" \
	"    Write FRU image from <location> to the given board storage\n" \
	"ivfru display <location>\n" \
	"    Display a decoded version of the FRU image at <location>\n" \
	"ivfru fix <location>\n" \
	"    Fix invalid FRU offsets\n" \
	"ivfru create <location> <mfgdate> <product> <sn> <pn> [<mfr>]\n" \
	"    Create a new FRU with common header and board area.\n" \
	"    <mfgdate> - Manufacturing date in format DD-MM-YYYY.\n" \
	"    <product> - Product Name field string.  Default field length 15.\n" \
	"    <sn> - Serial Number field string.  Default field length 10.\n" \
	"    <pn> - Part Number field string.  Default field length 16.\n" \
	"    <mfr> - Mfr field string (\"iVeia\" if not given).  Default field len 10.\n" \
	"    If the field string is shorter than the field len, spaces padded at end.\n" \
	"    If the field string is longer than the field len, command exits with error.\n" \
	"ivfru xcreate <location> <mfgdate> <product> <productlen> <sn> <snlen>\n" \
	"        <pn> <pnlen> [<mfr> <mfrlen>]\n" \
	"    Extended form of the ivfru create command that requires specifying the\n" \
	"    field length of each field string.  Each additional argument of this\n" \
	"    command of the form <*len> indicates the integer length of the previous\n" \
	"    field string.  A negative length means \"no fixed size\" (the field length\n" \
	"    will be exactly the length of the given field string).\n" \
	"ivfru add <location> <index> <hex_string>\n" \
	"    Add a custom mfg info field.\n" \
	"    <index> - 0-based index of the field in the board area.  If less than 0\n" \
	"        or greater than the highest indexed field, add field to end of list.\n" \
	"    <hex_string> - hex string of data in field, e.g. \"1A2B3C\"\n" \
	"ivfru rm <location> <index>\n" \
	"    Remove the custom mfg info field at given 0-based <index>.\n" \
	"\n" \
	"where common fields are:\n" \
	"    <board> is one of mb, io, or bp.\n" \
	"    <location> is a memory address (in U-Boot) or a file (in Linux)\n" \
	"\n" \
	"In U-Boot, all commands (when successful) set the filesize env variable with\n" \
	"the current size of the read/written/modified FRU image at <location>."

#endif // __IVFRU_COMMON_H__
