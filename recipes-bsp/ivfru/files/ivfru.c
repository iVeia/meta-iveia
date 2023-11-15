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
	const char *usage;
	const char *description;
};

static int do_ivfru_read(int argc, char *argv[]);
static int do_ivfru_write(int argc, char *argv[]);
static int do_ivfru_display(int argc, char *argv[]);
static int do_ivfru_fix(int argc, char *argv[]);
static int do_ivfru_create(int argc, char *argv[]);
static int do_ivfru_xcreate(int argc, char *argv[]);
static int do_ivfru_add(int argc, char *argv[]);
static int do_ivfru_rm(int argc, char *argv[]);
static void print_usage(int index);

static const struct CommandArgs command_args[] = {
	{"read", do_ivfru_read, 3, 3, "read <board> <location>", "Read FRU image from the given board storage into <location>"},
	{"write", do_ivfru_write, 3, 3, "write <board> <location>", "Write FRU image from <location> to the given board storage"},
	{"display", do_ivfru_display, 2, 2, "display <location>", "Display a decoded version of the FRU image at <location>"},
	{"fix", do_ivfru_fix, 2, 2, "fix <location>", "Fix invalid FRU offsets"},
	{"create", do_ivfru_create, 6, 7, "create <location> <mfgdate> <product> <sn> <pn> [<mfr>]", "Create a new FRU with common header and board area."},
	{"xcreate", do_ivfru_xcreate, 9, 11, "xcreate <location> <mfgdate> <product> <productlen> <sn> <snlen> <pn> <pnlen> [<mfr> <mfrlen>]", "Extended form of the create command that requires specifying the field length of each field string."},
	{"add", do_ivfru_add, 4, 4, "add <location> <index> <hex_string>", "Add a custom mfg info field."},
	{"rm", do_ivfru_rm, 3, 3, "rm <location> <index>", "Remove the custom mfg info field at given 0-based <index>."}
};

static void print_usage(int index) {
	if (index >= 0 && index < sizeof(command_args) / sizeof(command_args[0])) {
		printf("Usage:\n\tivfru %s\n\t\t%s\n", command_args[index].usage, command_args[index].description);
	} else {
		printf("Usage:\n\n");
		for (int i = 0; i < sizeof(command_args) / sizeof(command_args[0]); ++i) {
			printf("\tivfru %s\n\t\t%s\n\n", command_args[i].usage, command_args[i].description);
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Insufficient arguments.\n");
		print_usage(-1);
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
				print_usage(index);
				return CMD_RET_USAGE;
			}

			return do_function(argc, argv);
		}
	}

	printf("Invalid command.\n");
	print_usage(-1);
	return CMD_RET_USAGE;
}

static int do_ivfru_read(int argc, char *argv[])
{
	char *location;

	if(argc < 3)
		return CMD_RET_USAGE;

	enum ivfru_board board = ivfru_str2board(argv[1]);

	if(board >= MAX_IVFRU_BOARD)
		return CMD_RET_USAGE;

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

	if(board >= MAX_IVFRU_BOARD)
		return CMD_RET_USAGE;

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
	product_len = strtol(argv[4], NULL, 10);
	sn = argv[5];
	sn_len = strtol(argv[6], NULL, 10);
	pn = argv[7];
	pn_len = strtol(argv[8], NULL, 10);

	if(argc > 9) {
		mfr = argv[9];
		mfr_len = strtol(argv[10], NULL, 10);
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
	char *location;
	int index;
	int len;

	if(argc < 4)
		return CMD_RET_USAGE;

	location = argv[1];
	index = strtol(argv[2], NULL, 10);
	len = strlen(argv[3]) / 2;
	char bytestr[3];
	bytestr[2] = 0;

	char data[len];
	for(int i = 0; i < len; i++) {
		bytestr[0] = argv[3][i * 2];
		bytestr[1] = argv[3][i * 2 + 1];
		data[i] = strtoul(bytestr, NULL, 16);
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
	char *location;
	int index;

	if(argc < 3)
		return CMD_RET_USAGE;

	location = argv[1];
	index = strtol(argv[2], NULL, 10);

	int err = ivfru_rm(location, index);

	if(err == IVFRU_RET_INVALID_ARGUMENT)
		return CMD_RET_USAGE;
	if(err != IVFRU_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}
