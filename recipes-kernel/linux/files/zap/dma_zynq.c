/*
 * ZAP DMA engine
 *
 * (C) Copyright 2008-2012, iVeia, LLC
 */
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/irq.h>

#include <linux/of.h>
#include <linux/irq.h>
#include <linux/of_irq.h>

#include "_zap.h"
#include "dma.h"

#define MAPIT

volatile void * tx_vaddr;
volatile void * rx_vaddr;

#define rx_buffer2phys( vaddr ) \
	((unsigned long) vaddr - (unsigned long) pdma_if->rx_buffer_vaddr + pdma_if->rx_buffer_paddr )

#define tx_buffer2phys( vaddr ) \
	((unsigned long) vaddr - (unsigned long) pdma_if->tx_buffer_vaddr + pdma_if->tx_buffer_paddr )

#define tx_phys2buffer( phys ) \
	((unsigned long) phys - (unsigned long) pdma_if->tx_buffer_paddr + pdma_if->tx_buffer_vaddr )

///////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////
#if defined (CONFIG_ARCH_ZYNQMP)
    #define ZAP_REG_BASE_ADDR       0x98000000
    #define ZAP_IRQ                 105
#else
    #define ZAP_IRQ                 (91-32)
    #if defined (CONFIG_IVEIA_FRAMEWORK_ON_GP1)
        #define ZAP_REG_BASE_ADDR   0xB8000000
    #else
        #define ZAP_REG_BASE_ADDR	0x78000000
    #endif
#endif

#define ZAP_REG_BASE_SIZE	0x1000

#define ZAP_REG_NUM_PORTS   0x00000018
#define ZAP_REG_ISR	    0x00000004

//START INTERFACE DEPENDENT REGS
#define ZAP_REG_CSR	0x00000000
	#define CSR_TX_JUMBO_EN	(1 << 5)
	#define CSR_RX_JUMBO_EN	(1 << 4)
	#define CSR_TX_OOB		(1 << 3)
	#define CSR_RX_OOB		(1 << 2)
	#define CSR_TXEN		(1 << 1)
	#define CSR_RXEN		(1 << 0)
#define ZAP_REG_ICR	0x00000004
	#define ICR_SET_TX_FREE_RDY	(1 << 19)
	#define ICR_SET_TX_FULL_RDY	(1 << 18)
	#define ICR_SET_RXRDY		(1 << 17)
	#define ICR_SET_GLBL		(1 << 16)

	#define ICR_CLR_TX_FREE_RDY	(1 << 3)
	#define ICR_CLR_TX_FULL_RDY	(1 << 2)
	#define ICR_CLR_RXRDY		(1 << 1)
	#define ICR_CLR_GLBL		(1 << 0)

	#define ICR_INT_PEND		(1 << 31)
	#define ICR_MSK_TX_FREE_RDY	(1 << 19)
	#define ICR_MSK_TX_FULL_RDY	(1 << 18)
	#define ICR_MSK_RXRDY		(1 << 17)
	#define ICR_MSK_GLBL		(1 << 16)

	#define ICR_INT_TX_FREE_RDY	(1 << 3)
	#define ICR_INT_TX_FULL_RDY	(1 << 2)
	#define ICR_INT_RXRDY		(1 << 1)
	#define ICR_INT_GLBL		(1 << 0)

#define ZAP_REG_TBAR	0x00000008
#define ZAP_REG_RBAR	0x0000000C
#define ZAP_REG_BSR	0x00000010
	#define RX_SIZE_OOB_OVERFLOW_ERR	(0x80000000)
	#define RX_SIZE_DAT_OVERFLOW_ERR	(0x40000000)

#define ZAP_REG_HIGH_WATER_MARK	(0x00000018)
	#define HIGH_WATER_RX_SHIFT 0
	#define HIGH_WATER_TX_SHIFT	16

#define ZAP_REG_RX_FIFO_SIZE	(0x00000014)
#define ZAP_REG_TX_FIFO_SIZE	(0x0000001c)

#define ZAP_REG_MAX_RX_SIZE		(0x00000014)
	#define MAX_RX_OOB_SIZE_SHIFT	16
	#define MAX_RX_OOB_SIZE_MASK	0xFFFF0000
	#define MAX_RX_DAT_SIZE_SHIFT	0
	#define MAX_RX_DAT_SIZE_MASK	0x0000FFFF

