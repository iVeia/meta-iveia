/*
 * iv_z8_sequence_boot.c
 *
 * This module checks for valid boot images in an ordered list of boot devices.
 * For each entry in the list, the multi-boot value is update until the max 
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
#include <xil_io.h>
#include <xfsbl_main.h>
#include <xfsbl_hw.h>

#include "iv_z8_sequence_boot.h"


extern iv_boot_sequence_t boot_seq[];
extern int boot_dev_count;

static char *pr_boot_mode( uint32_t mode );
static void soft_reset( void );


int iv_sequence_boot( void )
{
    uint32_t boot_mode_user_reg = Xil_In32(CRL_APB_BOOT_MODE_USER);
    uint32_t boot_mode = boot_mode_user_reg & CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK;
    uint32_t alt_boot = (boot_mode_user_reg >> 12) & 0x1;
    iv_boot_sequence_t *seq_entry;
    u32 status;

    XFsbl_Printf(DEBUG_PRINT_ALWAYS, "\r\nIVEIA SEQ BOOT MODE (0x%08x): %s, %s\r\n", 
            boot_mode_user_reg, pr_boot_mode(boot_mode), alt_boot ? "ALT":"NORM");

    switch ( boot_mode )
    {
        case XFSBL_QSPI24_BOOT_MODE:
        case XFSBL_QSPI32_BOOT_MODE:
            // if booting from qspi, continue on to traverse sequential boot
            //   device list
            break;
        default:
            // good boot device determined in previous call to this function, 
            //   return to continue boot for selected device
            return XFSBL_SUCCESS;
            break;
    }
    

    for ( int boot_idx = 0; boot_idx < boot_dev_count; boot_idx++ )
    {
        seq_entry = &boot_seq[boot_idx];
        for ( int multi_boot = 0; multi_boot < seq_entry->num_attempts; multi_boot++ )
        {
            XFsbl_Printf(DEBUG_PRINT_ALWAYS, "Searching %s for valid boot image...%d\r\n", 
                    pr_boot_mode(seq_entry->mode), multi_boot);
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
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, "good image found, resetting\r\n");
                Xil_Out32(CRL_APB_BOOT_MODE_USER, 
                        (seq_entry->mode << 12) | (0x1 << 8) );
                soft_reset();
            }   
            else if ( status == XFSBL_ERROR_UNSUPPORTED_BOOT_MODE )
            {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, "UNKNOWN/UNTESTED BOOT MODE, SKIPPING\r\n");
                continue;
            }
        }
    }

    XFsbl_Printf(DEBUG_PRINT_ALWAYS, "SEQUENCE BOOT COMPLETE, NO VALID IMAGES FOUND\r\n");
    return XFSBL_FAILURE;
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

