/*
 * ZAP Buf pool
 *
 * (C) Copyright 2008, iVeia, LLC
 */
#ifndef _POOL_H_
#define _POOL_H_

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

#define POOL_FLAG_INUSE (0x01)

struct pool_entry {
	void * pbuf;
	unsigned long flags;
	unsigned long len;
	unsigned long ooblen;
	struct list_head * pcur_list;
	struct list_head list;
};

struct pool {
	unsigned long size;
	void * buf_vaddr;
	unsigned long packet_size;
	unsigned long aligned_packet_size;
	unsigned long num_packets;
	struct pool_entry * pentries;
	void * ppackets;
	spinlock_t lock;
	wait_queue_head_t freeq;
	wait_queue_head_t fifoq;
	struct list_head freelist;
	struct list_head fifolist;
	struct list_head usedlist;
};

static inline bool
pool_buf_available( 
    struct pool * ppool
    )
{
	return ( ! list_empty(&ppool->freelist));
}

static inline bool
pool_fifo_buf_available( 
    struct pool * ppool
    )
{
	return ( ! list_empty(&ppool->fifolist));
}


//
// Function declarations
//
int
pool_create(
	struct pool * ppool,
	void * buf_vaddr,
	unsigned long size,
	unsigned long max_packets
	);

void
pool_destroy(
	struct pool * ppool
	);

void *
pool_offset2pbuf(
	struct pool * ppool,
	unsigned long offset
	);

unsigned long
pool_pbuf2offset(
	struct pool * ppool,
	void * pbuf
	);

unsigned long
pool_total_size(
	struct pool * ppool
    );

unsigned long
pool_packets_offset(
	struct pool * ppool
	);

int
pool_resize(
	struct pool * ppool,
	unsigned long packet_size,
	unsigned long max_packets,
	unsigned long force
	);

int
pool_getbuf_try(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen
	);

int
pool_getbuf_timeout(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	int timeout
	);

int
pool_getbuf(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen
	);

int
pool_freebuf(
	struct pool * ppool,
	void * pbuf
	);

int
pool_busy(
	struct pool * ppool
	);

int
pool_enqbuf(
	struct pool * ppool,
	void * pbuf,
	unsigned long len,
	unsigned long ooblen,
	unsigned long flags
	);

int
pool_deqbuf_try(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags
	);

int
pool_deqbuf_timeout(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags,
	int timeout
	);

int
pool_deqbuf(
	struct pool * ppool,
	void ** ppbuf,
	unsigned long * plen,
	unsigned long * pooblen,
	unsigned long * pflags
	);

int
pool_freebufs_ready(
	struct pool * ppool,
	wait_queue_head_t ** ppq
	);

int
pool_fifobufs_ready(
	struct pool * ppool,
	wait_queue_head_t ** ppq
	);

int
pool_flush(
	struct pool * ppool
	);

void *
pool_vaddr(
	struct pool * ppool
	);

void
pool_dump(
	struct pool * ppool,
	unsigned long flags
	);

#endif