#define ZAP_REG_READ_VERSION    (0x00000018)

#define FPGA_VERSION_REG		(0x00000000)
	#define FPGA_VERSION_BOARD_SHIFT	24
	#define FPGA_VERSION_MAJOR_SHIFT	16
	#define FPGA_VERSION_MINOR_SHIFT	8
	#define FPGA_VERSION_RELEASE_SHIFT	0

#define _ZAP_REG_READ(reg)			\
	ioread32(&pdma_if->zap_reg[reg])
#define _ZAP_REG_WRITE(reg, val)			\
	iowrite32(val,&pdma_if->zap_reg[reg])
#define _ZAP_REG_WRITE_MASKED(reg, val, mask)	\
	_ZAP_REG_WRITE( reg, (val & mask) | (_ZAP_REG_READ(reg) & ~mask))

#define ZAP_POOL_MIN_SIZE  (0 * 1024 * 1024)

spinlock_t lock2;

struct dma_if_interface {
    int iDevice;
	int rx_on, tx_on;
	int rx_dma_count, tx_dma_count;
	struct workqueue_struct * prx_workqueue;
	struct workqueue_struct * ptx_workqueue;
    struct work_struct rx_work;
    struct work_struct tx_work;
};

struct dma_if {
	struct zap_dev * zap_dev;
	char * zap_reg;
	int irq;
	void * rx_buffer_vaddr;
	phys_addr_t rx_buffer_paddr;
	void * tx_buffer_vaddr;
	phys_addr_t tx_buffer_paddr;
	unsigned long rx_buffer_size;
	unsigned long tx_buffer_size;

	//Originaly in DMA High Level (pdma)
	//int on;
	//struct work_struct rx_work;
	//struct work_struct tx_work;

    struct dma_if_interface interface[ZAP_MAX_DEVICES];
};

struct dma_if dma_if;
struct dma_if * pdma_if = &dma_if;

uint32_t VSOC_ZAP_REG_READ(uint32_t reg) {
//printk("ZAP_REG_READ %08X, %08X, %08X\n", reg, 0, 0);
	uint32_t ulRetVal;
	unsigned long irqflags;
	spin_lock_irqsave( &lock2, irqflags );
	ulRetVal = _ZAP_REG_READ(reg);
	spin_unlock_irqrestore( &lock2, irqflags );
	return ulRetVal;
}

void VSOC_ZAP_REG_WRITE(uint32_t reg, uint32_t val) {
//printk("ZAP_REG_WRITE %08X, %08X, %08X\n", reg, val, 0);
	unsigned long irqflags;
	spin_lock_irqsave( &lock2, irqflags );
	_ZAP_REG_WRITE(reg, val);
	spin_unlock_irqrestore( &lock2, irqflags );
}
void VSOC_ZAP_REG_WRITE_MASKED(uint32_t reg, uint32_t val, uint32_t mask) {
//printk("ZAP_REG_WRITE_MASKED %08X, %08X, %08X\n", reg, val, mask);
	unsigned long irqflags;
	spin_lock_irqsave( &lock2, irqflags );
	_ZAP_REG_WRITE_MASKED(reg, val, mask);
	spin_unlock_irqrestore( &lock2, irqflags );
}


uint32_t ZAP_REG_READ(int iDevice, uint32_t reg){
    return VSOC_ZAP_REG_READ(iDevice * 0x00000020 + reg);
}

void ZAP_REG_WRITE(int iDevice, uint32_t reg, uint32_t val){
    VSOC_ZAP_REG_WRITE(iDevice * 0x00000020 + reg, val);
}

void ZAP_REG_WRITE_MASKED(int iDevice, uint32_t reg, uint32_t val, uint32_t mask){
    VSOC_ZAP_REG_WRITE_MASKED(iDevice * 0x00000020 + reg, val, mask);
}

///////////////////////////////////////////////////////////////////////////
//
// Private funcs
//
///////////////////////////////////////////////////////////////////////////

//
// Append a buffer descriptor to the tail of the BD ring.
//
void
dma_ll_put_rx(
    int iDevice,
	phys_addr_t pbuf, 
	unsigned long len 
	)
{
	ZAP_REG_WRITE(iDevice, ZAP_REG_RBAR, (uint32_t) pbuf);
}


