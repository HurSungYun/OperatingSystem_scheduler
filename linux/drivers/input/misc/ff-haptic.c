/*
 * ff haptic driver
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Author: Sanghyeon Lee <sirano06.lee@samsung.com>
 *			 Diwas Kumar <diwas.kumar@samsung.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <soc/sprd/hardware.h>

#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif /*CONFIG_HAS_WAKELOCK*/

#ifdef CONFIG_ARCH_SCX35
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#define FF_HAPTIC_NAME "ff-haptic"

#endif

#ifdef CONFIG_ARCH_SCX30G
#include <soc/sprd/chip_x30g/__regs_ana_sc2723_glb.h>
#define ANA_VIBR_CTRL0      (ANA_REG_GLB_VIBR_CTRL0)
#define BIT_VIBR_ON			(BIT(8))
#define BIT_LDO_VIBR_V		(BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0))
#endif

#define MAX_MAGNITUDE		0xffff
#define DEFAULT_MIN_MICROVOLT	1100000
#define DEFAULT_MAX_MICROVOLT	2700000
#define ERROR_NAME "error"

struct ff_haptic {
	struct device *dev;
	struct input_dev *input_dev;
	struct work_struct work;
	bool enabled;
	struct regulator *regulator;
	struct mutex mutex;
	int max_mV;
	int min_mV;
	int intensity; /* mV */
	int level;
};

static void ff_haptic_enable(struct ff_haptic *haptic, bool enable)
{
	mutex_lock(&haptic->mutex);

	if(enable){
		pr_info("ff-haptic : enable\n");
		sci_adi_write(ANA_VIBR_CTRL0, 0, BIT_VIBR_ON);
	}else{
		pr_info("ff-haptic : disable\n");
		sci_adi_write(ANA_VIBR_CTRL0, BIT_VIBR_ON, BIT_VIBR_ON);
	}
	mutex_unlock(&haptic->mutex);
}

static void ff_haptic_work(struct work_struct *work)
{
	struct ff_haptic *haptic = container_of(work,
						       struct ff_haptic,
						       work);
	if (haptic->level)
		ff_haptic_enable(haptic, true);
	else
		ff_haptic_enable(haptic, false);
}

static int ff_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct ff_haptic *haptic = input_get_drvdata(input);
	haptic->level = effect->u.rumble.strong_magnitude;
	if (!haptic->level)
		haptic->level = effect->u.rumble.weak_magnitude;

	haptic->intensity =
		(haptic->max_mV - haptic->min_mV) * haptic->level /
		MAX_MAGNITUDE;
	haptic->intensity = haptic->intensity + haptic->min_mV;

	if (haptic->intensity > haptic->max_mV)
		haptic->intensity = haptic->max_mV;
	if (haptic->intensity < haptic->min_mV)
		haptic->intensity = haptic->min_mV;

	schedule_work(&haptic->work);

	return 0;
}

static void ff_haptic_close(struct input_dev *input)
{
	struct ff_haptic *haptic = input_get_drvdata(input);

	cancel_work_sync(&haptic->work);
	ff_haptic_enable(haptic, false);
}

static int ff_haptic_probe(struct platform_device *pdev)
{
	struct ff_haptic *haptic;
	struct input_dev *input_dev;
	int error;
	printk("ff-haptic probe called");

	haptic = kzalloc(sizeof(*haptic), GFP_KERNEL);
	if (!haptic) {
		printk("ff-haptic : unable to allocate memory for haptic\n");
		return -ENOMEM;
	}
	input_dev = input_allocate_device();
	if (!input_dev) {
		printk("ff-haptic : unalbe to allocate memory\n");
		error =  -ENOMEM;
		goto err_kfree_mem;
	}
	INIT_WORK(&haptic->work, ff_haptic_work);
	mutex_init(&haptic->mutex);
	haptic->input_dev = input_dev;
	haptic->dev = &pdev->dev;

	haptic->input_dev->name = FF_HAPTIC_NAME;
	haptic->input_dev->dev.parent = &pdev->dev;
	haptic->input_dev->close = ff_haptic_close;
	haptic->enabled = false;
	input_set_drvdata(haptic->input_dev, haptic);
	input_set_capability(haptic->input_dev, EV_FF, FF_RUMBLE);
	error = input_ff_create_memless(input_dev, NULL,
				      ff_haptic_play);
	if (error) {
		printk("ff-haptic : input_ff_create_memless() failed : %d\n", error);
		goto err_put_regulator;
	}
	error = input_register_device(haptic->input_dev);
	if (error) {
		printk("ff-haptic : couldn't register input device : %d\n", error);
		goto err_destroy_ff;
	}

	/* regulator voltage : 3.23V */
	sci_adi_write(ANA_VIBR_CTRL0, 0xcb, BIT_LDO_VIBR_V);

	platform_set_drvdata(pdev, haptic);
	printk("ff-haptic probe success");
	return 0;

err_destroy_ff:
	input_ff_destroy(haptic->input_dev);
err_put_regulator:
	regulator_put(haptic->regulator);
err_kfree_mem:
	kfree(haptic);

	return error;
}

static int ff_haptic_remove(struct platform_device *pdev)
{
	struct ff_haptic *haptic = platform_get_drvdata(pdev);

	input_unregister_device(haptic->input_dev);

	return 0;
}

static struct platform_driver ff_haptic_driver = {
	.driver		= {
		.name	= FF_HAPTIC_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= ff_haptic_probe,
	.remove		= ff_haptic_remove,
};

static struct platform_device ff_haptic_device = {
	.name  = "ff-haptic",
	.id = -1,
};


module_platform_driver(ff_haptic_driver);

static int __init haptic_init(void){
	return platform_device_register(&ff_haptic_device);
}

static void __exit haptic_exit(void){
	return platform_device_unregister(&ff_haptic_device);
}


module_init(haptic_init);
module_exit(haptic_exit);

MODULE_ALIAS("platform:ff-haptic");
MODULE_DESCRIPTION("ff haptic driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanghyeon Lee <sirano06.lee@samsung.com> , Diwas Kumar <diwas.kumar@samsung.com>");
