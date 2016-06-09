/* drivers/video/sprdfb/lcd_s6e8aa5x01_mipi.c
 *
 * Support for s6e8aa5x01 mipi LCD device
 *
 * Copyright (C) 2015 Spreadtrum
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
#include <linux/device.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include "../sprdfb_panel.h"
#include <mach/gpio.h>
#include "s6e8aa5x01_dimming.h"

#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   150

/* Dimming data*/
#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DIM_BRIGHTNESS		17
#define GAMMA_CMD_CNT	36
#define DIMMING_COUNT	62
#define MTP_ADDR	0xC8
#define CHIP_ID_ADDR	0xD5
#define MTP_LEN	0x21
#define ELVSS_ADDR	0xB6
#define LDI_GAMMA	0xCA
#define DIM_OFFSET_CRITERIA_0	20	/* level 20 = 29nit*/
#define DIM_OFFSET_CRITERIA_1	14	/* level 14 = 20nit*/

#ifdef CONFIG_FB_ESD_SUPPORT
#define PANEL_POWER_MODE_REG 0x0A
#define PANEL_POWER_MODE_OFFSET 1
#define PANEL_POWER_MODE_LEN 2
#define PANEL_NOT_DEAD_STS 0x1C
#define PANEL_READ_CNT 4
#define DATA_TYPE_SET_RX_PKT_SIZE 0x37
#endif

typedef struct LCM_Init_Code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
} LCM_Init_Code;

typedef struct LCM_force_cmd_code_tag {
	unsigned int datatype;
	LCM_Init_Code real_cmd_code;
} LCM_Force_Cmd_Code;

struct panel_mtp {
	unsigned int reg;
	unsigned int size;
	unsigned char data[MAX_DATA];
};

extern uint32_t lcd_id_from_uboot;
extern uint8_t elvss_offset_from_uboot;
extern uint8_t mtp_offset_t[MTP_LEN + 6];
extern uint8_t hbm_offset_t[15];
extern uint8_t chip_id_t[5] ;
extern uint8_t color_offset_t[4];
extern uint8_t g_mtp_offset_error;

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) - 1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT) | len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT) | ms)
#define LCM_TAG_SEND  (1 << 0)
#define LCM_TAG_SLEEP (1 << 1)

enum {
	TEMP_RANGE_0 = 0,		/* 0 < temperature*/
	TEMP_RANGE_1,		/*-20 < temperature < =0*/
	TEMP_RANGE_2,		/*temperature <=-20*/
};

static bool first_boot = true;

static LCM_Init_Code init_data[] = {
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0x5A, 0x5A} },
	{LCM_SEND(2),		{0xCC, 0x4C} },
	{LCM_SEND(1),		{0x11} },
	{LCM_SLEEP(20)},
	{LCM_SEND(36),		{34, 0x00, 0xCA,
						0x01, 0x00, 0x01,
						0x00, 0x01, 0x00,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x00, 0x00, 0x00, } },
	{LCM_SEND(7),		{5, 0x00, 0xB2, 0x00, 0x0F, 0x00, 0x0F} },
	{LCM_SEND(5),		{3, 0x00, 0xB6, 0xBC, 0x0F} },
	{LCM_SEND(2),		{0xf7, 0x03} },
	{LCM_SEND(2),		{0xf7, 0x00} },
	{LCM_SEND(6),		{4, 0x00, 0xC0, 0xD8, 0xD8, 0x40} },
	{LCM_SEND(10),		{8, 0x00, 0xB8, 0x38, 0x00, 0x00, 0x60,
							0x44, 0x00, 0xA8} },
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0xA5, 0xA5} },
	{LCM_SLEEP(120)},
	{LCM_SEND(1),		{0x29} },
};

static LCM_Init_Code sleep_in[] =  {
	{LCM_SEND(1), {0x28} },
	{LCM_SEND(1), {0x10} },
	{LCM_SLEEP(120)},
};

static LCM_Init_Code sleep_out[] =  {
	{LCM_SEND(1), {0x11} },
	{LCM_SLEEP(120)},
	{LCM_SEND(1), {0x29} },
	{LCM_SLEEP(20)},
};

static LCM_Init_Code disp_off[] =  {
	{LCM_SEND(1), {0x28} },
};

static LCM_Init_Code sleep[] =  {
	{LCM_SEND(1), {0x10} },
	{LCM_SLEEP(120)},
};

static LCM_Init_Code test_key_on[] =  {
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0x5A, 0x5A} },
};

static LCM_Init_Code test_key_off[] =  {
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0xA5, 0xA5} },
};

static LCM_Init_Code gamma_update[] =  {
	{LCM_SEND(2),		{0xf7, 0x03} },
};

static LCM_Init_Code global_para[] =  {
	{LCM_SEND(2),		{0xB0, 0x00} },
};

