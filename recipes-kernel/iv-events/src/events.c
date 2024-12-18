/*
 * Events driver
 *
 * (C) Copyright 2008-2010, iVeia, LLC
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
 */
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/irq.h>

#include "events.h"
#include <linux/ioctl.h>

/*
 * Mapping to vsoc_mem of registers
 */
#define EVENTS_MASK_REG     0
#define EVENTS_PENDING_REG  1
#define EVENTS_MASK_EN_REG  2

struct events_dev {
	struct class *class;
	struct cdev cdev;
	struct dentry *dbg_dentry;
	struct semaphore sem;
	volatile u32 *vsoc_mem;
	u32 reg_base;
	u32 reg_sz;
    	int hw_irq;
    	int irq;
	int opened;
	struct list_head filelist;
};

struct events_file {
	struct list_head list;
	struct events_dev * dev;
	wait_queue_head_t waitq;
	u32 pending;
	u32 pending_mask;
	u32 all_events;
};

static dev_t events_dev_num = 0;

static struct events_dev *events_dev = NULL;

static u64 scratch = 0xdeadbeef;

#define ARE_REQUESTED_EVENTS_PENDING(f) \
	( f->all_events && (( f->pending & f->pending_mask ) == f->pending_mask )) \
	|| ( ! f->all_events && ( f->pending & f->pending_mask ))


static struct of_device_id gic_match[] = {
	{ .compatible = "arm,gic-400", },//Z8
	{ .compatible = "arm,cortex-a9-gic", },
	{ .compatible = "arm,cortex-a15-gic", },
	{ },
};


unsigned int xlate_irq(unsigned int hwirq)
{
        struct of_phandle_args irq_data;
        unsigned int irq;
        struct device_node *gic_node = NULL;

        //if (!gic_node)
        gic_node = of_find_matching_node(NULL, gic_match);

        if (WARN_ON(!gic_node))
                return hwirq;

        irq_data.np = gic_node;
        irq_data.args_count = 3;
        irq_data.args[0] = 0;
        irq_data.args[1] = hwirq; /* GIC SPI offset */
        irq_data.args[2] = IRQ_TYPE_LEVEL_HIGH;

        irq = irq_create_of_mapping(&irq_data);
        if (WARN_ON(!irq))
                irq = hwirq;

        pr_info("%s: hwirq %d, irq %d\n", __func__, hwirq, irq);

        return irq;
}


u32 events_get_events(struct events_dev * dev) 
{
	return (u32)(dev->vsoc_mem[EVENTS_PENDING_REG]);
}

/*
 * Called from intterupt context to wake_up all read()ers.
 */
void events_wake_up(struct events_dev * dev, int event)
{
	struct list_head * plist;
	list_for_each(plist, &dev->filelist) {
		struct events_file * pevents_file = list_entry(plist, struct events_file, list);
		pevents_file->pending |= (1 << event);
		if ( ARE_REQUESTED_EVENTS_PENDING(pevents_file)) {
			wake_up_interruptible( &pevents_file->waitq );
		}
	}

	return;
}
	

/*
 * For each pending event, clears it and wakes up read()ers.
 */
void events_foreach_wake_up(struct events_dev * dev)
{
	volatile unsigned int pending = dev->vsoc_mem[EVENTS_PENDING_REG];
	volatile unsigned int events = pending;
	int i;

	for ( i = 0; i < 31; i++ ) {
		if ( events & 1 )
			events_wake_up(dev, i);
		events >>= 1;
	}
	// clear the events
	dev->vsoc_mem[EVENTS_PENDING_REG] = pending;

	// disable all events from reporting interrupt
	dev->vsoc_mem[EVENTS_MASK_REG] = 0x00000000;

	return;
}


irqreturn_t events_isr(int irq, void *id)
{
	events_foreach_wake_up((struct events_dev *) id);
	return IRQ_HANDLED;
}



/*
 * open()
 */
int events_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct events_dev *dev = NULL;
	struct events_file *pevents_file = NULL;

	int success = 0;
	dev = container_of(inode->i_cdev, struct events_dev, cdev);

	pr_info("iv-events - OPEN (%d)\n", dev->opened);

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	disable_irq(dev->irq);

	pevents_file = kmalloc(sizeof(struct events_file), GFP_KERNEL);
	if (!pevents_file) {
		err = -ENOMEM;
		goto out;
	}

	init_waitqueue_head( &pevents_file->waitq );
	pevents_file->dev = dev;
	pevents_file->all_events = 0;
	pevents_file->pending = 0;
	pevents_file->pending_mask = 0;
	list_add(&pevents_file->list, &dev->filelist);
	filp->private_data = pevents_file;

	enable_irq(dev->irq);

	dev->opened++;
	
	success = 1;

out:
	if ( ! success ) {
		if ( pevents_file ) {
			list_del(&pevents_file->list);
		}
	}
	up(&dev->sem);

	return err;
}


/*
 * close()
 */