void
dma_ll_put_tx(
	phys_addr_t pbuf, 
	unsigned long len,
	unsigned long ooblen 
	)
{
}


//
// While DMA is running, get a buf for TX DMA.  If this fails, we've been
// interrupted, save error status and stop dma engine.
//
static void 
dma_rx_task(
	struct work_struct * work
	)
{
	void * pbuf;
	unsigned long len;
	int err;
    struct dma_if_interface * pdma_if_interface;
    int iDevice;
    phys_addr_t paddr;

    pdma_if_interface = container_of(work, struct dma_if_interface, rx_work);
    iDevice = pdma_if_interface->iDevice;

    //printk(KERN_ERR "ENTERED DMA_RX_TASK, iDevice = %d\n",iDevice);

	while ( pdma_if->interface[iDevice].rx_on ) {
		do {
			err = pool_getbuf_timeout( 
				&pdma_if->zap_dev->interface[iDevice].rx_pool, &pbuf, &len, HZ/2 );
		} while ( err == -EAGAIN && pdma_if->interface[iDevice].rx_on );
		if ( err ) {
			pdma_if->interface[iDevice].rx_on = 0;
		} else {
			paddr = rx_buffer2phys(pbuf);
			dma_ll_put_rx( iDevice, paddr, len );
		}
	}
}

static struct of_device_id gic_match[] = {
	{ .compatible = "arm,gic-400", },//Z8
	{ .compatible = "arm,cortex-a9-gic", },
	{ .compatible = "arm,cortex-a15-gic", },
	{ },
};

static struct device_node *gic_node;

unsigned int xlate_irq(unsigned int hwirq)
{
        struct of_phandle_args irq_data;
        unsigned int irq;

        if (!gic_node)
                gic_node = of_find_matching_node(NULL, gic_match);

        if (WARN_ON(!gic_node))
                return hwirq;

        irq_data.np = gic_node;
        irq_data.args_count = 3;
        irq_data.args[0] = 0;
        irq_data.args[1] = hwirq; /* GIC SPI offset */
        irq_data.args[2] = IRQ_TYPE_LEVEL_HIGH;

        irq = irq_create_of_mapping(&irq_data);
        if (WARN_ON(!irq))
                irq = hwirq;

        pr_info("%s: hwirq %d, irq %d\n", __func__, hwirq, irq);

        return irq;
}

