/*
 * iv_z8_user_boot_order.c
 *
 * This module allows for upper yocto layers- ie, customer specific layers that
 * build on meta-iveia to support their carriers- to provide a custom boot order by
 * defining an array of the following, name boot_seq[]:
 *
 * typedef struct {
 *      uint32_t mode;
 *      int num_attempts;
 * } iv_bootsequence_t;
 *
 * (C) Copyright 2020, iVeia
 */

 #include <xfsbl_main.h>
 #include <xfsbl_hw.h>

 #include "iv_z8_sequence_boot.h"


iv_boot_sequence_t boot_seq[] = {
    { .mode = XFSBL_SD0_BOOT_MODE, .num_attempts = 1 },
    { .mode = XFSBL_SD1_BOOT_MODE, .num_attempts = 1 },
    // Final fallback mode is always QSPI
};
int boot_dev_count = sizeof(boot_seq) / sizeof(boot_seq[0]);