static LCM_Init_Code aid_cmd[] = {
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xEE} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xE1} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xD4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xCF} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xC2} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xB4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xAF} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0xA3} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x96} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x91} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x82} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x75} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x70} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x54} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x50} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x42} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x33} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x1E} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x04, 0x10} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0xF2} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0xDE} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0xC6} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0xB1} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0x95} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0x73} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0x5F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0x43} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x03, 0x20} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0xFE} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0xDF} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0xB2} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0x90} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0x5E} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x02, 0x30} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xF4} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0xBF} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0x82} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0x40} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x01, 0x00} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0xB2} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x62} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
	{LCM_SEND(5),	{3, 0x00, 0xb2, 0x00, 0x0F} },
};

static LCM_Init_Code elvss_cmd[] = {
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x0f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x11} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x14} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x17} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x19} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1b} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1d} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xac, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1f} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1e} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1e} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1e} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1e} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1d} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1d} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1d} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1d} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1c} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1b} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x1a} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x18} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x18} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x17} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x16} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x14} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x13} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x12} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x10} },
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x0f} },
};

static LCM_Init_Code hbm_cmd[] = {
	{LCM_SEND(5),	{3, 0x00, 0xb6, 0xbc, 0x0f} },
	{LCM_SEND(2),	{0xb5, 0x50} },
	{LCM_SEND(2),	{0x53, 0xc0} },
	{LCM_SEND(37),	{35, 0x00, 0xB4, 0x0B,
						0x01, 0x00, 0x01,
						0x00, 0x01, 0x00,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x80, 0x80, 0x80,
						0x00, 0x00, 0x00, } },
	{LCM_SEND(2),	{0x55, 0x02} },
	{LCM_SEND(2),	{0xf7, 0x03} },
};

static LCM_Init_Code hbm_off[] = {
	{LCM_SEND(2),	{0x53, 0x00} },
};

static LCM_Init_Code acl_on[] =  {
	{LCM_SEND(2),	{0xb5, 0x50} },
	{LCM_SEND(2),	{0x55, 0x02} },
};

static LCM_Init_Code acl_off[] =  {
	{LCM_SEND(2),	{0xb5, 0x40} },
	{LCM_SEND(2),	{0x55, 0x00} },
};

static LCM_Init_Code dim_offset[] =  {
	{LCM_SEND(2),	{0xb0, 0x15} },
	{LCM_SEND(2),	{0xb6, 0x00} },
};

static LCM_Init_Code dim_offset_0[] =  {
	{LCM_SEND(2),	{0xb0, 0x15} },
	{LCM_SEND(2),	{0xb6, 0x00} },
};

static LCM_Init_Code dim_offset_1[] =  {
	{LCM_SEND(2),	{0xb0, 0x15} },
	{LCM_SEND(2),	{0xb6, 0x07} },
};

static LCM_Init_Code dim_offset_2[] =  {
	{LCM_SEND(2),	{0xb0, 0x15} },
	{LCM_SEND(2),	{0xb6, 0x05} },
};

static LCM_Init_Code temp_offset_0[] =  {
	{LCM_SEND(2),	{0xb0, 0x07} },
	{LCM_SEND(2),	{0xb8, 0x19} },
};

static LCM_Init_Code temp_offset_1[] =  {
	{LCM_SEND(2),	{0xb0, 0x07} },
	{LCM_SEND(2),	{0xb8, 0x8a} },
};

static LCM_Init_Code temp_offset_2[] =  {
	{LCM_SEND(2),	{0xb0, 0x07} },
	{LCM_SEND(2),	{0xb8, 0x94} },
};

static const unsigned int br_convert[DIMMING_COUNT] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 44, 46, 48, 50, 52, 54, 56, 58,
	60, 63, 66, 69, 73, 77, 81, 85, 89, 93,
	97, 100
};

static const unsigned char GAMMA_360[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};
struct panel_mtp reg_data;
static int32_t mipi_dcs_send(struct panel_spec *self,
				LCM_Init_Code *cmd , int len)
{
	int32_t i = 0;
	unsigned int tag;

	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;

	for (i = 0; i < len; i++) {
		tag = (cmd->tag >> 24);
		if (tag & LCM_TAG_SEND) {
			mipi_dcs_write(cmd->data, (cmd->tag & LCM_TAG_MASK));
			udelay(20);
		} else if (tag & LCM_TAG_SLEEP) {
			msleep(cmd->tag & LCM_TAG_MASK);
		}
		cmd++;
	}

	return 0;
}

#ifdef CONFIG_SPRDFB_MDNIE_LITE_TUNING
static void mdnie_cmds_send(struct sprdfb_device *fb_dev, struct mdnie_command *seq, u32 len)
{
	int i = 0;
	struct panel_spec *panel = fb_dev->panel;

#ifdef CONFIG_FB_VSYNC_SUPPORT
        if (fb_dev->ctrl->wait_for_vsync)
                fb_dev->ctrl->wait_for_vsync(fb_dev);
#endif

	for (i = 0; i < len; i++)
		mipi_dcs_send(panel, seq[i].sequence, seq[i].size);

	return;
}

