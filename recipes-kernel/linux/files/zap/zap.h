/*
 * ZAP (Zero-copy Application Port) driver
 *
 * (C) Copyright 2008, iVeia, LLC
 *
 * This header is intended to be included by both the ZAP driver and the ZAP
 * API Library that accesses the driver.  As such, it does not include
 * <ioctl.h>, which means that the driver or library need to include it before
 * including this file.  The included ioctl.h may come from different
 * locations.
 */
#ifndef _ZAP_H_
#define _ZAP_H_

/*
 * Ioctl definitions
 */
#define ZAP_IOC_MAGIC  'z'

#define ZAP_IOC_R_STATUS                _IOR(ZAP_IOC_MAGIC,  0, unsigned long)
#define ZAP_IOC_R_RX_HIGH_WATER_MARK    _IOR(ZAP_IOC_MAGIC,  1, unsigned long)
#define ZAP_IOC_R_TX_HIGH_WATER_MARK    _IOR(ZAP_IOC_MAGIC,  2, unsigned long)
//#define ZAP_IOC_R_RECV_TIMEOUT          _IOR(ZAP_IOC_MAGIC,  3, unsigned long)
//#define ZAP_IOC_W_RECV_TIMEOUT          _IOW(ZAP_IOC_MAGIC,  4, unsigned long)
//#define ZAP_IOC_R_ALLOC_TIMEOUT         _IOR(ZAP_IOC_MAGIC,  5, unsigned long)
//#define ZAP_IOC_W_ALLOC_TIMEOUT         _IOW(ZAP_IOC_MAGIC,  6, unsigned long)
#define ZAP_IOC_R_TX_MAX_SIZE           _IOR(ZAP_IOC_MAGIC,  7, unsigned long)
#define ZAP_IOC_W_TX_MAX_SIZE           _IOW(ZAP_IOC_MAGIC,  8, unsigned long)

#define ZAP_IOC_R_TX_HEADER_SIZE    _IOR(ZAP_IOC_MAGIC,  11, unsigned long)
#define ZAP_IOC_W_TX_HEADER_SIZE    _IOW(ZAP_IOC_MAGIC,  12, unsigned long)
#define ZAP_IOC_R_RX_MAX_SIZE           _IOR(ZAP_IOC_MAGIC,  13, unsigned long)
#define ZAP_IOC_W_RX_MAX_SIZE           _IOW(ZAP_IOC_MAGIC,  14, unsigned long)

#define ZAP_IOC_R_RX_HEADER_SIZE    _IOR(ZAP_IOC_MAGIC,  17, unsigned long)
#define ZAP_IOC_W_RX_HEADER_SIZE    _IOW(ZAP_IOC_MAGIC,  18, unsigned long)
#define ZAP_IOC_R_FAKEY                 _IOR(ZAP_IOC_MAGIC,  19, unsigned long)
#define ZAP_IOC_W_FAKEY                 _IOW(ZAP_IOC_MAGIC,  20, unsigned long)
//TODO: Implement:
#define ZAP_IOC_R_RX_DMA_ON             _IOR(ZAP_IOC_MAGIC,  21, unsigned long)
#define ZAP_IOC_W_RX_DMA_ON             _IOW(ZAP_IOC_MAGIC,  22, unsigned long)
#define ZAP_IOC_R_TX_DMA_ON             _IOR(ZAP_IOC_MAGIC,  23, unsigned long)
#define ZAP_IOC_W_TX_DMA_ON             _IOW(ZAP_IOC_MAGIC,  24, unsigned long)
#define ZAP_IOC_R_POOL_SIZE             _IOR(ZAP_IOC_MAGIC,  25, unsigned long)

#define ZAP_IOC_R_INSTANCE_COUNT		_IOR(ZAP_IOC_MAGIC,  26, unsigned long)
#define ZAP_IOC_MAXNR 26 

/*
 * Ioctl argument values.
 */
/* FAKEY modes */
#define IV_ZAP_OPT_FAKEY_MODE_OFF           (0)
#define IV_ZAP_OPT_FAKEY_MODE_FIXED_PATT    (1)
#define IV_ZAP_OPT_FAKEY_MODE_COUNTING      (2)
#define IV_ZAP_OPT_FAKEY_MODE_LOOPBACK      (3)
#define IV_ZAP_OPT_FAKEY_MODE_DELAYED_RECV  (4)

#define ZAP_DESC_FLAG_OVERFLOW_OOB              (0x02)
#define ZAP_DESC_FLAG_OVERFLOW_DATA             (0x04)
#define ZAP_DESC_FLAG_INVALID_APP_DATA          (0x08)
#define ZAP_DESC_FLAG_STREAMING_WITH_OOB_ERR    (0x40000000)
#define ZAP_DESC_FLAG_DMA_ERR                   (0x80000000)

//
// NOTE: This size is based on the FPGA FIFO size.  It would be better if we
// dynamically got this from the FPGA.  Also, separate TX and RX sizes would be
// good.  And, for TX the size could match the full FIFO size, while with RX,
// it would be better for the max size to be half the FIFO size or so (to allow
// buffering).
//
#define ZAP_PACKET_FPGA_MAX_SIZE (64*1024)

#endif
