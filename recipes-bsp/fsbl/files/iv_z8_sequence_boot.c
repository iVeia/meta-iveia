/*
 * iv_z8_sequence_boot.c
 *
 * This module checks for valid boot images in an ordered list of boot devices.
 * For each entry in the list, the multi-boot value is updated until the max
 * specified in the list entry and the corresponding device init function is
 * called.  If a valid boot image is found for the device / multi-boot combination,
 * the device init function returns success.  In this case, the boot mode user reg
 * is updated and a soft reset is performed, resulting in fsbl being re-executed
 * from selected device.
 *
 * This module is intended to be called from the start of Stage 2 in xfsbl_main and
 * execute its logic if booting from qspi.  For other boot modes, this module simply
 * returns to allow continuing to boot in the previously selected device.
 *
 * (C) Copyright 2020, iVeia
 */
#include <stdlib.h>
#include <string.h>
#include <xil_io.h>
#include <xfsbl_main.h>
#include <xfsbl_hw.h>
#include <xfsbl_qspi.h>
#include <xparameters.h>
#include <xiicps.h>

#include "iveia-ipmi.h"
#include "iv_z8_sequence_boot.h"

// Missing defs from xfsbl_hw.h
#define CRL_APB_BOOT_MODE_USER_ALT_BOOT_MODE_MASK    ((u32)0X0000F000U)
#define CRL_APB_BOOT_MODE_USER_USE_ALT_MASK    ((u32)0X00000100U)
#define CRL_APB_BOOT_MODE_USER_BOOT_MODE_SHIFT   0

extern iv_boot_sequence_t boot_seq[];
extern int boot_dev_count;

static char *pr_boot_mode( uint32_t mode );
static void soft_reset( void );


#define I2C_EEPROM_BUS_ID   XPAR_XIICPS_1_DEVICE_ID
#define I2C_EEPROM_ADDR		0x52
#define I2C_SCLK_RATE		100000

#define BOARD_PART_NUMBER_FIELD_SIZE (sizeof(((struct iv_ipmi *)0)->board_area.board_part_number))

int validate_ipmi(struct iv_ipmi * p_iv_ipmi)
{
    unsigned char * p;
    unsigned char sum;
    int i;
    int ret = -1;

    /*
     * Validate checksums
     */
    p = (unsigned char *) &p_iv_ipmi->common_header;
    sum = 0;
    for (i = 0; i < sizeof(p_iv_ipmi->common_header); i++) {
        sum += p[i];
    }
    if (sum != 0)
        goto validate_ipmi_exit;
    p = (unsigned char *) &p_iv_ipmi->board_area;
    sum = 0;
    for (i = 0; i < sizeof(p_iv_ipmi->board_area); i++) {
        sum += p[i];
    }
    if (sum != 0)
        goto validate_ipmi_exit;

    /*
     * Validate board area format version.
     */
    if (p_iv_ipmi->board_area.format_version != 1)
        goto validate_ipmi_exit;

    ret = 0;

validate_ipmi_exit:

    return ret;
}

/*
 * I2C setup and read of IPMI struct from EEPROM
 */
static int read_ipmi(struct iv_ipmi * ipmi)
{
	int Status;
	XIicPs_Config *Config;
    XIicPs Iic;

	Config = XIicPs_LookupConfig(I2C_EEPROM_BUS_ID);
	if (NULL == Config) return -1;

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) return -1;

	Status = XIicPs_SelfTest(&Iic);
	if (Status != XST_SUCCESS) return -1;

	XIicPs_SetSClk(&Iic, I2C_SCLK_RATE);

    /*
     * EEPROM (24AA32A) requires sending 2-byte memory address to read from.
     * IPMI struct starts at addr 0.  After writing, read back IPMI struct.
     */
    memset(ipmi, 0, 2);
	Status = XIicPs_MasterSendPolled(&Iic, (u8 *) ipmi, 2, I2C_EEPROM_ADDR);
	if (Status != XST_SUCCESS) return -1;
    memset(ipmi, 0, sizeof(*ipmi));
	Status = XIicPs_MasterRecvPolled(&Iic, (u8 *) ipmi, sizeof(struct iv_ipmi), I2C_EEPROM_ADDR);
	if (Status != XST_SUCCESS) return -1;

    if (validate_ipmi(ipmi)) return -1;

	return 0;
}

