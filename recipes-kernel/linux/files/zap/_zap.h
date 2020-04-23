#ifndef __ZAP_H_
#define __ZAP_H_

#include <linux/cdev.h>
#ifdef CONFIG_XILINX_VIRTEX
#include <platforms/4xx/xparameters/xparameters.h>
#endif
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include "zap.h"
#include "pool.h"

#define MODNAME "zap"

#define DEFAULT_PACKET_SIZE (0x1000)

#if defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_ARCH_ZYNQMP)
    #define ZAP_DMA_BD_BUF_SIZE (0x8000)
#endif

#define XPAR_PLB_ZAP_RD_BRAM_CTLR_SIZE \
	(XPAR_PLB_ZAP_RD_BRAM_CTLR_HIGHADDR - XPAR_PLB_ZAP_RD_BRAM_CTLR_BASEADDR + 1)
#define XPAR_PLB_ZAP_WR_BRAM_CTLR_SIZE \
	(XPAR_PLB_ZAP_WR_BRAM_CTLR_HIGHADDR - XPAR_PLB_ZAP_WR_BRAM_CTLR_BASEADDR + 1)

#define ZAP_MAX_DEVICES 16

struct zap_fpga_parameters{
    int num_interfaces;

	//Size of FPGA FIFO's (in bytes)
	unsigned long rx_dat_fifo_size;
	unsigned long rx_oob_fifo_size;

	unsigned long tx_dat_fifo_size;
	unsigned long tx_oob_fifo_size;

	int fpga_board_code;//1=LPe, 2=Z7e
	int fpga_version_major;
	int fpga_version_minor;
	char fpga_version_release;

};

struct zap_dev {
	unsigned int open_count;
	struct semaphore sem;
	struct cdev cdev;
	unsigned long status;

    struct zap_fpga_parameters fpga_params;

    //Parameters for Giant Pool, this pool will be split up among devices
	phys_addr_t rx_pool_paddr;
	phys_addr_t tx_pool_paddr;
    void * rx_pool_vaddr;
    void * tx_pool_vaddr;
    unsigned long rx_pool_size;
    unsigned long tx_pool_size;

    //Variables per interface

    //

	struct {
	    int fakey;
	    struct pool rx_pool;
	    struct pool tx_pool;
        void * rx_vaddr;
        void * tx_vaddr;
        unsigned long rx_size;
        unsigned long tx_size;
        phys_addr_t rx_paddr;
        phys_addr_t tx_paddr;

	    unsigned long rx_highwater;
	    unsigned long tx_highwater;
	    unsigned long rx_payload_max_size;
	    unsigned long tx_payload_max_size;
	    unsigned long rx_header_size;
	    unsigned long tx_header_size;
	    unsigned long rx_header_enable;
	    unsigned long tx_header_enable;

	} interface[ZAP_MAX_DEVICES];

};



ssize_t zap_read(struct file *filp, char __user *buf, size_t count,
				   loff_t *f_pos);
ssize_t zap_write(struct file *filp, const char __user *buf, size_t count,
					loff_t *f_pos);
long zap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

int zap_mmap(struct file *filp, struct vm_area_struct *vma);

struct zap_pool_entry {
	int foo;
};

#endif

