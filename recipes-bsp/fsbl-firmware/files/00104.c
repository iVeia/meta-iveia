/*
 * Machine specific FSBL hooks
 *
 * (C) Copyright 2020, iVeia
 */
void iv_z8_init_before_hook() 
{
	// reset I2C-0 and I2C-1
	// ref. https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/RST_LPD_IOU2-CRL_APB-Register
	// CRL_APB RST_LPD_IOU2
	//	 [10] i2c1_reset
	//	 [ 9] i2c0_reset
	PSU_Mask_Write (0xFF5E0238, 0x00000600, 0x00000000);
}

