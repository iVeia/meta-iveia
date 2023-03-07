/*
 * iv_z8_init.c
 *
 * (C) Copyright 2017-2020, iVeia
 */
#include <xil_io.h>
#include <sleep.h>

//
// Required: include C header file generated from SiLabs Clock Builder utility.
//
// Note: "#define code" is a workaround to ignore the code modifier in Silabs
// generated header.
//
#define code
#include "clkinit_100_125_26_125.h"

#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

void PSU_Mask_Write (unsigned long offset, unsigned long mask, unsigned long val)
{
	unsigned long RegVal = 0x0;
	RegVal = Xil_In32 (offset);
	RegVal &= ~(mask);
	RegVal |= (val & mask);
	Xil_Out32 (offset, RegVal);
}

#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(*array))


struct zynq_i2c_registers {
	u32 control;
	u32 status;
	u32 address;
	u32 data;
	u32 interrupt_status;
	u32 transfer_size;
	u32 slave_mon_pause;
	u32 time_out;
	u32 interrupt_mask;
	u32 interrupt_enable;
	u32 interrupt_disable;
};

/* Control register fields */
#define ZYNQ_I2C_CONTROL_RW		0x00000001
#define ZYNQ_I2C_CONTROL_MS		0x00000002
#define ZYNQ_I2C_CONTROL_NEA		0x00000004
#define ZYNQ_I2C_CONTROL_ACKEN		0x00000008
#define ZYNQ_I2C_CONTROL_HOLD		0x00000010
#define ZYNQ_I2C_CONTROL_SLVMON		0x00000020
#define ZYNQ_I2C_CONTROL_CLR_FIFO	0x00000040
#define ZYNQ_I2C_CONTROL_DIV_B_SHIFT	8
#define ZYNQ_I2C_CONTROL_DIV_B_MASK	0x00003F00
#define ZYNQ_I2C_CONTROL_DIV_A_SHIFT	14
#define ZYNQ_I2C_CONTROL_DIV_A_MASK	0x0000C000

/* Status register values */
#define ZYNQ_I2C_STATUS_RXDV	0x00000020
#define ZYNQ_I2C_STATUS_TXDV	0x00000040
#define ZYNQ_I2C_STATUS_RXOVF	0x00000080
#define ZYNQ_I2C_STATUS_BA	0x00000100

/* Interrupt register fields */
#define ZYNQ_I2C_INTERRUPT_COMP		0x00000001
#define ZYNQ_I2C_INTERRUPT_DATA		0x00000002
#define ZYNQ_I2C_INTERRUPT_NACK		0x00000004
#define ZYNQ_I2C_INTERRUPT_TO		0x00000008
#define ZYNQ_I2C_INTERRUPT_SLVRDY	0x00000010
#define ZYNQ_I2C_INTERRUPT_RXOVF	0x00000020
#define ZYNQ_I2C_INTERRUPT_TXOVF	0x00000040
#define ZYNQ_I2C_INTERRUPT_RXUNF	0x00000080
#define ZYNQ_I2C_INTERRUPT_ARBLOST	0x00000200

#define ZYNQ_I2C_FIFO_DEPTH		16
#define ZYNQ_I2C_TRANSFERT_SIZE_MAX	255 /* Controller transfer limit */

#define EINVAL  1
#define ETIMEDOUT   2

static void writel(unsigned int data, unsigned int addr){
    Xil_Out32(addr,data);
}

static unsigned int readl(unsigned int addr){
    return Xil_In32(addr);
}

static void jsm_setbits_le32(unsigned int addr, unsigned int mask){
    unsigned int temp;
    temp = readl(addr);
    writel(temp | mask,addr);
}

static void jsm_clrbits_le32(unsigned int addr, unsigned int mask){
    unsigned int temp;
    temp = readl(addr);
    writel(temp & (~mask),addr);
}

/* Wait for an interrupt */
static u32 zynq_i2c_wait(struct zynq_i2c_registers *zynq_i2c, u32 mask)
{
	int timeout, int_status;

	for (timeout = 0; timeout < 100; timeout++) {
		usleep(100);
		int_status = readl((unsigned int)&zynq_i2c->interrupt_status);
		if (int_status & mask)
			break;
	}

	/* Clear interrupt status flags */
	writel(int_status & mask, (unsigned int)&zynq_i2c->interrupt_status);

	return int_status & mask;
}

