/*
 * iv_z8_init.c
 *
 * (C) Copyright 2017-2020, iVeia
 */
#include <xil_io.h>
#include <sleep.h>

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

struct {
    unsigned char ucAddr;
    unsigned char ucVal;
    unsigned char ucMask;
} grInternalClkBufCmds[] =
{
{  0,0x00,0x00},
{  1,0x00,0x00},
{  2,0x00,0x00},
{  3,0x00,0x00},
{  4,0x00,0x00},
{  5,0x00,0x00},
{  6,0x00,0xDF},
{  7,0x00,0x00},
{  8,0x70,0x00},
{  9,0x0F,0x00},
{ 10,0x00,0x00},
{ 11,0x00,0x00},
{ 12,0x00,0x00},
{ 13,0x00,0x00},
{ 14,0x00,0x00},
{ 15,0x00,0x00},
{ 16,0x00,0x00},
{ 17,0x00,0x00},
{ 18,0x00,0x00},
{ 19,0x00,0x00},
{ 20,0x00,0x00},
{ 21,0x00,0x00},
{ 22,0x00,0x00},
{ 23,0x00,0x00},
{ 24,0x00,0x00},
{ 25,0x00,0x00},
{ 26,0x00,0x00},
{ 27,0x70,0x80},
{ 28,0x83,0xFF},
{ 29,0x42,0xFF},
{ 30,0xB0,0xFF},
{ 31,0xC0,0xFF},
{ 32,0xC0,0xFF},
{ 33,0xC0,0xFF},
{ 34,0xC0,0xFF},
{ 35,0xAA,0xFF},
{ 36,0x06,0xFF},
{ 37,0x06,0xFF},
{ 38,0x06,0xFF},
{ 39,0x06,0xFF},
{ 40,0x84,0xFF},
{ 41,0x10,0xFF},
{ 42,0x24,0x3F},
{ 43,0x00,0x00},
{ 44,0x00,0x00},
{ 45,0x00,0x00},
{ 46,0x00,0x00},
{ 47,0x3C,0x3C},
{ 48,0x25,0xFF},
{ 49,0x00,0x7F},
{ 50,0xC4,0xFF},
{ 51,0x07,0xF7},
{ 52,0x10,0xFF},
{ 53,0x80,0xFF},
{ 54,0x0A,0xFF},
{ 55,0x00,0xFF},
{ 56,0x00,0xFF},
{ 57,0x00,0xFF},
{ 58,0x00,0xFF},
{ 59,0x01,0xFF},
{ 60,0x00,0xFF},
{ 61,0x00,0xFF},
{ 62,0x00,0x3F},
{ 63,0x10,0xFF},
{ 64,0x00,0xFF},
{ 65,0x08,0xFF},
{ 66,0x00,0xFF},
{ 67,0x00,0xFF},
{ 68,0x00,0xFF},
{ 69,0x00,0xFF},
{ 70,0x01,0xFF},
{ 71,0x00,0xFF},
{ 72,0x00,0xFF},
{ 73,0x00,0x3F},
{ 74,0x10,0xFF},
{ 75,0x13,0xFF},
{ 76,0x2E,0xFF},
{ 77,0x24,0xFF},
{ 78,0x00,0xFF},
{ 79,0x00,0xFF},
{ 80,0x00,0xFF},
{ 81,0x0D,0xFF},
{ 82,0x00,0xFF},
{ 83,0x00,0xFF},
{ 84,0x00,0x3F},
{ 85,0x10,0xFF},
{ 86,0x00,0xFF},
{ 87,0x08,0xFF},
{ 88,0x00,0xFF},
{ 89,0x00,0xFF},
{ 90,0x00,0xFF},
{ 91,0x00,0xFF},
{ 92,0x01,0xFF},
{ 93,0x00,0xFF},
{ 94,0x00,0xFF},
{ 95,0x00,0x3F},
{ 96,0x10,0x00},
{ 97,0x00,0xFF},
{ 98,0x1E,0xFF},
{ 99,0x00,0xFF},
{100,0x00,0xFF},
{101,0x00,0xFF},
{102,0x00,0xFF},
{103,0x01,0xFF},
{104,0x00,0xFF},
{105,0x00,0xFF},
{106,0x80,0xBF},
{107,0x00,0xFF},
{108,0x00,0xFF},
{109,0x00,0xFF},
{110,0x40,0xFF},
{111,0x00,0xFF},
{112,0x00,0xFF},
{113,0x00,0xFF},
{114,0x40,0xFF},
{115,0x00,0xFF},
{116,0x80,0xFF},
{117,0x00,0xFF},
{118,0x40,0xFF},
{119,0x00,0xFF},
{120,0x00,0x7F},
{121,0x00,0xFF},
{122,0x40,0xFF},
{123,0x00,0xFF},
{124,0x00,0xFF},
{125,0x00,0xFF},
{126,0x00,0xFF},
{127,0x00,0xFF},
{128,0x00,0xFF},
{129,0x00,0x0F},
{130,0x00,0x7F},
{131,0x00,0xFF},
{132,0x00,0xFF},
{133,0x00,0xFF},
{134,0x00,0xFF},
{135,0x00,0xFF},
{136,0x00,0xFF},
{137,0x00,0xFF},
{138,0x00,0xFF},
{139,0x00,0xFF},
{140,0x00,0xFF},
{141,0x00,0xFF},
{142,0x00,0xFF},
{143,0x00,0xFF},
{144,0x00,0xFF},
{145,0x00,0x00},
{146,0xFF,0x00},
{147,0x00,0x00},
{148,0x00,0x00},
{149,0x00,0x00},
{150,0x00,0x00},
{151,0x00,0x00},
{152,0x00,0xFF},
{153,0x00,0xFF},
{154,0x00,0xFF},
{155,0x00,0xFF},
{156,0x00,0xFF},
{157,0x00,0xFF},
{158,0x00,0xFF},
{159,0x00,0xFF},
{160,0x00,0xFF},
{161,0x00,0xFF},
{162,0x00,0xFF},
{163,0x00,0xFF},
{164,0x00,0xFF},
{165,0x00,0xFF},
{166,0x00,0xFF},
{167,0x00,0xFF},
{168,0x00,0xFF},
{169,0x00,0xFF},
{170,0x00,0xFF},
{171,0x00,0xFF},
{172,0x00,0xFF},
{173,0x00,0xFF},
{174,0x00,0xFF},
{175,0x00,0xFF},
{176,0x00,0xFF},
{177,0x00,0xFF},
{178,0x00,0xFF},
{179,0x00,0xFF},
{180,0x00,0xFF},
{181,0x00,0xFF},
{182,0x00,0xFF},
{183,0x00,0xFF},
{184,0x00,0xFF},
{185,0x00,0xFF},
{186,0x00,0xFF},
{187,0x00,0xFF},
{188,0x00,0xFF},
{189,0x00,0xFF},
{190,0x00,0xFF},
{191,0x00,0xFF},
{192,0x00,0xFF},
{193,0x00,0xFF},
{194,0x00,0xFF},
{195,0x00,0xFF},
{196,0x00,0xFF},
{197,0x00,0xFF},
{198,0x00,0xFF},
{199,0x00,0xFF},
{200,0x00,0xFF},
{201,0x00,0xFF},
{202,0x00,0xFF},
{203,0x00,0xFF},
{204,0x00,0xFF},
{205,0x00,0xFF},
{206,0x00,0xFF},
{207,0x00,0xFF},
{208,0x00,0xFF},
{209,0x00,0xFF},
{210,0x00,0xFF},
{211,0x00,0xFF},
{212,0x00,0xFF},
{213,0x00,0xFF},
{214,0x00,0xFF},
{215,0x00,0xFF},
{216,0x00,0xFF},
{217,0x00,0x7F},
{218,0x00,0x00},
{219,0x00,0x00},
{220,0x00,0x00},
{221,0x0D,0x00},
{222,0x00,0x00},
{223,0x00,0x00},
{224,0xF4,0x00},
{225,0xF0,0x00},
{226,0x00,0x00},
{227,0x00,0x00},
{228,0x00,0x00},
{229,0x00,0x00},
{230,0x00,0x00},
{231,0x00,0x00},
{232,0x00,0x00},
{233,0x00,0x00},
{234,0x00,0x00},
{235,0x00,0x00},
{236,0x00,0x00},
{237,0x00,0x00},
{238,0x14,0x00},
{239,0x00,0x00},
{240,0x00,0x00},
{241,0x85,0x00},
{242,0x02,0x00},
{243,0xF0,0x00},
{244,0x00,0x00},
{245,0x00,0x00},
{246,0x00,0x00},
{247,0x00,0x00},
{248,0x00,0x00},
{249,0xA8,0x00},
{250,0x00,0x00},
{251,0x84,0x00},
{252,0x00,0x00},
{253,0x00,0x00},
{254,0x00,0x00},
{255, 1, 0xFF}, // set page bit to 1
{  0,0x00,0x00},
{  1,0x00,0x00},
{  2,0x00,0x00},
{  3,0x00,0x00},
{  4,0x00,0x00},
{  5,0x00,0x00},
{  6,0x00,0x00},
{  7,0x00,0x00},
{  8,0x00,0x00},
{  9,0x00,0x00},
{ 10,0x00,0x00},
{ 11,0x00,0x00},
{ 12,0x00,0x00},
{ 13,0x00,0x00},
{ 14,0x00,0x00},
{ 15,0x00,0x00},
{ 16,0x00,0x00},
{ 17,0x01,0x00},
{ 18,0x00,0x00},
{ 19,0x00,0x00},
{ 20,0x90,0x00},
{ 21,0x31,0x00},
{ 22,0x00,0x00},
{ 23,0x00,0x00},
{ 24,0x01,0x00},
{ 25,0x00,0x00},
{ 26,0x00,0x00},
{ 27,0x00,0x00},
{ 28,0x00,0x00},
{ 29,0x00,0x00},
{ 30,0x00,0x00},
{ 31,0x00,0xFF},
{ 32,0x00,0x7F},
{ 33,0x01,0xFF},
{ 34,0x00,0x7F},
{ 35,0x00,0xFF},
{ 36,0x90,0xFF},
{ 37,0x31,0xFF},
{ 38,0x00,0xFF},
{ 39,0x00,0x7F},
{ 40,0x01,0xFF},
{ 41,0x00,0x7F},
{ 42,0x00,0xFF},
{ 43,0x00,0xFF},
{ 44,0x00,0x00},
{ 45,0x00,0x00},
{ 46,0x00,0x00},
{ 47,0x00,0xFF},
{ 48,0x00,0x7F},
{ 49,0x01,0xFF},
{ 50,0x00,0x7F},
{ 51,0x00,0xFF},
{ 52,0x90,0xFF},
{ 53,0x31,0xFF},
{ 54,0x00,0xFF},
{ 55,0x00,0x7F},
{ 56,0x01,0xFF},
{ 57,0x00,0x7F},
{ 58,0x00,0xFF},
{ 59,0x00,0xFF},
{ 60,0x00,0x00},
{ 61,0x00,0x00},
{ 62,0x00,0x00},
{ 63,0x00,0xFF},
{ 64,0x00,0x7F},
{ 65,0x01,0xFF},
{ 66,0x00,0x7F},
{ 67,0x00,0xFF},
{ 68,0x90,0xFF},
{ 69,0x31,0xFF},
{ 70,0x00,0xFF},
{ 71,0x00,0x7F},
{ 72,0x01,0xFF},
{ 73,0x00,0x7F},
{ 74,0x00,0xFF},
{ 75,0x00,0xFF},
{ 76,0x00,0x00},
{ 77,0x00,0x00},
{ 78,0x00,0x00},
{ 79,0x00,0xFF},
{ 80,0x00,0x7F},
{ 81,0x00,0xFF},
{ 82,0x00,0x7F},
{ 83,0x00,0xFF},
{ 84,0x90,0xFF},
{ 85,0x31,0xFF},
{ 86,0x00,0xFF},
{ 87,0x00,0x7F},
{ 88,0x01,0xFF},
{ 89,0x00,0x7F},
{ 90,0x00,0xFF},
{ 91,0x00,0xFF},
{ 92,0x00,0x00},
{ 93,0x00,0x00},
{ 94,0x00,0x00},
{255, 0, 0xFF}, // set page bit to 0
// soft reset
//{246, 2, 0xFF}
}; // set page bit to 0

#define NUM_CLKBUF_COMMANDS ARRAY_SIZE(grInternalClkBufCmds)

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

static void init_gt_clks(unsigned char ucDev){
//CLK0=100MHz
//CLK1=125MHz
//CLK2=26MHz
//CLK3=125MHz

    int i,err;
    zynq_i2c_init(0,0);
    for (i=0;i<NUM_CLKBUF_COMMANDS;i++){
        if (grInternalClkBufCmds[i].ucMask != 0x00) {
            err = ClockGenWriteMasked( ucDev, grInternalClkBufCmds[i].ucAddr, grInternalClkBufCmds[i].ucVal, grInternalClkBufCmds[i].ucMask);
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
