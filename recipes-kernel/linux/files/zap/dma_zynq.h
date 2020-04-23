/*
 * ZAP DMA 
 *
 * (C) Copyright 2012, iVeia, LLC
 */
#ifndef _DMA_ZYNQ_H_
#define _DMA_ZYNQ_H_

/*
 * Zynq doesn't use BDs.  This is just the max allowed buffers.
 *
 * TODO: define the max based on the size of the buffer pool.
 */
#define DMA_BD_RX_NUM ( 10000 )
#define DMA_BD_TX_NUM ( 10000 )
#endif