static int zynq_i2c_read( u8 dev, unsigned int addr,
			 int alen, u8 *data, int length)
{
	u32 status;
	u32 i = 0;
	u8 *cur_data = data;
	struct zynq_i2c_registers *zynq_i2c;
    zynq_i2c = (struct zynq_i2c_registers *)0xff030000;

	/* Check the hardware can handle the requested bytes */
	if ((length < 0) || (length > ZYNQ_I2C_TRANSFERT_SIZE_MAX))
		return -EINVAL;

	/* Write the register address */
	jsm_setbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_CLR_FIFO |
		ZYNQ_I2C_CONTROL_HOLD);
	/*
	 * Temporarily disable restart (by clearing hold)
	 * It doesn't seem to work.
	 */
	jsm_clrbits_le32((unsigned int)(&zynq_i2c->control), ZYNQ_I2C_CONTROL_HOLD);
	writel(0xFF, (unsigned int)&zynq_i2c->interrupt_status);
	if (alen) {
		jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_RW);
		writel(dev, (unsigned int)&zynq_i2c->address);
		while (alen--)
			writel(addr >> (8 * alen), (unsigned int)&zynq_i2c->data);

		/* Wait for the address to be sent */
		if (!zynq_i2c_wait(zynq_i2c, ZYNQ_I2C_INTERRUPT_COMP)) {
			/* Release the bus */
			jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_HOLD);
			return -ETIMEDOUT;
		}
		//debug("Device acked address\n");
	}

	jsm_setbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_CLR_FIFO |
		ZYNQ_I2C_CONTROL_RW);
	/* Start reading data */
	writel(dev, (unsigned int)&zynq_i2c->address);
	writel(length, (unsigned int)&zynq_i2c->transfer_size);

	/* Wait for data */
	do {
		status = zynq_i2c_wait(zynq_i2c, ZYNQ_I2C_INTERRUPT_COMP |
			ZYNQ_I2C_INTERRUPT_DATA);
		if (!status) {
			/* Release the bus */
			jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_HOLD);
			return -ETIMEDOUT;
		}
		//debug("Read %d bytes\n",
		//      length - readl(&zynq_i2c->transfer_size));
		for (; i < length - readl((unsigned int)&zynq_i2c->transfer_size); i++)
			*(cur_data++) = readl((unsigned int)&zynq_i2c->data);
	} while (readl((unsigned int)&zynq_i2c->transfer_size) != 0);
	/* All done... release the bus */
	jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_HOLD);

	return 0;
}

static int zynq_i2c_write(u8 dev, unsigned int addr,
			  int alen, u8 *data, int length)
{
	u8 *cur_data = data;
	struct zynq_i2c_registers *zynq_i2c;
    zynq_i2c = (struct zynq_i2c_registers *)0xff030000;

	/* Write the register address */
	jsm_setbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_CLR_FIFO |
		ZYNQ_I2C_CONTROL_HOLD);
	jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_RW);
	writel(0xFF, (unsigned int)&zynq_i2c->interrupt_status);
	writel(dev, (unsigned int)&zynq_i2c->address);
	if (alen) {
		while (alen--)
			writel(addr >> (8 * alen), (unsigned int)&zynq_i2c->data);
		/* Start the tranfer */
		if (!zynq_i2c_wait(zynq_i2c, ZYNQ_I2C_INTERRUPT_COMP)) {
			/* Release the bus */
			jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_HOLD);
			return -ETIMEDOUT;
		}
		//debug("Device acked address\n");
	}

	while (length--) {
		writel(*(cur_data++), (unsigned int)&zynq_i2c->data);
		if (readl((unsigned int)&zynq_i2c->transfer_size) == ZYNQ_I2C_FIFO_DEPTH) {
			if (!zynq_i2c_wait(zynq_i2c, ZYNQ_I2C_INTERRUPT_COMP)) {
				/* Release the bus */
				jsm_clrbits_le32((unsigned int)&zynq_i2c->control,
					     ZYNQ_I2C_CONTROL_HOLD);
				return -ETIMEDOUT;
			}
		}
	}

	/* All done... release the bus */
	jsm_clrbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_HOLD);
	/* Wait for the address and data to be sent */
	if (!zynq_i2c_wait(zynq_i2c, ZYNQ_I2C_INTERRUPT_COMP))
		return -ETIMEDOUT;
	return 0;
}



