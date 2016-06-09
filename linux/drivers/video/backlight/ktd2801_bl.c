/*************** maintained by customer ***************************************/
#define CONFIG_BACKLIGHT_AMAZING 1
/*
 * linux/drivers/video/backlight/s2c_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* @file	drivers/video/backlight/s2c_bl.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/ktd2801_bl.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/firmware.h>

static DEFINE_SPINLOCK(bl_ctrl_lock);
#define BACKLIGHT_DEBUG 1
#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

#define EXPRESSWIRE_DIMMING

#ifdef EXPRESSWIRE_DIMMING
#define EW_DELAY 200
#define EW_DETECT 300
#define EW_WINDOW 900
#define DATA_START 10
#define LOW_BIT_L 20
#define LOW_BIT_H 5
#define HIGH_BIT_L 5
#define HIGH_BIT_H 20
#define END_DATA_L 10
#define END_DATA_H 400
#define SLEEP_OUT_DELAY 50
#endif

#define BACKLIGHT_DEV_NAME	"sprd_backlight"

#ifdef CONFIG_MACH_NEVISTD
#define GPIO_BL_CTRL	138
#else
#define GPIO_BL_CTRL	234
#endif

#ifdef CONFIG_MACH_NEVISTD
#define MAX_BRIGHTNESS	255
#define MIN_BRIGHTNESS	20
#define DEFAULT_BRIGHTNESS 130
#define DEFAULT_PULSE 20
#define DIMMING_VALUE	31
#define MAX_BRIGHTNESS_IN_BLU	32 // backlight-IC MAX VALUE
#else
#ifdef EXPRESSWIRE_DIMMING
#define HBM_BRIGHTNESS 102
#define MM_BRIGHTNESS 101
#define MAX_BRIGHTNESS 100
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 60
#define DEFAULT_PULSE 85
#define DIMMING_VALUE 5
#define MAX_BRIGHTNESS_IN_BLU 243 // backlight-IC MAX VALUE
#define MAX_BRIGHTNESS_INDEX MIN_BRIGHTNESS
#define MIN_BRIGHTNESS_INDEX MAX_BRIGHTNESS
#else
#define MAX_BRIGHTNESS	255
#define MIN_BRIGHTNESS	20
#define DEFAULT_BRIGHTNESS 122
#define DEFAULT_PULSE 20
#define DIMMING_VALUE	31
#define MAX_BRIGHTNESS_IN_BLU	32 // backlight-IC MAX VALUE
#endif
#endif

#ifndef EXPRESSWIRE_DIMMING
static int lcd_brightness = DEFAULT_PULSE;
#endif

int BL_brightness;
int is_poweron = 1;
int current_brightness;
int wakeup_brightness = DEFAULT_BRIGHTNESS;
static int is_hbm = 0;
static int is_mbm = 0;

struct notifier_block fb_notif;

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend_BL;
#endif

#ifdef EXPRESSWIRE_DIMMING
struct brt_value brt_table_ktd[] = {
	/* High brightness mode support for mdnie outdoor mode */
	{102, 243},
	/* Multimedia brightness mode */
	{101, 243},
	/* Maximum */
	{100, 201},
	{99, 198},
	{98, 195},
	{97, 192},
	{96, 189},
	{95, 186},
	{94, 183},
	{93, 180},
	{92, 177},
	{91, 174},
	{90, 171},
	{89, 168},
	{88, 165},
	{87, 162},
	{86, 159},
	{85, 156},
	{84, 153},
	{83, 150},
	{82, 147},
	{81, 144},
	{80, 141},
	{79, 138},
	{78, 135},
	{77, 132},
	{76, 129},
	{75, 126},
	{74, 123},
	{73, 120},
	{72, 117},
	{71, 114},
	{70, 111},
	{69, 108},
	{68, 105},
	{67, 102},
	{66, 99},
	{65, 96},
	{64, 93},
	{63, 91},
	{62, 89},
	{61, 87},
	/* Default */
	{60, 85},
	{59, 83},
	{58, 81},
	{57, 79},
	{56, 77},
	{55, 75},
	{54, 73},
	{53, 71},
	{52, 69},
	{51, 67},
	{50, 65},
	{49, 63},
	{48, 61},
	{47, 59},
	{46, 57},
	{45, 55},
	{44, 53},
	{43, 51},
	{42, 49},
	{41, 47},
	{40, 45},
	{39, 44},
	{38, 43},
	{37, 42},
	{36, 41},
	{35, 40},
	{34, 39},
	{33, 38},
	{32, 37},
	{31, 36},
	{30, 35},
	{29, 34},
	{28, 33},
	{27, 32},
	{26, 31},
	{25, 30},
	{24, 29},
	{23, 28},
	{22, 27},
	{21, 26},
	{20, 25},
	{19, 24},
	{18, 23},
	{17, 22},
	{16, 21},
	{15, 20},
	{14, 19},
	{13, 18},
	{12, 17},
	{11, 16},
	{10, 15},
	{9, 14},
	{8, 13},
	{7, 12},
	{6, 11},
	{5, 10},
	{4, 9},
	{3, 8},
	{2, 7},
	{1, 6},
	/* Minimum */
	{0, 5},
};
#else
struct brt_value brt_table_ktd[] = {
	{ 255,	0 }, /* Max */
	{ 250,	2 },
	{ 245,	3 },
	{ 240,	4 },
	{ 235,	5 },
	{ 230,	6 },
	{ 220,	7 },
	{ 210,	8 },
	{ 200,	9 },
	{ 190,	10 },
	{ 180,	11 },
	{ 170,	12 },
	{ 160,	13 },
	{ 150,	14 },
	{ 140,	15 },
	{ 130,	16 },
	{ 120,	17 }, /* default */
	{ 115,	18 },
	{ 110,	19 },
	{ 105,	20 },
	{ 100,	21 },
	{ 95,	22 },
	{ 90,	23 },
	{ 85,	24 },
	{ 80,	25 },
	{ 70,	26 }, /* Dimming */
	{ 60,	27 },
	{ 50,	28 },
	{ 40,	29 },
	{ 30,	30 },
	{ 20,	31 },
	{ 0,	31 }, /* Off */
};
#endif