/*
 * Extract the PN ordinal from the IPMI struct
 *
 * The board_part_number is of the form "205-NNNNN-VV-RR", where NNNNN is the
 * part number ordinal that we want.
 *
 * Return part number ordinal GREATER THAN zero on success, othwerwise there
 * was an error with the field or converting to int.
 */
int get_pn_ord(struct iv_ipmi * ipmi)
{
    char pn[BOARD_PART_NUMBER_FIELD_SIZE + 1];
    strncpy(pn, ipmi->board_area.board_part_number, BOARD_PART_NUMBER_FIELD_SIZE);
    pn[BOARD_PART_NUMBER_FIELD_SIZE] = '\0';

    char * pord = strchr(pn, '-');
    if (!pord) return -1;

    return atoi(pord + 1);
}

int iv_sequence_boot( void )
{
    uint32_t boot_mode_user_reg = Xil_In32(CRL_APB_BOOT_MODE_USER);
    uint32_t boot_mode = (boot_mode_user_reg & CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK) >>
                            CRL_APB_BOOT_MODE_USER_BOOT_MODE_SHIFT;
    uint32_t alt_boot = (boot_mode_user_reg & CRL_APB_BOOT_MODE_USER_USE_ALT_MASK) >>
                            CRL_APB_BOOT_MODE_USER_USE_ALT_SHIFT;
    iv_boot_sequence_t *seq_entry;
    u32 status;

    XFsbl_Printf(DEBUG_PRINT_ALWAYS, "iVeia seq boot mode (0x%08x): %s, %s\r\n",
            boot_mode_user_reg, pr_boot_mode(boot_mode), alt_boot ? "ALT":"NORM");

    //
    // alt_boot will be false for initial power up.  if alt_boot is true, that means
    // that we got here after reset from successful completion of sequence boot below,
    // so return success to continue on with boot
    //
    // Also, must do normal FSBL boot handling if in JTAG mode.
    //
    if ( alt_boot || boot_mode == XFSBL_JTAG_BOOT_MODE ) {
        return XFSBL_SUCCESS;
    }

    for ( int boot_idx = 0; boot_idx < boot_dev_count; boot_idx++ )
    {
        //
        // Current multiboot code does not validate images, and just finds
        // first available boot image (up to num_attempts).
        //
        // TODO: Possibly validate images XFsbl_PartitionLoad() mechanism.
        //
        seq_entry = &boot_seq[boot_idx];
        int num_attempts = seq_entry->num_attempts;
        int boot_by_pn = 0;
        struct iv_ipmi ipmi;
        if (num_attempts > MAX_MULTIBOOT_ATTEMPTS) {
            num_attempts = MAX_MULTIBOOT_ATTEMPTS;
        } else if (num_attempts < 0) {
            /*
             * num_attempts < 0 is special: it means try to boot relative to
             * the PN.  In this case, read the IPMI, get the part number
             * ordinal save for use later.
             */
            int err = read_ipmi(&ipmi);
            if (err) {
                num_attempts = 0;
            } else {
                boot_by_pn = get_pn_ord(&ipmi);
                if (boot_by_pn > 0) {
                    num_attempts = 1;
                }
            }
        }
        for ( int multi_boot = 0; multi_boot < num_attempts; multi_boot++ )
        {
            #define PRE_FMT "Searching %s for valid boot image..."
            char *mode = pr_boot_mode(seq_entry->mode);
            if (boot_by_pn) {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, PRE_FMT " (PN %05d)\r\n", mode, boot_by_pn);
                multi_boot = boot_by_pn;
            } else if (multi_boot == 0) {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, PRE_FMT "\r\n", mode);
            } else {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, PRE_FMT " (retry #%d)\r\n", mode, multi_boot);
            }
            Xil_Out32(CSU_CSU_MULTI_BOOT, multi_boot);

            switch ( seq_entry->mode )
            {
                case XFSBL_SD0_BOOT_MODE:
                case XFSBL_SD1_BOOT_MODE:
#if 0  // untested
                case XFSBL_EMMC_BOOT_MODE:
                case XFSBL_SD1_LS_BOOT_MODE:
#endif
                    status = XFsbl_SdInit( seq_entry->mode );
                    break;

                case XFSBL_QSPI24_BOOT_MODE:
                    status = XFsbl_Qspi24Init( seq_entry->mode );
                    break;

                case XFSBL_QSPI32_BOOT_MODE:
                    status = XFsbl_Qspi32Init( seq_entry->mode );
                    break;

#if 0  // untested
                case XFSBL_NAND_BOOT_MODE:
                    status = XFsbl_NandInit( seq_entry->mode );
                    break;
#endif
#if 0  // untested
                case XFSBL_USB_BOOT_MODE:
                    status = XFsbl_UsbInit( seq_entry->mode );
                    break;
#endif
                default:
                    status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
                    break;
            }

            if ( status == XFSBL_SUCCESS )
            {
                //
                // This is the last printf before reset.  Use poor-man's flush
                // (i.e. sleep) to get string out.
                // 30 millisecs @ 115200bps =~ 3500 bits - should be plenty (10x)!
                //
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, "Good image found, resetting...\r\n\r\n");
                usleep(100000);

                Xil_Out32(CRL_APB_BOOT_MODE_USER,
                        (seq_entry->mode << CRL_APB_BOOT_MODE_USER_ALT_BOOT_MODE_SHIFT) |
                        (0x1 << CRL_APB_BOOT_MODE_USER_USE_ALT_SHIFT));
                soft_reset();
            }
            else if ( status == XFSBL_ERROR_UNSUPPORTED_BOOT_MODE )
            {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, "Unsupported boot mode, skipping...\r\n");
                continue;
            }
        }
    }

    XFsbl_Printf(DEBUG_PRINT_ALWAYS, "Continuing with QSPI boot...\r\n");
    Xil_Out32(CSU_CSU_MULTI_BOOT, 0);
    return XFSBL_SUCCESS;
}