static irqreturn_t 
dma_isr(
	int irq, 
	void * dev_id
	)
{
	int err;
	int iRetVal;
	uint32_t len = 0;
	uint32_t ooblen = 0;
	unsigned long flags = 0;
	unsigned long ulTemp;
	void * pbuf;
	uint32_t icr;
    int iDevice;

    unsigned long ulLen, ulOoblen;

    unsigned long offset;
    phys_addr_t paddr;

	//
	// Ack interrupt, a writeback will ack all pending intr bits.
	//
	//icr = ZAP_REG_READ(ZAP_REG_ICR);
    icr = ZAP_REG_READ(0,ZAP_REG_ISR);
    iDevice = (int)((icr >> 8) & 0x000000ff);
    //iDevice = 1;

    //printk(KERN_ERR "dma_isr, iDevice = %d, icr = 0x%08x\n",iDevice,icr);

	if (! (icr & ICR_INT_PEND)) {
		// Spurious interrupt - should not occur.
		printk(KERN_ERR MODNAME ": ZAP DMA spurous interrupt (ICR: %08X)", icr);
		return IRQ_HANDLED;
	}
	

		if ( (icr & ICR_INT_RXRDY) && (icr & ICR_MSK_RXRDY) ) {

			paddr = (phys_addr_t)ZAP_REG_READ(iDevice, ZAP_REG_RBAR);
			offset = (unsigned long)paddr;

			pdma_if->interface[iDevice].rx_dma_count++;

			offset -= pdma_if->zap_dev->interface[iDevice].rx_paddr;
			offset -= pool_packets_offset(&pdma_if->zap_dev->interface[iDevice].rx_pool);
			pbuf = pool_offset2pbuf(&pdma_if->zap_dev->interface[iDevice].rx_pool, offset);

			len = ZAP_REG_READ(iDevice, ZAP_REG_BSR);

			flags = 0;
			if (pdma_if->zap_dev->interface[iDevice].rx_jumbo_pkt_enable == 0){
				if (len & RX_SIZE_OOB_OVERFLOW_ERR)
					flags |= ZAP_DESC_FLAG_OVERFLOW_OOB;

				if (len & RX_SIZE_DAT_OVERFLOW_ERR)
					flags |= ZAP_DESC_FLAG_OVERFLOW_DATA;

				ooblen = 0x00003FFF & (len >> 16);
				ooblen = ooblen << 2;//(Times 4)
				len &= 0x0000FFFF;
				len = len << 2;//(Times 4)

				if ( (!pdma_if->zap_dev->interface[iDevice].rx_header_enable) && ooblen!=0){
					printk(KERN_ERR MODNAME "**ERROR OOB_SIZE RETURNED AS %d\n",ooblen);
				}
			}else{ // jumbo_pkt_enable = 1
				ooblen = 0;
				len = len << 2;//Time 4
			}

#ifdef MAPIT
			//dma_unmap_single(pdma_if->zap_dev->zap_device, paddr, (size_t)len, DMA_FROM_DEVICE );
#else
			//dma_unmap_single( NULL, paddr, len, DMA_FROM_DEVICE );
#endif

			err = pool_enqbuf(&pdma_if->zap_dev->interface[iDevice].rx_pool, pbuf, (unsigned long)len, (unsigned long)ooblen, flags);
			if (err) {
				printk(KERN_ERR MODNAME "**ERROR RUNNING pool_enqbuf\n");
				pool_dump(&pdma_if->zap_dev->interface[iDevice].rx_pool, 0);
				BUG_ON(err);
			}
		}

		// TX
/*
		if (icr & ICR_INT_TXERR) {
			pool_dump(&pdma_if->zap_dev->tx_pool, 0);
			BUG_ON(1);
			//err = ZAP_REG_READ(ZAP_REG_ER);
			//printk(KERN_ERR MODNAME ": ZAP DMA TXER %08X", err);
			printk(KERN_ERR MODNAME ": ZAP DMA TXER");

		}
*/

		if ( (icr & ICR_INT_TX_FULL_RDY) && (icr & ICR_MSK_TX_FULL_RDY) ){
			if (list_empty(&pdma_if->zap_dev->interface[iDevice].tx_pool.fifolist)){
				ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_CLR_TX_FULL_RDY);
			}else{

				pdma_if->interface[iDevice].tx_dma_count++;

				iRetVal = pool_deqbuf_try(	&pdma_if->zap_dev->interface[iDevice].tx_pool, &pbuf, &ulLen, &ulOoblen, &flags );
				if (!iRetVal){
					printk(KERN_ERR MODNAME "ERROR: ICR_INT_TX_FULL_RDY Active, could not deqbuf\n");//THIS HAPPENS
					return -1;//FAIL
				}

				if (pdma_if->zap_dev->interface[iDevice].tx_jumbo_pkt_enable == 0){
					ulTemp = 0x0000ffff & (ulLen >> 2);
					ulTemp |= ((ulOoblen << 14) & 0xffff0000);
				}else{
					ulTemp = ulLen >> 2;
				}

				ZAP_REG_WRITE(iDevice, ZAP_REG_TBAR, (uint32_t)tx_buffer2phys( pbuf ));
				ZAP_REG_WRITE(iDevice, ZAP_REG_BSR, (uint32_t)ulTemp);

				if (list_empty(&pdma_if->zap_dev->interface[iDevice].tx_pool.fifolist)){
				//If we just wrote the last packet in the pool, unmask interrupt
					ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_CLR_TX_FULL_RDY);
				}
			}
		}

		if ( (icr & ICR_INT_TX_FREE_RDY) && (icr & ICR_MSK_TX_FREE_RDY)){

			ulTemp = (unsigned long)ZAP_REG_READ(iDevice, ZAP_REG_TBAR);

#ifdef MAPIT
			//dma_unmap_single( NULL, (char *) ulTemp, pdma_if->zap_dev->tx_payload_max_size, DMA_TO_DEVICE );
#endif
			pbuf = tx_phys2buffer(ulTemp);

			iRetVal = pool_freebuf(&pdma_if->zap_dev->interface[iDevice].tx_pool, pbuf);
			if (iRetVal < 0){
				printk(KERN_ERR MODNAME "ERROR: Pool_freebuf returned %d\n",iRetVal);
			}

		}

		//icr = ZAP_REG_READ(iDevice, ZAP_REG_ICR);

	//}

	return IRQ_HANDLED;
}


