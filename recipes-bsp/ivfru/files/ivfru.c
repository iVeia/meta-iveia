#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ivfru_common.h"
#include "ivfru_plat.h"

enum CommandRet {
	CMD_RET_SUCCESS,
	CMD_RET_FAILURE,
	CMD_RET_USAGE,
};

struct CommandArgs {
	const char *subcommand;
	int (*do_function)(int, char**);
	int min_args;
	int max_args;
};

static int do_ivfru_read(int argc, char *argv[]);
static int do_ivfru_write(int argc, char *argv[]);
static int do_ivfru_display(int argc, char *argv[]);
static int do_ivfru_fix(int argc, char *argv[]);
static int do_ivfru_create(int argc, char *argv[]);
static int do_ivfru_xcreate(int argc, char *argv[]);
static int do_ivfru_add(int argc, char *argv[]);
static int do_ivfru_rm(int argc, char *argv[]);
static void print_usage();

static const struct CommandArgs command_args[] = {
	{"read", do_ivfru_read, 3, 3},
	{"write", do_ivfru_write, 3, 3},
	{"display", do_ivfru_display, 2, 2},
	{"fix", do_ivfru_fix, 2, 2},
	{"create", do_ivfru_create, 6, 7},
	{"xcreate", do_ivfru_xcreate, 9, 11},
	{"add", do_ivfru_add, 4, 4},
	{"rm", do_ivfru_rm, 3, 3}
};


static void print_usage() {
	printf("ivfru <command> <options> - read/write IPMI FRU struct\n"
	"\n"
	"Usage:\n"
	"ivfru read <board> <location>\n"
	"  Read FRU image from the given board storage into <location>\n"
	"ivfru write <board> <location>\n"
	"  Write FRU image from <location> to the given board storage\n"
	"ivfru display <location>\n"
	"  Display a decoded version of the FRU image at <location>\n"
	"ivfru fix <location>\n"
	"  Fix invalid FRU offsets\n"
	"ivfru create <location> <mfgdate> <product> <sn> <pn> [<mfr>]\n"
	"  Create a new FRU with common header and board area.\n"
	"  <mfgdate> - Manufacturing date in format DD-MM-YYYY.\n"
	"  <product> - Product Name field string.  Default field length 15.\n"
	"  <sn> - Serial Number field string.  Default field length 10.\n"
	"  <pn> - Part Number field string.  Default field length 16.\n"
	"  <mfr> - Mfr field string (\"iVeia\" if not given).  Default field length 10.\n"
	"  If the field string is shorter than the field len, it is space padded at end.\n"
	"  If the field string is longer than the field len, command exits with error.\n"
	"ivfru xcreate <location> <mfgdate> <product> <productlen> <sn> <snlen>\n"
	"      <pn> <pnlen> [<mfr> <mfrlen>]\n"
	"  Extended form of the ivfru create command that requires specifying the field\n"
	"  length of each field string.  Each additional argument of this command of the\n"
	"  form <*len> indicates the integer length of the previous field string.  A\n"
	"  negative length means \"no fixed size\" (the field length will be exactly the\n"
	"  length of the given field string).\n"
	"ivfru add <location> <index> <hex_string>\n"
	"  Add a custom mfg info field.\n"
	"  <index> - 0-based index of the field in the board area.  If less than 0 \n"
	"    or greater than the highest indexed field, add field to end of list.\n"
	"  <hex_string> - hex string of data in field, e.g. \"1A2B3C\"\n"
	"ivfru rm <location> <index>\n"
	"  Remove the custom mfg info field at given 0-based <index>.\n"
	"\n"
	"where common fields are:\n"
	"  <board> is one of mb, io, or bp.\n"
	"  <location> is a memory address (in U-Boot) or a file (in Linux)\n"
	"\n"
	"In U-Boot, all commands (when successful) set the filesize env variable with\n"
	"the current size of the read/written/modified FRU image at <location>.\n");
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Insufficient arguments.\n");
		print_usage();
		return CMD_RET_USAGE;
	}

	// Shift program name
	argc--;
	argv++;

	char *command = argv[0];
	int (*do_function)(int, char**);

	int index;
	for (index = 0; index < sizeof(command_args) / sizeof(command_args[0]); ++index) {
		if (strcmp(command_args[index].subcommand, command) == 0) {
			do_function = command_args[index].do_function;

			if (argc < command_args[index].min_args || argc > command_args[index].max_args) {
				printf("Invalid number of arguments for %s command.\n", command);
				print_usage();
				return CMD_RET_USAGE;
			}

			int ret = do_function(argc, argv);
			if(ret == CMD_RET_USAGE)
				print_usage();
			return ret;
		}
	}

	printf("Invalid command.\n");
	print_usage();
	return CMD_RET_USAGE;
}

