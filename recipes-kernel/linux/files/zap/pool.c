/*
 * ZAP Buffer pool
 *
 * (C) Copyright 2008, iVeia, LLC
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <asm/page.h>
#include <asm/io.h>
#include "zap.h"
#include "pool.h"
#include "dma.h"

///////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
//
// Private funcs
//
///////////////////////////////////////////////////////////////////////////


static void *
pentry2pbuf(
	struct pool * ppool,
	struct pool_entry * pentry
	)
{
	return pentry->pbuf;
}


static struct pool_entry *
pbuf2pentry(
	struct pool * ppool,
	void * pbuf
	)
{
	int i = ((unsigned int) ( pbuf - ppool->ppackets )) / ppool->aligned_packet_size;
	return &ppool->pentries[i];

}


//
// Move a buf from the freelist to the usedlist
//
static void *
_getbuf(
	struct pool * ppool,
	unsigned long * plen
	)
{
	struct pool_entry * pentry;

	//
	// This function should only be called when list is not empty
	//
	if (unlikely(list_empty(&ppool->freelist))) {
		printk(KERN_ERR "zap freelist empty, and attempting _getbuf\n" );
		BUG();
	}
	pentry = list_entry(ppool->freelist.next, struct pool_entry, list);
	pentry->pcur_list = &ppool->usedlist;
	if ( plen ) *plen = pentry->len;
	list_move(ppool->freelist.next, &ppool->usedlist);
	return pentry2pbuf(ppool, pentry);
}


//
// Move a buf from the usedlist to the freelist
//
static void
_freebuf(
	struct pool * ppool,
	void * pbuf
	)
{
	struct pool_entry * pentry = pbuf2pentry(ppool, pbuf);
	pentry->pcur_list = &ppool->freelist;
	pentry->len = ppool->packet_size;
	list_move_tail(&pentry->list, &ppool->freelist);
}


//
// Move a buf from the usedlist to the fifolist tail
//
static void
_enqbuf(
	struct pool * ppool,
	void * pbuf,
	unsigned long len,
	unsigned long ooblen,
	unsigned long flags
	)
{
	struct pool_entry * pentry = pbuf2pentry(ppool, pbuf);
	pentry->pcur_list = &ppool->fifolist;
	pentry->len = len;
	pentry->ooblen = ooblen;
	pentry->flags = flags;
	list_move_tail(&pentry->list, &ppool->fifolist);
}


//
// Move a buf from the fifolist head to the usedlist
//
static char *
_deqbuf(
	struct pool * ppool,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags
	)
{
	struct pool_entry * pentry;

	//
	// This function should only be called when list is not empty
	//
	if (unlikely(list_empty(&ppool->fifolist))) {
		printk(KERN_ERR "zap freelist empty, and attempting _getbuf\n" );
		BUG();
	}
	pentry = list_entry(ppool->fifolist.next, struct pool_entry, list);
	pentry->pcur_list = &ppool->usedlist;
	if ( plen ) *plen = pentry->len;
	if ( pooblen ) *pooblen = pentry->ooblen;
	if ( pflags ) *pflags = pentry->flags;
	list_move(ppool->fifolist.next, &ppool->usedlist);
	return pentry2pbuf(ppool, pentry);
}


static int
is_valid_pbuf(
	struct pool * ppool,
	void * pbuf
	)
{
	struct pool_entry * pentry;

	if ( pbuf < ppool->ppackets ) goto fail;
	if ( pbuf > ppool->buf_vaddr + ppool->size ) goto fail;

	pentry = pbuf2pentry(ppool, pbuf);
	if ( pentry < ppool->pentries ) goto fail;
	if ( pentry > &ppool->pentries[ppool->num_packets] ) goto fail;
	if ( pentry->pbuf != pbuf ) goto fail;

	return 1;

fail:
	return 0;
}


///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////

int
pool_create(
	struct pool * ppool,
	void * buf_vaddr,
	unsigned long size
	)
{

	ppool->size = size;
	ppool->buf_vaddr = buf_vaddr;

	init_waitqueue_head( &ppool->freeq );
	init_waitqueue_head( &ppool->fifoq );

	INIT_LIST_HEAD( &ppool->freelist );
	INIT_LIST_HEAD( &ppool->fifolist );
	INIT_LIST_HEAD( &ppool->usedlist );

	spin_lock_init( &ppool->lock );

	return 0;
}


void
pool_destroy(
	struct pool * ppool
	)
{
	ppool->buf_vaddr = NULL;
}

unsigned long
pool_total_size(
	struct pool * ppool
    )
{
    return ppool->size;
}

unsigned long
pool_packets_offset(
	struct pool * ppool
	)
{
    //Packets are now at beginning of pool
	return 0;
}


void *
pool_offset2pbuf(
	struct pool * ppool,
	unsigned long offset
	)
{
	return ppool->ppackets + offset;
}


unsigned long
pool_pbuf2offset(
	struct pool * ppool,
	void * pbuf
	)
{
	return pbuf - ppool->ppackets;
}


int
pool_resize(
	struct pool * ppool,
	unsigned long packet_size,
	unsigned long max_packets,
	unsigned long force
	)
{
	unsigned long irqflags;
	unsigned long alloc_per_packet;
	int i;

	//
	// Packets cannot exceed the max size defined by the FPGA core.
	//
	if ( packet_size > ZAP_PACKET_FPGA_MAX_SIZE ) return -EFBIG;

	//
	// We need at least space for a packet, plus the pentries struct (page
	// aligned)
	//
	if ( packet_size > ppool->size - PAGE_ALIGN(sizeof(struct pool_entry))) return -ENOMEM;

	//
	// Check if pool is in use.  The user of the pool may NOT use any more
	// buffers while calling this routine, or they could case a race condition
	// (where we think the pool is not in use, and it becomes in use while
	// we're resizing it)
	//
	//if ( ! force && ( err = pool_busy(ppool)) != 0 ) return err;

	//
	// Resize the pool.  At this point, there are no outstanding bufs in use,
	// so all bufs are in either the fifo or free lists.  Nevertheless, we
	// protect this region anyway, in case a get/free/enq/deq function is
	// tried.
	//
	spin_lock_irqsave( &ppool->lock, irqflags );

	ppool->packet_size = packet_size;
	ppool->aligned_packet_size = ( packet_size + ( sizeof(int) - 1 )) & ~ ( sizeof(int) - 1 );
	alloc_per_packet = sizeof(struct pool_entry) + ppool->aligned_packet_size;
	ppool->num_packets = ( ppool->size - PAGE_SIZE ) / alloc_per_packet;//Why do we subtract PAGE_SIZE?

	ppool->num_packets = min( ppool->num_packets, max_packets );

	ppool->pentries = (struct pool_entry *) ppool->buf_vaddr;

	//ppool->ppackets = (char *) PAGE_ALIGN((unsigned long) &ppool->pentries[ppool->num_packets]);
    ppool->ppackets = ppool->buf_vaddr;//Responsibility of lowlevel DMA code to page align this address
    ppool->pentries = ppool->buf_vaddr + (ppool->num_packets * ppool->aligned_packet_size);

	INIT_LIST_HEAD( &ppool->freelist );
	INIT_LIST_HEAD( &ppool->fifolist );
	INIT_LIST_HEAD( &ppool->usedlist );

	for ( i = 0; i < ppool->num_packets; i++ ) {
		ppool->pentries[i].pbuf = ppool->ppackets + i * ppool->aligned_packet_size;
		ppool->pentries[i].flags = 0;
		ppool->pentries[i].pcur_list = &ppool->freelist;
		ppool->pentries[i].len = ppool->packet_size;
		list_add( &ppool->pentries[i].list, &ppool->freelist );
	}

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return 0;
}


int
pool_busy(
	struct pool * ppool
	)
{
	unsigned long irqflags;
	int busy;

	spin_lock_irqsave( &ppool->lock, irqflags );
	busy = ! list_empty(&ppool->usedlist);
	spin_unlock_irqrestore( &ppool->lock, irqflags );
	if (busy) return -EBUSY;

	return 0;
}


int
pool_bufinuse(
	struct pool * ppool,
	void * pbuf
	)
{
	struct pool_entry * pentry = pbuf2pentry(ppool, pbuf);
	return pentry->pcur_list == &ppool->usedlist;
}


int
pool_getbuf_try(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen
	)
{
	unsigned long irqflags;
	int gotbuf = 0;

	spin_lock_irqsave( &ppool->lock, irqflags );

	if ( ! list_empty(&ppool->freelist)) {
		gotbuf = 1;
		*ppbuf = _getbuf(ppool, plen);
	}

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return gotbuf;
}


int
pool_getbuf_timeout(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	int timeout
	)
{
	unsigned long irqflags;
	int err;

	spin_lock_irqsave( &ppool->lock, irqflags );

	while (list_empty(&ppool->freelist)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		err = wait_event_interruptible_timeout(
			ppool->freeq,
			( ! list_empty(&ppool->freelist)),
			timeout
			);
		if ( err < 0 ) return -ERESTARTSYS;
		if ( err == 0 ) return -EAGAIN;
		spin_lock_irqsave( &ppool->lock, irqflags );
	}

	*ppbuf = _getbuf(ppool, plen);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return 0;
}


int
pool_getbuf(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen
	)
{
	unsigned long irqflags;
	spin_lock_irqsave( &ppool->lock, irqflags );

	while (list_empty(&ppool->freelist)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		if (wait_event_interruptible(ppool->freeq, ( ! list_empty(&ppool->freelist)))) {
			return -ERESTARTSYS;
		}
		spin_lock_irqsave( &ppool->lock, irqflags );
	}

	*ppbuf = _getbuf(ppool, plen);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return 0;
}


int
pool_freebuf(
	struct pool * ppool,
	void * pbuf
	)
{
	unsigned long irqflags;
	spin_lock_irqsave( &ppool->lock, irqflags );

	if ( ! is_valid_pbuf(ppool, pbuf) || ! pool_bufinuse(ppool, pbuf)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		return -EINVAL;
	}
	_freebuf(ppool, pbuf);

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	wake_up_interruptible( &ppool->freeq );

	return 0;
}


int
pool_enqbuf(
	struct pool * ppool,
	void * pbuf,
	unsigned long len,
	unsigned long ooblen,
	unsigned long flags
	)
{
	unsigned long irqflags;
	spin_lock_irqsave( &ppool->lock, irqflags );

	if ( ! is_valid_pbuf(ppool, pbuf) || ! pool_bufinuse(ppool, pbuf)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		return -EINVAL;
	}
	_enqbuf(ppool, pbuf, len, ooblen, flags);

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	wake_up_interruptible( &ppool->fifoq );

	return 0;
}


int
pool_deqbuf_try(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags
	)
{
	unsigned long irqflags;
	int gotbuf = 0;

	spin_lock_irqsave( &ppool->lock, irqflags );

	if ( ! list_empty(&ppool->fifolist)) {
		gotbuf = 1;
		*ppbuf = _deqbuf(ppool, plen, pooblen, pflags);
	}

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return gotbuf;
}


int
pool_deqbuf_timeout(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags,
	int timeout
	)
{
	unsigned long irqflags;
	int err;

	spin_lock_irqsave( &ppool->lock, irqflags );

	while (list_empty(&ppool->fifolist)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		err = wait_event_interruptible_timeout(
			ppool->fifoq,
			( ! list_empty(&ppool->fifolist)),
			timeout
			);
		if ( err < 0 ) return -ERESTARTSYS;
		if ( err == 0 ) return -EAGAIN;
		spin_lock_irqsave( &ppool->lock, irqflags );
	}

	*ppbuf = _deqbuf(ppool, plen, pooblen, pflags);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return 0;
}


int
pool_deqbuf(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags
	)
{
	unsigned long irqflags;
	spin_lock_irqsave( &ppool->lock, irqflags );

	while (list_empty(&ppool->fifolist)) {
		spin_unlock_irqrestore( &ppool->lock, irqflags );
		if (wait_event_interruptible(ppool->fifoq, ( ! list_empty(&ppool->fifolist)))) {
			return -ERESTARTSYS;
		}
		spin_lock_irqsave( &ppool->lock, irqflags );
	}

	*ppbuf = _deqbuf(ppool, plen, pooblen, pflags);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return 0;
}


int
pool_freebufs_ready(
	struct pool * ppool,
	wait_queue_head_t ** ppq
	)
{
	unsigned long irqflags;
	int ready;

	*ppq = &ppool->freeq;

	spin_lock_irqsave( &ppool->lock, irqflags );
	ready = ! list_empty(&ppool->freelist);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return ready;
}


int
pool_fifobufs_ready(
	struct pool * ppool,
	wait_queue_head_t ** ppq
	)
{
	unsigned long irqflags;
	int ready;

	*ppq = &ppool->fifoq;

	spin_lock_irqsave( &ppool->lock, irqflags );
	ready = ! list_empty(&ppool->fifolist);
	spin_unlock_irqrestore( &ppool->lock, irqflags );

	return ready;
}


int
pool_flush(
	struct pool * ppool
	)
{
	return pool_resize( ppool, ppool->packet_size, ppool->num_packets, 1 );
}


void *
pool_vaddr(
	struct pool * ppool
	)
{
	return ppool->buf_vaddr;
}


void
pool_dump(
	struct pool * ppool,
	unsigned long flags
	)
{
	unsigned long irqflags;
	struct pool_entry * pentry;
	int free, used, fifo;

	free = used = fifo = 0;
	if ( flags == 0 ) printk( "---- ENTRIES start ----\n" );

	spin_lock_irqsave( &ppool->lock, irqflags );

	list_for_each_entry( pentry, &ppool->freelist, list ) {
		if ( flags == 0 ) printk( "FREE: *%08X, flags %08X, len %d.  %s\n",
			(unsigned int)pentry->pbuf, (unsigned int)pentry->flags, (unsigned int)pentry->len,
			pentry->pcur_list != &ppool->freelist ? "BAD" : ""
			);
		free++;
	}

	list_for_each_entry( pentry, &ppool->fifolist, list ) {
		if ( flags == 0 ) printk( "FIFO: *%08X, flags %08X, len %d.  %s\n",
			(unsigned int)pentry->pbuf, (unsigned int)pentry->flags, (unsigned int)pentry->len,
			pentry->pcur_list != &ppool->fifolist ? "BAD" : ""
			);
		fifo++;
	}
	list_for_each_entry( pentry, &ppool->usedlist, list ) {
		if ( flags == 0 ) printk( "USED: *%08X, flags %08X, len %d.  %s\n",
			(unsigned int)pentry->pbuf, (unsigned int)pentry->flags, (unsigned int)pentry->len,
			pentry->pcur_list != &ppool->usedlist ? "BAD" : ""
			);
		used++;
	}

	spin_unlock_irqrestore( &ppool->lock, irqflags );

	if ( flags == 0 ) printk( "---- ENTRIES end ----\n" );
	printk( "Pool free: %d, fifo %d, used %d\n", free, fifo, used );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "size", (unsigned int)ppool->size );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "buffer", (unsigned int)ppool->buf_vaddr );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "packet_size", (unsigned int)ppool->packet_size );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "aligned_packet_size",
				(unsigned int)ppool->aligned_packet_size );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "num_packets", (unsigned int)ppool->num_packets );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "pentries", (unsigned int)ppool->pentries );
	if ( flags == 0 )
		printk( "Pool %s: %08X\n", "ppackets", (unsigned int)ppool->ppackets );
}

