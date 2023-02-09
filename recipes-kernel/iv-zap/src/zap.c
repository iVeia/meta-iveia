/*
 * ZAP (Zero-copy Application Port) driver
 *
 * (C) Copyright 2008-2021, iVeia, LLC
 *
 * IVEIA IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
 * IVEIA IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
 * FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
 * ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * IVEIA EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
 * WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
 * CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * DESCRIPTION:
 *
 * The ZAP driver maintains a pool of buffers that are transferred to/from the
 * FPGA via DMA.  The flow of buffer descriptors is as follows (RX/TX is from
 * the processors perspective - RX means received by the processor):
 *
 * RX flow:
 *
 * Userspace       |       Kernelspace                 |   FPGA FIFO
 *                 |                                   |
 *          read() |       |---------------|    ISR    |   DMA done!
 *        /----------deq---| Fifo buf list |<---enq-----------------\
 *        |        |       |---------------|           |            |
 * mmap() |        |                                   |            |  DMA
 *        |        |                                   |            |
 *        | write()|       |---------------|    WQ     |            |
 *        \----------free->| Free buf list |----get--------> FIFO --/
 *                 |       |---------------|           |
 *                 |                                   |
 *
 * TX flow:
 *
 * Userspace       |       Kernelspace                 |   FPGA
 *                 |                                   |
 *          read() |       |---------------|    ISR    |   DMA done!
 *        /----------get---| Free buf list |<--free-----------------\
 *        |        |       |---------------|           |            |
 * mmap() |        |                                   |            |  DMA
 *        |        |                                   |            |
 *        | write()|       |---------------|    WQ     |            |
 *        \----------enq-->| Fifo buf list |---deq---------> FIFO --/
 *                 |       |---------------|           |
 *                 |                                   |
 *
 * For each pool (RX or TX), there are 3 lists of buffer pointers (pbuf).  A
 * pbuf is always in one of the lists at any time:
 *	- The Fifo buf lists contains pbufs that are filled with TX/RX data.
 *	- The Free buf lists contains pbufs that are NOT being used.
 *	- The Used buf list (not shown), contains pbufs that ARE being used, by
 *	either the userspace app or the FPGA.
 *
 * The buf lists are managed by pool.c.  The top-level char driver interface is
 * handled by this file.  The DMA interface is handled by dma.c.  The board
 * specific portion of DMA is handled by dma_*.c files.
 *
 * Functions for moving buffers between lists (shown in diagram):
 *	enq	pool_enqbuf	Move pbuf from Used to Fifo list
 *	deq	pool_deqbuf	Move pbuf from Fifo to Used list
 *	get	pool_getbuf	Move pbuf from Free to Used list
 *	free	pool_freebuf	Move pbuf from Used to Free list
 * Contexts:
 *	WQ - the DMA workqueues - are in dma.c.
 *	ISR - DMA ISRs - are board specific, and are therofore in dma_*.c
 */
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/dma-map-ops.h>
//#include <linux/kallsyms.h>


#include "_zap.h"
#include "pool.h"
#include "dma.h"


// arch_setup_dma_ops() may not be exported for arm64, so get via kallsyms_lookup_name()
//static void *ksym_arch_setup_dma_ops;
//typedef typeof(&arch_setup_dma_ops) arch_setup_dma_ops_fn;
//#define arch_setup_dma_ops (* (arch_setup_dma_ops_fn)ksym_arch_setup_dma_ops)


///////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////

#define MODNAME "zap"

//
// Define buffer pool constraints.
//
// Note: it is critical that the num_packets < num DMA buffer descriptors.
// If this were not true, then the buffer descriptor ring's tail would
// overwrite the head.
//
#define ZAP_POOL_MAX_RX_PACKETS ( DMA_BD_RX_NUM - 1 )
#define ZAP_POOL_MAX_TX_PACKETS ( DMA_BD_TX_NUM - 1 )

struct zap_dev * zap_devp = NULL;

///////////////////////////////////////////////////////////////////////////
//
// Private funcs
//
///////////////////////////////////////////////////////////////////////////

static int
is_tx_device(struct file *filp)
{
	//
	// By convention, zaprx devnos are even, zaptx devnos are odd.
	//
	return iminor(filp->f_path.dentry->d_inode) & 1;
}
	
static int
zap_device_num(struct file *filp)
{
        return iminor(filp->f_path.dentry->d_inode) >> 1;
}

