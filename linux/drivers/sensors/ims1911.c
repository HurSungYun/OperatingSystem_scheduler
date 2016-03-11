/*
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>

#include "sensors_core.h"
#include <linux/sensor/gp2a002.h>

#define REGS_PROX		0x0 /* Read  Only */
#define REGS_GAIN		0x1 /* Write Only */
#define REGS_HYS		0x2 /* Write Only */
#define REGS_CYCLE		0x3 /* Write Only */
#define REGS_OPMOD		0x4 /* Write Only */
#define REGS_CON		0x6 /* Write Only */

#define PROX_NONDETECT		0x2F
#define PROX_DETECT		0x0F

#define PROX_NONDETECT_MODE1	0x43
#define PROX_DETECT_MODE1	0x28
#define PROX_NONDETECT_MODE2	0x48
#define PROX_DETECT_MODE2	0x42
#define OFFSET_FILE_PATH	"/csa/sensor/prox_cal"

#define	MAX_IMS1911_REGISTER_COUNT	0x20

#define PROX_AVG_COUNT	40

#define PS_DEFAULT_HIGH_THRESHOLD	75
#define PS_DEFAULT_LOW_THRESHOLD	45

typedef enum {
	IMS1911_OPERATION_MODE_SEL = 0x00,
	IMS1911_INTERRUPT_FLAG = 0x01,
	IMS1911_PS_SETTING = 0x0A,
	IMS1911_LED_DRIVE_CURRENT = 0x0B,
	IMS1911_LED_DRIVE_PULSE = 0x0C,

	IMS1911_PS_INT_LOW_LSB = 0x0D,
	IMS1911_PS_INT_LOW_MSB = 0x0E,
	IMS1911_PS_INT_HIGH_LSB = 0x0F,
	IMS1911_PS_INT_HIGH_MSB = 0x10,	

	IMS1911_PS_DATA_LSB = 0x11,
	IMS1911_PS_DATA_MSB = 0x12,
} _IMS1911_REGISTER;

#define	PS_INT_ENABLE_BIT	2

u8 ims1911_default[MAX_IMS1911_REGISTER_COUNT] = {
	0x0d, 	// 0x00 operation mode select register
			// PS & INT Disable, Persistence Count 4
	0,		// 0x01 Interrupt Flag Register
	0,		// 0x02 Reserved
	0,		// 0x03 Reserved
	0,	    // 0x04 Reserved
	0,      // 0x05 Reserved
	0,      // 0x06 Reserved
	0,      // 0x07 Reserved        
	0,      // 0x08 Reserved
	0,      // 0x09 Reserved
	0x42,   // 0x0a, gain x2, ad conversion 0.4ms, ps wait time 50ms
	0x90,   // 0x0b, ir led current : 12.5mA
	127,    // 0x0c, led drive pulse : 128
	0x46,      // 0x0d, ps interrupt low lsb
	0,      // 0x0e, ps interrupt low msb
	0x73,   // 0x0f, ps interrupt high lsb
	0,      // 0x10, ps interrupt high msb
};

#define CHIP_NAME		"IMS1911"
#define CHIP_VENDOR		"ITM"

struct ims1911_data {
	struct i2c_client	*client;	
	struct input_dev *input;
	struct device *dev;

	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	struct mutex mutex;

	int prox_avg_enable;
	struct hrtimer prox_avg_timer;
	ktime_t prox_polling_time;
	struct work_struct work_prox_avg;
	struct workqueue_struct *wq_avg;
	int avg[3];

	int int_gpio_num;
	int irq;

	u16 ps_count;
	u16 high_threshold;
	u16 low_threshold;
	u16 caldata;
	int enable;
};