static uint32_t s6e8aa5x01_get_color_coordinates(struct sprdfb_device *fb_dev, uint32_t *co_ord)
{
	if (co_ord && color_offset_t) {
		co_ord[0] = color_offset_t[0] << 8 | color_offset_t[1]; /* X */
		co_ord[1] = color_offset_t[2] << 8 | color_offset_t[3]; /* Y */
		pr_info("mdnie: X = %d, Y = %d\n", co_ord[0], co_ord[1]);
	} else {
		pr_err("%s : wrong input param co_ord\n", __func__);
	}

	return 0;
}
#endif

static void s6e8aa5x01_test_key(struct panel_spec *self, bool enable)
{
	LCM_Init_Code *test_key = NULL;
	int32_t size = 0;

	if (enable) {
		test_key = test_key_on;
		size = ARRAY_SIZE(test_key_on);
	} else {
		test_key = test_key_off;
		size = ARRAY_SIZE(test_key_off);
	}

	mipi_dcs_send(self, test_key, size);
}

static int32_t s6e8aa5x01_mipi_init(struct panel_spec *self)
{
	mipi_set_cmd_mode_t mipi_set_cmd_mode =
			self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	pr_info("%s:\n", __func__);

	mipi_set_cmd_mode();
	mipi_eotp_set(0, 1);

	mipi_dcs_send(self, init_data, ARRAY_SIZE(init_data));

	if (self->hbm_on) {
		s6e8aa5x01_test_key(self, true);
		mipi_dcs_send(self, hbm_cmd, ARRAY_SIZE(hbm_cmd));
		s6e8aa5x01_test_key(self, false);
	}

	mipi_eotp_set(1, 1);

	return 0;
}

static uint32_t s6e8aa5x01_readid(struct panel_spec *self)
{
	uint8_t j = 0;
	uint8_t read_data[4] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode =
			self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write =
			self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read =
			self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	pr_info("%s:\n", __func__);

	mipi_set_cmd_mode();
	mipi_eotp_set(0, 0);

	for (j = 0; j < 4; j++) {
		param[0] = 0x03;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0x04, 3, read_data);
		LCD_PRINT("lcd_s6e8aa5x01_mipi read id 0x%x,0x%x,0x%x!\n",
				read_data[0], read_data[1], read_data[2]);
		if ((0x40 == read_data[0]) && (0x00 == read_data[1])
				&& (0x02 == read_data[2])) {
			LCD_PRINT("s6e8aa5x01 0x02 id success!\n");
			return 0x400002;
		} else if ((0x40 == read_data[0]) && (0x00 == read_data[1])
				&& (0x03 == read_data[2])) {
			LCD_PRINT("s6e8aa5x01 0x03 id success!\n");
			return 0x400003;
		}
	}

	mipi_eotp_set(0, 0);

	pr_info("lcd_s6e8aa5x01_mipi read id failed!\n");
	return 0;

}

static void panel_get_gamma_tbl(struct panel_spec *self,
						const unsigned char *data)
{
	int i;
#ifdef SMART_DIMMING_DEBUG
	int j;
#endif

	panel_read_gamma(self->dimming, data);
	panel_generate_volt_tbl(self->dimming);

	for (i = 0; i < MAX_GAMMA_CNT - 1; i++) {
		self->gamma_tbl[i][0] = 34;
		self->gamma_tbl[i][1] = 0x00;
		self->gamma_tbl[i][2] = LDI_GAMMA;
		panel_get_gamma(self->dimming, i, &self->gamma_tbl[i][3]);
	}
	self->gamma_tbl[MAX_GAMMA_CNT - 1][0] = 34;
	self->gamma_tbl[MAX_GAMMA_CNT - 1][1] = 0x00;
	memcpy(&self->gamma_tbl[MAX_GAMMA_CNT - 1][2],
			GAMMA_360, sizeof(GAMMA_360));

#ifdef SMART_DIMMING_DEBUG
	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		LCD_PRINT("[%d]", i);
		for (j = 0; j < 34; j++)
			LCD_PRINT("%2x,", self->gamma_tbl[i][j + 2]);
		LCD_PRINT("\n");
	}
#endif
	return;
}

