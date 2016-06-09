/*
 * CORERIVER TOUCHCORE 360L touchkey driver
 *
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Author: Taeyoon Yoon <tyoony.yoon@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_TC360_H
#define __LINUX_TC360_H
#define TC300K_DEVICE	"sec_touchkey"
#define TC300K_MAX_KEY	4

struct tc300k_platform_data {
	u32	keycodes[TC300K_MAX_KEY];
	u32	gpio_scl;
	u32	gpio_sda;
	u32	gpio_int;
	u32	gpio_en;
	u32	irq_gpio_flags;
	u32	sda_gpio_flags;
	u32	scl_gpio_flags;
	u32	vcc_gpio_flags;
	u32	gpio_2p8_en;
	u32	vcc_gpio2p8_flags;
	int	udelay;
	int	num_key;
	int 	sensing_ch_num;
	const char *vcc_en_ldo_name;
	const char *vdd_led_ldo_name;
	const char *fw_name;
	u8	suspend_type;
	u8	exit_flag;
	bool	firmup;
};
#endif /* __LINUX_TC360_H */