#define NB_BRT_LEVEL (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))

#if defined(CONFIG_FB_SC8825) && defined(CONFIG_FB_LCD_NT35510_MIPI)
extern bool is_first_frame_done;
#endif

#ifdef CONFIG_MACH_NEVISTD
extern int spa_lpm_charging_mode_get(void);
#endif
#ifdef CONFIG_EARLYSUSPEND
static void ktd253_backlight_early_suspend(struct early_suspend *h)
{
	is_poweron = 0;
	ktd_backlight_set_brightness(0);
#ifdef EXPRESSWIRE_DIMMING
	gpio_set_value(GPIO_BL_CTRL, 0);
#endif
	return;
}

static void ktd253_backlight_late_resume(struct early_suspend *h)
{
	int i = 0;
	unsigned long flags;

#if defined(CONFIG_FB_SC8825) && defined(CONFIG_FB_LCD_NT35510_MIPI)
	while (i++ < 10 && !is_first_frame_done) {
		printk(KERN_INFO "[Backlight] wait first vsync_done, sleep 200msec\n", __func__);
		msleep(200);
	}
#endif
#ifdef EXPRESSWIRE_DIMMING
	spin_lock_irqsave(&bl_ctrl_lock, flags);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(200);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(300);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(400);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
#endif
	ktd_backlight_set_brightness(wakeup_brightness);
	is_poweron = 1;
}
#else
static int ktd2801_backlight_suspend(void)
{
	BLDBG("[BACKLIGHT] ktd2801_backlight_suspend\n");

	is_poweron = 0;

#ifdef EXPRESSWIRE_DIMMING
	gpio_set_value(GPIO_BL_CTRL, 0);
#endif

	return 0;
}

static int ktd2801_backlight_resume(void)
{
	unsigned long flags;

	BLDBG("[BACKLIGHT] ktd2801_backlight_resume\n");

	msleep(SLEEP_OUT_DELAY);

#ifdef EXPRESSWIRE_DIMMING
	/*
	 * EW_WIN(900us) = EW_DELAY(200) + EW_DETECT(300) + END_DATA_H(400)
	 */
	spin_lock_irqsave(&bl_ctrl_lock, flags);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(EW_DELAY);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(EW_DETECT);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(END_DATA_H);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
#endif
	ktd_backlight_set_brightness(wakeup_brightness);

	is_poweron = 1;

	return 0;
}