static int s6e8aa5x01_check_mtp(struct panel_spec *self)
{
	LCM_Init_Code *g_para = NULL;
	LCM_Init_Code *hbm = NULL;
	uint8_t i = 0;
	uint8_t mtp_offset[MTP_LEN + 6] = {0};
	uint8_t elvss_offset = 0;
	uint8_t hbm_g[15] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	int32_t size = 0;
	mipi_force_write_t mipi_force_write =
			self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read =
			self->info.mipi->ops->mipi_force_read;
	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;

	pr_info("%s:\n", __func__);

	s6e8aa5x01_test_key(self, true);

	/* MTP OFFSET READ*/
	param[0] = MTP_LEN + 6;
	param[1] = 0x00;
	for (i = 0; i < 3; i++) {
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(MTP_ADDR, MTP_LEN + 6, mtp_offset);
		if (mtp_offset[30] != 2 || mtp_offset[31] != 3 ||
			mtp_offset[32] != 2) {
			g_mtp_offset_error = 1;
			pr_err("%s: fail to read mtp offset[%d]\n",
					__func__, i);
		} else {
			g_mtp_offset_error = 0;
			break;
		}
	}

	/* ELVSS OFFSET */
	g_para = global_para;
	size = ARRAY_SIZE(global_para);
	param[0] = 0x1;
	param[1] = 0x00;
	mipi_force_write(0x37, param, 2);
	g_para->data[1] = 0x15;	/*B6 22th para*/
	mipi_dcs_write(g_para->data, (g_para->tag & LCM_TAG_MASK));
	read_rtn = mipi_force_read(ELVSS_ADDR, 0x1, &elvss_offset);
	dim_offset_0[1].data[1] = elvss_offset;

	/* HBM GAMMA */
	param[0] = 0xf;
	param[1] = 0x00;
	mipi_force_write(0x37, param, 2);
	g_para->data[1] = 0x48;	/*C8 34th~39th, 73th ~ 87th*/
	mipi_dcs_write(g_para->data, (g_para->tag & LCM_TAG_MASK));
	read_rtn = mipi_force_read(MTP_ADDR, 0xf, hbm_g);

	hbm = &hbm_cmd[3];
	memcpy(&hbm->data[4], &mtp_offset[MTP_LEN], 6);
	memcpy(&hbm->data[10], hbm_g, 0xf);

	/* CHIP ID READ*/
	param[0] = 0x05;
	param[1] = 0x00;
	mipi_force_write(0x37, param, 2);
	read_rtn = mipi_force_read(CHIP_ID_ADDR, 5, chip_id_t);

	s6e8aa5x01_test_key(self, false);

	panel_get_gamma_tbl(self, mtp_offset);
	self->mtp_done = true;

	return 0;
}

static uint32_t s6e8aa5x01_apply_mtp_from_uboot(struct panel_spec *self)
{
	LCM_Init_Code *hbm = NULL;
	pr_info("%s:\n", __func__);

	if (first_boot && (g_mtp_offset_error == 0)) {
		first_boot = false;

		dim_offset_0[1].data[1] = elvss_offset_from_uboot;

		hbm = &hbm_cmd[3];
		memcpy(&hbm->data[4], &mtp_offset_t[MTP_LEN], 6);
		memcpy(&hbm->data[10], hbm_offset_t, 0xf);

		panel_get_gamma_tbl(self, mtp_offset_t);
		self->mtp_done = true;
	} else {
		pr_err("%s : failure during mtp offset read from BL mtp offset error = %d\n",
			__func__, g_mtp_offset_error);
	}
	return 0;
}

static int32_t s6e8aa5x01_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	LCM_Init_Code *display_off = NULL;
	unsigned int tag;
	int32_t size = 0;
	mipi_set_cmd_mode_t mipi_set_cmd_mode =
			self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_set_video_mode_t mipi_set_video_mode =
			self->info.mipi->ops->mipi_set_video_mode;
	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	pr_info("%s: is_sleep = %d\n", __func__, is_sleep);

	mipi_eotp_set(0, 1);

	if (is_sleep) {
		if (first_boot && (g_mtp_offset_error == 1)) {
			display_off = disp_off;
			size = ARRAY_SIZE(disp_off);
			mipi_set_cmd_mode();
			for (i = 0; i < size; i++) {
				tag = (display_off->tag >> 24);
				if (tag & LCM_TAG_SEND)
					mipi_dcs_write(display_off->data,
					(display_off->tag & LCM_TAG_MASK));
				else if (tag & LCM_TAG_SLEEP)
					msleep(display_off->tag & LCM_TAG_MASK);
				display_off++;
			}
			s6e8aa5x01_check_mtp(self);
			mipi_set_video_mode();
			first_boot = false;
			sleep_in_out = sleep;
			size = ARRAY_SIZE(sleep);
		} else {
			sleep_in_out = sleep_in;
			size = ARRAY_SIZE(sleep_in);
		}
	} else {
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}

	mipi_dcs_send(self, sleep_in_out, size);

	mipi_eotp_set(1, 1);

	return 0;
}

