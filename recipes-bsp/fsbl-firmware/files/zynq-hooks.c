/*
 * Zynq FSBL Hooks - Setup clkgen and other init.
 *
 * (C) Copyright 2023, iVeia
 */
#include "fsbl.h"
#include "xstatus.h"
#include "fsbl_hooks.h"
#include "xparameters.h"
#include "xiicps.h"
#include "xil_printf.h"

#define I2C_DEVICE_ID		XPAR_XIICPS_1_DEVICE_ID
#define I2C_CLKGEN_ADDR		0x68
#define I2C_CLK_RATE		100000

#define MAX_REGS    32

static int i2c_reg_write(XIicPs * pIic, u8 addr, u8 reg, u8 val)
{
    u8 buf[2];

    buf[0] = reg;
    buf[1] = val;
	return XIicPs_MasterSendPolled(pIic, buf, sizeof(buf), addr);
}


static int Si5326SetDirect(
    XIicPs * pIic,
    unsigned int nBwSel,
    unsigned int nN1_Hs,
    unsigned int nNc1_Ls,
    unsigned int nNc2_Ls,
    unsigned int nN2_Hs,
    unsigned int nN2_Ls,
    unsigned int nN31,
    unsigned int nN32
    ) 
{
    struct {
        unsigned short reg;
        unsigned short val;
    } regs[MAX_REGS];
    int i;

    i = 0;

    memset(regs, 0, sizeof(regs));

    //BWSEL
    regs[i].reg = 2;
    regs[i++].val = (nBwSel << 4) & 0x0FF;

    //N1_HS
    regs[i].reg = 25;
    regs[i++].val = (((nN1_Hs-4) << 5) & 0x0E0);

    //NC1_LS
    regs[i].reg = 31;
    regs[i++].val = ((nNc1_Ls - 1) >> 16) & 0x00F;
    regs[i].reg = 32;
    regs[i++].val = ((nNc1_Ls - 1) >> 8) & 0x0FF;
    regs[i].reg = 33;
    regs[i++].val = ((nNc1_Ls - 1) >> 0) & 0x0FF;

    //NC2_LS
    regs[i].reg = 34;
    regs[i++].val = ((nNc2_Ls - 1) >> 16) & 0x00F;
    regs[i].reg = 35;
    regs[i++].val = ((nNc2_Ls - 1) >> 8) & 0x0FF;
    regs[i].reg = 36;
    regs[i++].val = ((nNc2_Ls - 1) >> 0) & 0x0FF;

    //N2_HS / N2_LS
    regs[i].reg = 40;
    regs[i++].val = (((nN2_Hs-4) << 5) & 0x0E0) | (((nN2_Ls - 1) >> 16) & 0x00F);
    regs[i].reg = 41;
    regs[i++].val = ((nN2_Ls - 1) >> 8) & 0x0FF;
    regs[i].reg = 42;
    regs[i++].val = ((nN2_Ls - 1) >> 0) & 0x0FF;

    //N31
    regs[i].reg = 43;
    regs[i++].val = ((nN31 - 1) >> 16) & 0x007;
    regs[i].reg = 44;
    regs[i++].val = ((nN31 - 1) >> 8) & 0x0FF;
    regs[i].reg = 45;
    regs[i++].val = ((nN31 - 1) >> 0) & 0x0FF;

    //N32
    regs[i].reg = 46;
    regs[i++].val = ((nN32 - 1) >> 16) & 0x007;
    regs[i].reg = 47;
    regs[i++].val = ((nN32 - 1) >> 8) & 0x0FF;
    regs[i].reg = 48;
    regs[i++].val = ((nN32 - 1) >> 0) & 0x0FF;

    int Status;
    int num_regs = i;
    for (i = 0; i < num_regs; i++) {
        Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, regs[i].reg, regs[i].val);
    }

    // Run ICAL
    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 136, 0x40);

}


/*
 * Init SiLabs Si5326 Clock Generator
 *
 * Make SiLabs Clockgen pass through CLKIN2.  This increases power by 0.1 Watts,
 * may want to make this board dependent.
 *
 * CLK2 input = 156.25 MHz
 *
 * CLK1 output = 125 MHz
 * CLK2 output = 156.25 MHz
 */
static int clkgen_init(XIicPs * pIic)
{
    int Status;

    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 0,  0x34);
	if (Status != XST_SUCCESS) return XST_FAILURE;
    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 3,  0x45);
	if (Status != XST_SUCCESS) return XST_FAILURE;
    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 4,  0x12);
	if (Status != XST_SUCCESS) return XST_FAILURE;
    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 6,  0x3f);
	if (Status != XST_SUCCESS) return XST_FAILURE;
    Status = i2c_reg_write(pIic, I2C_CLKGEN_ADDR, 21, 0x7c);
	if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = Si5326SetDirect(
        pIic,
        10,     // BWSEL
        4,      // N1_HS
        10,     // NC1_LS
        8,      // NC2_LS
        8,      // N2_HS
        316,    // N2_LS
        79,     // N31 (don't care)
        79      // N32
        );
	if (Status != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}


/*
 * Init I2C device
 *
 * @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
 */
static int i2c_init(XIicPs * pIic, u16 DeviceId)
{
	int Status;
	XIicPs_Config *Config;

	Config = XIicPs_LookupConfig(DeviceId);
	if (Config == NULL) return XST_FAILURE;

	Status = XIicPs_CfgInitialize(pIic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) return XST_FAILURE;

	XIicPs_SetSClk(pIic, I2C_CLK_RATE);

	return XST_SUCCESS;
}


/*
 * Overridden Xilinx FsblHookBeforeHandoff() hook function
 */
u32 FsblHookBeforeHandoff(void)
{
    XIicPs Iic;
	u32 Status;

	Status = i2c_init(&Iic, I2C_DEVICE_ID);
	if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = clkgen_init(&Iic);
	if (Status != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}


#define USB_RST_CTRL    0xF8000210
#define USB0_CPU1X_RST  0x00000001
#define USB1_CPU1X_RST  0x00000002

/*
 * Hold USB0/1 controllers in reset (if `active`)
 */
static void usb_reset(int active)
{
    const u32 mask = USB0_CPU1X_RST | USB1_CPU1X_RST;

    SlcrUnlock();
    Xil_Out32(USB_RST_CTRL, (Xil_In32(USB_RST_CTRL) & ~mask) | (active ? mask : 0));
    SlcrLock();
}


/*
 * Overridden hook from just BEFORE ps7_init()
 */
void ps7_init_prehook()
{
    /*
     * USB must be held in reset while pins are configured, otherwise ULPI
     * interface can get out of whack, and linux drivers will not see PHY.
     */
    usb_reset(1);
}


/*
 * Overridden hook from just AFTER ps7_init()
 */
void ps7_init_posthook()
{
    usb_reset(0);
}
