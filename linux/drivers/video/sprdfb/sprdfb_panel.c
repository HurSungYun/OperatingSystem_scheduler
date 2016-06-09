/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <mach/gpio.h>
#ifdef CONFIG_LCD_CLASS_DEVICE
#include <linux/platform_device.h>
#include <linux/lcd.h>
#endif
#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"

#define MTP_LEN 0x21

static LIST_HEAD(panel_list_main);/* for main_lcd*/
static LIST_HEAD(panel_list_sub);/* for sub_lcd */
static DEFINE_MUTEX(panel_mutex);
static unsigned char mtp_offset_from_uboot[((MTP_LEN + 6) * 2) + 1] = {0};
static unsigned char hbm_offset_from_uboot[(15 * 2) + 1] = {0};
static unsigned char chip_id_from_uboot[(5 * 2) + 1] = {0};
static unsigned char color_offset_from_uboot[(4 * 2) + 1] = {0};

uint32_t lcd_id_from_uboot = 0;
unsigned long lcd_base_from_uboot = 0;
uint8_t elvss_offset_from_uboot = 0;
uint8_t mtp_offset_t[MTP_LEN + 6] = {0};
uint8_t hbm_offset_t[15] = {0};
uint8_t chip_id_t[5] = {0};
uint8_t color_offset_t[4] = {0};
uint8_t g_mtp_offset_error = 0;

extern struct panel_if_ctrl sprdfb_mcu_ctrl;
extern struct panel_if_ctrl sprdfb_rgb_ctrl;
#ifndef CONFIG_FB_SCX15
extern struct panel_if_ctrl sprdfb_mipi_ctrl;
#endif
extern void sprdfb_panel_remove(struct sprdfb_device *dev);

#ifdef CONFIG_SPRDFB_MDNIE_LITE_TUNING
extern int sprdfb_mdnie_reg (struct sprdfb_device *dev, mdnie_w w, mdnie_r r, mdnie_c c);
extern void mdnie_state_restore(struct sprdfb_device *fb_dev);
#endif

#ifdef ENABLE_CLK_HS_ON_INIT
extern int32_t sprdfb_dsi_hs_ready(struct sprdfb_device *dev);
#endif

static void parse_string_to_hex(unsigned char *input, uint8_t *output, uint8_t len)
{
	int i = 0, j = 1, l = 0;

	while (input[i] != '\0' && l < len) {
		if (input[j] >= '0' && input[j] <= '9') {
			output[l] = input[j] - '0';
		} else if (input[j] >= 'a' && input[j] <= 'f') {
			output[l] = input[j] - 'a' + 10;
		} else {
			output[l] = 0;
			pr_err("Error while parsing in %s\n", __FUNCTION__);
		}

		if (input[i] >= '0' && input[i] <= '9') {
			output[l] = output[l] + ((input[i] - '0') * 16);
		} else if (input[i] >= 'a' && input[i] <= 'f') {
			output[l] = output[l] + ((input[i] - 'a' + 10) * 16);
		} else {
			pr_err("Error parsing in %s\n", __FUNCTION__);
		}
		l++;
		i = i + 2;
		j = j + 2;
	}
}

static int __init lcd_id_get(char *str)
{
	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D')) {
		sscanf(&str[2], "%x", &lcd_id_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]LCD Panel ID from uboot: 0x%x\n", __FUNCTION__, lcd_id_from_uboot);
	return 1;
}
__setup("lcd_id=", lcd_id_get);
static int __init lcd_base_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%lx", &lcd_base_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]LCD Panel Base from uboot: 0x%lx\n", __FUNCTION__, lcd_base_from_uboot);
	return 1;
}
__setup("lcd_base=", lcd_base_get);