#define ims_info(fmt, args...)	\
		printk(KERN_INFO "ims1911 : [%-18s:%5d]" fmt, \
		__func__, __LINE__, ## args);

#define ims_err(fmt, args...)	\
		printk(KERN_ERR "ims1911 : [%-18s:%5d]" fmt, \
		__func__, __LINE__, ## args);

enum {
	OFF = 0,
	ON,
};

typedef enum {
	I2C_FAIL = 0,
	I2C_SUCCESS
}_I2C_RESULT;

#define ims1911_bit_test(val, n)	((val) & (1<<(n)))

static u8 i2c_read_byte(struct ims1911_data *data, u8 reg, u8 *val)
{
	s32 ret;
	ret = i2c_smbus_read_byte_data(data->client, reg);
	if(ret < 0) {
		ims_err("i2c read failed(%d)\n", ret);
		return I2C_FAIL;
	}
	*val = ret;
	return I2C_SUCCESS;
}

static u8 i2c_write_byte(struct ims1911_data *data, u8 reg, u8 val)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(data->client, reg, val);
	if(ret < 0) {
		ims_err("i2c write failed(%d)\n", ret);
		return I2C_FAIL;
	}
	return I2C_SUCCESS;
}

static int ims1911_regulator_onoff(struct ims1911_data *data, bool onoff)
{
	struct device *dev = &data->client->dev;
	struct regulator *led_vdd;
	int ret;

	ims_info("%s\n", (onoff) ? "on" : "off");

	led_vdd = devm_regulator_get(dev, "ims1911-led");
	if (IS_ERR(led_vdd)) {
		ims_err("cannot get led_vdd\n");
		return -ENOMEM;
	}

	regulator_set_voltage(led_vdd, 3300000, 3300000);

	if (onoff) {
		ret = regulator_enable(led_vdd);
		if (ret) {
			ims_err("enable vdd failed (%d)\n", ret);
			return ret;
		}
	} else {
		ret = regulator_disable(led_vdd);
		if (ret) {
			ims_err("disable vdd failed (%d)\n", ret);
			return ret;
		}
	}

	devm_regulator_put(led_vdd);
	msleep(20);

	return 0;
}

static u8 read_ps_result(struct ims1911_data *data, u16 *val)
{
	u8 read_val = 0;

	if(i2c_read_byte(data, IMS1911_PS_DATA_LSB, &read_val) == I2C_FAIL) {
		ims_err("cannot read IMS1911_PS_DATA_LSB reg\n");
		return I2C_FAIL;
	}
	
	*val = read_val & 0xff;		
	if(i2c_read_byte(data, IMS1911_PS_DATA_MSB, &read_val) == I2C_FAIL) {
		ims_err("cannot read IMS1911_PS_DATA_MSB reg\n");
		return I2C_FAIL;
	}

	*val = *val | ((read_val & 0xff) << 8);	

	return I2C_SUCCESS;	
}

static int ims1911_set_threshold(struct ims1911_data *data,
	u16 low, u16 high)
{
	u8 val;

	ims_info("hi threshold=%d, low threshold=%d\n", high, low);

	val = (low & 0x00ff);
	i2c_write_byte(data, IMS1911_PS_INT_LOW_LSB, val);

	val = ((low & 0xff00) >> 8);
	i2c_write_byte(data, IMS1911_PS_INT_LOW_MSB, val);

	val = (high & 0x00ff);
	i2c_write_byte(data, IMS1911_PS_INT_HIGH_LSB, val);

	val = ((high & 0xff00) >> 8);
	i2c_write_byte(data, IMS1911_PS_INT_HIGH_MSB, val);

	return 0;
}

#if 0
static ssize_t adc_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}

static ssize_t state_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}
#endif

static ssize_t proximity_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t proximity_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_VENDOR);
}