///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////

int
dma_ll_start_tx(
	int iDevice
	)
{

	pdma_if->interface[iDevice].tx_dma_count = 0;

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TXEN);

	udelay(1);
	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_TXEN, CSR_TXEN);

	if (pdma_if->zap_dev->interface[iDevice].tx_header_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_TX_OOB, CSR_TX_OOB);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TX_OOB);	

	if (pdma_if->zap_dev->interface[iDevice].tx_jumbo_pkt_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_TX_JUMBO_EN, CSR_TX_JUMBO_EN);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TX_JUMBO_EN);

	//Implement masked in future?
	//ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TXERR | ICR_SET_TX_FREE_RDY | ICR_SET_GLBL);
    ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TX_FREE_RDY | ICR_SET_GLBL);
	//ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TXERR | ICR_SET_GLBL);

	pdma_if->interface[iDevice].tx_on = 1;

	return 0;
}

int
dma_ll_start_rx(
	int iDevice
	)
{
	unsigned long ulPayloadWords;
	unsigned long ulOobWords;
	unsigned long ulTemp;
	int err = 0;

	pdma_if->interface[iDevice].rx_dma_count = 0;

/*
	pdma_if->irq = 91;
	err = request_irq(pdma_if->irq, dma_isr, 0, "zap_dma", NULL);
	if (err) {
		pdma_if->irq = -1;
		return err;
	}
*/

	//
	// Reset Zap and enable interrupts
	//

	ulPayloadWords = pdma_if->zap_dev->interface[iDevice].rx_payload_max_size >> 2;
	if (pdma_if->zap_dev->interface[iDevice].rx_header_enable)
		ulOobWords = pdma_if->zap_dev->interface[iDevice].rx_header_size >> 2;
	else
		ulOobWords = 0;

	ulPayloadWords -= ulOobWords;

	if (pdma_if->zap_dev->interface[iDevice].rx_jumbo_pkt_enable == 0){
		ulTemp = 0;
		ulTemp |= ( (ulPayloadWords - 1) << MAX_RX_DAT_SIZE_SHIFT ) & MAX_RX_DAT_SIZE_MASK;
		ulTemp |= ( (ulOobWords - 1) << MAX_RX_OOB_SIZE_SHIFT ) & MAX_RX_OOB_SIZE_MASK;
	}else{
		ulTemp = (ulPayloadWords - 1);
	}

	ZAP_REG_WRITE(iDevice, ZAP_REG_MAX_RX_SIZE, (uint32_t)ulTemp);

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RXEN);
	udelay(1);
	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_RXEN, CSR_RXEN);

	if (pdma_if->zap_dev->interface[iDevice].rx_header_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_RX_OOB, CSR_RX_OOB);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RX_OOB);	

	if (pdma_if->zap_dev->interface[iDevice].rx_jumbo_pkt_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_RX_JUMBO_EN, CSR_RX_JUMBO_EN);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RX_JUMBO_EN);

	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_RXRDY | ICR_SET_GLBL);

	pdma_if->interface[iDevice].rx_on = 1;
	queue_work( pdma_if->interface[iDevice].prx_workqueue, &pdma_if->interface[iDevice].rx_work );
	//queue_work( pdma_if->ptx_workqueue, &pdma_if->tx_work );

	return err;
}

int
dma_ll_stop_tx(
	int iDevice
	)
{

	int err;
	pdma_if->interface[iDevice].tx_on = 0;

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TXEN);
	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_CLR_TX_FREE_RDY);

	//Need to somehow refill pool?

	//flush_workqueue( pdma_if->prx_workqueue );
	flush_workqueue( pdma_if->interface[iDevice].ptx_workqueue );

	err = pool_flush( &pdma_if->zap_dev->interface[iDevice].tx_pool );
	if ( err ) return err;
	err = pool_busy( &pdma_if->zap_dev->interface[iDevice].tx_pool );
	if ( err ) return err;

	pdma_if->interface[iDevice].tx_on = 0;

	return 0;

}

int
dma_ll_stop_rx(
	int iDevice
	)
{
	int err;

	pdma_if->interface[iDevice].rx_on = 0;

	//
	// Disable zap.
	//
	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RXEN);
