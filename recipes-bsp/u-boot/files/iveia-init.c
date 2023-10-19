// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2010 - 2020 iVeia
 * Brian Silverman <bsilverman@iveia.com>
 *
 * Read and process iVeia EEPROM IPMI-based board info.  Required to determine
 * MAC address.
 *
 */
#include <common.h>
#include <env.h>
#include <linux/ctype.h>
#include <fdtdec.h>
#include <i2c.h>
#include <spi.h>
#include <spi_flash.h>
#include <iveia_version.h>
#include <ivfru_common.h>
#include "iveia-ipmi.h"

DECLARE_GLOBAL_DATA_PTR;

/*
 * MAC address constants
 *
 * The 48-bit MAC address is generated from a board SN as follows:
 * - A board SN is always 5 alphanumeric chars
 * - The SN is converted to a number by mapping each char to 5-bits, using
 * CHARMAP.  CHARMAP is exactly 32 chars.  That is, an 'A' is 0, a '9' is 31.
 * Note that an SN (and therefore CHARMAP) excludes 0/1/I/O to make the SN more
 * readable.
 * - The numeric SN is therefore 25-bits.  The 23 lsbs are used for the MAC addr:
 *      24 lsbs of MAC = (numeric_sn & 0x7FFFFF) << 1
 * - There are two available MAC addrs per board, differing in the MAC lsb.
 * - The 24 msbs of the MAC is the iVeia OUI.
 */
#define CHARMAP             "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

#define SN_LEN              5

#define MAC_LEN             6
#define NIC_SPECIFIC_LEN    3

#define IVEIA_OUI           0x002168


static bool mb_ipmi_complete = 0;
static bool io_ipmi_complete = 0;
static bool bp_ipmi_complete = 0;


static char * class2string(IV_BOARD_CLASS class)
{
    switch (class) {
        case IV_BOARD_CLASS_MB: return IV_MB_STRING;
        case IV_BOARD_CLASS_IO: return IV_IO_STRING;
        case IV_BOARD_CLASS_BP: return IV_BP_STRING;
        default: return NULL;
    }
}

/*
 * iv_board_get_field() - return a static pointer to the given field or subfield.
 *
 * buf must be a buffer of at least MAX_KERN_CMDLINE_FIELD_LEN bytes.
 *
 * Returns field (stored in buf), or NULL on no match.
 */
static char * iv_board_get_field(char * buf, IV_BOARD_CLASS class, IV_BOARD_FIELD field,
                    IV_BOARD_SUBFIELD subfield)
{
    char * s = buf;
    char * p;
    char * pfield;
    char * board;

    board = class2string(class);
    if (!board) return NULL;

    p = env_get(board);
    if (!p) {
        s[0] = '\0';
    } else {
        strncpy(s, p, MAX_KERN_CMDLINE_FIELD_LEN);
        s[MAX_KERN_CMDLINE_FIELD_LEN - 1] = '\0';
    }

    if (field == IV_BOARD_FIELD_NONE)
        return s;

    pfield = strsep(&s, ",");
    if (field == IV_BOARD_FIELD_PN) {
        s = pfield;
        if (subfield == IV_BOARD_SUBFIELD_NONE)
            return s;

        pfield = strsep(&s, "-"); // Skip first subfield
        pfield = strsep(&s, "-");
        if (subfield == IV_BOARD_SUBFIELD_PN_ORDINAL)
            return pfield;
        pfield = strsep(&s, "-");
        if (subfield == IV_BOARD_SUBFIELD_PN_VARIANT)
            return pfield;
        pfield = strsep(&s, "-");
        if (subfield == IV_BOARD_SUBFIELD_PN_REVISION)
            return pfield;
    }
    pfield = strsep(&s, ",");
    if (field == IV_BOARD_FIELD_SN)
        return pfield;
    pfield = strsep(&s, ",");
    if (field == IV_BOARD_FIELD_NAME)
        return pfield;

    return NULL;
}

/*
 * iv_board_match() - match board type against string
 *
 * Returns non-zero on match
 */
static int iv_board_match(IV_BOARD_CLASS class, IV_BOARD_FIELD field,
                    IV_BOARD_SUBFIELD subfield, char * s)
{
    char buf[MAX_KERN_CMDLINE_FIELD_LEN];
    char * p = iv_board_get_field(buf, class, field, subfield);
    return p && (strcmp(p, s) == 0);
}

/*
 * iv_mb_board_ord_match() - match mother board ordinal against string
 *
 * Returns non-zero on match
 */