static void
setup_multiple_pools(int iNumPools)
{
    int i;
    unsigned long iRxSize,iTxSize;
    //unsigned int ulRxTopAddr;

    iRxSize = zap_devp->rx_pool_size / iNumPools;
    iTxSize = zap_devp->tx_pool_size / iNumPools;

    for (i = 0; i < iNumPools; i++) {
        zap_devp->interface[i].rx_paddr = (phys_addr_t)PAGE_ALIGN(((uintptr_t)zap_devp->rx_pool_paddr + iRxSize * i));
        zap_devp->interface[i].tx_paddr = (phys_addr_t)PAGE_ALIGN(((uintptr_t)zap_devp->tx_pool_paddr + iRxSize * i));
    }

    if (iNumPools == 1) {
        zap_devp->interface[iNumPools-1].rx_size = zap_devp->rx_pool_size;
        zap_devp->interface[iNumPools-1].tx_size = zap_devp->tx_pool_size;
    } else {
        //Fill in sizes
        for (i = 0; i < iNumPools-1; i++) {
            zap_devp->interface[i].rx_size = zap_devp->interface[i+1].rx_paddr - zap_devp->interface[i].rx_paddr;
            zap_devp->interface[i].tx_size = zap_devp->interface[i+1].tx_paddr - zap_devp->interface[i].tx_paddr;
        }
        zap_devp->interface[iNumPools-1].rx_size = zap_devp->rx_pool_paddr + zap_devp->rx_pool_size - zap_devp->interface[iNumPools-1].rx_paddr;
        zap_devp->interface[iNumPools-1].tx_size = zap_devp->tx_pool_paddr + zap_devp->tx_pool_size - zap_devp->interface[iNumPools-1].tx_paddr;
    }

#if defined(DEBUG)
    //PRINT RX RESULTS
    printk(KERN_ERR "****SETUP_MULTIPLE_POOLS****\n");
    printk(KERN_ERR "rx_paddr = 0x%llx\n",zap_devp->rx_pool_paddr);
    printk(KERN_ERR "rx_size  = 0x%lx\n",zap_devp->rx_pool_size);
    for (i=0;i<iNumPools;i++){
        printk(KERN_ERR "\t**POOL %d\n",i);
        printk(KERN_ERR "\t\trx_paddr = 0x%llx\n",zap_devp->interface[i].rx_paddr);
        printk(KERN_ERR "\t\trx_size  = 0x%lx\n",zap_devp->interface[i].rx_size);
    }

    //PRINT TX RESULTS
    printk(KERN_ERR "****SETUP_MULTIPLE_POOLS****\n");
    printk(KERN_ERR "tx_paddr = 0x%llx\n",zap_devp->tx_pool_paddr);
    printk(KERN_ERR "tx_size  = 0x%lx\n",zap_devp->tx_pool_size);
    for (i=0;i<iNumPools;i++){
        printk(KERN_ERR "\t**POOL %d\n",i);
        printk(KERN_ERR "\t\ttx_paddr = 0x%llx\n",zap_devp->interface[i].tx_paddr);
        printk(KERN_ERR "\t\ttx_size  = 0x%lx\n",zap_devp->interface[i].tx_size);
    }
#endif

    return;
}

static int 
IsJumboPacketSupported(void)
{
	#define VMAJOR_REQ       2
	#define VMINOR_REQ       2
	#define VRELEASE_REQ     'a'
	#define VMAJOR       zap_devp->fpga_params.fpga_version_major
	#define VMINOR       zap_devp->fpga_params.fpga_version_minor
	#define VRELEASE     zap_devp->fpga_params.fpga_version_release

	if (VMAJOR < VMAJOR_REQ)
		return 0;

	if (VMINOR < VMINOR_REQ)
		return 0;
    
	if (VRELEASE < VRELEASE_REQ)
		return 0;

	return 1;
}


///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////

int zap_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data){
	int len = 0;

    //re-read parameters incase a reconfiguration has occured
    dma_ll_update_fpga_parameters();

	len+= sprintf(buf + len, "FPGA CODE: %d\n",zap_devp->fpga_params.fpga_board_code);
	len+= sprintf(buf + len, "FPGA Version: v%d_%d_%c\n",zap_devp->fpga_params.fpga_version_major, 
            zap_devp->fpga_params.fpga_version_minor, zap_devp->fpga_params.fpga_version_release);
    len+= sprintf(buf + len, "NUM_INTERFACES = %d\n",zap_devp->fpga_params.num_interfaces);
	len+= sprintf(buf + len, "RX_DAT_FIFO_SIZE = %d\n",(unsigned int)zap_devp->fpga_params.rx_dat_fifo_size);
	len+= sprintf(buf + len, "RX_OOB_FIFO_SIZE = %d\n",(unsigned int)zap_devp->fpga_params.rx_oob_fifo_size);
	len+= sprintf(buf + len, "TX_DAT_FIFO_SIZE = %d\n",(unsigned int)zap_devp->fpga_params.tx_dat_fifo_size);
	len+= sprintf(buf + len, "TX_OOB_FIFO_SIZE = %d\n",(unsigned int)zap_devp->fpga_params.tx_oob_fifo_size);
	len+= sprintf(buf + len, "RX_MAX=%d\n",(unsigned int)zap_devp->interface[0].rx_payload_max_size);
	len+= sprintf(buf + len, "RX_HDR=%d\n",(unsigned int)zap_devp->interface[0].rx_header_size);
	len+= sprintf(buf + len, "TX_MAX=%d\n",(unsigned int)zap_devp->interface[0].tx_payload_max_size);
	len+= sprintf(buf + len, "TX_HDR=%d\n",(unsigned int)zap_devp->interface[0].tx_header_size);
	len+= sprintf(buf + len, "RX_DMA=%d\n",dma_ll_rx_dma_count(0));
	len+= sprintf(buf + len, "TX_DMA=%d\n",dma_ll_tx_dma_count(0));
	return len;
}