/*
	if (pdma_if->irq >= 0){
		free_irq(pdma_if->irq, NULL);
		pdma_if->irq = -1;
	}
*/

	flush_workqueue( pdma_if->interface[iDevice].prx_workqueue );
	//flush_workqueue( pdma_if->ptx_workqueue );

	err = pool_flush( &pdma_if->zap_dev->interface[iDevice].rx_pool );
	if ( err ) return err;

	err = pool_busy( &pdma_if->zap_dev->interface[iDevice].rx_pool );
	if ( err ) return err;

	pdma_if->interface[iDevice].rx_on = 0;

	return 0;
}

int 
dma_ll_init(
	struct zap_dev * dev,
	phys_addr_t * prx_pool_paddr,
	void ** prx_pool_vaddr,
	unsigned long * prx_pool_size,
	phys_addr_t * ptx_pool_paddr,
	void ** ptx_pool_vaddr,
	unsigned long * ptx_pool_size
	)
{
	unsigned long zap_pool_paddr;
	unsigned long zap_pool_size;
	unsigned long pow2_boundary_pages = 1;

	int err;
    int i;

	//memset( pdma, 0, sizeof(struct dma) );

	// 
	// Create buffer pool at the end of mem, half for tx, half for rx, with a
	// small chunk at the end for DMA buf descriptors..  The size is the rest
	// of RAM not allocated to Linux, up to the next power-of-two boundary.
	//

#if 1
	while (pow2_boundary_pages < get_num_physpages()) pow2_boundary_pages <<= 1;
	zap_pool_paddr = get_num_physpages() * PAGE_SIZE;
	zap_pool_size =  (pow2_boundary_pages - get_num_physpages()) * PAGE_SIZE;

	if (zap_pool_size < ZAP_POOL_MIN_SIZE) {
		printk(
			KERN_ERR MODNAME ": pool size (%08X) is less than minimum (%08X)\n", 
			(unsigned int) zap_pool_size, 
			ZAP_POOL_MIN_SIZE 
			);
		return -ENOMEM;
	}
	//zap_pool_size = 8*1024*1024;

#ifdef MAPIT
	pdma_if->rx_buffer_paddr = __pa(rx_vaddr);
	pdma_if->rx_buffer_size = 4*1024*1024;
	pdma_if->tx_buffer_paddr = __pa(tx_vaddr);
	pdma_if->tx_buffer_size = 4*1024*1024;

    //Syntax tells compiler that I know it's volatile. Supresses "assignment discards 'volatile' qualifier from pointer target type" warning
	pdma_if->rx_buffer_vaddr = *(void **)&rx_vaddr;
	pdma_if->tx_buffer_vaddr = *(void **)&tx_vaddr;
#else
//Removed code, it was confusing
#endif
#endif

//END HERE
	*prx_pool_paddr = pdma_if->rx_buffer_paddr;
	*prx_pool_vaddr = pdma_if->rx_buffer_vaddr;
	*prx_pool_size= pdma_if->rx_buffer_size;
	*ptx_pool_paddr = pdma_if->tx_buffer_paddr;
	*ptx_pool_vaddr = pdma_if->tx_buffer_vaddr;
	*ptx_pool_size = pdma_if->tx_buffer_size;

	pdma_if->zap_dev = dev;

	pdma_if->irq = -1;

	pdma_if->zap_reg = ioremap(ZAP_REG_BASE_ADDR, ZAP_REG_BASE_SIZE);
	if (pdma_if->zap_reg == NULL) return -ENOMEM;

    for (i=0;i<ZAP_MAX_DEVICES;i++){
	    pdma_if->interface[i].rx_on = 0;
	    pdma_if->interface[i].tx_on = 0;
        pdma_if->interface[i].iDevice = i;
	    pdma_if->interface[i].prx_workqueue = create_singlethread_workqueue( "dma_rx_wq" );
	    pdma_if->interface[i].ptx_workqueue = create_singlethread_workqueue( "dma_tx_wq" );
	    INIT_WORK( &pdma_if->interface[i].rx_work, dma_rx_task );
	    //INIT_WORK( &pdma_if->interface[i].tx_work, dma_tx_task );
    }

	/* The IRQ resource */
	pdma_if->irq = xlate_irq(ZAP_IRQ);
	if (pdma_if->irq <= 0) {
		pr_err("get_resource for IRQ for ZAP dev failed\n");
		return -ENODEV;
	
	}
    err = request_irq(pdma_if->irq, dma_isr, 0, "zap_dma", NULL);
	if (err) {
		pr_err("unable to request ZAP IRQ\n");
		return err;
	}


	return 0;
}

