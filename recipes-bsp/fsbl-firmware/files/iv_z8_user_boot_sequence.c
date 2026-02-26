/*
 * iv_z8_user_boot_order.c
 *
 * This module allows for upper yocto layers- ie, customer specific layers that
 * build on meta-iveia to support their carriers- to provide a custom boot order by
 * defining a user boot sequence.
 *
 * Three boot sequences are available, selected at compile time:
 *
 *   FORCE_QSPI24_BOOT_ONLY  - boot directly from QSPI24 only
 *   FORCE_QSPI32_BOOT_ONLY  - boot directly from QSPI32 only
 *   (neither defined)       - default: try SD, fall back to QSPI32
 *
 * (C) Copyright 2020, iVeia
 */

 #include <xfsbl_main.h>
 #include <xfsbl_hw.h>

 #include "iv_z8_sequence_boot.h"

#if defined(FORCE_QSPI24_BOOT_ONLY)

iv_boot_sequence_t user_boot_seq[] = {
    { .mode = XFSBL_QSPI24_BOOT_MODE, .num_attempts = 1 },
};

#elif defined(FORCE_QSPI32_BOOT_ONLY)

iv_boot_sequence_t user_boot_seq[] = {
    { .mode = XFSBL_QSPI32_BOOT_MODE, .num_attempts = 1 },
};

#else

/*
 * Default boot sequence.
 *
 * Note that the last mode is QSPI32 - this is the fallback mode to run
 * directly out of QSPI.  However, because QSPI24 is the setting of the boot
 * mode pins for Atlas boards, a restart in QSPI32 is required to correctly
 * access devices over 16MB in size.
 */
iv_boot_sequence_t user_boot_seq[] = {
    { .mode = XFSBL_SD0_BOOT_MODE, .num_attempts = 1 },
    { .mode = XFSBL_SD0_BOOT_MODE, .num_attempts = -1 },  // Boot by part number
    { .mode = XFSBL_SD1_BOOT_MODE, .num_attempts = 1 },
    { .mode = XFSBL_QSPI32_BOOT_MODE, .num_attempts = 1 },
};

#endif

int user_boot_dev_count = sizeof(user_boot_seq) / sizeof(user_boot_seq[0]);