static int32_t s6e8aa5x01_set_elvss_offset(struct panel_spec *self)
{
	int level;
	int stage1_offset[7] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x0F};
	int stage2_offset[7] = {0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x13, 0x13};

	level = self->br_map[self->bl];

	switch (self->temp_stage) {
	case TEMP_RANGE_0:
		mipi_dcs_send(self, temp_offset_0,
				ARRAY_SIZE(temp_offset_0));
		if (level > DIM_OFFSET_CRITERIA_0)
			mipi_dcs_send(self, dim_offset_0,
					ARRAY_SIZE(dim_offset_0));
		else if (level > DIM_OFFSET_CRITERIA_1)
			mipi_dcs_send(self, dim_offset_1,
					ARRAY_SIZE(dim_offset_1));
		else
			mipi_dcs_send(self, dim_offset_2,
					ARRAY_SIZE(dim_offset_2));
		break;
	case TEMP_RANGE_1:
		mipi_dcs_send(self, temp_offset_1,
				ARRAY_SIZE(temp_offset_1));
		if (level > DIM_OFFSET_CRITERIA_0) {
			mipi_dcs_send(self, dim_offset_0,
				ARRAY_SIZE(dim_offset_0));
		} else {
			 if (level > DIM_OFFSET_CRITERIA_1)
				dim_offset[1].data[1] =
				stage1_offset[DIM_OFFSET_CRITERIA_0 - level];
			else
				dim_offset[1].data[1] = stage1_offset[6];

			mipi_dcs_send(self, dim_offset, ARRAY_SIZE(dim_offset));
		}
		break;
	case TEMP_RANGE_2:
		mipi_dcs_send(self, temp_offset_2,
				ARRAY_SIZE(temp_offset_2));
		if (level > DIM_OFFSET_CRITERIA_0) {
			mipi_dcs_send(self, dim_offset_0,
					ARRAY_SIZE(dim_offset_0));
		 } else {
			 if (level > DIM_OFFSET_CRITERIA_1)
				dim_offset[1].data[1] =
				stage2_offset[DIM_OFFSET_CRITERIA_0 - level];
			else
				dim_offset[1].data[1] = stage2_offset[6];

			mipi_dcs_send(self, dim_offset, ARRAY_SIZE(dim_offset));
		}
		break;
	default:
		break;
	}

	pr_debug("%s: TEMP STAGE[%d]\n", __func__, self->temp_stage);

	return 0;
}

static int32_t s6e8aa5x01_dimming_adjust(struct panel_spec *self,
						uint16_t brightness)
{
	int dim_brightness;

	if (self->bl > DIM_BRIGHTNESS)
		dim_brightness = DIM_BRIGHTNESS;
	else
		dim_brightness = self->bl;

	pr_info("%s: br[%d], adjust_br[%d]\n", __func__,
			brightness, dim_brightness);

	return dim_brightness;
}

static int32_t s6e8aa5x01_set_acl(struct sprdfb_device *fb_dev,
						unsigned int step)
{
	struct panel_spec *self = fb_dev->panel;

	if (step)
		mipi_dcs_send(self, acl_on, ARRAY_SIZE(acl_on));
	else
		mipi_dcs_send(self, acl_off, ARRAY_SIZE(acl_off));

	pr_info("%s: ACL[%d]\n", __func__, step);

	return 0;
}