int events_release(struct inode *inode, struct file *filp)
{
	struct events_file *pevents_file = filp->private_data; 
	struct events_dev *dev = pevents_file->dev;
	int err = 0;

	pr_info("iv-events - RELEASE\n");

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	disable_irq(dev->irq);
	list_del(&pevents_file->list);
	enable_irq(dev->irq);
	kfree( pevents_file );

	dev->opened--;
	BUG_ON(dev->opened < 0);

	up(&dev->sem);
	
	return err;
}

long events_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */

	if (_IOC_TYPE(cmd) != EVENTS_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > EVENTS_IOC_MAXNR) return -ENOTTY;
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
	if (err) return -EFAULT;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	switch(cmd) {
		case EVENTS_IOC_R_INSTANCE_COUNT :			
			__put_user(dev->opened,(unsigned long __user *)arg);					
			break;		
		default:  /* redundant, as cmd was checked against MAXNR */
			retval = -ENOTTY;
			break;
	}
	up(&dev->sem);
	return retval;

}

/*
 * read()
 */
ssize_t events_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	ssize_t retval = 0;
	u32 events;
	int err;
	int i;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	//
	// Verify count/buf is word aligned, and at least a word big.
	//
	if ( count != sizeof(u32) ) {
		retval = -EFAULT;
		goto out;
	}

	//
	// Block until pending bits set, then clear them.  
	//
	// We're sending back the pending events to the app, so now we can
	// clear them.  We have to be careful, though to clear them atomically,
	// because we can be resetting them in the ISR.  We don't have to do
	// the whole thing atomically, because we're only clearing the bits
	// we're send back to the user.
	//
	up(&dev->sem);
	err = wait_event_interruptible(
		pevents_file->waitq, ARE_REQUESTED_EVENTS_PENDING(pevents_file)
		);
	if (err) return -ERESTARTSYS;
	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	events = pevents_file->pending & pevents_file->pending_mask;
	for (i = 0; i < 32; i++) {
		if ( events & ( 1 << i )) {
			pevents_file->pending &= ~(1 << i);
		}
	}

	if (copy_to_user(buf, &events, count)) {
		retval = -EFAULT;
		goto out;
	}

	retval = count;

out:
	up(&dev->sem);
	return retval;
}


/*
 * write()
 *
 * A write consists of a u32 that is the bitmask of events to wait for.  The
 * high bit is special (and means we can really only use 31 events) - it
 * indicates, if set, to wait for all events, instead of any event.
 */