static int __init mtp_offset_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%s", &mtp_offset_from_uboot);
	}

	parse_string_to_hex(mtp_offset_from_uboot, mtp_offset_t, MTP_LEN + 6);

	if (mtp_offset_t[30] != 2 || mtp_offset_t[31] != 3 ||
		mtp_offset_t[32] != 2)
		g_mtp_offset_error = 1;

	printk(KERN_INFO "sprdfb: [%s]Panel mtp offset from uboot: %s\n", __FUNCTION__, mtp_offset_from_uboot);

	return 1;
}
__setup("mtp_offset=", mtp_offset_get);

static int __init elvss_offset_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%x", &elvss_offset_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]Panel elvss offset from uboot: 0x%x\n", __FUNCTION__, elvss_offset_from_uboot);
	return 1;
}
__setup("elvss_offset=", elvss_offset_get);

static int __init hbm_offset_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%s", &hbm_offset_from_uboot);
	}

	parse_string_to_hex(hbm_offset_from_uboot, hbm_offset_t, 15);

	printk(KERN_INFO "sprdfb: [%s]Panel hbm offset from uboot: %s\n", __FUNCTION__, hbm_offset_from_uboot);

	return 1;
}
__setup("hbm_offset=", hbm_offset_get);

static int __init chip_id_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%s", &chip_id_from_uboot);
	}

	parse_string_to_hex(chip_id_from_uboot, chip_id_t, 5);

	printk(KERN_INFO "sprdfb: [%s]Panel chip id from uboot: %s\n", __FUNCTION__, chip_id_from_uboot);

	return 1;
}
__setup("chip_id=", chip_id_get);

static int __init color_offset_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%s", &color_offset_from_uboot);
	}

	parse_string_to_hex(color_offset_from_uboot, color_offset_t, 4);

	printk(KERN_INFO "sprdfb: [%s]Panel color offset from uboot: %s\n", __FUNCTION__, color_offset_from_uboot);

	return 1;
}
__setup("color_offset=", color_offset_get);

static int32_t panel_reset_dispc(struct panel_spec *self)
{
        uint16_t timing1, timing2, timing3;

        if((NULL != self) && (0 != self->reset_timing.time1) &&
            (0 != self->reset_timing.time2) && (0 != self->reset_timing.time3)) {
            timing1 = self->reset_timing.time1;
            timing2 = self->reset_timing.time2;
            timing3 = self->reset_timing.time3;
        }else {
            timing1 = 20;
            timing2 = 20;
            timing3 = 20;
        }
#ifdef CONFIG_FB_LCD_S6E8AA5X01_MIPI
	if (gpio_is_valid(self->rst_gpio)) {
		gpio_direction_output(self->rst_gpio, 1);
		usleep_range(10000, 11000);
		gpio_direction_output(self->rst_gpio, 0);
		usleep_range(5000, 6000);
		gpio_direction_output(self->rst_gpio, 1);
		/* wait 10ms util the lcd is stable */
		usleep_range(5000, 6000);
	}
#else
	dispc_write(1, DISPC_RSTN);
	usleep_range(timing1*1000, timing1*1000+500);
	dispc_write(0, DISPC_RSTN);
	usleep_range(timing2*1000, timing2*1000+500);
	dispc_write(1, DISPC_RSTN);

	/* wait 10ms util the lcd is stable */
	usleep_range(timing3*1000, timing3*1000+500);
#endif
	return 0;
}

static int32_t panel_set_resetpin_dispc(struct panel_spec *self, uint32_t status)
{
#ifdef CONFIG_FB_LCD_S6E8AA5X01_MIPI
	if (gpio_is_valid(self->rst_gpio)) {
		if (status)
			gpio_direction_output(self->rst_gpio, 1);
		else
			gpio_direction_output(self->rst_gpio, 0);
	}
#else
	if(0 == status){
		dispc_write(0, DISPC_RSTN);
	}else{
		dispc_write(1, DISPC_RSTN);
	}
#endif
	return 0;
}

static int panel_power_on(struct sprdfb_device *dev, bool enable)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], enter\n",__FUNCTION__);

	/* panel ldo en*/
	if (dev->panel->ops->panel_power_on)
		dev->panel->ops->panel_power_on(dev, enable);

	return 0;
}


