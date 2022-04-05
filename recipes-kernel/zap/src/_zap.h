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

#define ZAP_MAX_DEVICES 16

struct zap_fpga_parameters {
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

struct zap_if {
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
	unsigned long rx_jumbo_pkt_enable;
	unsigned long tx_jumbo_pkt_enable;
};

struct zap_dev {
    struct device *dev;
    u32 num_devices;
	struct cdev cdev;
    dev_t node;
    struct class *class;
	unsigned int open_count;
	struct semaphore sem;
	unsigned long status;

    u64 reg_base;
    u64 reg_sz;
    u32 hw_irq;

    struct zap_fpga_parameters fpga_params;

    //Parameters for Giant Pool, this pool will be split up among devices
	phys_addr_t rx_pool_paddr;
	phys_addr_t tx_pool_paddr;
    unsigned long rx_pool_size;
    unsigned long tx_pool_size;

    //Variables per interface
    struct zap_if *interface;
};


ssize_t zap_read(struct file *filp, char __user *buf, size_t count,
				   loff_t *f_pos);
ssize_t zap_write(struct file *filp, const char __user *buf, size_t count,
					loff_t *f_pos);
long zap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

int zap_mmap(struct file *filp, struct vm_area_struct *vma);

#endif

