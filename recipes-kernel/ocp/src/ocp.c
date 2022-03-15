/*
 * OCP char driver
 *
 * (C) Copyright 2008-2020, iVeia, LLC
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
 * Licensed under the GNU/GPL with iVeia disclaimer. See LICENSE for details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "ocp.h"

 
struct ocp_mapping {
    u64 phys;
    u64 size;
    void *virt;
};

struct ocp_dev {
    struct semaphore sem;
    unsigned int instance_count;

    dev_t node;
    struct class *class;
    struct device *dev;
    struct cdev cdev;

    int num_addr_spaces;
    struct ocp_mapping *mappings;
};

#define MODNAME "ocp"



static unsigned int minor_to_mapping_index(unsigned int minor){
    if ( minor == 0 )
        return 0;
    return minor - 1;
}

/*
 * open()
 */
int ocp_open(struct inode *inode, struct file *filp)
{
    struct ocp_dev * dev;
    dev = container_of(inode->i_cdev, struct ocp_dev, cdev);
    filp->private_data = dev;
    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
    dev->instance_count++;    
    up(&dev->sem);
    return 0;
}

/*
 * close()
 */
int ocp_release(struct inode *inode, struct file *filp)
{
    struct ocp_dev *dev = filp->private_data;  
    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
    dev->instance_count--;
    up(&dev->sem);
    return 0;
}

/*
 * read()
 */
ssize_t ocp_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct ocp_dev *dev = filp->private_data; 
    unsigned int minor = iminor(filp->f_path.dentry->d_inode);
    ssize_t retval = 0;

    if (minor > dev->num_addr_spaces) return -ENODEV;

#if defined(CONFIG_ARCH_ZYNQMP)
    if (count == 8)
        printk(KERN_ERR MODNAME 
            "WARNING: ocp_read(8 bytes). OCP Bus is currently 32-bits wide. "
            "(64-bit system : User read 'unsigned long', driver expected 'uint32_t').\n");
#endif

    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

    if (*f_pos >= dev->mappings[minor_to_mapping_index(minor)].size) goto out;
    if (*f_pos + count > dev->mappings[minor_to_mapping_index(minor)].size) {
        count = dev->mappings[minor_to_mapping_index(minor)].size - *f_pos;
    }
    if ( count % sizeof(unsigned int) != 0 || ((unsigned int) buf) % sizeof(unsigned int) != 0 )
    {
        retval = -EFAULT;
        goto out;
    }

    if (copy_to_user(buf, (void *)((uintptr_t)dev->mappings[minor_to_mapping_index(minor)].virt + *f_pos), count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;

    retval = count;

out:
    up(&dev->sem);
    return retval;
}

/*
 * write()
 */
ssize_t ocp_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct ocp_dev *dev = filp->private_data; 
    unsigned int minor = iminor(filp->f_path.dentry->d_inode);
    ssize_t retval = 0;

    if (minor > dev->num_addr_spaces) return -ENODEV;

#if defined(CONFIG_ARCH_ZYNQMP)
    if (count == 8)
        printk(KERN_ERR MODNAME 
            "WARNING: ocp_write(8 bytes). OCP Bus is currently 32-bits wide. "
            "(64-bit system : User wrote 'unsigned long', driver expected 'uint32_t').\n");
#endif

    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

    if (*f_pos >= dev->mappings[minor_to_mapping_index(minor)].size) goto out;
    if (*f_pos + count > dev->mappings[minor_to_mapping_index(minor)].size) {
        count = dev->mappings[minor_to_mapping_index(minor)].size - *f_pos;
    }
    if ( count % sizeof(unsigned int) != 0 || ((unsigned int) buf) % sizeof(unsigned int) != 0 )
    {
        retval = -EFAULT;
        goto out;
    }

    if (copy_from_user((void *)(dev->mappings[minor_to_mapping_index(minor)].virt + *f_pos), buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;

    retval = count;

out:
    up(&dev->sem);
    return retval;
}

long ocp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    int err = 0;
    int retval = 0;
    struct ocp_dev *dev = filp->private_data;

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != OCP_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > OCP_IOC_MAXNR) return -ENOTTY;

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
        case OCP_IOC_R_INSTANCE_COUNT:          
            __put_user(dev->instance_count,(unsigned long __user *)arg);            
            break;                      

        default:  /* redundant, as cmd was checked against MAXNR */
            retval = -ENOTTY;
            break;
    }
    up(&dev->sem);
    return retval;

}

loff_t ocp_llseek(struct file *filp, loff_t off, int whence)
{
    struct ocp_dev *dev = filp->private_data;
    unsigned int minor = iminor(filp->f_path.dentry->d_inode);
    loff_t newpos;

    if (minor > dev->num_addr_spaces) return -ENODEV;

    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = off;
        break;

      case 1: /* SEEK_CUR */
        newpos = filp->f_pos + off;
        break;

      case 2: /* SEEK_END */
        newpos = dev->mappings[minor_to_mapping_index(minor)].size + off;
        break;

      default: /* can't happen */
        newpos = -EINVAL;
        goto out;
    }
    if (newpos < 0) {
        newpos = -EINVAL;
        goto out;
    }

    if ( newpos % sizeof(unsigned int) != 0 ) {
        newpos = -EFAULT;
        goto out;
    }

    filp->f_pos = newpos;

out:
    up(&dev->sem);
    return newpos;
}

