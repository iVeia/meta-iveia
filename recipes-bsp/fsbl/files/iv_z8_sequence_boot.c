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
    uint32_t alt_boot = (boot_mode_user_reg >> CRL_APB_BOOT_MODE_USER_ALT_BOOT_MODE_SHIFT) & 0x1;
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
        for ( int multi_boot = 0; multi_boot < seq_entry->num_attempts; multi_boot++ )
        {
            #define PRE_FMT "Searching %s for valid boot image..."
            #define FMT_0 (PRE_FMT "\r\n")
            #define FMT_N (PRE_FMT " (retry #%d)\r\n")
            char *mode = pr_boot_mode(seq_entry->mode);
            if (multi_boot == 0) {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, FMT_0, mode);
            } else {
                XFsbl_Printf(DEBUG_PRINT_ALWAYS, FMT_N, mode, multi_boot);
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

