/*
 * Machine specific FSBL hooks
 *
 * (C) Copyright 2025, iVeia
 */
#include "psu_init.h"
#include <xil_io.h>

int iv_z8_has_clock_chip()
{
    return 0;
}

#define IV_GPIO_DIRM_0_OFFSET           0x00FF0A0204 // [25: 0] Direction for MIO [25:0], 0=input, 1=output
#define IV_GPIO_OEN_0_OFFSET            0x00FF0A0208 // [25: 0] Output Enable for MIO [25:0], 0=disabled, 1=enabled
#define IV_GPIO_MASK_DATA_0_MSW_OFFSET  0x00FF0A0004 // [25:16] Mask for MIO [25:16], 0=update, 1=mask out (ignore)
                                                     // [ 9: 0] Data for MIO [25:16], 0=low, 1=high


static void iv_00108_enable_clocks( void )
{
    // enable oscillators for PS-MGT REF and PL, controlled by MIO [20:17]


    // [4:0] Configure MIO[20:16] as high output, 1=configure, 0=leave as-is
    u32 pins = 0;

    // PS_MIO17  GTRCLK_100_EN  100MHz  PS_MGTREFCLK0
    pins |= ( 1 << ( 17 - 16 ) );

    // PS_MIO18  GTRCLK_125_EN  125MHz  PS_MGTREFCLK1
    pins |= ( 1 << ( 18 - 16 ) );

    // PS_MIO19  GTRCLK_108_EN  108MHz  PS_MGTREFCLK2
    pins |= ( 1 << ( 19 - 16 ) );

    // PS_MIO20  PLCLK_200_EN   200MHz  L14P/N
    pins |= ( 1 << ( 20 - 16 ) );


    // set GPIO MIO Direction (DIRM) and GPIO MIO Output Enable (OEN); same bits
    // mask write to preserve other pins
    u32 dirm = pins << 16;
    PSU_Mask_Write( IV_GPIO_DIRM_0_OFFSET, dirm, dirm );
    PSU_Mask_Write( IV_GPIO_OEN_0_OFFSET,  dirm, dirm );

    // set MASK_DATA; register has built-in mask write
    u32 mask = pins << 16;
    u32 data = pins;
    Xil_Out32( IV_GPIO_MASK_DATA_0_MSW_OFFSET, ( 0xFFFF0000 & ~mask ) | data );
}

void iv_z8_init_before_hook()
{
    iv_00108_enable_clocks();

    // NOTE: the default iv_z8_init_before_hook configures MIO 8/9 to be i2c-1;
    // this is does not apply to 00108 (it uses MIO 36/37 for i2c-1)

    return;
}