static char *pr_boot_mode( uint32_t mode )
{
    switch ( mode )
    {
        case XFSBL_JTAG_BOOT_MODE:
            return "JTAG";
            break;
        case XFSBL_QSPI24_BOOT_MODE:
            return "QSPI24";
            break;
        case XFSBL_QSPI32_BOOT_MODE:
            return "QSPI32";
            break;
        case XFSBL_SD0_BOOT_MODE:
            return "SD0";
            break;
        case XFSBL_NAND_BOOT_MODE:
            return "NAND";
            break;
        case XFSBL_SD1_BOOT_MODE:
            return "SD1";
            break;
        case XFSBL_EMMC_BOOT_MODE:
            return "EMMC";
            break;
        case XFSBL_USB_BOOT_MODE:
            return "USB";
            break;
        case XFSBL_SD1_LS_BOOT_MODE:
            return "SD1_LS";
            break;
        default:
            break;
    }

    return "null";
}


static void soft_reset( void )
{
   u32 RegValue;

   XFsbl_Printf(DEBUG_PRINT_ALWAYS, "Performing System Soft Reset\n\r");

   /**
   * Due to a bug in 1.0 Silicon, PS hangs after System Reset if RPLL is used.
   * Hence, just for 1.0 Silicon, bypass the RPLL clock before giving
   * System Reset.
   */
   if (XGetPSVersion_Info() == (u32)XPS_VERSION_1)
   {
           RegValue = XFsbl_In32(CRL_APB_RPLL_CTRL) | CRL_APB_RPLL_CTRL_BYPASS_MASK;
           XFsbl_Out32(CRL_APB_RPLL_CTRL, RegValue);
   }

   /* make sure every thing completes */
   dsb();
   isb();

   /* Soft reset the system */
   RegValue = XFsbl_In32(CRL_APB_RESET_CTRL);
   XFsbl_Out32(CRL_APB_RESET_CTRL, RegValue | CRL_APB_RESET_CTRL_SOFT_RESET_MASK);

   /* loop until reset */
   while(1)
       ;;

   return;
}