/*
 * Open and close
 */
int zap_open(struct inode *inode, struct file *filp)
{
	struct zap_dev * dev;
	ssize_t retval = 0;
	int err;
    int iDevice;
	
	dev = container_of(inode->i_cdev, struct zap_dev, cdev);
	filp->private_data = dev;

    iDevice = zap_device_num(filp);

    dev_dbg(dev->dev, "%s(), Device %d\n", __func__, iDevice);

	if (down_interruptible(&dev->sem)) 
        return -ERESTARTSYS;

	if ( (filp->f_flags & O_ACCMODE) != O_RDONLY ) {

		dma_ll_update_fpga_parameters(); //Incase FPGA Reprogram has occured

        if (zap_device_num(filp) >= dev->fpga_params.num_interfaces) {
            up(&dev->sem);
            return -ENXIO;
        }

        setup_multiple_pools(zap_devp->fpga_params.num_interfaces);

        if (is_tx_device(filp)) {
			dev->interface[iDevice].tx_payload_max_size = dev->fpga_params.tx_dat_fifo_size;

	        err = pool_create(&zap_devp->interface[iDevice].tx_pool, (void *)zap_devp->interface[iDevice].tx_paddr, 
                    zap_devp->interface[iDevice].tx_size);
	        if (err) {
		        printk(KERN_ERR "RX pool_create error %d\n", err);
		        //goto fail;
	        }

			err = pool_resize( &dev->interface[iDevice].tx_pool, 
                    dev->interface[iDevice].tx_payload_max_size, 
                    ZAP_POOL_MAX_TX_PACKETS, 
                    0
			        );
			if (err) {
				printk(KERN_ERR "pool_resize (tx) error %d\n", err);
				//goto fail;
			}
		} else {
			dev->interface[iDevice].rx_payload_max_size = dev->fpga_params.rx_dat_fifo_size;

	        err = pool_create(&zap_devp->interface[iDevice].rx_pool, (void *)zap_devp->interface[iDevice].rx_paddr, 
                    zap_devp->interface[iDevice].rx_size);
	        if (err) {
		        printk(KERN_ERR "RX pool_create error %d\n", err);
		        //goto fail;
	        }

			err = pool_resize(&dev->interface[iDevice].rx_pool, 
                    dev->interface[iDevice].rx_payload_max_size,
				    ZAP_POOL_MAX_RX_PACKETS,
				    0
				    );
			if (err) {
				printk(KERN_ERR "pool_resize (rx) error %d\n", err);
				//goto fail;
			}
		}

	}

	dev->open_count++;
	up(&dev->sem);

	return retval;
}

int zap_release(struct inode *inode, struct file *filp)
{
	struct zap_dev * dev;
    int iDevice;

	dev = container_of(inode->i_cdev, struct zap_dev, cdev);
	filp->private_data = dev;

    iDevice = zap_device_num(filp);

    dev_dbg(dev->dev, "%s(), Device %d\n", __func__, iDevice);

    if (dma_tx_is_on(iDevice)) {
        dma_stop_tx(iDevice);
	}

    if (dma_rx_is_on(iDevice)) {
        dma_stop_rx(iDevice);
	}

	if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
	}

	dev->open_count--;

	if ( dev->open_count < 0 ) {
		printk(KERN_WARNING "zap: device released without open.\n");
		dev->open_count = 0;
	}

	up(&dev->sem);
	
	return 0;
}


//This was added because Z8 was getting kernel panics on dma_map_single(NULL..)
//http://stackoverflow.com/questions/19952968/dma-map-single-minimum-requirements-to-struct-device
static struct device zap_device = {
    .init_name = "zap_device",
    .coherent_dma_mask = ~0,             // dma_alloc_coherent(): allow any address
    .dma_mask = &zap_device.coherent_dma_mask,  // other APIs: use the same mask as coherent
    };

/*
 * Data management: read and write
 */