static int32_t s6e8aa5x01_set_brightnees(struct sprdfb_device *fb_dev,
						uint16_t brightness)
{
	struct panel_spec *self = fb_dev->panel;
	LCM_Init_Code *update = NULL;
	LCM_Init_Code *aid = NULL;
	LCM_Init_Code *elvss = NULL;
	int level = 0;
	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	if ((int)brightness < MIN_BRIGHTNESS ||
		brightness > MAX_BRIGHTNESS) {
		pr_err("lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}
	if (!self->mtp_done) {
		pr_info("%s: gamma table is not generated yet.\n", __func__);
		return -EINVAL;
	}
	if (brightness == MIN_BRIGHTNESS)
		brightness = s6e8aa5x01_dimming_adjust(self, brightness);

	self->bl = brightness;
	level = self->br_map[brightness];

#ifdef CONFIG_FB_VSYNC_SUPPORT
	if (fb_dev->ctrl->wait_for_vsync)
		fb_dev->ctrl->wait_for_vsync(fb_dev);
#endif
	mipi_eotp_set(0, 1);
	s6e8aa5x01_test_key(self, true);

	aid = &aid_cmd[level];
	elvss = &elvss_cmd[level];
	update = gamma_update;
	mipi_dcs_write(self->gamma_tbl[level],
			(LCM_SEND(36) & LCM_TAG_MASK));
	mipi_dcs_send(self, aid, 1);
	mipi_dcs_send(self, elvss, 1);

	s6e8aa5x01_set_elvss_offset(self);

	if (level == (DIMMING_COUNT - 1) && !self->acl)
		s6e8aa5x01_set_acl(fb_dev, 0);
	else
		s6e8aa5x01_set_acl(fb_dev, 1);

	mipi_dcs_send(self, update, ARRAY_SIZE(gamma_update));

	s6e8aa5x01_test_key(self, false);

	mipi_eotp_set(1, 1);

	pr_info("%s: brightness[%d], level[%d], temp[%d]\n",
			__func__, brightness, level, self->temp_stage);

	return 0;
}

static int32_t s6e8aa5x01_set_hbm(struct sprdfb_device *fb_dev,
						bool enable)
{
	struct panel_spec *self = fb_dev->panel;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	mipi_eotp_set(0, 1);

	s6e8aa5x01_test_key(self, true);

	if (enable)
		mipi_dcs_send(self, hbm_cmd, ARRAY_SIZE(hbm_cmd));
	else
		mipi_dcs_send(self, hbm_off, ARRAY_SIZE(hbm_off));

	s6e8aa5x01_test_key(self, false);

	mipi_eotp_set(1, 1);

	pr_info("%s: HBM[%s]\n", __func__, enable ? "ON" : "OFF");

	/* Recover previous register value */
	if (!enable)
		s6e8aa5x01_set_brightnees(fb_dev, self->bl);

	return 0;
}

static int32_t s6e8aa5x01_dimming_init(struct panel_spec *self,
						struct device *dev)
{
	int start = 0, end, i, offset = 0;
	int ret;

	self->dimming = devm_kzalloc(dev, sizeof(*self->dimming),
								GFP_KERNEL);
	if (!self->dimming) {
		dev_err(dev, "failed to allocate dimming.\n");
		ret = -ENOMEM;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		self->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
								GFP_KERNEL);
		if (!self->gamma_tbl[i]) {
			dev_err(dev,
					"failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto err_free_dimming;
		}
	}

	self->br_map = devm_kzalloc(dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (!self->br_map) {
		dev_err(dev, "failed to allocate br_map\n");
		ret = -ENOMEM;
		goto err_free_gamma_tbl;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&self->br_map[start], i, end - start + 1);
		start = end + 1;
	}

	return 0;

err_free_gamma_tbl:
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (self->gamma_tbl[i])
			devm_kfree(dev, self->gamma_tbl[i]);
err_free_dimming:
	devm_kfree(dev, self->dimming);
	pr_err("%s: fail to dimming init.\n", __func__);
	return ret;
}

static ssize_t show_chip_id_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[20];

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
				chip_id_t[0], chip_id_t[1], chip_id_t[2],
				chip_id_t[3], chip_id_t[4]);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t panel_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;

	return sprintf(buf, "%s\n", panel->hbm_on ? "on" : "off");
}

static ssize_t panel_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;

	if (!strncmp(buf, "on", 2))
		panel->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		panel->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (fb_dev->power > FB_BLANK_NORMAL) {
		pr_warn("%s:hbm control before lcd enable.\n", __func__);
		return -EPERM;
	}

	s6e8aa5x01_set_hbm(fb_dev, panel->hbm_on);

	pr_info("%s:HBM %s.\n", __func__, panel->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t panel_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;

	return sprintf(buf, "%d\n", panel->acl);
}

static ssize_t panel_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;
	unsigned long value;
	int level = 0,rc;
	mipi_eotp_set_t mipi_eotp_set =
			panel->info.mipi->ops->mipi_eotp_set;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value > 1) {
		pr_warn("%s:unsupported acl step.\n", __func__);
		return -EPERM;
	}

	panel->acl = value;

	if (fb_dev->power > FB_BLANK_NORMAL) {
		pr_warn("%s:acl control before lcd enable.\n", __func__);
		return -EPERM;
	}

	level = panel->br_map[panel->bl];
	if (level == (DIMMING_COUNT - 1)) {
		mipi_eotp_set(0, 1);

		s6e8aa5x01_test_key(panel, true);

		s6e8aa5x01_set_acl(fb_dev, panel->acl);

		s6e8aa5x01_test_key(panel, false);

		mipi_eotp_set(1, 1);
	}

	pr_info("%s:acl[%d].\n", __func__, panel->acl);

	return size;
}

static ssize_t panel_elvss_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;

	return sprintf(buf, "%d\n", panel->temp_stage);
}

static ssize_t panel_elvss_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;
	unsigned long value;
	int rc;
	mipi_eotp_set_t mipi_eotp_set =
			panel->info.mipi->ops->mipi_eotp_set;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value > TEMP_RANGE_2) {
		pr_warn("%s:unsupported temp range.\n", __func__);
		return -EPERM;
	}

	panel->temp_stage = value;

	if (fb_dev->power > FB_BLANK_NORMAL) {
		pr_warn("%s:elvss control before lcd enable.\n", __func__);
		return -EPERM;
	}
	mipi_eotp_set(0, 1);

	s6e8aa5x01_test_key(panel, true);

	s6e8aa5x01_set_elvss_offset(panel);

	s6e8aa5x01_test_key(panel, false);

	mipi_eotp_set(1, 1);

	pr_info("%s:stage[%d].\n", __func__, panel->temp_stage);

	return size;
}