static ssize_t proximity_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);
	u16 adc = 0;

	if (read_ps_result(data, &adc) != I2C_SUCCESS)
		ims_err("read_ps_result is failed\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ims1911_data *data = dev_get_drvdata(dev);
	int64_t value;
	int ret;

	ret = kstrtoll(buf, 10, &value);
	if (ret < 0)
		return ret;

	data->high_threshold = value;

	ims1911_set_threshold(data, data->low_threshold,
		data->high_threshold);

	return size;
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);


	return snprintf(buf, PAGE_SIZE, "%d\n", data->high_threshold);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ims1911_data *data = dev_get_drvdata(dev);
	int64_t value;
	int ret;

	ret = kstrtoll(buf, 10, &value);
	if (ret < 0)
		return ret;

	data->low_threshold = value;

	ims1911_set_threshold(data, data->low_threshold,
		data->high_threshold);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);


	return snprintf(buf, PAGE_SIZE, "%d\n", data->low_threshold);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d,%d,%d\n", data->avg[0],
		data->avg[1], data->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ims1911_data *data = dev_get_drvdata(dev);
	bool new_value = false;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		ims_err("invalid value %d\n", *buf);
		return -EINVAL;
	}

	ims_info("average enable = %d\n", new_value);
	mutex_lock(&data->mutex);

	if (new_value == 1) {
		if (!data->enable) {
			ims1911_regulator_onoff(data, ON);
			i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL, 0x09);
		}
		ims_info("starting poll timer, delay %lldns\n", ktime_to_ns(data->prox_polling_time));
		hrtimer_start(&data->prox_avg_timer,
			data->prox_polling_time, HRTIMER_MODE_REL);
	} else {
		ims_info("cancelling prox avg poll timer\n");
		hrtimer_cancel(&data->prox_avg_timer);
		cancel_work_sync(&data->work_prox_avg);
		if (!data->enable) {
			//ims1911_regulator_onoff(data, OFF);
			i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL, 0x01);
		}

	}
	mutex_unlock(&data->mutex);

	return size;
}

static ssize_t proximity_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u, %u, %u\n", data->caldata,
		data->high_threshold, data->low_threshold);
}

static ssize_t proximity_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int cal_mode = 0;

	if (sysfs_streq(buf, "1"))
		cal_mode = 1;
	else if (sysfs_streq(buf, "0"))
		cal_mode = 0;
	else {
		ims_err("invalid value %d\n", *buf);
		return -EINVAL;
	}

	return size;
}

static DEVICE_ATTR(name, S_IRUGO, proximity_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, proximity_vendor_show, NULL);
static DEVICE_ATTR(state, S_IRUGO, proximity_adc_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cal_show, proximity_cal_store);

static struct device_attribute *proxi_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_state,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	NULL,
};

static int ims1911_vdd_control(struct ims1911_data *data, bool onoff)
{
	struct device *dev = &data->client->dev;
	struct regulator *vdd;
	int ret;

	ims_info("%s\n", (onoff) ? "on" : "off");

	vdd = devm_regulator_get(dev, "ims1911-vdd");
	if (IS_ERR(vdd)) {
		ims_err("cannot get vdd\n");
		return -ENOMEM;
	}

	regulator_set_voltage(vdd, 3000000, 3000000);

	if (onoff) {
		ret = regulator_enable(vdd);
		if (ret) {
			ims_err("enable vdd failed (%d)\n", ret);
			return ret;
		}
	} else {
		ret = regulator_disable(vdd);
		if (ret) {
			ims_err("disable vdd failed (%d)\n", ret);
			return ret;
		}
	}

	devm_regulator_put(vdd);
	msleep(20);

	return 0;

}

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ims1911_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->enable);
}

static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ims1911_data *data = dev_get_drvdata(dev);
	int value = 0;
	int err = 0;

	err = kstrtoint(buf, 10, &value);
	if (err) {
		ims_err("kstrtoint failed.");
		return -EINVAL;
	}
	if (value != 0 && value != 1) {
		ims_err("wrong value(%d)\n", value);
		return -EINVAL;
	}

	mutex_lock(&data->mutex);

	if (data->enable != value) {
		ims_info("enable(%d)\n", value);
		if (value) {
			ims1911_regulator_onoff(data, ON);
			i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL, 0x0d);
			ims1911_set_threshold(data,  PS_DEFAULT_LOW_THRESHOLD, PS_DEFAULT_HIGH_THRESHOLD);
			enable_irq(data->irq);
			enable_irq_wake(data->irq);
			data->enable = value;

			input_report_abs(data->input, ABS_DISTANCE, 1);
			input_sync(data->input);
		} else {
			i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL, 0x01);		
			disable_irq_wake(data->irq);
			disable_irq(data->irq);
//			ims1911_regulator_onoff(data, OFF);
			data->enable = value;
		}
	} else {
		ims_err("wrong cmd for enable\n");
	}
	mutex_unlock(&data->mutex);

#ifdef CONFIG_SLEEP_MONITOR
	sensors_set_enable_mask(value, SENSORS_ENABLE_PROXY);