__attribute__((unused))
static int iv_mb_board_ord_match(char * s)
{
    return iv_board_match(IV_BOARD_CLASS_MB, IV_BOARD_FIELD_PN,
                    IV_BOARD_SUBFIELD_PN_ORDINAL, s);
}

/*
 * iv_io_board_ord_match() - match io board ordinal against string
 *
 * Returns non-zero on match
 */
static int iv_io_board_ord_match(char * s)
{
    return iv_board_match(IV_BOARD_CLASS_IO, IV_BOARD_FIELD_PN,
                    IV_BOARD_SUBFIELD_PN_ORDINAL, s);
}

static void strcpy_ipmi_field(char * dest, char * src, int len)
{
    int i;
    char * p;

    /*
     * Copy src to dest, and remove trailing spaces.  Dest must have enough
     * space for null termination.
     */
    memcpy(dest, src, len);
    dest[len] = '\0';
    for (i = len - 1; i >= 0; i--) {
        if (dest[i] != ' ')
            break;
        dest[i] = '\0';
    }

    /*
     * Force underscores to dashes, spaces to underscores, commas to slashes.
     * We want no whitespace to make it easy to pass via the command line (we
     * can convert the underscores back later if we want).  And comma is the
     * field separator.
     */
    for (p = dest; *p != '\0'; p++) {
        if (*p == '_') {
            *p = '-';
        } else if (*p == ' ') {
            *p = '_';
        } else if (*p == ',') {
            *p = '/';
        }
    }
}

static void print_board_info(IV_BOARD_CLASS class)
{
    char *realname;
    char buf[MAX_KERN_CMDLINE_FIELD_LEN];

    switch ( class ) {
        case IV_BOARD_CLASS_MB:
            realname = "Main Board";
            break;
        case IV_BOARD_CLASS_IO:
            realname = "IO Board";
            break;
        case IV_BOARD_CLASS_BP:
            realname = "BP Board";
            break;
        default:
            return;
            break;
    }

    printf("%s:\n", realname);
    printf("\tPart Name:     %s\n",
        iv_board_get_field(buf, class, IV_BOARD_FIELD_NAME, IV_BOARD_SUBFIELD_NONE));
    printf("\tPart Number:   %s\n",
        iv_board_get_field(buf, class, IV_BOARD_FIELD_PN, IV_BOARD_SUBFIELD_NONE));
    printf("\tSerial Number: %s\n",
        iv_board_get_field(buf, class, IV_BOARD_FIELD_SN, IV_BOARD_SUBFIELD_NONE));
}

/*
 * set_ipmi_env() - Read the IPMI struct, and env_set() based on part/serial read.
 */
static void set_ipmi_env(IV_BOARD_CLASS class, char *product_raw, int product_len, char *sn_raw, int sn_len, char *pn_raw, int pn_len, void *fdt )
{
    char pn[MAX_KERN_CMDLINE_FIELD_LEN];
    char sn[MAX_KERN_CMDLINE_FIELD_LEN];
    char name[MAX_KERN_CMDLINE_FIELD_LEN];
    char ipmi_string_buf[MAX_KERN_CMDLINE_FIELD_LEN];
    char * varname = class2string(class);
    char forced_varname[64];
    char * ipmi_string;

    /*
     * If env vars were saved (i.e. saveenv), we don't want that old info here
     * otherwise it might be wrong if the EEPROM read fails
     */
    env_set(varname, NULL);

    /*
     * If there's an env var of the form "varname_forced", then we use it
     * instead of reading the IPMI info.
     *
     * Otherwise, read and parse the IPMI info.  If unreadable, use the default
     * env var.
     */
    sprintf(forced_varname, "%s_forced", varname);
    ipmi_string = env_get(forced_varname);
    if (ipmi_string == NULL || strlen(ipmi_string) == 0) {
        strcpy_ipmi_field(pn, pn_raw, pn_len);
        strcpy_ipmi_field(sn, sn_raw, sn_len);
        strcpy_ipmi_field(name, product_raw, product_len);
        sprintf(ipmi_string_buf, "%s,%s,%s", pn, sn, name);

        ipmi_string = ipmi_string_buf;
    } else {
        printf("Using 'forced' values for board information.\n");
    }

    /*
     * Verify string seems valid (has exactly 2 commas), and is printable.
     * Convert spaces to underscores.
     */
    int commas = 0;
    int space = 0;
    int nonprintable = 0;
    char * p;
    bool invalid =0;
    for (p = ipmi_string; *p != '\0'; p++) {
        if (*p == ' ') {
            *p = '_';
            space++;
        }
        if (!isprint(*p))
            nonprintable++;
        if (*p == ',')
            commas++;
    }
    if (space > 0) {
        printf("iVeia Board string contains spaces, they've been converted "
                "to underscores.\n");
    }
    if (nonprintable > 0) {
        printf("iVeia Board string contains non-printable characters.\n");
        invalid = 1;
    }
    if (commas != 2) {
        printf("iVeia Board string '%s' is invalid (needs exactly 2 commas).\n", ipmi_string);
        invalid = 1;
    }

    if ( invalid ) { 
        ipmi_string = DEFAULT_IPMI_STRING;
        printf("ERROR: Using hardcoded defaults for %s board information.\n", varname);
    }

    env_set(varname, ipmi_string);


    char varname_ord[MAX_KERN_CMDLINE_FIELD_LEN];
    char buf[MAX_KERN_CMDLINE_FIELD_LEN];
    p = iv_board_get_field(buf, class, IV_BOARD_FIELD_PN, IV_BOARD_SUBFIELD_PN_ORDINAL);
    if (p && (strlen(p) + strlen(varname) < sizeof(varname_ord))) {
        strcpy(varname_ord, varname);
        strcat(varname_ord, "_ord");
        env_set(varname_ord, p);
    }
    
    return;
}

