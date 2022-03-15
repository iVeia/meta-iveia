/*
 * iv_z8_sequence_boot.h
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
#ifndef _IV_Z8_SEQUENCE_BOOT_H_
#define _IV_Z8_SEQUENCE_BOOT_H_

#define MAX_MULTIBOOT_ATTEMPTS 30

typedef struct {
    /*
     * A Xilinx defined boot mode that is supported by the boot sequencer.
     * Includes: XFSBL_SD0_BOOT_MODE, XFSBL_SD1_BOOT_MODE,
     * XFSBL_QSPI32_BOOT_MODE.
     */
    uint32_t mode;

    /*
     * Number of attempts using the Zynqmp Multiboot. Max is MAX_MULTIBOOT_ATTEMPTS.
     *
     * If 0, sequencer will skip this method.
     *
     * If -1, Multiboot will be used with the iVeia SoM part number ordinal.
     * For example, if the board was an iVeia atlas-ii-z8ev (which has a part
     * number ordinal of 00127), and if this was booting via SD, the sequencer
     * would attempt to load the file BOOT0127.BIN.
     */
    int num_attempts;
} iv_boot_sequence_t;


#endif  // _IV_Z8_SEQUENCE_BOOT_H_