ssize_t zap_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct zap_dev * dev = filp->private_data;
	ssize_t retval = 0;
	void * pbuf;
	unsigned long len;
	unsigned long ooblen = 0;
	unsigned long flags = 0;
	unsigned long read_data[4];
    int iDevice;
    dma_addr_t dma_addr;

	if (count != sizeof(read_data)) 
        return -EINVAL;
	if (!access_ok((void __user *)buf, count)) 
        return -EFAULT;

    iDevice = zap_device_num(filp);

	if ( is_tx_device(filp)) {
		if ( filp->f_flags & O_NONBLOCK ) {
			if ( ! pool_getbuf_try( &dev->interface[iDevice].tx_pool, &pbuf, &len )) 
                return -EAGAIN;
		} else {
			retval = pool_getbuf( &dev->interface[iDevice].tx_pool, &pbuf, &len );
			if ( retval ) 
                return retval;
		}
	} else {
		if ( filp->f_flags & O_NONBLOCK ) {
			if ( ! pool_deqbuf_try( &dev->interface[iDevice].rx_pool, &pbuf, &len, &ooblen, &flags )) 
                return -EAGAIN;
		} else {
			retval = pool_deqbuf( &dev->interface[iDevice].rx_pool, &pbuf, &len, &ooblen, &flags );
			if ( retval ) 
                return retval;
		}

        dma_addr = dma_map_single(&zap_device, (void *)pbuf, (size_t)len, DMA_FROM_DEVICE );
        dma_unmap_single(&zap_device, dma_addr, (size_t)len, DMA_FROM_DEVICE );
	}

	read_data[0] = pool_pbuf2offset(
		is_tx_device(filp) ? &dev->interface[iDevice].tx_pool : &dev->interface[iDevice].rx_pool,
		pbuf
		);
	read_data[1] = len;
	read_data[2] = ooblen;
	read_data[3] = flags;
	if (__copy_to_user(buf, read_data, count))
        return -EFAULT;

	return count;
}

ssize_t zap_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct zap_dev * dev = filp->private_data;
	int err;
	void * pbuf;
	unsigned long len;
	unsigned long ooblen;
	unsigned long write_data[3];
    int iDevice;
    dma_addr_t dma_addr;

	if (count != sizeof(write_data)) 
        return -EINVAL;
	if (!access_ok((void __user *)buf, count)) 
        return -EFAULT;

    iDevice = zap_device_num(filp);

	if (__copy_from_user(write_data, buf, count))
        return -EFAULT;

	len = write_data[1];
	ooblen = write_data[2];
	if ( is_tx_device(filp)) {

		//
		// As a special case, a tx packet that has a length of -1 is a packet
		// being requested to be freed, not sent.
		//
		pbuf = pool_offset2pbuf( &dev->interface[iDevice].tx_pool, write_data[0] );
		if ( len == (unsigned long) -1 ) {
			err = pool_freebuf( &dev->interface[iDevice].tx_pool, pbuf );
		} else {

			//If Header size !=0 and header is enabled, throw error
			if (ooblen != 0 && !dev->interface[iDevice].tx_header_enable)
				return -EPERM;

			//Check to make sure Packet larger than max size is not being sent
			if ((len > dev->interface[iDevice].tx_payload_max_size) || 
                    ((ooblen > dev->interface[iDevice].tx_header_size) && 
                     dev->interface[iDevice].tx_header_enable) )
				return -EINVAL;

            dma_addr = dma_map_single(&zap_device,pbuf,len,DMA_TO_DEVICE);
            dma_unmap_single(&zap_device, dma_addr, len, DMA_TO_DEVICE);

			err = pool_enqbuf( &dev->interface[iDevice].tx_pool, pbuf, len, ooblen, 0 );

	#if defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_ARCH_ZYNQMP)
			dma_ll_tx_write_buf(iDevice);
	#else
			dma_ll_tx_write_buf(iDevice);
    #endif
		}
		if ( err ) 
            return err;
	} else {
		pbuf = pool_offset2pbuf( &dev->interface[iDevice].rx_pool, write_data[0] );
		err = pool_freebuf( &dev->interface[iDevice].rx_pool, pbuf );
		if ( err ) 
            return err;
	#if defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_ARCH_ZYNQMP)
		//dma_ll_rx_free_buf();
	#else
		dma_ll_rx_free_buf(iDevice);
    #endif
	}

	return count;
}

/*
 * The ioctl() implementation
 */
