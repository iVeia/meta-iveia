#ifndef _IVEIA_IPMI_H_
#define _IVEIA_IPMI_H_

/*
 * SN and IPMI board info defines
 */
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

#define IV_MB_I2C_BUS           (1)
#define IV_MB_ALT_I2C_BUS       (0)
#define IV_IO_I2C_BUS           (0)
#define IV_IO_ALT_I2C_BUS       (-1)
#define IV_BP_I2C_BUS           (0)
#define IV_BP_ALT_I2C_BUS       (-1)
#define IV_MB_EEPROM_ADDR       0x52
#define IV_IO_EEPROM_ADDR       0x53
#define IV_BP_EEPROM_ADDR       0x54

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

#endif