static int do_ivfru_read(int argc, char *argv[])
{
	char *location;

	if(argc < 3)
		return CMD_RET_USAGE;

	enum ivfru_board board = ivfru_str2board(argv[1]);

	if(board >= MAX_IVFRU_BOARD) {
		printf("Invalid board: %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	location = argv[2];

	int err = ivfru_read(board, location, 0);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	return CMD_RET_FAILURE;
}

static int do_ivfru_write(int argc, char *argv[])
{
	char *location;

	if(argc < 3)
		return CMD_RET_USAGE;

	enum ivfru_board board = ivfru_str2board(argv[1]);

	if(board >= MAX_IVFRU_BOARD) {
		printf("Invalid board: %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	location = argv[2];

	int err = ivfru_write(board, location);

	if(err == IVFRU_RET_SUCCESS)
		return CMD_RET_SUCCESS;
	return CMD_RET_FAILURE;
}

static int do_ivfru_display(int argc, char *argv[])
{
	char *location;

	if(argc < 2)
		return CMD_RET_USAGE;

	location = argv[1];

	ivfru_display(location);

	return CMD_RET_SUCCESS;
}

static int do_ivfru_fix(int argc, char *argv[])
{
	char *location;

	if(argc < 2)
		return CMD_RET_USAGE;

	location = argv[1];

	ivfru_fix(location);

	return CMD_RET_SUCCESS;
}

static int do_ivfru_create(int argc, char *argv[])
{
	char *location;
	char *mfgdate;
	char *product;
	char *sn;
	char *pn;
	char *mfr;

	if(argc < 6)
		return CMD_RET_USAGE;

	location = argv[1];
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

static int do_ivfru_xcreate(int argc, char *argv[])
{
	char *end;
	char *location;
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

	location = argv[1];
	mfgdate = argv[2];
	product = argv[3];
	product_len = strtol(argv[4], &end, 10);
	if(end != argv[4] + strlen(argv[4])) {
		printf("Invalid productlen: %s\n", argv[4]);
		return CMD_RET_USAGE;
	}
	sn = argv[5];
	sn_len = strtol(argv[6], &end, 10);
	if(end != argv[6] + strlen(argv[6])) {
		printf("Invalid snlen: %s\n", argv[6]);
		return CMD_RET_USAGE;
	}
	pn = argv[7];
	pn_len = strtol(argv[8], &end, 10);
	if(end != argv[8] + strlen(argv[8])) {
		printf("Invalid pnlen: %s\n", argv[8]);
		return CMD_RET_USAGE;
	}

	if(argc > 9) {
		mfr = argv[9];
		mfr_len = strtol(argv[10], &end, 10);
		if(end != argv[10] + strlen(argv[10])) {
			printf("Invalid mfrlen: %s\n", argv[10]);
			return CMD_RET_USAGE;
		}
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


static int do_ivfru_add(int argc, char *argv[])
{
	char *end;
	char *location;
	int index;
	int len;

	if(argc < 4)
		return CMD_RET_USAGE;

	location = argv[1];
	index = strtol(argv[2], &end, 10);
	if(end != argv[2] + strlen(argv[2])) {
		printf("Invalid index: %s\n", argv[2]);
		return CMD_RET_USAGE;
	}
	len = strlen(argv[3]) / 2;
	char bytestr[3];
	bytestr[2] = 0;

	char data[len];
	for(int i = 0; i < len; i++) {
		bytestr[0] = argv[3][i * 2];
		bytestr[1] = argv[3][i * 2 + 1];
		data[i] = strtoul(bytestr, &end, 16);
		if(end != bytestr + 2) {
			printf("Invalid hex string: %s\n", argv[3]);
			return CMD_RET_USAGE;
		}
	}

	int err = ivfru_add(location, index, data, len);

	if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;
	if(err != IVFRU_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_ivfru_rm(int argc, char *argv[])
{
	char *end;
	char *location;
	int index;

	if(argc < 3)
		return CMD_RET_USAGE;

	location = argv[1];
	index = strtol(argv[2], &end, 10);
	if(end != argv[2] + strlen(argv[2])) {
		printf("Invalid index: %s\n", argv[2]);
		return CMD_RET_USAGE;
	}

	int err = ivfru_rm(location, index);

	if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;
	if(err != IVFRU_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}