int 
dma_ll_cleanup(
	void
	)
{
    int i;

	if (pdma_if->zap_reg) {
		iounmap(pdma_if->zap_reg);
	}

	if (pdma_if->rx_buffer_vaddr)
		iounmap(pdma_if->rx_buffer_vaddr);
	if (pdma_if->tx_buffer_vaddr)
		iounmap(pdma_if->tx_buffer_vaddr);

    for (i=0;i<ZAP_MAX_DEVICES;i++){
	    destroy_workqueue( pdma_if->interface[i].prx_workqueue );
	    destroy_workqueue( pdma_if->interface[i].ptx_workqueue );
    }

	if (pdma_if->irq >= 0){
		free_irq(pdma_if->irq, NULL);
		pdma_if->irq = -1;
	}

	return 0;
}

int 
dma_ll_rx_is_on(
	int iDevice
	)
{
	return pdma_if->interface[iDevice].rx_on;
}

int 
dma_ll_tx_is_on(
	int iDevice
	)
{
	return pdma_if->interface[iDevice].tx_on;
}

void
dma_ll_tx_write_buf(
    int iDevice)
{
	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TX_FULL_RDY);
}

void
dma_ll_update_high_water_marks(int iDevice){
	unsigned long ulTemp;

	ulTemp = (unsigned long)ZAP_REG_READ(iDevice, ZAP_REG_HIGH_WATER_MARK);
	pdma_if->zap_dev->interface[iDevice].rx_highwater = 4 * ((ulTemp >> HIGH_WATER_RX_SHIFT) & 0x0000ffff);
	pdma_if->zap_dev->interface[iDevice].tx_highwater = 4 * ((ulTemp >> HIGH_WATER_TX_SHIFT) & 0x0000ffff);
}

int dma_ll_rx_dma_count(int iDevice){ return pdma_if->interface[iDevice].rx_dma_count; }
int dma_ll_tx_dma_count(int iDevice){ return pdma_if->interface[iDevice].tx_dma_count; }

void
dma_ll_update_fpga_parameters()
{
	uint32_t ulTemp;

    //Cannot do this while a device is open, because I need to alter the return value of some registers
    //This was done so that the register map looks like that of the old Zynq ZAP interface
  if (pdma_if->zap_dev->open_count == 0){

    ZAP_REG_WRITE(0,ZAP_REG_READ_VERSION,0x00000001);

	ulTemp = ZAP_REG_READ(1,FPGA_VERSION_REG);

	pdma_if->zap_dev->fpga_params.fpga_board_code = (ulTemp >> FPGA_VERSION_BOARD_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_major = (ulTemp >> FPGA_VERSION_MAJOR_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_minor = (ulTemp >> FPGA_VERSION_MINOR_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_release = (ulTemp >> FPGA_VERSION_RELEASE_SHIFT) & 0x000000ff;

	//Update FIFO Size Registers
	ulTemp = ZAP_REG_READ(0, ZAP_REG_RX_FIFO_SIZE);
	pdma_if->zap_dev->fpga_params.rx_dat_fifo_size = 4 * (ulTemp & 0x0000ffff);
	pdma_if->zap_dev->fpga_params.rx_oob_fifo_size = 4 * ((ulTemp >> 16) & 0x0000ffff);

	ulTemp = ZAP_REG_READ(0, ZAP_REG_TX_FIFO_SIZE);
	pdma_if->zap_dev->fpga_params.tx_dat_fifo_size = 4 * (ulTemp & 0x0000ffff);
	pdma_if->zap_dev->fpga_params.tx_oob_fifo_size = 4 * ((ulTemp >> 16) & 0x0000ffff);

    ulTemp = ZAP_REG_READ(0,ZAP_REG_NUM_PORTS);
    pdma_if->zap_dev->fpga_params.num_interfaces = (int) ulTemp;

    if (pdma_if->zap_dev->fpga_params.num_interfaces == 0) //Incase an older FPGA version is present
        pdma_if->zap_dev->fpga_params.num_interfaces = 1;

    ZAP_REG_WRITE(0,ZAP_REG_READ_VERSION,0x00000000);

  }

}