/*
 * Convert 5 digit alphanumeric SN to MAC address
 *
 * See top of file for conversion mapping.
 *
 * Returns 48-bit MAC address lsbs of 64-bit long, or zero on error.
 */
static unsigned long long sn_to_mac(char * sn)
{
    unsigned long long numeric_sn = 0;
    unsigned long long mac;

    if (strlen(sn) != SN_LEN) return 0;

    for (int i = 0; i < SN_LEN; i++) {
        int j;
        for (j = 0; j < sizeof(CHARMAP) - 1; j++) {
            if (toupper(sn[i]) == CHARMAP[j]) break;
        }
        if (j >= sizeof(CHARMAP) - 1) return 0;

        numeric_sn <<= SN_LEN;
        numeric_sn += j;
    }
    numeric_sn <<= 1; // Extra lsb for two macs per SN
    if (numeric_sn > 0xFFFFFF) return 0;

    mac = (((unsigned long long) IVEIA_OUI) << (8*NIC_SPECIFIC_LEN)) | numeric_sn;

    return mac;
}

static int sn_to_mac_unittests(void)
{
    int pass = 1;

    pass = pass && sn_to_mac("C6J36") == 0x0021685C4678LL;
    pass = pass && sn_to_mac("B9LYW") == 0x0021683F55A8LL;
    pass = pass && sn_to_mac("AAAAA") == 0x002168000000LL;
    pass = pass && sn_to_mac("H9999") == 0x002168FFFFFELL;
    pass = pass && sn_to_mac("JAAAA") == 0;
    pass = pass && sn_to_mac("ABCDE") == sn_to_mac("abcde");
    pass = pass && sn_to_mac("AA0AA") == 0;
    pass = pass && sn_to_mac("AA#AA") == 0;

    return pass;
}

static void set_mac_addrs(void)
{
    char buf[MAX_KERN_CMDLINE_FIELD_LEN];
    char * mb_sn;
    char * io_sn;
    unsigned long long mb_mac, io_mac;
    unsigned long long macs[4];

    mb_sn = iv_board_get_field(buf, IV_BOARD_CLASS_MB, IV_BOARD_FIELD_SN, IV_BOARD_SUBFIELD_NONE);
    mb_mac = mb_sn ? sn_to_mac(mb_sn) : 0;
    io_sn = iv_board_get_field(buf, IV_BOARD_CLASS_IO, IV_BOARD_FIELD_SN, IV_BOARD_SUBFIELD_NONE);
    io_mac = io_sn ? sn_to_mac(io_sn) : 0;
    memset(macs, 0, sizeof(macs));
    if (mb_mac && io_mac) {
        macs[0] = mb_mac;
        macs[1] = mb_mac + 1;
        macs[2] = io_mac;
        macs[3] = io_mac + 1;
    } else if (mb_mac) {
        macs[0] = mb_mac;
        macs[1] = mb_mac + 1;
    } else if (io_mac) {
        macs[0] = io_mac;
        macs[1] = io_mac + 1;
    }

    unsigned long long mac;
    uint8_t macbytes[MAC_LEN];
    for (int m = 0; m < ARRAY_SIZE(macs); m++) {
        mac = macs[m];
        if (mac == 0) continue;
        for (int i = MAC_LEN - 1; i >= 0; i--) {
            macbytes[i] = mac & 0xFF;
            mac >>= 8;
        }
        char var[32];
        if (m == 0) {
            sprintf(var, "ethaddr");
        } else {
            sprintf(var, "eth%daddr", m);
        }
        if (!env_get(var))
            eth_env_set_enetaddr(var, macbytes);
    }
}