static ssize_t panel_read_mtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, pos = 0;

	pos += sprintf(buf+pos, "[0x%X]", reg_data.reg);
	for (i = 0; i < reg_data.size; i++)
		pos += sprintf(buf+pos, "0x%X, ", reg_data.data[i]);
	pos += sprintf(buf+pos, "\n");

	return pos;
}

static ssize_t panel_read_mtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct panel_spec *panel = fb_dev->panel;
	LCM_Init_Code *g_para = NULL;
	int reg, offset, len = 0;
	uint8_t param[2] = {0};
	mipi_force_write_t mipi_force_write =
			panel->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read =
			panel->info.mipi->ops->mipi_force_read;
	mipi_dcs_write_t mipi_dcs_write =
			panel->info.mipi->ops->mipi_dcs_write;


	if (fb_dev->power > FB_BLANK_NORMAL) {
		pr_warn("%s:hbm control before lcd enable.\n", __func__);
		return -EPERM;
	}

	sscanf(buf, "%x, %x, %x", &reg, &len, &offset);

	s6e8aa5x01_test_key(panel, true);
	/* MTP READ*/
	reg_data.size = len;
	reg_data.reg = reg;
	g_para = global_para;
	param[0] = len;
	param[1] = 0x00;
	mipi_force_write(0x37, param, 2);
	if (offset != 0) {
		g_para->data[1] = offset;
		mipi_dcs_write(g_para->data, (g_para->tag & LCM_TAG_MASK));
	}

	mipi_force_read(reg, len, reg_data.data);

	s6e8aa5x01_test_key(panel, false);

	return size;
}

static struct device_attribute ld_dev_attrs[] = {
	__ATTR(chip_id, S_IRUGO, show_chip_id_info, NULL),
	__ATTR(hbm, S_IRUGO | S_IWUSR, panel_hbm_show, panel_hbm_store),
	__ATTR(acl, S_IRUGO | S_IWUSR, panel_acl_show, panel_acl_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, panel_elvss_show, panel_elvss_store),
	__ATTR(read_mtp, S_IRUGO | S_IWUSR, panel_read_mtp_show,
			panel_read_mtp_store),
};

static int32_t s6e8aa5x01_power_on(struct sprdfb_device *fb_dev,
					bool enable)
{
	struct panel_spec *panel = fb_dev->panel;

	if (gpio_is_valid(panel->ldo_en)) {
		gpio_direction_output(panel->ldo_en, enable);
		usleep_range(10000, 11000);
		pr_info("%s: %s.\n", __func__, enable ? "ON" : "OFF");
	}

	return 0;
}

static int32_t s6e8aa5x01_get_type(char *buf, unsigned int panel_id)
{
	char temp[15];

	sprintf(temp, "SDC_%06x\n", panel_id);
	strcat(buf, temp);

	return strlen(buf);
}

static int32_t s6e8aa5x01_init(struct sprdfb_device *fb_dev, struct device *dev)
{
	struct panel_spec *panel = fb_dev->panel;
	int i;
	int ret = 0;

	if (!gpio_is_valid(panel->ldo_en)) {
		pr_warn("%s: Invalid gpio pin : %d\n", __func__, panel->ldo_en);
	} else {
		ret = gpio_request(panel->ldo_en, "LCD_LDO_EN");
		if (ret < 0) {
			pr_err("fail to request gpio(LCD_LDO_EN)\n");
			return ret;
		}
	}

	if (!gpio_is_valid(panel->rst_gpio)) {
		pr_warn("%s: Invalid gpio pin : %d\n", __func__, panel->rst_gpio);
	} else {
		ret = gpio_request(panel->rst_gpio, "LCD_RST_GPIO");
		if (ret < 0) {
			pr_err("fail to request gpio(LCD_RST_GPIO)\n");
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ld_dev_attrs); i++) {
		ret = device_create_file(dev,
				&ld_dev_attrs[i]);
		if (ret < 0) {
			pr_err("failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(dev,
					&ld_dev_attrs[i]);
		}
	}

	panel->temp_stage = TEMP_RANGE_0;

	return ret;
}
#ifdef CONFIG_FB_ESD_SUPPORT
static int sprdfb_s6e8aa5x01_dsi_rx_cmds(struct panel_spec *panel, u8 regs,
				u16 get_len, u8 *data, bool in_bllp)
{
	int ret;
	struct ops_mipi *ops = panel->info.mipi->ops;
	u8 pkt_size[2];
	u16 work_mode = panel->info.mipi->work_mode;

	if (!data) {
		pr_err("Expected registers or data is NULL.\n");
		return -EIO;
	}
	if (!ops->mipi_set_cmd_mode || !ops->mipi_force_read
			|| !ops->mipi_force_write) {
		pr_err("%s: Expected functions are NULL.\n", __func__);
		return -EFAULT;
	}

	if (!in_bllp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_lp_mode();
		else
			ops->mipi_set_data_lp_mode();
		/*
		 * If cmds is transmitted in LP mode, not in BLLP,
		 * commands mode should be enabled
		 * */
		ops->mipi_set_cmd_mode();
	}

	ops->mipi_eotp_set(0, 1);
	pkt_size[0] = get_len & 0xff;
	pkt_size[1] = (get_len >> 8) & 0xff;

	ops->mipi_force_write(DATA_TYPE_SET_RX_PKT_SIZE, pkt_size,
				sizeof(pkt_size));

	ret = ops->mipi_force_read(regs, get_len, data);
	if (ret < 0) {
		pr_err("%s: dsi read id fail\n", __func__);
		return -EINVAL;
	}

	if (!in_bllp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_hs_mode();
		else
			ops->mipi_set_data_hs_mode();
	}

	ops->mipi_eotp_set(1, 1);

	return 0;
}