void ktd_backlight_shutdown()
{
	pr_debug("%s: starts\n", __func__);

	gpio_direction_output(GPIO_BL_CTRL, 0);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int power;

	pr_info("[%s]: Event[%lu]\n", __func__, event);

	/* If we aren't interested in this event, skip it immediately ... */
	switch (event) {
	case FB_EVENT_BLANK:
		power = *(int *)evdata->data;
		switch (power) {
		case FB_BLANK_UNBLANK:
			ktd2801_backlight_resume();
			break;
		default:
			return 0;
		}
		break;
	case FB_EARLY_EVENT_BLANK:
		power = *(int *)evdata->data;
		switch (power) {
		case FB_BLANK_POWERDOWN:
			ktd2801_backlight_suspend();
			break;
		default:
			return 0;
		}
		break;
	case FB_EVENT_SUSPEND:
		ktd_backlight_shutdown();
		break;
	default:
		return 0;
	}

	return 0;
}

static int ktd_register_fb(void)
{
	memset(&fb_notif, 0, sizeof(fb_notif));
	fb_notif.notifier_call = fb_notifier_callback;

	return fb_register_client(&fb_notif);
}

static void ktd_unregister_fb(void)
{
	fb_unregister_client(&fb_notif);
}
#endif

#ifdef EXPRESSWIRE_DIMMING
void ktd_backlight_set_brightness(int level)
{
	int i = 0;
	unsigned char brightness = 5;
	int bit_map[8] = { 0, };
	unsigned long flags = 0;

	if (level < 0) {
		pr_info("%s: level[%d], Suspend!\n", __func__, level);

		gpio_set_value(GPIO_BL_CTRL, 0);

		return;
	}

	if (level < MIN_BRIGHTNESS) {
		brightness = DIMMING_VALUE;
	} else {
		for (i = 0; i < NB_BRT_LEVEL ; ++i) {
			if (level <= brt_table_ktd[i].level
					&& level > brt_table_ktd[i + 1].level) {
				brightness = brt_table_ktd[i].tune_level;
				break;
			}
		}
	}

	pr_info("%s: level[%d], brightness[%d]\n", __func__, level, brightness);

	for (i = 0; i < 8; i++) {
		bit_map[i] = brightness & 0x01;
		brightness >>= 1;
	}
	spin_lock_irqsave(&bl_ctrl_lock, flags);

	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(DATA_START);
	for (i = 7; i >= 0; i--) {
		if (bit_map[i]) {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(HIGH_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(HIGH_BIT_H);
		} else {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(LOW_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(LOW_BIT_H);
		}
	}
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(END_DATA_L);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(END_DATA_H);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
	return;
}
#else
void ktd_backlight_set_brightness(int level)
{
	int tune_level = 0;


	spin_lock(&bl_ctrl_lock);
	if (level > 0) {
		if (level < MIN_BRIGHTNESS) {
			tune_level = DIMMING_VALUE; /* DIMMING */
		} else {
			int i;
			for (i = 0; i < NB_BRT_LEVEL; i++) {
				if (level <= brt_table_ktd[i].level
					&& level > brt_table_ktd[i+1].level) {
					tune_level = brt_table_ktd[i].tune_level;
					break;
				}
			}
		}
	} /*  BACKLIGHT is KTD model */
	printk("set_brightness : level(%d) tune (%d)\n",level, tune_level);
	current_brightness = level;

	if (!level) {
		gpio_set_value(GPIO_BL_CTRL, 0);
		mdelay(3);
		lcd_brightness = tune_level;
	} else {
		int pulse;

		if (unlikely(lcd_brightness < 0)) {
			int val = gpio_get_value(GPIO_BL_CTRL);
			if (val) {
				lcd_brightness = 0;
			gpio_set_value(GPIO_BL_CTRL, 0);
			mdelay(3);
				printk(KERN_INFO "LCD Baklight init in boot time on kernel\n");
			}
		}

#ifdef CONFIG_MACH_NEVISTD
		if(spa_lpm_charging_mode_get())
		msleep(500);
#endif
		if (!lcd_brightness) {
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(3);
			lcd_brightness = MAX_BRIGHTNESS_IN_BLU;
		}

		pulse = (tune_level - lcd_brightness + MAX_BRIGHTNESS_IN_BLU)
						% MAX_BRIGHTNESS_IN_BLU;

		for (; pulse > 0; pulse--) {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(3);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(3);
		}

		lcd_brightness = tune_level;
	}
	mdelay(1);
	spin_unlock(&bl_ctrl_lock);
	return;
}
#endif

static int ktd_backlight_update_status(struct backlight_device *bl)
{
	if (is_hbm)
		wakeup_brightness = HBM_BRIGHTNESS;
	else if (is_mbm)
		wakeup_brightness = MM_BRIGHTNESS;
	else
		wakeup_brightness = bl->props.brightness;

	if (is_poweron)
		ktd_backlight_set_brightness(wakeup_brightness);

	return 0;
}

#ifdef EXPRESSWIRE_DIMMING
/*
 * Updates the brightness mapping table(255 values of BLIC mapped to
 * 100 values of Tizen platform).
 * @param buf: File path which contains brightness mapping table.
 * 			   Expected file content format:
 * 			   Tizen_brightness_maximum_value	Mapping_BLIC_maximum_value
 * 			   .
 * 			   .
 * 			   .
 * 			   Tizen_brightness_minimum_value	Mapping_BLIC_minimum_value
 */
static ssize_t ktd_backlight_update_brightness_table(struct device *dev,
		struct device_attribute *attr, char *buf, size_t count)
{
	const struct firmware *fw = NULL;
	const char *file_path = NULL;
	char *line = NULL;
	int iter = 0;
	int ret = 0;

	pr_info("[%s]: Tuning data file[%s], Length[%u]\n", __func__, buf,
			strlen(buf));

	file_path = kstrndup(buf, (strlen(buf) - 1), GFP_KERNEL);
	if (file_path == NULL) {
		pr_err("%s: Not enough memory\n", __func__);

		ret = -ENOMEM;
		goto exit;
	}

	ret = request_firmware(&fw, file_path, dev);
	if (ret) {
		pr_err("%s: Failed to request firmware\n", __func__);

		ret = -EIO;
		goto exit;
	}

	pr_info("%s: File size[%u]\n", __func__, strlen(fw->data));

	iter = 0;

	while ((line = strsep((char **) &(fw->data), "\n"))
			&& (iter < NB_BRT_LEVEL )) {
		ret = sscanf(line, "%d\t%d\n", &(brt_table_ktd[iter].level),
				&(brt_table_ktd[iter].tune_level));
		if (ret != 2) {
			pr_err("%s: Line doesn't have 2 integers\n", __func__);

			ret = -EINVAL;
			goto exit;
		}

		++iter;
	}

	pr_info("%s: BLIC max[%d], default[%d], min[%d]\n", __func__,
			brt_table_ktd[MAX_BRIGHTNESS_INDEX].tune_level,
			brt_table_ktd[DEFAULT_BRIGHTNESS].tune_level,
			brt_table_ktd[MIN_BRIGHTNESS_INDEX].tune_level);

exit:
	release_firmware(fw);

	if (file_path)
		kfree(file_path);

	if (ret < 0)
		pr_err("%s: Error code[%d]\n", __func__, ret);

	ret = count;

	return ret;
}

/*
 * Shows the platform to BLIC brightness values mapping for maximum,
 * default & minimum brightness values
 * @param buf: The values of maximum, minimum & default brightness are put
 * 			   up as output in this buffer
 * 			   Output format:
 *			   BLIC max[%d], default[%d], min[%d]
 */
static ssize_t ktd_backlight_show_brightness_table(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "BLIC max[%d], default[%d], min[%d]\n",
			brt_table_ktd[MAX_BRIGHTNESS_INDEX].tune_level,
			brt_table_ktd[DEFAULT_BRIGHTNESS].tune_level,
			brt_table_ktd[MIN_BRIGHTNESS_INDEX].tune_level);
}

static ssize_t ktd_backlight_show_hbm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (is_hbm == 1) ? "on" : "off");
}