static enum ivfru_board boardclass2board( IV_BOARD_CLASS brd_class )
{
    return brd_class - 1;
}

static int board_scan_ipmi( IV_BOARD_CLASS brd_class, void *fdt )
{
    char *product_raw;
    char *sn_raw;
    char *pn_raw;
    int product_len;
    int sn_len;
    int pn_len;

    enum ivfru_board board = boardclass2board(brd_class);
    int ivfru_len;
	ivfru_get_size_from_storage(board, &ivfru_len);

    char iv_ipmi[ivfru_len];
    if(ivfru_read(board, &iv_ipmi, 1) != IVFRU_RET_SUCCESS)
        return -EIO;

    if(ivfru_validate(&iv_ipmi, NULL, 0) != IVFRU_RET_SUCCESS)
        return -EBADMSG;

    ivfru_get_bia_predefined_fields(&iv_ipmi, &product_raw, &product_len, &sn_raw, &sn_len, &pn_raw, &pn_len);
    set_ipmi_env(brd_class, product_raw, product_len, sn_raw, sn_len, pn_raw, pn_len, fdt);
    print_board_info(brd_class);

    return 0;
}


int iv_ipmi_chosen( void *fdt )
{
    int chosen_node;
	char *varnames[] = { 
		IV_MB_STRING, IV_MB_STRING"_ord", 
		IV_IO_STRING, IV_IO_STRING"_ord", 
		IV_MB_STRING, IV_MB_STRING"_ord" };
	char *value;

	chosen_node = fdt_path_offset(fdt, "/chosen");
    if ( chosen_node <= 0 )
		return -ENODEV;

	for ( int i = 0; i < sizeof(varnames)/sizeof(varnames[0]); i++ )
	{
		value = env_get(varnames[i]);
		if ( value )
			fdt_setprop_string( fdt, chosen_node, varnames[i], value );
	}


	return 0;
}


int iv_ipmi_scan( void *fdt )
{
    if ( !mb_ipmi_complete ) {
        if ( board_scan_ipmi( IV_BOARD_CLASS_MB, fdt ) == 0 ) {
            mb_ipmi_complete = 1;
        }
    }
    if ( !io_ipmi_complete ) {
        if ( board_scan_ipmi( IV_BOARD_CLASS_IO, fdt ) == 0 ) {
            io_ipmi_complete = 1;
        }
    }
    if ( !bp_ipmi_complete ) {
        if ( board_scan_ipmi( IV_BOARD_CLASS_BP, fdt ) == 0 ) {
            bp_ipmi_complete = 1;
        }
    }

    // SET ETH MAC ADDR's
    if (sn_to_mac_unittests()) {
        set_mac_addrs();
    } else {
        printf("WARNING: MAC addr algorithm failed, MACs will be unassigned/random\n");
    }

    return 0;
}

/*
 * misc_init_r() - called during the U-Boot init sequence for misc init.
 * Called before network setup, whch is required for configuring MAC addrs.
 */
int misc_init_r(void)
{
    printf("Machine:     iVeia %s\n", IVEIA_MACHINE);
    printf("Src commit:  %s\n", IVEIA_SRC_BUILD_HASH);
	#ifdef IVEIA_META_1_LAYER
		printf("Layer (%s) commit: %s\n", IVEIA_META_1_LAYER, IVEIA_META_1_BUILD_HASH);
	#else
		printf("Layer commit: unknown\n");
	#endif
	#ifdef IVEIA_META_2_LAYER
		printf("Layer (%s) commit: %s\n", IVEIA_META_2_LAYER, IVEIA_META_2_BUILD_HASH);
	#endif
	#ifdef IVEIA_META_3_LAYER
		printf("Layer (%s) commit: %s\n", IVEIA_META_3_LAYER, IVEIA_META_3_BUILD_HASH);
	#endif
	#ifdef IVEIA_META_4_LAYER
		printf("Layer (%s) commit: %s\n", IVEIA_META_4_LAYER, IVEIA_META_4_BUILD_HASH);
	#endif
	#if IVEIA_NUM_LAYERS > 4
		#error More than four meta-iveia* layers not supported
	#endif

    iv_ipmi_scan( gd->fdt_blob );

    return 0;
}