static void zynq_i2c_init( int requested_speed,
			  int slaveadd)
{
	struct zynq_i2c_registers *zynq_i2c;
    zynq_i2c = (struct zynq_i2c_registers *)0xff030000;

    //printf("****zynq_i2c_init 0x%0x****\n",zynq_i2c);

	/* 111MHz / ( (3 * 17) * 22 ) = ~100KHz */
	writel((16 << ZYNQ_I2C_CONTROL_DIV_B_SHIFT) |
		(2 << ZYNQ_I2C_CONTROL_DIV_A_SHIFT), (unsigned int)(&zynq_i2c->control));

	/* Enable master mode, ack, and 7-bit addressing */
	jsm_setbits_le32((unsigned int)&zynq_i2c->control, ZYNQ_I2C_CONTROL_MS |
		ZYNQ_I2C_CONTROL_ACKEN | ZYNQ_I2C_CONTROL_NEA);
}

#define NUM_CLKBUF_COMMANDS ARRAY_SIZE(Reg_Store)

static int ClockGenWrite(
    unsigned char nDev,
    unsigned char nReg,
    unsigned char nVal
    )
{
    unsigned char data = nVal;
    return zynq_i2c_write(nDev, nReg, 1, &data, 1);
}

static int
ClockGenRead(
    unsigned char nDev,
    unsigned char nReg,
    unsigned char * pnVal
    )
{
    return zynq_i2c_read( nDev, nReg, 1, pnVal, 1);
}

static int ClockGenWriteMasked(
    unsigned char nDev,
    unsigned char nReg,
    unsigned char nVal,
    unsigned char nMask
    )
{
    int iRetVal;
    unsigned char data;
    iRetVal = ClockGenRead(nDev, nReg, &data);
    if (iRetVal) return iRetVal;

    data &= (~nMask);
    data |= (nVal & nMask);
    iRetVal = ClockGenWrite(nDev,nReg,data);
    return iRetVal;
}