static ssize_t ktd_backlight_store_hbm(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct backlight_device *bd = to_backlight_device(dev);
	int ret = count;

	pr_info("[%s]: HBM[%s]\n", __func__, buf);

	if (strncmp(buf, "on", 2) == 0) {
		is_hbm = 1;
	} else {
		is_hbm = 0;
	}

	ktd_backlight_update_status(bd);

	ret = count;

	return ret;
}

static ssize_t ktd_backlight_show_mbm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (is_mbm == 1) ? "on" : "off");
}

static ssize_t ktd_backlight_store_mbm(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct backlight_device *bd = to_backlight_device(dev);
	int ret = count;

	pr_info("[%s]: MBM[%s]\n", __func__, buf);

	if (strncmp(buf, "on", 2) == 0) {
		is_mbm = 1;
	} else {
		is_mbm = 0;
	}

	ktd_backlight_update_status(bd);

	ret = count;

	return ret;
}

static struct device_attribute bl_device_attributes[] = {
	__ATTR(brightness_table, 0644, ktd_backlight_show_brightness_table,
			ktd_backlight_update_brightness_table),
	__ATTR(hbm, 0644, ktd_backlight_show_hbm, ktd_backlight_store_hbm),
	__ATTR(mbm, 0644, ktd_backlight_show_mbm, ktd_backlight_store_mbm),
};
#endif

