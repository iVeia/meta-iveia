#ifndef __EVENTS_H_
#define __EVENTS_H_
/*
 * Ioctl definitions
 */
#define EVENTS_IOC_MAGIC  'e'

#define EVENTS_IOC_R_INSTANCE_COUNT		_IOR(EVENTS_IOC_MAGIC,  0, unsigned long)

#define EVENTS_IOC_MAXNR 0

#endif
