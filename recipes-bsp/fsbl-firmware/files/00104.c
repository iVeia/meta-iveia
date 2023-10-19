/*
 * Machine specific FSBL hooks
 *
 * (C) Copyright 2020, iVeia
 */
void iv_z8_init_before_hook() 
{
	PSU_Mask_Write (0XFF5E0238, 0x00000600, 0x00000000);
}