static int ktd_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] ktd_backlight_get_brightness\n");

	BL_brightness = bl->props.brightness;

	return BL_brightness;
}

static const struct backlight_ops ktd_backlight_ops = {
	.update_status	= ktd_backlight_update_status,
	.get_brightness	= ktd_backlight_get_brightness,
};

static int ktd_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bl = NULL;
	struct backlight_properties props = {0,};
	unsigned long flags = 0;
	int i = 0;
	int result = 0;

	printk("[BACKLIGHT] ktd253b_backlight_probe\n");

	props.max_brightness = MAX_BRIGHTNESS;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register(BACKLIGHT_DEV_NAME, &pdev->dev, NULL,
					&ktd_backlight_ops, &props);
	if (IS_ERR(bl))
	{
		dev_err(&pdev->dev, "failed to register backlight\n");
		result = PTR_ERR(bl);

		goto error_exit;
	}

	bl->props.max_brightness = MAX_BRIGHTNESS;
	bl->props.brightness = DEFAULT_BRIGHTNESS;

	platform_set_drvdata(pdev, bl);

#ifdef EXPRESSWIRE_DIMMING
	for (i = 0; i < ARRAY_SIZE(bl_device_attributes); i++) {
		result = device_create_file(&bl->dev,
				&bl_device_attributes[i]);
		if (result < 0) {
			pr_err("%s: Failed to add backlight device sysfs entries\n",
					__func__);

			goto error_exit;
		}
	}
#endif

#ifndef CONFIG_EARLYSUSPEND
	ktd_register_fb();
#endif

	if(gpio_request(GPIO_BL_CTRL,"BL_CTRL"))
		printk(KERN_ERR "Request GPIO failed,""gpio: %d \n", GPIO_BL_CTRL);
#ifdef EXPRESSWIRE_DIMMING
	spin_lock_irqsave(&bl_ctrl_lock, flags);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(1500);
	udelay(2000);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(EW_DELAY);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(EW_DETECT);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(EW_DETECT);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend_BL.suspend = ktd253_backlight_early_suspend;
	early_suspend_BL.resume  = ktd253_backlight_late_resume;
	early_suspend_BL.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&early_suspend_BL);
#endif
	ktd_backlight_update_status(bl);

	return result;

error_exit:
	if (bl)
		backlight_device_unregister(bl);

	return result;
}

static int ktd_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	ktd_unregister_fb();
	backlight_device_unregister(bl);
	gpio_direction_output(GPIO_BL_CTRL, 0);

	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = "sprd,sprd_backlight", },
	{ }
};

static struct platform_driver ktd_backlight_driver = {
	.driver		= {
		.name	= BACKLIGHT_DEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = backlight_of_match,
	},
	.probe		= ktd_backlight_probe,
	.remove		= ktd_backlight_remove,
};

static int __init ktd_backlight_init(void)
{
	return platform_driver_register(&ktd_backlight_driver);
}
module_init(ktd_backlight_init);

static void __exit ktd_backlight_exit(void)
{
	platform_driver_unregister(&ktd_backlight_driver);
}
module_exit(ktd_backlight_exit);

MODULE_DESCRIPTION("KTD based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd-backlight");
