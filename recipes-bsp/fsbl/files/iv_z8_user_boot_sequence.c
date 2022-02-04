/*
 * iv_z8_user_boot_order.c
 *
 * This module allows for upper yocto layers- ie, customer specific layers that
 * build on meta-iveia to support their carriers- to provide a custom boot order by
 * defining an array of the following, name boot_seq (see iv_z8_sequence_boot.h)
 *
 * (C) Copyright 2020, iVeia
 */

 #include <xfsbl_main.h>
 #include <xfsbl_hw.h>

 #include "iv_z8_sequence_boot.h"

/*
 * Boot sequence.  Can be modified to alter the sequence.
 *
 * Note that the last mode is QSPI32 - this is the fallback mode to run
 * directly out of QSPI.  However, because QSPI24 is the setting of the boot
 * mode pins for Atlas boards, a restart in QSPI32 is required to correctly
 * access devices over 16MB in size.
 */
iv_boot_sequence_t boot_seq[] = {
    { .mode = XFSBL_SD0_BOOT_MODE, .num_attempts = 1 },
    { .mode = XFSBL_SD0_BOOT_MODE, .num_attempts = -1 },  // Boot by part number
    { .mode = XFSBL_SD1_BOOT_MODE, .num_attempts = 1 },
    { .mode = XFSBL_QSPI32_BOOT_MODE, .num_attempts = 1 },
};
int boot_dev_count = sizeof(boot_seq) / sizeof(boot_seq[0]);

