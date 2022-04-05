/*
 * ZAP DMA engine
 *
 * (C) Copyright 2008-2021, iVeia, LLC
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

#include "_zap.h"
#include "dma.h"

///////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////

struct dma {
	struct zap_dev * zap_dev;
	struct semaphore sem;
};

struct dma dma;
struct dma * pdma = &dma;

///////////////////////////////////////////////////////////////////////////
//
// Private funcs
//
///////////////////////////////////////////////////////////////////////////

//
// starts the dma engine.
//
// Note: DMA engine MUST already be stopped!
//
static int
dma_start_rx_unsafe(
	int iDevice
	)
{
	int err = 0;

	err = pool_flush( &pdma->zap_dev->interface[iDevice].rx_pool );
	if ( err ) 
        return err;

	err = pool_busy( &pdma->zap_dev->interface[iDevice].rx_pool );
	if ( err ) 
        return err;

	dma_ll_start_rx(iDevice);

	return err;
}

static int
dma_start_tx_unsafe(
	int iDevice
	)
{
	int err = 0;

	err = pool_flush( &pdma->zap_dev->interface[iDevice].tx_pool );
	if ( err ) 
        return err;

	err = pool_busy( &pdma->zap_dev->interface[iDevice].tx_pool );
	if ( err ) 
        return err;

	dma_ll_start_tx(iDevice);

	return err;
}


static int
dma_stop_rx_unsafe(
	int iDevice
	)
{
	// Shut off DMA.  This will cause workqueues to stop.

	dma_ll_stop_rx(iDevice);

	return 0;
}

static int
dma_stop_tx_unsafe(
	int iDevice
	)
{
	// Shut off DMA.  This will cause workqueues to stop.

	dma_ll_stop_tx(iDevice);

	return 0;
}


///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////

int 
dma_init(
	struct zap_dev * dev,
    phys_addr_t rx_pool_paddr,
	unsigned long rx_pool_size,
    phys_addr_t tx_pool_paddr,
	unsigned long tx_pool_size
	)
{
	int err;

	err = dma_ll_init(dev, rx_pool_paddr, rx_pool_size, 
            tx_pool_paddr, tx_pool_size);
	if (err)
		return err;

	pdma->zap_dev = dev;

	sema_init( &pdma->sem , 1);

	return 0;
}


int 
dma_cleanup(
	int iNumPorts
	)
{
	int err;
	int i;

    for (i = 0; i < iNumPorts; i++) {
	    err = dma_stop_rx(i);
	    if ( err ) 
            return err;

	    err = dma_stop_tx(i);
	    if ( err ) 
            return err;
    }

	dma_ll_cleanup();

	return 0;
}


int 
dma_start_rx(
	int iDevice
	)
{
	int err;

	if ( down_interruptible( &pdma->sem )) 
        return -ERESTARTSYS;

	err = dma_stop_rx_unsafe(iDevice);
	if ( ! err ) {
		err = dma_start_rx_unsafe(iDevice);
	}

	up( &pdma->sem );

	return err;
}

int 
dma_start_tx(
	int iDevice
	)
{
	int err;

	if ( down_interruptible( &pdma->sem )) 
        return -ERESTARTSYS;

	err = dma_stop_tx_unsafe(iDevice);
	if ( ! err ) {
		err = dma_start_tx_unsafe(iDevice);
	}

	up( &pdma->sem );

	return err;
}


int 
dma_stop_rx(
	int iDevice
	)
{
	int err;

	if ( down_interruptible( &pdma->sem )) 
        return -ERESTARTSYS;

	err = dma_stop_rx_unsafe(iDevice);

	up( &pdma->sem );

	return err;
}

int 
dma_stop_tx(
	int iDevice
	)
{
	int err;

	if ( down_interruptible( &pdma->sem )) 
        return -ERESTARTSYS;

	err = dma_stop_tx_unsafe(iDevice);

	up( &pdma->sem );

	return err;
}


int 
dma_rx_is_on(
	int iDevice
	)
{
	return dma_ll_rx_is_on(iDevice);
}

int 
dma_tx_is_on(
	int iDevice
	)
{
	return dma_ll_tx_is_on(iDevice);
}

