#ifndef __IVFRU_COMMON_H__
#define __IVFRU_COMMON_H__

enum ivfru_ret {
	IVFRU_RET_SUCCESS,
	IVFRU_RET_INVALID_ARGUMENT,
	IVFRU_RET_IO_ERROR,
	IVFRU_RET_MEM_ERROR,
	IVFRU_RET_INVALID_DEVICE_TREE,
	IVFRU_RET_INVALID_FRU_DATA,
	IVFRU_RET_FRU_NOT_FIXED,
	IVFRU_RET_FRU_ALREADY_FIXED,
};

enum ivfru_board {
	IVFRU_BOARD_MB,
	IVFRU_BOARD_IO,
	IVFRU_BOARD_BP,
	MAX_IVFRU_BOARD,
};

enum ivfru_tlb_type {
	IVFRU_TLB_TYPE_11,
	MAX_TLB_TYPE,
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

int ivfru_read(enum ivfru_board board, void *location);
int ivfru_write(enum ivfru_board board, void *location);
int ivfru_display(void *location);
int ivfru_fix(void *location);
int ivfru_create(void *location, const char *mfgdate, const char *product, const char *sn, const char *pn, const char *mfr);
int ivfru_add(void *location, int index, char *hex_string);
int ivfru_rm(void *location, int index);

#endif // __IVFRU_COMMON_H__