static int panel_reset(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], enter\n",__FUNCTION__);

	//clk/data lane enter LP
	if(NULL != dev->panel->if_ctrl->panel_if_before_panel_reset){
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	}
	usleep_range(5000, 5500);

	//reset panel
	dev->panel->ops->panel_reset(dev->panel);

	return 0;
}

static int panel_sleep(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], enter\n",__FUNCTION__);

	//send sleep cmd to lcd
	if (dev->panel->ops->panel_enter_sleep != NULL) {
		dev->panel->ops->panel_enter_sleep(dev->panel,1);
	}
	msleep(100);
	//clk/data lane enter LP
	if((NULL != dev->panel->if_ctrl->panel_if_before_panel_reset)
		&&(SPRDFB_PANEL_TYPE_MIPI == dev->panel->type))
	{
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	}
	return 0;
}

static void panel_set_resetpin(uint16_t dev_id,  uint32_t status, struct panel_spec *panel )
{
	pr_debug("sprdfb: [%s].\n",__FUNCTION__);

	/*panel set reset pin status*/
	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_set_resetpin_dispc(panel, status);
	}
}


static int32_t panel_before_resume(struct sprdfb_device *dev)
{
	/*restore  the reset pin to high*/
	panel_set_resetpin(dev->dev_id, 1, dev->panel);
	return 0;
}

static int32_t panel_after_suspend(struct sprdfb_device *dev)
{
	/*set the reset pin to low*/
	panel_set_resetpin(dev->dev_id, 0, dev->panel);
	usleep_range(1000, 1100);

	return 0;
}

static bool panel_check(struct panel_cfg *cfg)
{
	bool rval = true;

	if(NULL == cfg || NULL == cfg->panel){
		printk(KERN_ERR "sprdfb: [%s] :Invalid Param!\n", __FUNCTION__);
		return false;
	}

	pr_debug("sprdfb: [%s], dev_id = %d, lcd_id = 0x%x, type = %d\n",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);

	switch(cfg->panel->type){
	case SPRDFB_PANEL_TYPE_MCU:
		cfg->panel->if_ctrl = &sprdfb_mcu_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_RGB:
	case SPRDFB_PANEL_TYPE_LVDS:
		cfg->panel->if_ctrl = &sprdfb_rgb_ctrl;
		break;
#ifndef CONFIG_FB_SCX15
	case SPRDFB_PANEL_TYPE_MIPI:
		cfg->panel->if_ctrl = &sprdfb_mipi_ctrl;
		break;
#endif
	default:
		printk("sprdfb: [%s]: erro panel type.(%d,%d, %d)",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);
		cfg->panel->if_ctrl = NULL;
		break;
	};

	if(cfg->panel->if_ctrl->panel_if_check){
		rval = cfg->panel->if_ctrl->panel_if_check(cfg->panel);
	}
	return rval;
}

static int panel_mount(struct sprdfb_device *dev, struct panel_spec *panel)
{
	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	/* TODO: check whether the mode/res are supported */
	dev->panel = panel;

	if(NULL == dev->panel->ops->panel_reset){
		if(SPRDFB_MAINLCD_ID == dev->dev_id){
			dev->panel->ops->panel_reset = panel_reset_dispc;
		}
	}

	panel->if_ctrl->panel_if_mount(dev);

	return 0;
}


