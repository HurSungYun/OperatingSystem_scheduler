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

#include <asm/irqflags.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <soc/sprd/pm_debug.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/adi.h>
#include <linux/interrupt.h>
#include <soc/sprd/irqs.h>
#include <soc/sprd/gpio.h>
#include <soc/sprd/iomap.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#ifdef CONFIG_SLEEP_MONITOR
int cp0_sleep_monitor_read(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	unsigned int cp_slp_status0;
	int pretty;

	cp_slp_status0 = sci_glb_read(REG_PMU_APB_CP_SLP_STATUS_DBG0, -1UL);

	*raw_val = cp_slp_status0;

	if (cp_slp_status0 & 0x8) /* check mcu_stop bit */
		pretty = DEVICE_ON_LOW_POWER;
	else
		pretty = DEVICE_ON_ACTIVE2;

	return pretty;
}

static struct sleep_monitor_ops cp0_sleep_monitor_ops = {
	.read_cb_func = cp0_sleep_monitor_read,
};
#endif

static int __init sleep_monitor_scx35_init(void)
{

#ifdef CONFIG_SLEEP_MONITOR
			sleep_monitor_register_ops(NULL, &cp0_sleep_monitor_ops,
			SLEEP_MONITOR_CP0);
#endif

	return 0;
}

device_initcall(sleep_monitor_scx35_init);

