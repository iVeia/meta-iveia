// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2010 - 2020 iVeia
 * Brian Silverman <bsilverman@iveia.com>
 *
 * Read and process iVeia EEPROM IPMI-based board info.  Required to determine
 * MAC address.
 *
 * Note: currently uses CONFIG_DM_I2C_COMPAT for accessing I2C bus - this
 * method is deprecated.
 */
#include <common.h>
#include <linux/ctype.h>
#include <i2c.h>

typedef enum {
    IV_BOARD_CLASS_NONE,
    IV_BOARD_CLASS_MB,
    IV_BOARD_CLASS_IO,
    IV_BOARD_CLASS_BP,
} IV_BOARD_CLASS;

typedef enum {
    IV_BOARD_FIELD_NONE,
    IV_BOARD_FIELD_PN,
    IV_BOARD_FIELD_SN,
    IV_BOARD_FIELD_NAME,
} IV_BOARD_FIELD;

typedef enum {
    IV_BOARD_SUBFIELD_NONE,
    IV_BOARD_SUBFIELD_PN_ORDINAL,
    IV_BOARD_SUBFIELD_PN_VARIANT,
    IV_BOARD_SUBFIELD_PN_REVISION,
} IV_BOARD_SUBFIELD;

#define IV_MB_I2C_BUS       1
#define IV_IO_I2C_BUS       0
#define IV_BP_I2C_BUS       0
#define IV_MB_EEPROM_ADDR   0x52
#define IV_IO_EEPROM_ADDR   0x53
#define IV_BP_EEPROM_ADDR   0x54

#define MAX_KERN_CMDLINE_FIELD_LEN 64
#define DEFAULT_IPMI_STRING "none,none,Unknown"
#define IV_MB_STRING "iv_mb"
#define IV_IO_STRING "iv_io"
#define IV_BP_STRING "iv_bp"

/*
 * iVeia IPMI board struct
 */
struct iv_common_header {
    char format_version;
    char internal_use_area;
    char chassis_info_area;
    char board_info_area;
    char product_info_area;
    char multirecord_area;
    char pad;
    char checksum;
};

/*
 * The board area is treated as a fixed structure, even though it does follow
 * the IPMI convention that the fields have a dynamic length.  The lengths can
 * be checked for verification purposes, but otherwise can be safely ignored.
 */
struct iv_board_area {
    char format_version;
    char length;
    char language_code;
    char mfg_date_time[3];
    char board_manufacturer_len;
    char board_manufacturer[10];
    char board_product_name_len;
    char board_product_name[16];
    char board_serial_number_len;
    char board_serial_number[10];
    char board_part_number_len;
    char board_part_number[15];
    char fru_file_id;
    char end_of_fields_marker;
    char checksum;
};

struct iv_ipmi {
    struct iv_common_header common_header;
    struct iv_board_area board_area;
};

/*
 * Backplanes - Some carriers support backplanes.  Only read the backplane
 * EEPROM if for ORDs of the carriers listed below.
 */
char * boards_with_backplanes[] = {
};

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

    if (class == IV_BOARD_CLASS_MB)
        board = IV_MB_STRING;
    else if (class == IV_BOARD_CLASS_IO)
        board = IV_IO_STRING;
    else if (class == IV_BOARD_CLASS_BP)
        board = IV_BP_STRING;
    else
        return NULL;

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

/*
 * read_ipmi() - Read a specific IPMI struct on a EEPROM.
 */