static int s6e8aa5x01_dsi_panel_dead(struct panel_spec *panel)
{
	int i, cnt = PANEL_READ_CNT;
	u8 regs = PANEL_POWER_MODE_REG;
	u8 data[PANEL_POWER_MODE_LEN] = { 0 };
	bool in_bllp = false;

#ifdef FB_CHECK_ESD_IN_VFP
	in_bllp = true;
#endif
	do {
		sprdfb_s6e8aa5x01_dsi_rx_cmds(panel, regs,
					PANEL_POWER_MODE_LEN, data, in_bllp);

		i = PANEL_POWER_MODE_OFFSET;
		pr_debug("%s: reading panel power mode(0x%2x) is 0x%x",
					__func__, regs, data[i]);
		if ((data[i] & PANEL_NOT_DEAD_STS) == PANEL_NOT_DEAD_STS)
			return true;
	} while (cnt-- > 0);

	pr_err("panel is dead, read power mode is 0x%x.\n", data[i]);
	return false;
}
#endif

static uint32_t s6e8aa5x01_dummy(struct panel_spec *self)
{
	pr_debug("%s:", __func__);

	return 0;
}

static struct panel_operations lcd_s6e8aa5x01_mipi_operations = {
	.panel_init = s6e8aa5x01_mipi_init,
	.panel_readid = s6e8aa5x01_readid,
	.panel_enter_sleep = s6e8aa5x01_enter_sleep,
	.panel_set_brightness = s6e8aa5x01_set_brightnees,
	.panel_dimming_init = s6e8aa5x01_dimming_init,
	.panel_before_resume = s6e8aa5x01_dummy,
#ifdef CONFIG_SPRDFB_MDNIE_LITE_TUNING
	.panel_get_color_coordinates = s6e8aa5x01_get_color_coordinates,
	.panel_send_mdnie_cmds = mdnie_cmds_send,
#endif
	.panel_initialize = s6e8aa5x01_init,
	.panel_get_type = s6e8aa5x01_get_type,
	.panel_power_on = s6e8aa5x01_power_on,
#ifdef CONFIG_FB_ESD_SUPPORT
	.panel_esd_check = s6e8aa5x01_dsi_panel_dead,
#endif
	.panel_read_offset = s6e8aa5x01_apply_mtp_from_uboot,
};

static struct timing_rgb lcd_s6e8aa5x01_mipi_timing = {
	.hfp = 84,  /* unit: pixel */
	.hbp = 90,
	.hsync = 40,
	.vfp = 14, /* unit: line */
	.vbp = 6,
	.vsync = 4,
};

static struct info_mipi lcd_s6e8aa5x01_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 500 * 1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_s6e8aa5x01_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_s6e8aa5x01_mipi_spec = {
	.width = 720,
	.height = 1280,
	.width_mm = 62,
	.height_mm = 110,
	.fps	= 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.oled = true,
	.ldo_en = 167,
	.rst_gpio = 103,
	.suspend_mode = SEND_SLEEP_CMD,
	.info = {
		.mipi = &lcd_s6e8aa5x01_mipi_info
	},
	.ops = &lcd_s6e8aa5x01_mipi_operations,
};

struct panel_cfg lcd_s6e8aa5x01_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x0,
	.lcd_name = "lcd_s6e8aa5x01_mipi",
	.panel = &lcd_s6e8aa5x01_mipi_spec,
};

static int __init lcd_s6e8aa5x01_mipi_init(void)
{
	lcd_s6e8aa5x01_mipi.lcd_id = lcd_id_from_uboot;
	sprdfb_panel_register(&lcd_s6e8aa5x01_mipi);

	return 0;
}

subsys_initcall(lcd_s6e8aa5x01_mipi_init);