#endif

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static void ims1911_work_func_prox_avg(struct work_struct *work)
{
	struct ims1911_data *data = container_of(work, struct ims1911_data,
		work_prox_avg);
	u16 proximity_value = 0;
	int min = 0, max = 0, avg = 0;
	int i = 0;

	for (i = 0; i < PROX_AVG_COUNT; i++) {
		read_ps_result(data, &proximity_value);
		avg += proximity_value;
		if (!i)
			min = proximity_value;
		if (proximity_value < min)
			min = proximity_value;

		if (proximity_value > max)
			max = proximity_value;
		msleep(40);
	}
	avg /= i;
	data->avg[0] = min;
	data->avg[1] = avg;
	data->avg[2] = max;
}

static enum hrtimer_restart ims1911_prox_timer_func(struct hrtimer *timer)
{
	struct ims1911_data *data = container_of(timer, struct ims1911_data,
		prox_avg_timer);

	queue_work(data->wq_avg, &data->work_prox_avg);
	hrtimer_forward_now(&data->prox_avg_timer, data->prox_polling_time);

	return HRTIMER_RESTART;
}



static irqreturn_t ims1911_irq_handler(int irq, void *dev)
{
	struct ims1911_data *data = dev;
	u8 status = 0;

	ims_info("%d\n", irq);

	if (gpio_get_value(data->int_gpio_num)) {
		ims_err("invalid interrupt occured\n");
		i2c_write_byte(data, IMS1911_INTERRUPT_FLAG, 0x00); 	// clear int
		goto exit;
	}

	mutex_lock(&data->mutex);
	// disable ps int (bit 2 : ps int enable)
	i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL,
		ims1911_default[IMS1911_OPERATION_MODE_SEL] & 0x0B);

	// read int status	
	if(i2c_read_byte(data, IMS1911_INTERRUPT_FLAG, &status) == I2C_FAIL) {
		ims_err("cannot read IMS1911_INTERRUPT_FLAG reg\n");
	}
	
//	ims_info("status = %d\n", status);
	// ps int occured
//	if(ims1911_bit_test(status, 2) || ims1911_bit_test(status, 3)) {
		if(read_ps_result(data, &data->ps_count) == I2C_FAIL) {
			ims_err("cannot read ps result\n");
		}
		ims_info("ps count = %d\n", data->ps_count);
//	}

	if (data->ps_count >= data->high_threshold) {
		ims1911_set_threshold(data, data->low_threshold, 0xffff);

		input_report_abs(data->input, ABS_DISTANCE, 0);
		input_sync(data->input);
		ims_info("near event is reported\n");
	} else if (data->ps_count <= data->low_threshold) {
		ims1911_set_threshold(data, 0, data->high_threshold);

		input_report_abs(data->input, ABS_DISTANCE, 1);
		input_sync(data->input);
		ims_info("far event is reported\n");
	}		
	// clear all int status
	i2c_write_byte(data, IMS1911_INTERRUPT_FLAG, 0x00); 

	// enable ps int
	i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL,
		ims1911_default[IMS1911_OPERATION_MODE_SEL]); 
	mutex_unlock(&data->mutex);

	wake_lock_timeout(&data->prx_wake_lock, 3*HZ);

exit:
	ims_info("done\n");

	return IRQ_HANDLED;
}

static int ims1911_setup_irq(struct ims1911_data *data)
{
	int ret;

	ret = gpio_request(data->int_gpio_num, "gpio_proximity_out");
	if (ret < 0) {
		ims_err("gpio %d request failed (%d)\n", data->int_gpio_num, ret);
		return ret;
	}

	ret = gpio_direction_input(data->int_gpio_num);
	if (ret < 0) {
		ims_err("failed gpio %d as input (%d)\n", data->int_gpio_num, ret);
		goto err_gpio_direction_input;
	}

	data->irq = gpio_to_irq(data->int_gpio_num);
	ret = request_threaded_irq(data->irq, NULL, ims1911_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
		"proximity_int", data);
	if (ret < 0) {
		ims_err("request_irq(%d) failed for gpio %d (%d)\n", data->irq,
			data->int_gpio_num, ret);
		goto err_request_irq;
	}

	ims_info("request_irq(%d) success for gpio %d\n", data->irq, data->int_gpio_num);

	disable_irq(data->irq);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(data->int_gpio_num);
done:

	return ret;
}