static int read_ipmi(int bus, int addr, struct iv_ipmi * p_iv_ipmi)
{
    unsigned char * p;
    unsigned char sum;
    int i;
    int ret = -1;
    int err;
    unsigned int oldbus;

    oldbus = i2c_get_bus_num();
    i2c_set_bus_num(bus);

    err = i2c_read(addr, 0, 2, (uchar *) p_iv_ipmi, sizeof(*p_iv_ipmi));
    if (err != 0)
        goto read_ipmi_exit;

    /*
     * Validate checksums
     */
    p = (unsigned char *) &p_iv_ipmi->common_header;
    sum = 0;
    for (i = 0; i < sizeof(p_iv_ipmi->common_header); i++) {
        sum += p[i];
    }
    if (sum != 0)
        goto read_ipmi_exit;
    p = (unsigned char *) &p_iv_ipmi->board_area;
    sum = 0;
    for (i = 0; i < sizeof(p_iv_ipmi->board_area); i++) {
        sum += p[i];
    }
    if (sum != 0)
        goto read_ipmi_exit;

    /*
     * Validate board area format version.
     */
    if (p_iv_ipmi->board_area.format_version != 1)
        goto read_ipmi_exit;

    ret = 0;

read_ipmi_exit:
    i2c_set_bus_num(oldbus);

    return ret;
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

static void print_board_info(IV_BOARD_CLASS class, char * realname)
{
    char buf[MAX_KERN_CMDLINE_FIELD_LEN];
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
static void set_ipmi_env(int bus, int addr, IV_BOARD_CLASS class)
{
    char pn[MAX_KERN_CMDLINE_FIELD_LEN];
    char sn[MAX_KERN_CMDLINE_FIELD_LEN];
    char name[MAX_KERN_CMDLINE_FIELD_LEN];
    char ipmi_string_buf[MAX_KERN_CMDLINE_FIELD_LEN];
    char * varname = class2string(class);
    char extended_varname[64];
    struct iv_ipmi iv_ipmi;
    char * ipmi_string;
    int ret;

    /*
     * If there's an env var of the form "varname_forced", then we use it
     * instead of reading the IPMI info.
     *
     * Otherwise, read and parse the IPMI info.  If unreadable, use the default
     * env var.
     */
    sprintf(extended_varname, "%s_forced", varname);
    ipmi_string = env_get(extended_varname);
    if (ipmi_string == NULL || strlen(ipmi_string) == 0) {
        ret = read_ipmi(bus, addr, &iv_ipmi);
        if (ret < 0) {

            sprintf(extended_varname, "%s_default", varname);
            ipmi_string = env_get(extended_varname);
            if (ipmi_string == NULL || strlen(ipmi_string) == 0) {
                printf("No default iVeia Board env var exists.  "
                        "Using hardcoded defaults.\n");
                ipmi_string = DEFAULT_IPMI_STRING;
            } else {
                printf("Cannot read %s IPMI EEPROM, "
                        "using defaults environment defaults.\n", varname);
            }
        } else {
            strcpy_ipmi_field(pn, iv_ipmi.board_area.board_part_number,
                sizeof(iv_ipmi.board_area.board_part_number));
            strcpy_ipmi_field(sn, iv_ipmi.board_area.board_serial_number,
                sizeof(iv_ipmi.board_area.board_serial_number));
            strcpy_ipmi_field(name, iv_ipmi.board_area.board_product_name,
                sizeof(iv_ipmi.board_area.board_product_name));
            sprintf(ipmi_string_buf, "%s,%s,%s", pn, sn, name);

            ipmi_string = ipmi_string_buf;
        }
    }

    /*
     * Verify string seems valid (has exactly 2 commas), and is printable.
     * Convert spaces to underscores.
     */
    {
        int commas = 0;
        int space = 0;
        int nonprintable = 0;
        char * p;
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
            printf("iVeia Board string contains non-printable characters.  "
                    "Using hardcoded defaults.\n");
            ipmi_string = DEFAULT_IPMI_STRING;
        }
        if (commas != 2) {
            printf("iVeia Board string '%s' is invalid.  "
                    "Using hardcoded defaults.\n", ipmi_string);
            ipmi_string = DEFAULT_IPMI_STRING;
        }
    }

    env_set(varname, ipmi_string);
    {
        char varname_ord[MAX_KERN_CMDLINE_FIELD_LEN];
        char buf[MAX_KERN_CMDLINE_FIELD_LEN];
        char * p = iv_board_get_field(buf, class, IV_BOARD_FIELD_PN, IV_BOARD_SUBFIELD_PN_ORDINAL);
        if (p && (strlen(p) + strlen(varname) < sizeof(varname_ord))) {
            strcpy(varname_ord, varname);
            strcat(varname_ord, "_ord");
            env_set(varname_ord, p);
        }
    }
}

/*
 * board_scan() - Scan EEPROMs for connected boards.
 */
static void board_scan(void)
{
    int i;

    set_ipmi_env(IV_MB_I2C_BUS, IV_MB_EEPROM_ADDR, IV_BOARD_CLASS_MB);
    print_board_info(IV_BOARD_CLASS_MB, "Main Board");

    set_ipmi_env(IV_IO_I2C_BUS, IV_IO_EEPROM_ADDR, IV_BOARD_CLASS_IO);
    print_board_info(IV_BOARD_CLASS_IO, "IO Board");

    for (i = 0; i < ARRAY_SIZE(boards_with_backplanes); i++) {
        if (iv_io_board_ord_match(boards_with_backplanes[i])) {
            set_ipmi_env(IV_BP_I2C_BUS, IV_BP_EEPROM_ADDR, IV_BOARD_CLASS_BP);
            print_board_info(IV_BOARD_CLASS_BP, "Backplane");
        }
    }
}

/*
 * misc_init_r() - called during the U-Boot init sequence for misc init.
 * Called before network setup, whch is required for configuring MAC addrs.
 */
int misc_init_r(void)
{
    i2c_init(100000, 0);
    board_scan();

    return 0;
}
