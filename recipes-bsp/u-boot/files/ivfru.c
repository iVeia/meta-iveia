// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <command.h>
#include <vsprintf.h>
#include "ivfru_common.h"

static int do_ivfru_read(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;

	if(argc < 3)
		return CMD_RET_USAGE;

	enum ivfru_board board = ivfru_str2board(argv[1]);

	if(board >= MAX_IVFRU_BOARD)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[2], NULL, 16);
	ivfru_plat_set_buffer(location);

	int err = ivfru_read(board, location, 0);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	return CMD_RET_FAILURE;
}

static int do_ivfru_write(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;

	if(argc < 3)
		return CMD_RET_USAGE;

	enum ivfru_board board = ivfru_str2board(argv[1]);

	if(board >= MAX_IVFRU_BOARD)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[2], NULL, 16);
	ivfru_plat_set_buffer(location);

	int err = ivfru_write(board, location);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	return CMD_RET_FAILURE;
}

static int do_ivfru_display(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;

	if(argc < 2)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	ivfru_plat_set_buffer(location);

	ivfru_display(location);

	return CMD_RET_SUCCESS;
}

static int do_ivfru_fix(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;

	if(argc < 2)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	ivfru_plat_set_buffer(location);

	ivfru_fix(location);

	return CMD_RET_SUCCESS;
}

static int do_ivfru_create(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;
	char *mfgdate;
	char *product;
	char *sn;
	char *pn;
	char *mfr;

	if(argc < 6)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	ivfru_plat_set_buffer(location);
	mfgdate = argv[2];
	product = argv[3];
	sn = argv[4];
	pn = argv[5];

	if(argc > 6)
		mfr = argv[6];
	else
		mfr = NULL;

	int err = ivfru_create(location, mfgdate, product, sn, pn, mfr);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	else if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;

	return CMD_RET_FAILURE;
}

static int do_ivfru_xcreate(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;
	char *mfgdate;
	char *product;
	int product_len;
	char *sn;
	int sn_len;
	char *pn;
	int pn_len;
	char *mfr;
	int mfr_len;

	if(argc < 9 || argc == 10)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	mfgdate = argv[2];
	product = argv[3];
	product_len = simple_strtol(argv[4], NULL, 10);
	sn = argv[5];
	sn_len = simple_strtol(argv[6], NULL, 10);
	pn = argv[7];
	pn_len = simple_strtol(argv[8], NULL, 10);

	if(argc > 9) {
		mfr = argv[9];
		mfr_len = simple_strtol(argv[10], NULL, 10);
	} else {
		mfr = NULL;
		mfr_len = 0;
	}

	int err = ivfru_xcreate(location, mfgdate, product, product_len, sn, sn_len, pn, pn_len, mfr, mfr_len);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	else if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;

	return CMD_RET_FAILURE;
}


static int do_ivfru_add(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;
	int index;
	int len;

	if(argc < 4)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	index = simple_strtol(argv[2], NULL, 10);
	len = strlen(argv[3]) / 2;
	char bytestr[3];
	bytestr[2] = 0;

	char data[len];
	for(int i = 0; i < len; i++) {
		bytestr[0] = argv[3][i * 2];
		bytestr[1] = argv[3][i * 2 + 1];
		data[i] = simple_strtoul(bytestr, NULL, 16);
	}

	int err = ivfru_add(location, index, data, len);

	if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;
	if(err != IVFRU_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_ivfru_rm(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	void *location;
	int index;

	if(argc < 3)
		return CMD_RET_USAGE;

	location = (void *)simple_strtoul(argv[1], NULL, 16);
	index = simple_strtol(argv[2], NULL, 10);

	int err = ivfru_rm(location, index);

	if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;
	if(err != IVFRU_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static struct cmd_tbl cmd_ivfru_sub[] = {
	U_BOOT_CMD_MKENT(read,   3, 1, do_ivfru_read,   "", ""),
	U_BOOT_CMD_MKENT(write,  3, 0, do_ivfru_write,  "", ""),
	U_BOOT_CMD_MKENT(display, 2, 1, do_ivfru_display, "", ""),
	U_BOOT_CMD_MKENT(fix,    2, 0, do_ivfru_fix,    "", ""),
	U_BOOT_CMD_MKENT(create, 7, 1, do_ivfru_create, "", ""),
	U_BOOT_CMD_MKENT(xcreate, 11, 1, do_ivfru_xcreate, "", ""),
	U_BOOT_CMD_MKENT(add,    4, 0, do_ivfru_add,    "", ""),
	U_BOOT_CMD_MKENT(rm,     3, 0, do_ivfru_rm,     "", ""),
};

/*
 * Process a ivfru sub-command
 */
static int do_ivfru(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[])
{
	struct cmd_tbl *c = NULL;

	/* Strip off leading 'ivfru' command argument */
	argc--;
	argv++;

	if (argc)
		c = find_cmd_tbl(argv[0], cmd_ivfru_sub,
				 ARRAY_SIZE(cmd_ivfru_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

U_BOOT_CMD(
	ivfru, 12, 1, do_ivfru,
	"read/write IPMI FRU struct",
	""
);