ssize_t events_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	ssize_t retval = 0;
	u32 pending_mask;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if ( count != sizeof(u32) ) {
		retval = -EFAULT;
		goto out;
	}

	if (copy_from_user(&pending_mask, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	pevents_file->all_events = pending_mask & 0x80000000;
	pevents_file->pending_mask = pending_mask & ( ~ 0x80000000 );

  	// allow these events to cause interrupts
	dev->vsoc_mem[EVENTS_MASK_EN_REG] = pending_mask;

	retval = count;
out:
	up(&dev->sem);
	return retval;
}


unsigned int events_poll(struct file *filp, poll_table *wait)
{
	struct events_file * pevents_file = filp->private_data; 
	unsigned int mask = 0;

	poll_wait(filp, &pevents_file->waitq, wait);
	if ( ARE_REQUESTED_EVENTS_PENDING(pevents_file)) {
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}


struct file_operations events_fops = {
	.owner =    THIS_MODULE,
	.read =     events_read,
	.write =    events_write,
	.unlocked_ioctl = 	events_ioctl,
	.open =     events_open,
	.poll =     events_poll,
	.release =  events_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static int events_remove(struct platform_device *pdev)
{
	pr_info("iv-events - REMOVE\n");

	/* Get rid of our char dev entries */
	if (events_dev) {
		if ( events_dev->irq) {
			free_irq(events_dev->irq, events_dev);
		}

		if ( events_dev->dbg_dentry ) {
			debugfs_remove_recursive(events_dev->dbg_dentry);
		}

		if ( events_dev->vsoc_mem) {
		    iounmap( (char *) events_dev->vsoc_mem );
		}

		device_destroy( events_dev->class, MKDEV(MAJOR(events_dev_num), 0));
		class_destroy( events_dev->class );
		cdev_del(&events_dev->cdev);
		unregister_chrdev_region(events_dev_num, 1);
		kfree(events_dev);
		events_dev = NULL;
	}

	return 0;
}


static int levels_get(void *data, u64 *val)
{
	struct events_dev *dev = (struct events_dev *)data;

	*val = (u64)events_get_events(dev);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(levels_fops, levels_get, NULL, "0x%08llx\n");

static int trigger_set(void *data, u64 val)
{
	struct events_dev *dev = (struct events_dev *)data;
	u32 mask = (u32)val;
	int i;

	for ( i = 0; i < 31; i++ ) {
		if ( mask & 1 )
			events_wake_up(dev, i);
		mask >>= 1;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(trigger_fops, NULL, trigger_set, "0x%08llx\n");

static int scratch_get(void *data, u64 *val)
{
	struct events_dev *dev = (struct events_dev *)data;

	*val = scratch;

	return 0;
}

static int scratch_set(void *data, u64 val)
{
	struct events_dev *dev = (struct events_dev *)data;

	scratch = val;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(scratch_fops, scratch_get, scratch_set, "0x%08llx\n");

static int events_probe(struct platform_device *pdev)
{
	int err;
	struct dentry *dbg_dentry;

	pr_info("iv-events - PROBE\n");

	/*
	 * Get a range of minor numbers to work with, using static major.
	 */
	err = alloc_chrdev_region(&events_dev_num, 0, 1, MODNAME);
	if (err) {
		goto fail;
	}

	events_dev = kmalloc(sizeof(struct events_dev), GFP_KERNEL);
	if (!events_dev) {
		err = -ENOMEM;
		goto fail;
	}
	memset(events_dev, 0, sizeof(struct events_dev));

	events_dev->class = class_create(THIS_MODULE, "events");
	if (IS_ERR(events_dev->class)){
		err = PTR_ERR(events_dev->class);
		goto fail;
	}

	device_create(events_dev->class, NULL, events_dev_num , NULL, "events");
	
	sema_init(&events_dev->sem, 1);
	
	cdev_init(&events_dev->cdev, &events_fops);
	events_dev->cdev.owner = THIS_MODULE;
	events_dev->cdev.ops = &events_fops;
	err = cdev_add (&events_dev->cdev, events_dev_num, 1);
	if (err) {
		printk(KERN_WARNING MODNAME ": Error %d adding device", err);
		goto fail;
	}

	INIT_LIST_HEAD(&events_dev->filelist);
	events_dev->opened = 0;

	err = of_property_read_u32(pdev->dev.of_node, "irq", &events_dev->hw_irq);
	if ( err )
	{
		dev_err(&pdev->dev, "irq not specified in device tree\n");
		goto fail;
	}
	dev_info(&pdev->dev, "irq %u\n", events_dev->hw_irq);

	if ( of_property_read_u32_index(pdev->dev.of_node, "reg", 0, &events_dev->reg_base ) != 0 ) {
		dev_err(&pdev->dev, "dt reg ERROR (0)\n");
		goto fail;
	}
	if ( of_property_read_u32_index(pdev->dev.of_node, "reg", 1, &events_dev->reg_sz ) != 0 ) {
		dev_err(&pdev->dev, "dt reg ERROR (1)\n");
		goto fail;
	}
	dev_info(&pdev->dev, "pl reg base: 0x%x, sz 0x%x\n", events_dev->reg_base, events_dev->reg_sz); 

	events_dev->vsoc_mem = (unsigned int *)ioremap( events_dev->reg_base, events_dev->reg_sz );
	if ( events_dev->vsoc_mem == NULL ) {
		err = -ENOMEM;
		goto fail;
	}

	//  clear any pending events
	events_dev->vsoc_mem[EVENTS_PENDING_REG] = 0xFFFFFFFF;

	dbg_dentry = debugfs_create_dir(MODNAME, 0);
	if (!dbg_dentry) {
    		dev_err(&pdev->dev, "failed to create debugfs dir\n");
		err = -ENOMEM;
		goto fail;
	}	
	events_dev->dbg_dentry = dbg_dentry;

	dbg_dentry = debugfs_create_file( "levels", 0444, events_dev->dbg_dentry, events_dev, &levels_fops);
	if (!dbg_dentry) {
    		dev_err(&pdev->dev, "failed to create debugfs file\n");
		err = -ENOMEM;
		goto fail;
	}

	dbg_dentry = debugfs_create_file( "trigger_mask", 0222, events_dev->dbg_dentry, events_dev, &trigger_fops);
	if (!dbg_dentry) {
    		dev_err(&pdev->dev, "failed to create debugfs file\n");
		err = -ENOMEM;
		goto fail;
	}

	dbg_dentry = debugfs_create_file( "scratch", 0666, events_dev->dbg_dentry, events_dev, &scratch_fops);
	if (!dbg_dentry) {
    		dev_err(&pdev->dev, "failed to create debugfs file\n");
		err = -ENOMEM;
		goto fail;
	}

	events_dev->irq = xlate_irq(events_dev->hw_irq);
	err = request_irq(events_dev->irq, (irq_handler_t)events_isr, 0, MODNAME, events_dev);
	if ( err ) goto fail;

	return 0;

fail:
	events_remove(pdev);
	return err;
}


static const struct of_device_id events_of_match[] = {
	{ .compatible = "iv,events", },
	{ /* end of list */ }
};
MODULE_DEVICE_TABLE(of, events_of_match);

static struct platform_driver events_driver = {
	.probe = events_probe,
	.remove = events_remove,
	.driver = {
		.name = "iv-events",
		.of_match_table = events_of_match,
	},
};
module_platform_driver(events_driver);

MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("Events char driver");
MODULE_LICENSE("GPL");
