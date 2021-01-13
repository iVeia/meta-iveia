/*
 * ZAP DMA 
 *
 * (C) Copyright 2008, iVeia, LLC
 */
#ifndef _DMA_H_
#define _DMA_H_

#include "_zap.h"
#if defined(CONFIG_XILINX_VIRTEX)
#include "dma_v5.h"
#elif defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_ARCH_ZYNQMP)
#include "dma_zynq.h"
#elif defined(CONFIG_MACH_IV_ATLAS_I_LPE)
#include "omap_zap_dma.h"
#else
#error Invalid processor board for iVeia ZAP.
#endif


/*
 * DMA high-level functions
 */
int 
dma_init(
	struct zap_dev * dev,
	phys_addr_t * prx_pool_paddr,
	void ** prx_pool_vaddr,
	unsigned long * prx_pool_size,
	phys_addr_t * ptx_pool_paddr,
	void ** ptx_pool_vaddr,
	unsigned long * ptx_pool_size
	);

int 
dma_cleanup(
	int iNumPorts
	);

int 
dma_start_rx(
	int iDevice
	);

int 
dma_start_tx(
	int iDevice
	);

int 
dma_stop_rx(
	int iDevice
	);

int 
dma_stop_tx(
	int iDevice
	);

int 
dma_rx_is_on(
	int iDevice
	);

int 
dma_tx_is_on(
	int iDevice
	);

/*
 * DMA low-level functions
 */

int
dma_ll_start_rx(
	int iDevice
	);

int
dma_ll_start_tx(
	int iDevice
	);

int
dma_ll_stop_rx(
	int iDevice
	);

int
dma_ll_stop_tx(
	int iDevice
	);

int 
dma_ll_init(
	struct zap_dev * dev,
	phys_addr_t * prx_pool_paddr,
	void ** prx_pool_start,
	unsigned long * prx_pool_size,
	phys_addr_t * ptx_pool_paddr,
	void ** ptx_pool_start,
	unsigned long * ptx_pool_size
	);

int 
dma_ll_cleanup(
	void
	);

int 
dma_ll_rx_is_on(
	int iDevice
	);

int 
dma_ll_tx_is_on(
	int iDevice
	);

void
dma_ll_rx_free_buf(
	int iDevice
	);

void
dma_ll_tx_write_buf(
	int iDevice
	);

void
dma_ll_update_high_water_marks(
	int iDevice
	);

int dma_ll_rx_dma_count(int iDevice);

int dma_ll_tx_dma_count(int iDevice);

void
dma_ll_update_fpga_parameters(void);

#endif