static int ims1911_parse_dt(struct ims1911_data *data, struct device *dev)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	u32 temp;

	data->int_gpio_num = of_get_named_gpio_flags(np, "ims1911,irq-gpio", 0,
		&flags);
	if (data->int_gpio_num < 0) {
		ims_err("get irq_gpio(%d) error\n", data->int_gpio_num);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "ims1911,ps_setting", &temp);
	if (ret < 0) {
		ims_err("invalid ps_setting value\n");
	} else
		ims1911_default[IMS1911_PS_SETTING] = (u8)temp;

	ret = of_property_read_u32(np, "ims1911,led_drive_current", &temp);
	if (ret < 0) {
		ims_err("invalid led_drive_current value\n");
	} else
		ims1911_default[IMS1911_LED_DRIVE_CURRENT] = (u8)temp;

	ret = of_property_read_u32(np, "ims1911,led_drive_pulse", &temp);
	if (ret < 0) {
		ims_err("invalid led_drive_pulse value\n");
	} else
		ims1911_default[IMS1911_LED_DRIVE_PULSE] = (u8)temp;

	ret = of_property_read_u32(np, "ims1911,ps_int_low_threshold", &temp);
	if (ret < 0) {
		ims_err("invalid ps_int_low_threshold value\n");
	} else {
		ims1911_default[IMS1911_PS_INT_LOW_LSB] = (u8)(temp & 0xff);
		ims1911_default[IMS1911_PS_INT_LOW_MSB] = (u8)(temp >> 8);
	}


	ret = of_property_read_u32(np, "ims1911,ps_int_high_threshold", &temp);
	if (ret < 0) {
		ims_err("invalid ps_int_high_threshold value\n");
	} else {
		ims1911_default[IMS1911_PS_INT_HIGH_LSB] = (u8)(temp & 0xff);
		ims1911_default[IMS1911_PS_INT_HIGH_MSB] = (u8)(temp >> 8);
	}

	return 0;
}



static int ims1911_detect(struct ims1911_data *data)
{
	u8 val;

	if(i2c_read_byte(data, IMS1911_OPERATION_MODE_SEL, &val) == I2C_FAIL) {
		dev_err(&data->client->dev, "ims1911 i2c fail : not found\n");
		return -1;
	}
#if 0	
	if(val != 0) {
		dev_err(&data->client->dev, "ims1911 0x00 reg is not 0 (%d): not found\n", val);		
		return -1;
	}
#else
	ims_info("read value = %d\n", val);
#endif
	return 0;		
}

static int ims1911_initialize(struct ims1911_data *data)
{
	ims_info("initialize\n");

	i2c_write_byte(data, IMS1911_PS_SETTING,
		ims1911_default[IMS1911_PS_SETTING]);
	i2c_write_byte(data, IMS1911_LED_DRIVE_CURRENT,
		ims1911_default[IMS1911_LED_DRIVE_CURRENT]);
	i2c_write_byte(data, IMS1911_LED_DRIVE_PULSE,
		ims1911_default[IMS1911_LED_DRIVE_PULSE]);

	i2c_write_byte(data, IMS1911_PS_INT_LOW_LSB,
		ims1911_default[IMS1911_PS_INT_LOW_LSB]);
	i2c_write_byte(data, IMS1911_PS_INT_LOW_MSB,
		ims1911_default[IMS1911_PS_INT_LOW_MSB]);
	i2c_write_byte(data, IMS1911_PS_INT_HIGH_LSB,
		ims1911_default[IMS1911_PS_INT_HIGH_LSB]);
	i2c_write_byte(data, IMS1911_PS_INT_HIGH_MSB,
		ims1911_default[IMS1911_PS_INT_HIGH_MSB]);	
 	i2c_write_byte(data, IMS1911_OPERATION_MODE_SEL, 0x01);

	data->high_threshold = ((ims1911_default[IMS1911_PS_INT_HIGH_MSB] << 8)
		| ims1911_default[IMS1911_PS_INT_HIGH_LSB]);
	data->low_threshold = ((ims1911_default[IMS1911_PS_INT_LOW_MSB] << 8)
		| ims1911_default[IMS1911_PS_INT_LOW_LSB]);
	
	return 0;
}