static int
__IV_ResetSi5338Pll(unsigned char nDev ,int bClkIn){
    //if bClkIn : Monitor CLKIN(CLK 1&2 or 3 for LOS)
    //if !bClkIn : Monitor FB_CLK(CLK 5&6 or 4 for LOS)
    //Follows Chart on pg 22 of Si5338 Data Sheet (Figure 9. I2C Programming Procedure)
    int iRetVal;
    int iCount;
    unsigned char ucTemp;

    //Disable Outputs
    iRetVal = ClockGenWriteMasked(nDev,230,0x10,0x10);if (iRetVal) return iRetVal;

    //Pause LOL
    iRetVal = ClockGenWriteMasked(nDev,241,0x80,0x80);if (iRetVal) return iRetVal;

    //Write new configuration to device (DONE)
        //Enable PLL
    iRetVal = ClockGenWriteMasked(nDev,50,0xC0,0xC0);if (iRetVal) return iRetVal;

    //Validate Input Clock status
    iRetVal = ClockGenRead(nDev,218,&ucTemp);if (iRetVal) return iRetVal;
    if ((ucTemp & 0x04) && bClkIn){
        xil_printf("ERROR, __IV_ResetSi5338Pll(). CLKIN is not valid\n");
        return -1;
    }

    if ((ucTemp & 0x08) && !bClkIn){
        xil_printf("ERROR, __IV_ResetSi5338Pll(). FB_CLK is not valid\n");
        return -2;
    }

    //Conigure PLL for Locking
    iRetVal = ClockGenWriteMasked(nDev,49,0x00,0x80);if (iRetVal) return iRetVal;

    //Initiate Locking of Pll
    iRetVal = ClockGenWrite(nDev,246,0x02);if (iRetVal) return iRetVal;

    //Wait 25 ms
    usleep(25*1000);

    //Restart LOL
    iRetVal = ClockGenWriteMasked(nDev,241,0x00,0x80);if (iRetVal) return iRetVal;
    iRetVal = ClockGenWrite(nDev,241,0x65);if (iRetVal) return iRetVal;

    //Confirm PLL Lock Status
    #define PLL_TRIES   1000000
    iCount = 0;
    do{
        iRetVal = ClockGenRead(nDev,218,&ucTemp);if (iRetVal) return iRetVal;
        iCount++;
    }while((ucTemp & 0x15) && (iCount < PLL_TRIES));

    if (ucTemp & 0x15){
        xil_printf("ERROR, __IV_ResetSi5338Pll(). PLL Did Not Lock, TIMED OUT\n");
        return -3;
    }

    //Copy FCAL Values to Active Registers
    iRetVal = ClockGenRead(nDev,237,&ucTemp);if (iRetVal) return iRetVal;
    ucTemp &= 0x03;
    ucTemp |= 0x14;
    iRetVal = ClockGenWrite(nDev,47,ucTemp);if (iRetVal) return iRetVal;
    iRetVal = ClockGenRead(nDev,236,&ucTemp);if (iRetVal) return iRetVal;
    iRetVal = ClockGenWrite(nDev,46,ucTemp);if (iRetVal) return iRetVal;
    iRetVal = ClockGenRead(nDev,235,&ucTemp);if (iRetVal) return iRetVal;
    iRetVal = ClockGenWrite(nDev,45,ucTemp);if (iRetVal) return iRetVal;

    //Set PLL to use FCAL Values
    iRetVal = ClockGenWriteMasked(nDev,49,0x80,0x80);if (iRetVal) return iRetVal;

    //If using down-spread...SKIP

    //Enable Outputs
    iRetVal = ClockGenWriteMasked(nDev,230,0x00,0x10);if (iRetVal) return iRetVal;

    return 0;
}

static void init_gt_clks(unsigned char ucDev)
{
    int i,err;
    zynq_i2c_init(0,0);
    for (i=0;i<NUM_CLKBUF_COMMANDS;i++){
        if (Reg_Store[i].Reg_Mask != 0x00) {
            err = ClockGenWriteMasked(
                ucDev, Reg_Store[i].Reg_Addr, Reg_Store[i].Reg_Val, Reg_Store[i].Reg_Mask);
            if (err){
                xil_printf("ERROR, ClockGenWriteMasked() returned %d\n",err);
                return;
            }
        }
    }

    err = __IV_ResetSi5338Pll(ucDev, 1);
    if (err){
        xil_printf("ERROR, __IV_ResetSi5338Pll returned %d\n",err);
    }
}

#define IOU_SLCR_MIO_PIN_8_OFFSET 0xff180020
#define IOU_SLCR_MIO_PIN_9_OFFSET 0xff180024

void __attribute__((weak)) iv_z8_init_before_hook()
{
    PSU_Mask_Write (0XFF180208, 0x0003DFC3, 0x00000082);
    PSU_Mask_Write (0XFF5E0238, 0x00100000, 0x00000000);
    PSU_Mask_Write (0XFF5E0238, 0x00000600, 0x00000000);

    PSU_Mask_Write (IOU_SLCR_MIO_PIN_8_OFFSET ,0x000000FEU ,0x00000040U);
    PSU_Mask_Write (IOU_SLCR_MIO_PIN_9_OFFSET ,0x000000FEU ,0x00000040U);

    //Take out of reset
    jsm_clrbits_le32(0xff5e0238, 1 << 10);
}

void __attribute__((weak)) iv_z8_init_after_hook()
{
}

int __attribute__((weak)) iv_z8_has_clock_chip()
{
    return 1;
}

void iv_z8_init(){
    iv_z8_init_before_hook();
    if (iv_z8_has_clock_chip()) {
        init_gt_clks(0x70);
    }
    iv_z8_init_after_hook();
}
