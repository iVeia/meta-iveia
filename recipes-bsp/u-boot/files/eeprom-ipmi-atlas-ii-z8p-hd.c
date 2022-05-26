/*
 * (C) Copyright 2010 - 2022 iVeia
 * Brian Silverman <bsilverman@iveia.com>
 *
 * Read and process non-iVeia EEPROM IPMI-based board info, specific to the
 * atlas-ii-z8p-hd for some configurations.  Required to determine MAC address.
 *
 * Note: currently uses CONFIG_DM_I2C_COMPAT for accessing I2C bus - this
 * method is deprecated.
 */
#include <common.h>
#include <linux/ctype.h>
#include <i2c.h>
#include "iveia-ipmi.h"

//
// The alternate board area defined here is of fixed length, with an iVeia SN
// at a fixed offset.
//
#define BOARD_AREA_OFFSET           0x08
#define BOARD_AREA_LEN              72
#define FORMAT_VERSION_OFFSET       0
#define FORMAT_VERSION_EXPECTED     4
#define SN_OFFSET                   0x37
#define SN_LEN                      5
#define BOARD_PN_STR                "205-00130-XX-XX"
#define BOARD_NAME_STR              "Atlas-II-Z8p-HD"

/*
 * Override read_alternate_ipmi().  See weak reference of function.
 */
char * read_alternate_ipmi(char * ipmi_string_buf)
{
    int err;
    unsigned int oldbus;
    char alt_ipmi_board_area[BOARD_AREA_LEN];
    char * s = NULL;

    oldbus = i2c_get_bus_num();
    i2c_set_bus_num(IV_MB_ALT_I2C_BUS);

    err = i2c_read(
        IV_MB_EEPROM_ADDR,
        BOARD_AREA_OFFSET,
        2,
        (uchar *) alt_ipmi_board_area,
        sizeof(alt_ipmi_board_area)
        );
    if (err != 0)
        goto read_alternate_ipmi_exit;

    /*
     * Validate board area format version and checksum.
     */
    if (alt_ipmi_board_area[FORMAT_VERSION_OFFSET] != FORMAT_VERSION_EXPECTED)
        goto read_alternate_ipmi_exit;
    unsigned char sum = 0;
    for (int i = 0; i < sizeof(alt_ipmi_board_area); i++) {
        sum += alt_ipmi_board_area[i];
    }
    if (sum != 0)
        goto read_alternate_ipmi_exit;

    int len;
    char * sn;
    sn = &alt_ipmi_board_area[SN_OFFSET];
    sn[SN_LEN] = '\0';
    len = snprintf(
        ipmi_string_buf, MAX_KERN_CMDLINE_FIELD_LEN, "%s,%s,%s", BOARD_PN_STR, sn, BOARD_NAME_STR
        );
    if (len >= MAX_KERN_CMDLINE_FIELD_LEN)
        goto read_alternate_ipmi_exit;

    s = ipmi_string_buf;

read_alternate_ipmi_exit:
    i2c_set_bus_num(oldbus);

    return s;
}