int panel_init(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(!dev->panel->if_ctrl->panel_if_init(dev)){
		printk(KERN_ERR "sprdfb: [%s]: panel_if_init fail!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int panel_ready(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(NULL != dev->panel->if_ctrl->panel_if_ready){
		dev->panel->if_ctrl->panel_if_ready(dev);
	}

	return 0;
}


static struct panel_spec *adapt_panel_from_uboot(uint16_t dev_id)
{
	struct panel_cfg *cfg;
	struct list_head *panel_list;

	pr_debug("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev_id);

	if (lcd_id_from_uboot == 0) {
		printk("sprdfb: [%s]: Not got lcd id from uboot\n", __FUNCTION__);
		return NULL;
	}

	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		if(lcd_id_from_uboot == cfg->lcd_id) {
			printk(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__,cfg->lcd_id);
			return cfg->panel;
		}
	}
	printk(KERN_ERR "sprdfb: [%s]: Failed to match LCD Panel from uboot!\n", __FUNCTION__);

	return NULL;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev)
{
	struct panel_cfg *cfg;
	struct panel_cfg *dummy_cfg = NULL;
	struct list_head *panel_list;
	uint32_t id;

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		if(0xFFFFFFFF == cfg->lcd_id){
			dummy_cfg = cfg;
			continue;
		}
		printk("sprdfb: [%s]: try panel 0x%x\n", __FUNCTION__, cfg->lcd_id);
		panel_mount(dev, cfg->panel);
#ifndef CONFIG_SC_FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == cfg->lcd_id) {
			pr_debug(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__, cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			return cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	if(dummy_cfg != NULL){
		printk("sprdfb: [%s]: Can't find read panel, Use dummy panel!\n", __FUNCTION__);
		panel_mount(dev, dummy_cfg->panel);
#ifndef CONFIG_SC_FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == dummy_cfg->lcd_id) {
			pr_debug(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__, dummy_cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			return dummy_cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	printk(KERN_ERR "sprdfb:  [%s]: failed to attach LCD Panel!\n", __FUNCTION__);
	return NULL;
}


bool sprdfb_panel_get(struct sprdfb_device *dev)
{
	struct panel_spec *panel = NULL;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return false;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	panel = adapt_panel_from_uboot(dev->dev_id);
	if (panel) {
		dev->panel_ready = true;
		panel_mount(dev, panel);
		panel_init(dev);
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static ssize_t show_lcd_info(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	unsigned int panel_id = lcd_id_from_uboot;

	if (fb_dev->panel->ops->panel_get_type)
		return fb_dev->panel->ops->panel_get_type(buf, panel_id);

	return sprintf(buf, "Unknown panel %X", panel_id);
}

static int sprdfb_set_power(struct lcd_device *ld, int power)
{
	int ret = 0;
	struct sprdfb_device *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL)
		return -EINVAL;

	if (lcd->power == power) {
		printk("power is same as previous mode\n");
		return -EINVAL;
	}

	lcd->power = power;

	printk("sprdfb: [%s] power[%d]\n", __FUNCTION__, lcd->power);

	return ret;
}

static int sprdfb_get_power(struct lcd_device *ld)
{
	struct sprdfb_device *lcd = lcd_get_data(ld);

	printk("sprdfb: [%s] power[%d]\n", __FUNCTION__, lcd->power);

	return lcd->power;
}

static struct lcd_ops ld_ops = {
	.get_power = sprdfb_get_power,
	.set_power = sprdfb_set_power,
};

static ssize_t show_lcd_info(struct device *dev,
			struct device_attribute *attr, char *buf);

#ifdef CONFIG_FB_DEBUG_LCD_TUNING
static ssize_t lcd_init_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	printk("++ [%s] current lcd init seq\n", __func__);

	return 0;
}

static ssize_t lcd_init_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int ret = 0;
	printk("++ [%s] current lcd init seq\n", __func__);
	ret = lcd_store_panel_init_cmd();

	return ret;
}
#endif

static struct device_attribute lcd_device_attributes[] = {
	__ATTR(lcd_type, S_IRUGO, show_lcd_info, NULL),
#ifdef CONFIG_FB_DEBUG_LCD_TUNING
	__ATTR(lcd_tune, 0664, lcd_init_show, lcd_init_store),
#endif
};

static bool sprdfb_lcd_class_register(struct platform_device *pdev,
						struct sprdfb_device *dev)
{
	int i, ret;

	dev->ld = lcd_device_register("panel", &pdev->dev, dev, &ld_ops);
	if (IS_ERR(dev->ld)) {
		printk("failed to register ld ops\n");
		return false;
	}

	dev->power = FB_BLANK_UNBLANK;

	for (i = 0; i < ARRAY_SIZE(lcd_device_attributes); i++) {
		ret = device_create_file(&dev->ld->dev,
				&lcd_device_attributes[i]);
		if (ret < 0) {
			printk("failed to add ld dev sysfs entries\n");
			goto err_lcd;
		}
	}

	return true;
err_lcd:
	lcd_device_unregister(dev->ld);
	return false;
}
#endif

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static int panel_update_brightness(struct backlight_device *bd)
{
	struct sprdfb_device *dev = bl_get_data(bd);
	int ret = 0;

	if (dev->power > FB_BLANK_NORMAL) {
		pr_err("%s: invalid power[%d]\n", __func__, dev->power);
		return -EPERM;
	}
	ret = dev->panel->ops->panel_set_brightness(dev, bd->props.brightness);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static struct backlight_ops backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_update_brightness,
};

static bool sprdfb_backlight_class_register(struct platform_device *pdev,
						struct sprdfb_device *dev)
{
	int ret;

	dev->bd = backlight_device_register("panel", &pdev->dev, dev,
			&backlight_ops, NULL);
	if (IS_ERR(dev->bd)) {
		dev_err(&pdev->dev, "failed to register backlight ops.\n");
		return false;
	}

	dev->bd->props.max_brightness = 100;
	dev->bd->props.brightness = 60;
	dev->bd->props.type = BACKLIGHT_PLATFORM;

	return true;
}
#endif


bool sprdfb_panel_init(struct platform_device *pdev, struct sprdfb_device *dev)
{

	if (lcd_id_from_uboot == 0) {
		printk("sprdfb: panel is not connected\n");
		return true;
	}
#if defined(CONFIG_LCD_CLASS_DEVICE)
	if(!sprdfb_lcd_class_register(pdev, dev)) {
		printk("sprdfb: failed to register lcd_class\n");
		return false;
	}
#endif

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	if (dev->panel->oled) {
		if (!sprdfb_backlight_class_register(pdev, dev)) {
			printk("sprdfb: failed to register backlight_class\n");
			return false;
		}
		if (dev->panel->ops->panel_dimming_init)
			dev->panel->ops->panel_dimming_init(dev->panel, &pdev->dev);
	}
#endif
	if (dev->panel->ops->panel_initialize) {
		if (dev->panel->ops->panel_initialize(dev, &pdev->dev)) {
			printk("sprdfb: failed to intialize panel.\n");
			return false;
		}
	}
#ifdef CONFIG_SPRDFB_MDNIE_LITE_TUNING
	sprdfb_mdnie_reg(dev, dev->panel->ops->panel_send_mdnie_cmds, NULL, dev->panel->ops->panel_get_color_coordinates);
#endif

	if (dev->panel->ops->panel_read_offset)
		dev->panel->ops->panel_read_offset(dev->panel);

	return true;
}

bool sprdfb_panel_probe(struct platform_device *pdev, struct sprdfb_device *dev)
{
	struct panel_spec *panel;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return false;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	/* can not be here in normal; we should get correct device id from uboot */
	panel = adapt_panel_from_readid(dev);

	if (panel) {
		if (!sprdfb_panel_init(pdev, dev)) {
			printk("sprdfb: failed to register panel class\n");
			return false;
		}
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}

void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	/*Jessica TODO: */
	if(NULL != self->ops->panel_invalidate_rect){
		self->ops->panel_invalidate_rect(self, left, top, right, bottom);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_invalidate(struct panel_spec *self)
{
	/*Jessica TODO:*/
	if(NULL != self->ops->panel_invalidate){
		self->ops->panel_invalidate(self);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_before_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_before_refresh){
		dev->panel->if_ctrl->panel_if_before_refresh(dev);
	}
}

void sprdfb_panel_after_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_after_refresh){
		dev->panel->if_ctrl->panel_if_after_refresh(dev);
	}
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
void sprdfb_panel_change_fps(struct sprdfb_device *dev, int fps_level)
{
	if (dev->panel->ops->panel_change_fps!= NULL) {
		printk("sprdfb: [%s] fps_level= %d\n", __FUNCTION__,fps_level);
		dev->panel->ops->panel_change_fps(dev->panel,fps_level);
	}
}
#endif

#ifdef CONFIG_FB_ESD_SUPPORT
/*return value:  0--panel OK.1-panel has been reset*/
uint32_t sprdfb_panel_ESD_check(struct sprdfb_device *dev)
{
	int32_t result = 0;
	uint32_t if_status = 0;

//	printk("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);

	dev->check_esd_time++;

	if(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type){
		if (dev->panel->ops->panel_esd_check != NULL) {
			result = dev->panel->ops->panel_esd_check(dev->panel);
			pr_debug("sprdfb: [%s] panel check return %d\n", __FUNCTION__, result);
		}
	}else if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
		dev->esd_te_waiter++;
		dev->esd_te_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		result  = wait_event_interruptible_timeout(dev->esd_te_queue,
			          dev->esd_te_done, msecs_to_jiffies(600));
		pr_debug("sprdfb: after wait (%d)\n", result);
		dispc_clear_bits(BIT(1), DISPC_INT_EN);
		if(!result){ /*time out*/
			printk("sprdfb: [%s] esd check  not got te signal!!!!\n", __FUNCTION__);
			dev->esd_te_waiter = 0;
			result = 0;
		}else{
			pr_debug("sprdfb: [%s] esd check  got te signal!\n", __FUNCTION__);
			result = 1;
		}
#else
		if (dev->panel->ops->panel_esd_check != NULL) {
			result = dev->panel->ops->panel_esd_check(dev->panel);
//			pr_debug("sprdfb: [%s] panel check return %d\n", __FUNCTION__, result);
		}

#endif
	}


	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		return 0;
	}

	if(result == 0){
		dev->panel_reset_time++;

		if(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type){
			if(NULL != dev->panel->if_ctrl->panel_if_get_status){
				if_status = dev->panel->if_ctrl->panel_if_get_status(dev);
			}
		}else if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
			if_status = 2; /*need reset dsi as default for dpi mode*/
		}

		if(0 == if_status){
			printk("sprdfb: [%s] fail! Need reset panel.(%d,%d,%d)\n",	__FUNCTION__,
                            dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
			panel_reset(dev);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
		}else{
			printk("sprdfb: [%s] fail! Need reset panel and panel if!!!!(%d,%d,%d)\n",__FUNCTION__,
                            dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
			dev->reset_dsi_time++;
			if(NULL != dev->panel->if_ctrl->panel_if_suspend){
				dev->panel->if_ctrl->panel_if_suspend(dev);
			}

			mdelay(10);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			panel_init(dev);
			panel_reset(dev);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
		}
		pr_debug("sprdfb: [%s]return 1\n",__FUNCTION__);
		return 1;
	}
//	pr_debug("sprdfb: [%s]return 0\n",__FUNCTION__);
	return 0;
}
#endif

void sprdfb_panel_suspend(struct sprdfb_device *dev)
{
	if(NULL == dev->panel){
		return;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
#if 0
	//step1-1 clk/data lane enter LP
	if(NULL != dev->panel->if_ctrl->panel_if_before_panel_reset){
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	}

	//step1-2 enter sleep  (another way : reset panel)
	/*Jessica TODO: Need do some I2c, SPI, mipi sleep here*/
	/* let lcdc sleep in */

	if (dev->panel->ops->panel_enter_sleep != NULL) {
		dev->panel->ops->panel_enter_sleep(dev->panel,1);
	}
	msleep(100);
#else
	//step1 send lcd sleep cmd or reset panel directly
	if(dev->panel->suspend_mode == SEND_SLEEP_CMD){
		panel_sleep(dev);
	}else{
		panel_reset(dev);
	}
#endif

	//step2 clk/data lane enter ulps
	if(NULL != dev->panel->if_ctrl->panel_if_enter_ulps){
		dev->panel->if_ctrl->panel_if_enter_ulps(dev);
	}

	//step3 turn off mipi
	if(NULL != dev->panel->if_ctrl->panel_if_suspend){
		dev->panel->if_ctrl->panel_if_suspend(dev);
	}

	//step4 reset pin to low
	if (dev->panel->ops->panel_after_suspend != NULL) {
		//himax mipi lcd may define empty function
		dev->panel->ops->panel_after_suspend(dev->panel);
	}
	else{
		panel_after_suspend(dev);
	}

	panel_power_on(dev, false);
}

void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep)
{
	if(NULL == dev->panel){
		return;
	}

	printk(KERN_INFO "sprdfb: [%s], dev->enable= %d, from_deep_sleep = %d\n",__FUNCTION__, dev->enable, from_deep_sleep);
#if 0
	/*Jessica TODO: resume i2c, spi, mipi*/
	if(NULL != dev->panel->if_ctrl->panel_if_resume){
		dev->panel->if_ctrl->panel_if_resume(dev);
	}
	panel_ready(dev);
#endif
	//step1 reset pin to high
	if (dev->panel->ops->panel_before_resume != NULL) {
		//himax mipi lcd may define empty function
		dev->panel->ops->panel_before_resume(dev->panel);
	}
	else{
		panel_before_resume(dev);
	}

	if(from_deep_sleep){

		panel_power_on(dev, true);
		//step2 turn on mipi
		panel_init(dev);

		//step3 reset panel
		panel_reset(dev);
#ifdef ENABLE_CLK_HS_ON_INIT
		//enable clk lane entern HS
		sprdfb_dsi_hs_ready(dev);
#endif
		//step4 panel init
		dev->panel->ops->panel_init(dev->panel);

		//step5 clk/data lane enter HS
		panel_ready(dev);
	}else{
		//step2 turn on mipi
		/*Jessica TODO: resume i2c, spi, mipi*/
		if(NULL != dev->panel->if_ctrl->panel_if_resume){
			dev->panel->if_ctrl->panel_if_resume(dev);
		}

		//step3 sleep out
		if(NULL != dev->panel->ops->panel_enter_sleep){
			dev->panel->ops->panel_enter_sleep(dev->panel,0);
		}

		//step4 clk/data lane enter HS
		panel_ready(dev);
	}
#ifdef CONFIG_SPRDFB_MDNIE_LITE_TUNING
	mdnie_state_restore(dev);
#endif
}

void sprdfb_panel_remove(struct sprdfb_device *dev)
{
	if(NULL == dev->panel){
		return;
	}

	/*Jessica TODO:close panel, i2c, spi, mipi*/
	if(NULL != dev->panel->if_ctrl->panel_if_uninit){
		dev->panel->if_ctrl->panel_if_uninit(dev);
	}
	dev->panel = NULL;
}


int sprdfb_panel_register(struct panel_cfg *cfg)
{
	pr_debug("sprdfb: [%s], panel id = %d\n",__FUNCTION__, cfg->dev_id);

	if(!panel_check(cfg)){
		printk("sprdfb: [%s]: panel check fail!id = %d\n",__FUNCTION__,  cfg->dev_id);
		return -1;
	}

	mutex_lock(&panel_mutex);

	if (cfg->dev_id == SPRDFB_MAINLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_main);
	} else if (cfg->dev_id == SPRDFB_SUBLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_sub);
	} else {
		list_add_tail(&cfg->list, &panel_list_main);
		list_add_tail(&cfg->list, &panel_list_sub);
	}

	mutex_unlock(&panel_mutex);

	return 0;
}

 