static int ims1911_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct  ims1911_data *data;
	int ret;

	ims_info("start\n");

	data = kzalloc(sizeof(struct ims1911_data), GFP_KERNEL);		
	if (!data) {
		dev_err(&client->dev, "error cannot alloc ims1911 dev\n");
		return -ENOMEM;	
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ims_err("i2c functionality failed\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		ret = ims1911_parse_dt(data, &client->dev);
		if (ret < 0) {
			ims_err("of_node error\n");
			goto err_parse_dt;
		}
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	ims1911_vdd_control(data, ON);
	ims1911_regulator_onoff(data, ON);

	ret = ims1911_detect(data);
	if (ret < 0)	
		goto err_sensor_detect;
	
	ims1911_initialize(data);

	data->input = input_allocate_device();
	if (!data->input) {
		ims_err("could not allocate input device\n");
		goto err_input_allocate_device_proximity;
	}

	input_set_drvdata(data->input, data);
	data->input->name = "proximity_sensor";
	input_set_capability(data->input, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(data->input, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(data->input);
	if (ret < 0) {
		ims_err("could not register input device\n");
		goto err_input_register_device_proximity;
	}
	ret = sensors_create_symlink(&data->input->dev.kobj,
		data->input->name);
	if (ret < 0) {
		ims_err("create sysfs symlink error\n");
		goto err_sysfs_create_symlink_proximity;
	}

	ret = sysfs_create_group(&data->input->dev.kobj,
		&proximity_attribute_group);
	if (ret) {
		ims_err("create sysfs group error\n");
		goto err_sysfs_create_group_proximity;
	}
	mutex_init(&data->mutex);
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");

	data->wq_avg = create_singlethread_workqueue("prox_wq_avg");
	if (!data->wq_avg) {
		ret = -ENOMEM;
		ims_err("could not create workqueue\n");
		goto err_create_avg_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox_avg, ims1911_work_func_prox_avg);
	data->prox_avg_enable = 0;

	hrtimer_init(&data->prox_avg_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->prox_polling_time = ns_to_ktime(2000 * NSEC_PER_MSEC);
	data->prox_avg_timer.function = ims1911_prox_timer_func;

	ret = ims1911_setup_irq(data);
	if (ret < 0)
		goto err_request_gpio;

	ret = sensors_register(data->dev, data, proxi_attrs,
		"proximity_sensor");
	if (ret < 0) {
		ims_err("could not sensors_register\n");
		goto err_sensors_register;
	}

//	ims1911_regulator_onoff(data, OFF);

	ims_info("done\n");
	return 0;

err_sensors_register:
	free_irq(data->irq, data);
	gpio_free(data->int_gpio_num);
err_request_gpio:
	destroy_workqueue(data->wq_avg);
err_create_avg_workqueue:
	mutex_destroy(&data->mutex);
	wake_lock_destroy(&data->prx_wake_lock);
	sysfs_remove_group(&data->input->dev.kobj,
		&proximity_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&data->input->dev.kobj,
		data->input->name);
err_sysfs_create_symlink_proximity:
	input_unregister_device(data->input);
err_input_register_device_proximity:
	input_free_device(data->input);
err_input_allocate_device_proximity:
err_sensor_detect:
err_parse_dt:
	kfree(data);

	ims_err("is FAILED (%d)\n", ret);
	return ret;
}

static const struct i2c_device_id ims1911_id[] = {
	{"ims1911", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ims1911_id);

static struct of_device_id ims1911_i2c_match_table[] = {
	{ .compatible = "ims1911",},
	{},
};

MODULE_DEVICE_TABLE(of, ims1911_i2c_match_table);

static struct i2c_driver ims1911_driver = {
	.driver = {
		.name = "ims1911",
		.owner = THIS_MODULE,
		.of_match_table = ims1911_i2c_match_table,

	},
	.probe		= ims1911_probe,
	.id_table	= ims1911_id,
};

module_i2c_driver(ims1911_driver);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for ims1911");
MODULE_LICENSE("GPL");
