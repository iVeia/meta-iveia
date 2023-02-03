/*
 * iVeia passthrough driver
 *
 * Copyright iVeia 2021
 *
 * Contact: Maxwell Walter (mwalter@iveia.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG 1

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#define IVPASS_WIDTH_MIN 640  
#define IVPASS_WIDTH_MAX 10328
#define IVPASS_HEIGHT_MIN 480 
#define IVPASS_HEIGHT_MAX 7760


struct ivpass_info {
  struct device *dev;
  struct v4l2_subdev subdev;
  
  // Currently one in / one out
  struct media_pad pads[2];

  struct v4l2_mbus_framefmt formats[2];
  struct v4l2_mbus_framefmt default_format;

  bool streaming;
};


static inline struct ivpass_info *get_info_from_subdev(struct v4l2_subdev *subdev) {
  return container_of(subdev, struct ivpass_info, subdev);
}

static struct v4l2_subdev_core_ops ivpass_cops = {
  //None
};

static int ivpass_s_stream(struct v4l2_subdev *subdev, int enable) {
  struct ivpass_info *info = get_info_from_subdev(subdev);
  dev_dbg(info->dev, "Stream on\n");
  
  // Just return true always
  return 0;
}

static struct v4l2_subdev_video_ops ivpass_vops = {
  .s_stream = ivpass_s_stream,
};


static int ivpass_enum_mbus_code(struct v4l2_subdev *subdev,
                               struct v4l2_subdev_pad_config *cfg,
                               struct v4l2_subdev_mbus_code_enum *code) {
  struct v4l2_mbus_framefmt *format;
  
  if (code->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
    pr_warn("ivpass: Which is active: %d\n", code->which);
    return -EINVAL;
  }
  
  if (code->index) {
    pr_warn("ivpass: index is %d\n", code->index);
    return -EINVAL;
  }

  // My try_format just passes the format through, so this is basically just
  //  saying "yes" to every format that gets passed in
  format = v4l2_subdev_get_try_format(subdev, cfg, code->pad);
  code->code = format->code;
  
  return 0;
}
static int ivpass_enum_frame_size(struct v4l2_subdev *subdev,
                                  struct v4l2_subdev_pad_config *config,
                                  struct v4l2_subdev_frame_size_enum *fsize) {
  // Hard coded for now - limit to the max size of Xilinx framebuffer
  fsize->min_width =  IVPASS_WIDTH_MIN;
  fsize->max_width =  IVPASS_WIDTH_MAX;
  fsize->min_height = IVPASS_HEIGHT_MIN;
  fsize->max_height = IVPASS_HEIGHT_MAX;
    
  return 0;
}

static int ivpass_get_fmt(struct v4l2_subdev *subdev,
                          struct v4l2_subdev_pad_config *cfg,
                          struct v4l2_subdev_format *fmt) {
  struct ivpass_info *info = get_info_from_subdev(subdev);
  
  switch (fmt->which) {
  case V4L2_SUBDEV_FORMAT_TRY:
    fmt->format =  *v4l2_subdev_get_try_format(subdev, cfg, fmt->pad);
    break;
  case V4L2_SUBDEV_FORMAT_ACTIVE:
    fmt->format =  info->formats[fmt->pad];
    break;
  default:
    break;
  }
  dev_dbg(info->dev, "Trying to get format to (%d x %d) %d %d %d %d %d :: %d\n",
          fmt->format.width,
          fmt->format.height,
          fmt->format.code,
          fmt->format.field,
          fmt->format.colorspace,
          fmt->format.ycbcr_enc,
          fmt->format.quantization,
          fmt->which);
  
  return 0;
}

static int ivpass_set_fmt(struct v4l2_subdev *subdev,
                          struct v4l2_subdev_pad_config *cfg,
                          struct v4l2_subdev_format *fmt) {
  struct ivpass_info *info = get_info_from_subdev(subdev);
  
  clamp_t(unsigned int, fmt->format.width, IVPASS_WIDTH_MIN, IVPASS_WIDTH_MAX);
  clamp_t(unsigned int, fmt->format.height, IVPASS_HEIGHT_MIN, IVPASS_HEIGHT_MAX);
  
  dev_dbg(info->dev, "Trying to set format to (%d x %d) %d %d %d %d %d (pad:%d):: %d\n",
          fmt->format.width,
          fmt->format.height,
          fmt->format.code,
          fmt->format.field,
          fmt->format.colorspace,
          fmt->format.ycbcr_enc,
          fmt->format.quantization,
          fmt->pad,
          fmt->which);


  switch(fmt->which) {
  case V4L2_SUBDEV_FORMAT_TRY:
    *v4l2_subdev_get_try_format(subdev, cfg, fmt->pad) = fmt->format;
    break;
  case V4L2_SUBDEV_FORMAT_ACTIVE:
    info->formats[fmt->pad] = fmt->format;
    break;
  default:
    return -EINVAL;
  }

  return 0;
}

static struct v4l2_subdev_pad_ops ivpass_pops = {
  .enum_mbus_code       = ivpass_enum_mbus_code,
  .enum_frame_size      = ivpass_enum_frame_size,
  .get_fmt              = ivpass_get_fmt,
  .set_fmt              = ivpass_set_fmt,
};


static struct v4l2_subdev_ops ivpass_ops = {
  .core  = &ivpass_cops,
  .video = &ivpass_vops,
  .pad   = &ivpass_pops,
};

static int ivpass_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh) {
  struct ivpass_info *info = get_info_from_subdev(subdev);
  struct v4l2_mbus_framefmt *format;

  dev_dbg(info->dev, "open\n");

  // Set to the default format on open
  format = v4l2_subdev_get_try_format(subdev, fh->pad, 0);
  *format = info->default_format;
  
  format = v4l2_subdev_get_try_format(subdev, fh->pad, 1);
  *format = info->default_format;
  
  return 0;
}
static int ivpass_close(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh) {
  return 0;
}
static const struct v4l2_subdev_internal_ops ivpass_internal_ops = {
  .open = ivpass_open,
  .close = ivpass_close,
};

static const struct media_entity_operations ivpass_media_ops = {
  .link_validate = v4l2_subdev_link_validate,
};


static int ivpass_probe(struct platform_device *pdev) {
  struct ivpass_info *info;
  struct v4l2_subdev *subdev;
  u32 ret;

  info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
  if(!info) {
    return -ENOMEM;
  }
  info->dev = &pdev->dev;

  dev_warn(&pdev->dev, "probing iVeia passthrough driver\n");

  // Use Xilinx' defaults here (just because)
  info->default_format.code = MEDIA_BUS_FMT_VYYUYY8_1X24;
  info->default_format.field = V4L2_FIELD_NONE;
  info->default_format.colorspace = V4L2_COLORSPACE_REC709;

  // Currently, pad0 is input, pad1 though padN is output
  info->pads[0].flags = MEDIA_PAD_FL_SOURCE;
  info->formats[0] = info->default_format;
  info->pads[1].flags = MEDIA_PAD_FL_SINK;
  info->formats[1] = info->default_format;

  subdev = &info->subdev;
  v4l2_subdev_init(subdev, &ivpass_ops);
  subdev->dev = info->dev;
  subdev->internal_ops = &ivpass_internal_ops;
  strlcpy(subdev->name, dev_name(&pdev->dev), sizeof(subdev->name));
  v4l2_set_subdevdata(subdev, info);
  subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
  subdev->entity.ops = &ivpass_media_ops;

  ret = media_entity_pads_init(&subdev->entity, 2, info->pads);
  if (ret < 0) {
    dev_dbg(&pdev->dev, "Failed to init media pads\n");
    goto error;
  }

  platform_set_drvdata(pdev, info);

  ret = v4l2_async_register_subdev(subdev);
  if (ret < 0) {
    dev_err(&pdev->dev, "failed to register subdev\n");
    goto error;
  }

  return 0;

error:
  media_entity_cleanup(&subdev->entity);
  return ret;
};

static int ivpass_remove(struct platform_device *pdev) {
  struct ivpass_info *info = platform_get_drvdata(pdev);
  struct v4l2_subdev *subdev = &info->subdev;
  
  v4l2_async_unregister_subdev(subdev);
  media_entity_cleanup(&subdev->entity);
  
  return 0;
}


static const struct of_device_id ivpass_of_table[] = {
  {.compatible = "iveia,passthrough"},
  {}
};
MODULE_DEVICE_TABLE(of, ivpass_of_table);



static int __maybe_unused ivpass_pm_suspend(struct device *dev) {
	return 0;
}
static int __maybe_unused ivpass_pm_resume(struct device *dev) {
	return 0;
}
static SIMPLE_DEV_PM_OPS(ivpass_pm_ops, ivpass_pm_suspend, ivpass_pm_resume);


static struct platform_driver ivpass_driver = {
	.driver = {
		.name		= "iVeia passthrough driver",
		.pm		= &ivpass_pm_ops,
		.of_match_table	= ivpass_of_table,
	},
	.probe			= ivpass_probe,
	.remove			= ivpass_remove,
};
module_platform_driver(ivpass_driver);

MODULE_AUTHOR("Maxwell Walter <mwalter@iveia.com>");
MODULE_DESCRIPTION("iVeia passthrough driver");
MODULE_LICENSE("GPL v2");
