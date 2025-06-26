/*
 * iVeia HH1 LCD Framebuffer driver
 *
 * (C) Copyright 2021, iVeia, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pm.h>

/*********************************************************************
 *
 * Globals
 *
 *********************************************************************/

#define MODNAME "ivfbdev"
#define IV_SCREEN_WIDTH (1024)
#define IV_SCREEN_HEIGHT (768)
#define IV_BYTES_PER_PIXEL (2)
#define ANDROID_BYTES_PER_PIXEL 2
#if 0
#if defined(CONFIG_MACH_IV_ATLAS_I_Z7E)
    #define VIDEOMEMSTART	(0x1F000000)
    #define VIDEOMEMFINISH	(0x1FFFFFFF)
#elif defined(CONFIG_ARCH_IV_ATLAS_II_Z8)
    #define VIDEOMEMSTART	(0x7F000000)
    #define VIDEOMEMFINISH	(0x7FFFFFFF)
#else  //OZZY
    #define VIDEOMEMSTART	(0x7F000000)
    #define VIDEOMEMFINISH	(0x7FFFFFFF)
#endif
#else
    #define VIDEOMEMSTART	(0x7F000000)
    #define VIDEOMEMFINISH	(0x7FFFFFFF)
#endif

//#define VIDEOMEMSIZE	(IV_BYTES_PER_PIXEL*IV_SCREEN_WIDTH*IV_SCREEN_HEIGHT)
#define VIDEOMEMSIZE   VIDEOMEMFINISH-VIDEOMEMSTART

static void * fb_addr = NULL;

static struct fb_var_screeninfo iveia_fb_default = {
	.xres           = IV_SCREEN_WIDTH,
	.yres           = IV_SCREEN_HEIGHT,
	.xres_virtual   = IV_SCREEN_WIDTH,
	.yres_virtual   = IV_SCREEN_HEIGHT,
	.bits_per_pixel = IV_BYTES_PER_PIXEL * 8,
//    .transp.offset  = 0,
//    .transp.length  = 8,
#if 0
    .blue.offset    = 11,
    .blue.length    = 5,
    .green.offset   = 5,
    .green.length   = 6,
    .red.offset     = 0,
    .red.length     = 5,
#else
    .blue.offset    = 0,
    .blue.length    = 5,
    .green.offset   = 5,
    .green.length   = 6,
    .red.offset     = 11,
    .red.length     = 5,
#endif
};

static struct fb_fix_screeninfo iveia_fb_fix = {
	.id             = "iVeia HH1 FB",
	.type           = FB_TYPE_PACKED_PIXELS,
	.visual         = FB_VISUAL_TRUECOLOR,
    .line_length    = IV_SCREEN_WIDTH * IV_BYTES_PER_PIXEL,
	.smem_start     = VIDEOMEMSTART,
	.smem_len       = VIDEOMEMSIZE,
};

static struct fb_ops iveia_fb_ops = {
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
};

static u32 pseudo_palette[17] = {0};  // required by sys_imageblit (only used for fb console)

struct iveia_fb_par {
    unsigned long * misc_reg;
};

/*********************************************************************
 *
 * Module probe/remove
 *
 *********************************************************************/

static int iveia_fb_probe(struct platform_device *dev)
{
	struct fb_info * info = NULL;
    struct iveia_fb_par * par = NULL;
	int ret, i;

    u64 dt_reg[2];
    u32 dt_resolution[3];

    for ( i = 0; i < sizeof(dt_reg)/sizeof(dt_reg[0]); i++ )
    {
        ret = of_property_read_u64_index( dev->dev.of_node, 
                           "reg", i, &dt_reg[i] );
        if (ret)
        {
            printk(KERN_ERR "ivfbdev: dt ERR: reg property\n");
            goto out;
        }
    }
    printk(KERN_ERR "ivfbdev: dt reg: 0x%llx 0x%llx\n", 
            dt_reg[0], dt_reg[1]);

    for ( i = 0; i < sizeof(dt_resolution)/sizeof(dt_resolution[0]); i++ )
    {
        ret = of_property_read_u32_index( dev->dev.of_node, 
                           "resolution", i, &dt_resolution[i] );
        if (ret)
        {
            printk(KERN_ERR "ivfbdev: dt ERR: resolution property\n");
            goto out;
        }
    }
    printk(KERN_ERR "ivfbdev: dt resolution: %u x %u, %u bpp\n", 
            dt_resolution[0], dt_resolution[1], dt_resolution[2]);


    fb_addr = ioremap(iveia_fb_fix.smem_start, iveia_fb_fix.smem_len);
	if ( ! fb_addr ) return -ENOMEM;

    /*
     * Don't erase the screen - u-boot already put up a splash screen.
     */
    if (0) {
        memset(fb_addr, 0, iveia_fb_fix.smem_len);
    }

	info = framebuffer_alloc(sizeof(struct fb_info), &dev->dev);
	if (!info) goto out;

	info->screen_base = (char __iomem *) fb_addr;
	info->fbops = &iveia_fb_ops;

    info->var = iveia_fb_default;
	info->fix = iveia_fb_fix;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->pseudo_palette = pseudo_palette;

    par = info->par;

	ret = register_framebuffer(info);
	if (ret < 0) goto out;

	platform_set_drvdata(dev, info);

	printk(KERN_INFO
	       "fb%d: iVeia frame buffer device, using %ldK of video memory\n",
	       info->node, (unsigned long) (iveia_fb_fix.smem_len >> 10));
	return 0;

out:
	printk(KERN_WARNING "fb: Could not intitalize fb device (err %d)\n", ret);
    if (info)
        framebuffer_release(info);
    if (fb_addr)
        iounmap(fb_addr);

	return ret;
}

static int iveia_fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
    struct iveia_fb_par * par;

	if (info) {
		unregister_framebuffer(info);
        if (fb_addr)
            iounmap(fb_addr);
		framebuffer_release(info);

        par = info->par;
	}
	return 0;
}

/*********************************************************************
 *
 * Module init/exit
 *
 *********************************************************************/

/* Match table for OF platform binding */
static const struct of_device_id ivfbdev_of_match[] = {
	{ .compatible = "iveia,fbdev", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, ivfbdev_of_match);

static struct platform_driver iveia_fb_driver = {
	.probe	    = iveia_fb_probe,
	.remove     = iveia_fb_remove,
	.driver     = {
		.name	= MODNAME,
		.of_match_table = ivfbdev_of_match,
	},
};
module_platform_driver(iveia_fb_driver);

MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("iVeia Framebuffer driver");
MODULE_LICENSE("GPL");

