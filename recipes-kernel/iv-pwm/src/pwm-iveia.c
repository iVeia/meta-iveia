/*
 * Driver for iVeia PL PWM controller
 *
 * Copyright (C) 2021 iVeia inc
 *    Maxwell Walter (mwalter@iveia.com)
 *
 * Licensed under GPLv2.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#define CONTROL_REG         0x00
#define  CONTROL_REG_ENABLE 0x02
#define DUTY_REG            0x04
#define PERIOD_REG          0x08
#define CLOCK_REG           0x0C

struct iveia_pwm_chip {
	struct pwm_chip chip;
	void __iomem *base;
};


static const struct of_device_id iveia_pwm_dt_ids[] = {
  { .compatible = "iveia,pl-pwm",},
  {}
};
MODULE_DEVICE_TABLE(of, iveia_pwm_dt_ids);

static int iveia_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
        struct iveia_pwm_chip *iveia_pwm = container_of(chip, struct iveia_pwm_chip, chip);

	struct pwm_state cstate;
	pwm_get_state(pwm, &cstate);

	if (state->enabled) {
          //TODO: disable when we need to change period
          u64 pwm_clock = readl(iveia_pwm->base + CLOCK_REG); // PWM input clock in Hz
          u64 period_reg = (cstate.period * pwm_clock) / 1000000000;
          u64 duty_reg = (cstate.duty_cycle * pwm_clock) / 1000000000;
          //dev_info(chip->dev, "apply (%llu) :: %d -> %llu(0x%llX) :: %d -> %llu(0x%llX)\n",
          //         pwm_clock,
          //         cstate.period, period_reg, period_reg,
          //         cstate.duty_cycle, duty_reg, duty_reg);
          writel((u32)period_reg, iveia_pwm->base + PERIOD_REG);
          writel((u32)duty_reg, iveia_pwm->base + DUTY_REG);
          
          writel(CONTROL_REG_ENABLE, iveia_pwm->base + CONTROL_REG);
	} else if (cstate.enabled) {
          // Not enabled, so just leave it be
          writel(0x0, iveia_pwm->base + CONTROL_REG); // disable        
        }

	return 0;
}

static const struct pwm_ops iveia_pwm_ops = {
	.apply = iveia_pwm_apply,
	.owner = THIS_MODULE,
};

static int iveia_pwm_probe(struct platform_device *pdev)
{
	struct iveia_pwm_chip *iveia_pwm;
	struct resource *res;
	int ret;


        dev_info(&pdev->dev, "probing iVeia PWM\n");
	iveia_pwm = devm_kzalloc(&pdev->dev, sizeof(*iveia_pwm), GFP_KERNEL);
	if (!iveia_pwm) {
          return -ENOMEM;
        }

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iveia_pwm->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(iveia_pwm->base)) {
          return PTR_ERR(iveia_pwm->base);
        }

	iveia_pwm->chip.dev = &pdev->dev;
	iveia_pwm->chip.ops = &iveia_pwm_ops;

	if (pdev->dev.of_node) {
		iveia_pwm->chip.of_xlate = of_pwm_xlate_with_flags;
		iveia_pwm->chip.of_pwm_n_cells = 3;
	}
        
	iveia_pwm->chip.base = -1;
	iveia_pwm->chip.npwm = 1;

	ret = pwmchip_add(&iveia_pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip %d\n", ret);
                return ret;
	}

	platform_set_drvdata(pdev, iveia_pwm);

        writel(0x0, iveia_pwm->base + CONTROL_REG); // disable        

	return ret;

}

static int iveia_pwm_remove(struct platform_device *pdev)
{
	struct iveia_pwm_chip *iveia_pwm = platform_get_drvdata(pdev);

	return pwmchip_remove(&iveia_pwm->chip);
}

static struct platform_driver iveia_pwm_driver = {
	.driver = {
		.name = "iveia-pl-pwm",
		.of_match_table = of_match_ptr(iveia_pwm_dt_ids),
	},
	.probe = iveia_pwm_probe,
	.remove = iveia_pwm_remove,
};
module_platform_driver(iveia_pwm_driver);

MODULE_DESCRIPTION("iVeia PL PWM driver");
MODULE_AUTHOR("Maxwell Walter (mwalter@iveia.com)");
MODULE_LICENSE("GPL v2");
