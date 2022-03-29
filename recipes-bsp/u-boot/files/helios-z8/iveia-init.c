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

/*
 * read_ipmi_i2c() - Read a specific IPMI struct on a I2C EEPROM.
 */
static int read_ipmi_i2c(int bus, int addr, int offset, struct iv_ipmi * p_iv_ipmi)
{
    int err;
	struct udevice *dev;

	err = i2c_get_chip_for_busnum(bus, addr, 2, &dev);
    if (err != 0)
	{
		printf("%s() - i2c_get_chip_for_busnum ERR %d\n", __func__, err);
        return err;
	}

    err = dm_i2c_read(dev, 0, (uchar *) p_iv_ipmi, sizeof(*p_iv_ipmi));
    if (err != 0)
	{
		printf("%s() - dm_i2c_read ERR %d\n", __func__, err);
        return err;
	}

    return err;
}

/*
 * read_ipmi_qspi() - Read a specific IPMI struct on QSPI 
 */
static int read_ipmi_qspi(int bus, int cs, int offset, struct iv_ipmi * p_iv_ipmi)
{
    int err;
    struct spi_flash *board_flash;
    struct udevice *dev;

    /* speed and mode will be read from DT */
    err = spi_flash_probe_bus_cs(bus, cs, 3000000, 0, &dev);
    if (err) {
	    printf("!spi_flash_probe_bus_cs() failed");
	    return -1;
    }

    board_flash = dev_get_uclass_priv(dev);

    err = spi_flash_read(board_flash, offset, sizeof(*p_iv_ipmi), (uchar *) p_iv_ipmi);
    if (err)
        printf("%s() - spi_flash_read ERR %d\n", __func__, err);

    return err;
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
static void set_ipmi_env(IV_BOARD_CLASS class, struct iv_ipmi *iv_ipmi, void *fdt )
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
        strcpy_ipmi_field(pn, iv_ipmi->board_area.board_part_number,
            sizeof(iv_ipmi->board_area.board_part_number));
        strcpy_ipmi_field(sn, iv_ipmi->board_area.board_serial_number,
            sizeof(iv_ipmi->board_area.board_serial_number));
        strcpy_ipmi_field(name, iv_ipmi->board_area.board_product_name,
            sizeof(iv_ipmi->board_area.board_product_name));
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


static int board_scan_ipmi( IV_BOARD_CLASS brd_class, void *fdt )
{
    int ipmi_node, class_node;
    int bus = -1, addr = 0, offset = 0; 
	const fdt32_t *val;
    struct iv_ipmi iv_ipmi;
    char *class_node_name;
    
	ipmi_node = fdt_node_offset_by_compatible(fdt, 0, "iv,ipmi");
    if ( ipmi_node < 0 )
    {
        return -ENOENT;
    }

    class_node_name = class2string( brd_class );

    class_node = fdt_subnode_offset(fdt, ipmi_node, class_node_name);
    if (class_node < 0) {
        //printf("IV IPMI: %s DT NODE NOT FOUND\n", class_node_name);
        return -ENXIO;
    }

    if ( (val = fdt_getprop(fdt, class_node, "i2c", NULL) )) {
        bus = fdt32_to_cpu(*(val + 0));
        addr = fdt32_to_cpu(*(val + 1));
        offset = fdt32_to_cpu(*(val + 2));
        read_ipmi_i2c(bus, addr, offset, &iv_ipmi);
    } else if ( (val = fdt_getprop(fdt, class_node, "qspi", NULL) )) {
        int cs  = -1; 
        bus = fdt32_to_cpu(*(val + 0));
        cs = fdt32_to_cpu(*(val + 1));
        offset = fdt32_to_cpu(*(val + 2));
        read_ipmi_qspi( bus, cs, offset, &iv_ipmi );
    } else {
        printf("IV IPMI: %s DT NODE PROPERTY UNSUPPORTED\n", class_node_name);
        return -EOPNOTSUPP;
    }

    int err = 0;

    /*
     * Validate IPMI checksums and format
     */
    unsigned char *p, sum;
    int i;

    p = (unsigned char *)&iv_ipmi.common_header;
    for (i = 0, sum = 0; i < sizeof(iv_ipmi.common_header); i++) {
        sum += p[i];
    }
    if (sum != 0 ) {
        err = -EBADMSG;
        goto out;
    }

    p = (unsigned char *)&iv_ipmi.board_area;
    for (i = 0, sum = 0; i < sizeof(iv_ipmi.board_area); i++) {
        sum += p[i];
    }
    if (sum != 0 ) {
        err = -EBADMSG;
        goto out;
    }

    /*
     * Validate board area format version.
     */
    if (iv_ipmi.board_area.format_version != 1) {
        err = -EBADMSG;
        goto out;
    }
    
out:
    if ( err != 0 )
        memset( &iv_ipmi, 0x00, sizeof(iv_ipmi) );

    set_ipmi_env(brd_class, &iv_ipmi, fdt);
    print_board_info(brd_class);

    return err;
}


int set_ipmi_chosen( void *fdt )
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

	set_ipmi_chosen( fdt );

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
    printf("Meta commit: %s\n", IVEIA_META_BUILD_HASH);

    iv_ipmi_scan( gd->fdt_blob );

    return 0;
}