long zap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	int retval = 0;
	struct zap_dev * dev = filp->private_data;
	unsigned long ulTemp;
    int iDevice;
	
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */

	if (_IOC_TYPE(cmd) != ZAP_IOC_MAGIC) 
        return -ENOTTY;
	if (_IOC_NR(cmd) > ZAP_IOC_MAXNR) 
        return -ENOTTY;

    iDevice = zap_device_num(filp);

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err =  !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	}
	if (err) 
        return -EFAULT;

	if (down_interruptible(&dev->sem)) 
        return -ERESTARTSYS;

	switch(cmd) {
		case ZAP_IOC_R_STATUS:
			__put_user( dev->status, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_R_RX_HIGH_WATER_MARK:
			dma_ll_update_high_water_marks(iDevice);
			__put_user( dev->interface[iDevice].rx_highwater, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_R_TX_HIGH_WATER_MARK:
			dma_ll_update_high_water_marks(iDevice);
			__put_user( dev->interface[iDevice].tx_highwater, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_R_TX_MAX_SIZE:
			__put_user( dev->interface[iDevice].tx_payload_max_size, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_W_TX_MAX_SIZE:
			//retval = dma_stop();
			retval = dma_stop_tx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			//If IB Max Size > TX FIFO Size, return error
			if (dev->interface[iDevice].tx_header_enable) {
				if (((ulTemp - dev->interface[iDevice].tx_header_size) > dev->fpga_params.tx_dat_fifo_size) && 
                        (dev->interface[iDevice].tx_jumbo_pkt_enable == 0)) {
					retval = -EFBIG;
					break;
				}
			}else{
				if ((ulTemp > dev->fpga_params.tx_dat_fifo_size) && 
                        (dev->interface[iDevice].tx_jumbo_pkt_enable == 0)) {
					retval = -EFBIG;
					break;
				}
			}

			dev->interface[iDevice].tx_payload_max_size = ulTemp;

			retval = pool_resize(
				&dev->interface[iDevice].tx_pool,
				dev->interface[iDevice].tx_payload_max_size,
				ZAP_POOL_MAX_TX_PACKETS,
				0
				);
			break;

		case ZAP_IOC_R_TX_HEADER_SIZE:
			__put_user( dev->interface[iDevice].tx_header_size, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_W_TX_HEADER_SIZE:
			//retval = dma_stop();
			retval = dma_stop_tx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			//If IB Max Size > TX FIFO Size, return error
			if (ulTemp > dev->fpga_params.tx_oob_fifo_size) {
				retval = -EFBIG;
				break;
			}

			if (ulTemp >= dev->interface[iDevice].tx_payload_max_size) {
				retval = -EFBIG;
				break;
			}

			if ( ((dev->interface[iDevice].tx_payload_max_size - ulTemp) > dev->fpga_params.tx_dat_fifo_size) && 
                    (dev->interface[iDevice].tx_jumbo_pkt_enable == 0)) {
				retval = -EFBIG;
				break;
			}

			// Set tx_header_enable, User no longer has this control
			if (ulTemp == 0)
				dev->interface[iDevice].tx_header_enable = 0;
			else
				dev->interface[iDevice].tx_header_enable = 1;

			dev->interface[iDevice].tx_header_size = ulTemp;

			break;
		case ZAP_IOC_R_RX_MAX_SIZE:
			__put_user( dev->interface[iDevice].rx_payload_max_size, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_W_RX_MAX_SIZE:
			//retval = dma_stop();
			retval = dma_stop_rx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			//If IB Max Size > TX FIFO Size, return error
			if (dev->interface[iDevice].rx_header_enable) {
				if ( ((ulTemp - dev->interface[iDevice].rx_header_size) > dev->fpga_params.rx_dat_fifo_size) && 
                        (dev->interface[iDevice].rx_jumbo_pkt_enable == 0)) {
					retval = -EFBIG;
					break;
				}
			}else{
				if ( (ulTemp > dev->fpga_params.rx_dat_fifo_size) && 
                        (dev->interface[iDevice].rx_jumbo_pkt_enable == 0)) {
					retval = -EFBIG;
					break;
				}
			}
			dev->interface[iDevice].rx_payload_max_size = ulTemp;
			retval = pool_resize(
				&dev->interface[iDevice].rx_pool,
				dev->interface[iDevice].rx_payload_max_size,
				ZAP_POOL_MAX_RX_PACKETS,
				0
				);
			break;


		case ZAP_IOC_R_RX_HEADER_SIZE:
			__put_user( dev->interface[iDevice].rx_header_size, (unsigned long __user *)arg);
			break;
		case ZAP_IOC_W_RX_HEADER_SIZE:
			//retval = dma_stop();
			retval = dma_stop_rx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			//If IB Max Size > RX FIFO Size, return error
			if (ulTemp > dev->fpga_params.rx_oob_fifo_size){
				retval = -EFBIG;
				break;
			}

			if (ulTemp >= dev->interface[iDevice].rx_payload_max_size){
				retval = -EFBIG;
				break;
			}

			if ( ((dev->interface[iDevice].rx_payload_max_size - ulTemp) > dev->fpga_params.rx_dat_fifo_size) && 
                    (dev->interface[iDevice].rx_jumbo_pkt_enable == 0)) {
				retval = -EFBIG;
				break;
			}

			// Set rx_header_enable, User no longer has this control
			if (ulTemp == 0)
				dev->interface[iDevice].rx_header_enable = 0;
			else
				dev->interface[iDevice].rx_header_enable = 1;

			dev->interface[iDevice].rx_header_size = ulTemp;

			break;
		case ZAP_IOC_R_RX_JUMBO_EN:
			__put_user( dev->interface[iDevice].rx_jumbo_pkt_enable, (unsigned long __user *)arg);
			break;

		case ZAP_IOC_W_RX_JUMBO_EN:
			//retval = dma_stop();
			retval = dma_stop_rx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			if (ulTemp == 0)
				dev->interface[iDevice].rx_jumbo_pkt_enable = 0;
			else{
				if (!IsJumboPacketSupported()) {
					retval = -EINVAL;
					printk(KERN_ERR "ZAP : FPGA Image does not support Jumbo Packets\n");
					break;
				}
				dev->interface[iDevice].rx_jumbo_pkt_enable = 1;
			}

			break;

		case ZAP_IOC_R_TX_JUMBO_EN:
			__put_user( dev->interface[iDevice].tx_jumbo_pkt_enable, (unsigned long __user *)arg);
			break;

		case ZAP_IOC_W_TX_JUMBO_EN:
			//retval = dma_stop();
			retval = dma_stop_tx(iDevice);
			if ( retval ) 
                break;
			__get_user( ulTemp, (unsigned long __user *)arg);

			if (ulTemp == 0)
				dev->interface[iDevice].tx_jumbo_pkt_enable = 0;
			else{
				if (!IsJumboPacketSupported()) {
					retval = -EINVAL;
					printk(KERN_ERR "ZAP : FPGA Image does not support Jumbo Packets\n");
					break;
				}
				dev->interface[iDevice].tx_jumbo_pkt_enable = 1;
			}

			break;

		case ZAP_IOC_R_FAKEY:
			//__put_user( dev->fakey, (unsigned long __user *)arg);
            __put_user( (dev->interface[zap_device_num(filp)].fakey),(unsigned long __user *)arg); 
			break;
		case ZAP_IOC_W_FAKEY:
			//__get_user( dev->fakey, (unsigned long __user *)arg);
            __get_user( (dev->interface[zap_device_num(filp)].fakey),(unsigned long __user *)arg); 
			break;
		case ZAP_IOC_R_RX_DMA_ON:
			{
				unsigned long dma_on = dma_rx_is_on(iDevice);
				__put_user( dma_on, (unsigned long __user *)arg);
			}
			break;
		case ZAP_IOC_W_RX_DMA_ON:
			{
				unsigned long dma_on;
				__get_user( dma_on, (unsigned long __user *)arg);
				if ( dma_on ) {
					retval = dma_start_rx(iDevice);
				} else {
					retval = dma_stop_rx(iDevice);
				}
			}
			break;
		case ZAP_IOC_R_TX_DMA_ON:
			{
				unsigned long dma_on = dma_tx_is_on(iDevice);
				__put_user( dma_on, (unsigned long __user *)arg);
			}
			break;
		case ZAP_IOC_W_TX_DMA_ON:
			{
				unsigned long dma_on;
				__get_user( dma_on, (unsigned long __user *)arg);
				if ( dma_on ) {
					retval = dma_start_tx(iDevice);
				}else{
					retval = dma_stop_tx(iDevice);
				}
			}
			break;
		case ZAP_IOC_R_POOL_SIZE:
			{
				unsigned long size;
				size = pool_total_size( is_tx_device(filp) ? &dev->interface[iDevice].tx_pool : &dev->interface[iDevice].rx_pool );
				__put_user( size, (unsigned long __user *)arg);
			}
			break;

		case ZAP_IOC_R_INSTANCE_COUNT:			
			__put_user(dev->open_count,(unsigned long __user *)arg);				
			break;						

		default:  /* redundant, as cmd was checked against MAXNR */
			retval = -ENOTTY;
			break;
	}

	up(&dev->sem);

	return retval;

}

int zap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct zap_dev * dev = filp->private_data;
	int ret;
	unsigned long pfn;
	unsigned long available_size;
	unsigned long requested_size;
	unsigned long pool_size;
	phys_addr_t pool_phys_start;
    int iDevice;

    iDevice = zap_device_num(filp);

	if ( is_tx_device(filp)) {
		pool_size = pool_total_size( &dev->interface[iDevice].tx_pool );
		pool_phys_start = dev->interface[iDevice].tx_paddr + pool_packets_offset( &dev->interface[iDevice].tx_pool );
	} else {
		pool_size = pool_total_size( &dev->interface[iDevice].rx_pool );
		pool_phys_start = dev->interface[iDevice].rx_paddr + pool_packets_offset( &dev->interface[iDevice].rx_pool );
	}

	requested_size = vma->vm_end - vma->vm_start;
	available_size = pool_size;
	pfn = ( pool_phys_start) >> PAGE_SHIFT;

	if (requested_size > available_size) 
        return -EINVAL;

    dev_info(zap_devp->dev, "mmap %s 0x%0llx -> 0x%0lx (0x%0lx)\n", 
            is_tx_device(filp) ? "tx":"rx", 
            pool_phys_start, vma->vm_start, pool_size);

    // setting pgprot_noncached results in performance hit in processing rx buff in user space
	//vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = remap_pfn_range(vma, vma->vm_start, pfn, requested_size, vma->vm_page_prot);
	if (ret) 
        return -EAGAIN;

	return 0;
}

unsigned int zap_poll(struct file *filp, poll_table *wait)
{
	struct zap_dev * dev = filp->private_data;
	unsigned int mask = 0;
	wait_queue_head_t * pq;
	int ready;
    int iDevice;

	if (down_interruptible(&dev->sem)) 
        return 0;

    iDevice = zap_device_num(filp);

	/*
	 * Get qait queue.  Note, we must test for ready AFTER the call to
	 * poll_wait(), otherwise we create a race condition.  Hence the two calls
	 * to the *ready() function.
	 */
	if ( is_tx_device(filp)) {
		pool_freebufs_ready(&dev->interface[iDevice].tx_pool, &pq);
	} else {
		pool_fifobufs_ready(&dev->interface[iDevice].rx_pool, &pq);
	}
	poll_wait(filp, pq, wait);

	if ( is_tx_device(filp)) {
		ready = pool_freebufs_ready(&dev->interface[iDevice].tx_pool, &pq);
	} else {
		ready = pool_fifobufs_ready(&dev->interface[iDevice].rx_pool, &pq);
	}
	if (ready) {
		mask |= POLLIN | POLLRDNORM;
	}

	up(&dev->sem);

	return mask;
}


struct file_operations zap_fops = {
	.owner =	THIS_MODULE,
	.read =	 zap_read,
	.write =	zap_write,
	.unlocked_ioctl =	zap_ioctl,
	.open =	 zap_open,
	.release =  zap_release,
	.mmap =	 zap_mmap,
	.poll =	 zap_poll,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static int zap_remove(struct platform_device *pdev)
{
    int i;

    pr_info("zap - REMOVE\n");

	dma_cleanup( zap_devp->num_devices );

	if (zap_devp) {
        for( i = 0; i < zap_devp->num_devices; i++) {
	        pool_destroy(&zap_devp->interface[i].rx_pool);
	        pool_destroy(&zap_devp->interface[i].tx_pool);
            device_destroy( zap_devp->class, MKDEV(MAJOR(zap_devp->node),(i*2)) );
            device_destroy( zap_devp->class, MKDEV(MAJOR(zap_devp->node),(i*2)+1) );
        }
        class_destroy( zap_devp->class );
        cdev_del(&zap_devp->cdev);
        unregister_chrdev_region(zap_devp->node, zap_devp->num_devices * 2);

        if (zap_devp->interface)
            kfree(zap_devp->interface);
		kfree(zap_devp);
        zap_devp = NULL;
	}

    return 0;
}

static int zap_probe(struct platform_device *pdev)
{
	int err, i;
    struct device_node *np;
    struct reserved_mem *rmem = NULL;
    u64 rx_pool_sz, tx_pool_sz;    

    pr_info("zap - PROBE\n");

	zap_devp = kzalloc(sizeof(struct zap_dev), GFP_KERNEL);
	if (!zap_devp) {
		err = -ENOMEM;
		goto fail;
	}
	memset(zap_devp, 0, sizeof(struct zap_dev));
    platform_set_drvdata(pdev, zap_devp);
	zap_devp->dev = &pdev->dev;

    err = of_property_read_u32(pdev->dev.of_node, "num-devices", &zap_devp->num_devices);
    if ( err )
        zap_devp->num_devices = ZAP_MAX_DEVICES;
    dev_info(zap_devp->dev, "num_devices %u\n", zap_devp->num_devices);

    err = of_property_read_u32(pdev->dev.of_node, "irq", &zap_devp->hw_irq);
    if ( err )
    {
        dev_err(zap_devp->dev, "irq not specified in device tree\n");
		goto fail;
    }
    dev_info(zap_devp->dev, "irq %u\n", zap_devp->hw_irq);

    if ( of_property_read_u64_index(pdev->dev.of_node, "reg", 0, &zap_devp->reg_base ) != 0 ) {
        dev_err(zap_devp->dev, "dt reg ERROR (0)\n");
        goto fail;
    }
    if ( of_property_read_u64_index(pdev->dev.of_node, "reg", 1, &zap_devp->reg_sz ) != 0 ) {
        dev_err(zap_devp->dev, "dt reg ERROR (1)\n");
        goto fail;
    }
    dev_info(zap_devp->dev, "pl reg base: 0x%0llx, sz 0x%0llx\n", zap_devp->reg_base, zap_devp->reg_sz); 


    /* Get reserved memory region from Device-tree */
    np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
    if (!np) {
      dev_err(zap_devp->dev, "No %s specified\n", "memory-region");
      goto fail;
    }

    rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		dev_err(zap_devp->dev, "unable to acquire memory-region\n");
		return -EINVAL;
    }

    dev_info(zap_devp->dev, "memory-region: 0x%0llx 0x%0llx\n", rmem->base, rmem->size);

    if ( of_find_property(pdev->dev.of_node, "pool-sizes", NULL) ) {
        if ( of_property_read_u64_index(pdev->dev.of_node, "pool-sizes", 0, &tx_pool_sz ) != 0 ) {
            dev_err(zap_devp->dev, "dt pool-sizes ERROR (0)\n");
            goto fail;
        }
        if ( of_property_read_u64_index(pdev->dev.of_node, "pool-sizes", 1, &rx_pool_sz ) != 0 ) {
            dev_err(zap_devp->dev, "dt pool-sizes ERROR (1)\n");
            goto fail;
        }
        if ( (tx_pool_sz + rx_pool_sz) > rmem->size )
        {
            dev_err(zap_devp->dev, "dt pool-sizes ERROR\n");
            goto fail;
        }
    } else {
        tx_pool_sz = rx_pool_sz = rmem->size / 2;
    }

    //create_proc_read_entry("iveia/zap", 0, NULL, zap_read_procmem, NULL);
	/*
	 * Get a range of minor numbers to work with, using static major.
	 *
	 * For each ZAP FPGA instance, we create 2 minor numbers that point to the
	 * same cdev.  One is for tx buffers, the other for rx buffers.
	 *
	 * Even though ZAP_MAX_DEVICES is defined, we currently only support one
	 * ZAP instance, and we only create one cdev.
	 */
	err = alloc_chrdev_region(&zap_devp->node, 0, zap_devp->num_devices * 2, MODNAME);
	if (err) {
		goto fail;
	}

	zap_devp->class = class_create(THIS_MODULE, "zap");
	if (IS_ERR(zap_devp->class)){
		err = PTR_ERR(zap_devp->class);
		goto fail;
	}

	
    sema_init(&zap_devp->sem, 1);

	cdev_init(&zap_devp->cdev, &zap_fops);
	zap_devp->cdev.owner = THIS_MODULE;
	zap_devp->cdev.ops = &zap_fops;
	err = cdev_add (&zap_devp->cdev, zap_devp->node, zap_devp->num_devices * 2);
	if (err) {
		printk(KERN_ERR "Error %d adding zap\n", err);
		goto fail;
	}

	zap_devp->status = 0;

    zap_devp->interface = kzalloc( zap_devp->num_devices * sizeof(struct zap_if), GFP_KERNEL );
    if ( !zap_devp->interface ) {
		printk(KERN_ERR "ZAP kzalloc FAIL\n");
        goto fail;
    }

    for (i=0; i < zap_devp->num_devices; i++ ) {
	    zap_devp->interface[i].rx_highwater = 0;
	    zap_devp->interface[i].tx_highwater = 0;
	    zap_devp->interface[i].rx_header_enable = 0;
	    zap_devp->interface[i].tx_header_enable = 0;
	    zap_devp->interface[i].rx_header_size = 0;
	    zap_devp->interface[i].tx_header_size = 0;
		device_create(zap_devp->class, NULL, MKDEV(MAJOR(zap_devp->node),i*2), NULL, "zaprx%d",i);
		device_create(zap_devp->class, NULL, MKDEV(MAJOR(zap_devp->node),(i*2)+1), NULL, "zaptx%d",i);
	    zap_devp->interface[i].rx_jumbo_pkt_enable = 0;
	    zap_devp->interface[i].tx_jumbo_pkt_enable = 0;
    }

    dma_set_coherent_mask(zap_devp->dev, 0xFFFFFFFF);

    zap_devp->rx_pool_paddr = rmem->base;
    zap_devp->rx_pool_size = rx_pool_sz;

    zap_devp->tx_pool_paddr = zap_devp->rx_pool_paddr + zap_devp->rx_pool_size;
    zap_devp->tx_pool_size = tx_pool_sz;
    dev_info(zap_devp->dev, "rx_pool 0x%llx (0x%lx), tx_pool 0x%llx (0x%lx)\n", 
            zap_devp->rx_pool_paddr, zap_devp->rx_pool_size, 
            zap_devp->tx_pool_paddr, zap_devp->tx_pool_size);

	err = dma_init(zap_devp, 
            zap_devp->rx_pool_paddr, zap_devp->rx_pool_size,
		    zap_devp->tx_pool_paddr, zap_devp->tx_pool_size);
	if (err) {
		printk(KERN_ERR "ZAP DMA init error %d\n", err);
		goto fail;
	}

	//ksym_arch_setup_dma_ops = (void *)kallsyms_lookup_name("arch_setup_dma_ops");
	arch_setup_dma_ops(&zap_device, 0, 0, NULL,false);

	return 0;

  fail:
	zap_remove( pdev );
	return err;
}


static const struct of_device_id zap_of_match[] = {
	{ .compatible = "iv,zap", },
	{ /* end of list */ }
};
MODULE_DEVICE_TABLE(of, zap_of_match);

static struct platform_driver zap_driver = {
	.probe = zap_probe,
	.remove = zap_remove,
	.driver = {
		.name = "zap",
		.of_match_table = zap_of_match,
	},
};
module_platform_driver(zap_driver);

MODULE_DESCRIPTION("iVeia ZAP driver");
MODULE_AUTHOR("iVeia, LLC");
MODULE_LICENSE("GPL");