static int ocp_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    unsigned int ulPhysStart;
    unsigned int available_size;
    unsigned int pfn;
    size_t size = vma->vm_end - vma->vm_start;

    struct ocp_dev *dev = filp->private_data; 
    unsigned int minor = iminor(filp->f_path.dentry->d_inode);

    if (minor > dev->num_addr_spaces) return -ENODEV;


    ulPhysStart = dev->mappings[minor_to_mapping_index(minor)].phys;
    ulPhysStart += (vma->vm_pgoff << PAGE_SHIFT);
    available_size = dev->mappings[minor_to_mapping_index(minor)].size;

    if ((vma->vm_pgoff << PAGE_SHIFT) + size > available_size)
        return -EINVAL;

    pfn = (ulPhysStart >> PAGE_SHIFT);

    vma->vm_page_prot = phys_mem_access_prot(filp, pfn,
                         size,
                         vma->vm_page_prot);
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) return -EAGAIN;

    return 0;
}

struct file_operations ocp_fops = {
    .owner =    THIS_MODULE,
    .read =     ocp_read,
    .write =    ocp_write,
    .mmap  =    ocp_mmap,
    .unlocked_ioctl =   ocp_ioctl,
    .llseek =   ocp_llseek,
    .open =     ocp_open,
    .release =  ocp_release,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static int ocp_remove(struct platform_device *pdev)
{
	struct ocp_dev *ocp_devp = platform_get_drvdata(pdev);
    int i;

    pr_info("ocp: REMOVE\n");

    if ( ocp_devp->mappings ) {
        for ( i = 0; i < ocp_devp->num_addr_spaces; i++ ) {
            if ( ocp_devp->mappings[i].virt ) {
                iounmap( (void *)ocp_devp->mappings[i].virt );
            }

            device_destroy( ocp_devp->class, MKDEV(MAJOR(ocp_devp->node),i+1) );
        }

        kfree( ocp_devp->mappings );
    }

    device_destroy( ocp_devp->class, MKDEV(MAJOR(ocp_devp->node),0) );
    class_destroy( ocp_devp->class );
    cdev_del(&ocp_devp->cdev);
    unregister_chrdev_region(ocp_devp->node, ocp_devp->num_addr_spaces);

    kfree(ocp_devp);

    return 0;
}

static int ocp_probe(struct platform_device *pdev)
{
    int err;
    int i;
    struct ocp_dev *ocp_devp;
    struct ocp_mapping *mapping;

    pr_info("ocp: PROBE\n");

    ocp_devp = kzalloc(sizeof(struct ocp_dev), GFP_KERNEL);
    if (!ocp_devp) {
        err = -ENOMEM;
        goto fail;
    }
	platform_set_drvdata(pdev, ocp_devp);
	ocp_devp->dev = &pdev->dev;

    /*
     * Get a range of minor numbers to work with, using static major.
     */
    err = alloc_chrdev_region(&ocp_devp->node, 0, ocp_devp->num_addr_spaces + 1, MODNAME);
    if (err) {
        goto fail;
    }
    

    cdev_init(&ocp_devp->cdev, &ocp_fops);
    ocp_devp->cdev.owner = THIS_MODULE;
    ocp_devp->cdev.ops = &ocp_fops;
    err = cdev_add (&ocp_devp->cdev, ocp_devp->node, ocp_devp->num_addr_spaces + 1);
    if (err) {
        printk(KERN_NOTICE MODNAME ": Error %d adding ocp\n", err);
        goto fail;
    }

    //TODO: allocate mappings array here, free in remove()
    ocp_devp->num_addr_spaces = of_property_count_elems_of_size(pdev->dev.of_node, "reg", sizeof(u64));
    ocp_devp->num_addr_spaces /= 2;
    ocp_devp->mappings = kzalloc(sizeof(struct ocp_mapping) * ocp_devp->num_addr_spaces, GFP_KERNEL);

    ocp_devp->class = class_create(THIS_MODULE, "ocp");
    if (IS_ERR(ocp_devp->class))
        return PTR_ERR(ocp_devp->class);

    device_create(ocp_devp->class, NULL, MKDEV(MAJOR(ocp_devp->node),0), NULL, "ocp");

    sema_init(&ocp_devp->sem, 1);
    ocp_devp->instance_count = 0;

    for ( i = 0; i < ocp_devp->num_addr_spaces; i++ ) {
        mapping = &ocp_devp->mappings[i];

        if ( of_property_read_u64_index(pdev->dev.of_node, "reg", (i*2), &mapping->phys ) != 0 ) {
            break;
        }
        if ( of_property_read_u64_index(pdev->dev.of_node, "reg", (i*2)+1, &mapping->size ) != 0 ) {
            break;
        }

        mapping->virt = ioremap( mapping->phys, mapping->size );
        if ( mapping->virt == NULL ) {
            dev_err(ocp_devp->dev, ": Error mapping ocp space\n" );
            err = -ENOMEM;
            goto fail;
        }

        dev_info(ocp_devp->dev, "map 0x%llx -> 0x%llx (0x%llx)\n", mapping->phys, mapping->virt, mapping->size);

        device_create(ocp_devp->class, NULL, MKDEV(MAJOR(ocp_devp->node),i+1), NULL, "ocp%d",i);
    }

    return 0;

fail:
    ocp_remove( pdev );
    return err;
}


/* Match table for OF platform binding */
static const struct of_device_id ocp_of_match[] = {
	{ .compatible = "iveia,ocp", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, ocp_of_match);

static struct platform_driver ocp_driver = {
	.probe = ocp_probe,
	.remove = ocp_remove,
	.driver = {
		.name = "ocp",
		.of_match_table = ocp_of_match,
	},
};
module_platform_driver(ocp_driver);

MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("OCP char driver");
MODULE_LICENSE("GPL");
