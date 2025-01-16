/*
 * Machine specific FSBL hooks
 *
 * (C) Copyright 2020, iVeia
 */
#include "psu_init.h"

int iv_z8_has_clock_chip()
{
    return 0;
}

void iv_z8_init_before_hook()
{
    // the default iv_z8_init_before_hook configures MIO 8/9 to be i2c-1;
    // this is does not apply to 00108 (it uses MIO 36/37 for i2c-1)
    //
    // override the hook to do nothing
    //
    return;
}

