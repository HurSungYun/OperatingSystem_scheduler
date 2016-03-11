/* drivers/video/sprdfb/lcd/s6e8aa5x01_dimming.c
 *
 * MIPI-DSI based s6e8aa5x01 AMOLED panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include "s6e8aa5x01_dimming.h"

/*#define SMART_DIMMING_DEBUG*/
#define RGB_COMPENSATION 30

static unsigned int ref_gamma[NUM_VREF][CI_MAX] = {
	{0x00, 0x00, 0x00},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

static const unsigned short ref_cd_tbl[MAX_GAMMA_CNT] = {
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 118, 127, 135, 142, 151,
	160, 172, 180, 192, 192, 192, 192, 192, 192, 192,
	192, 204, 215, 228, 240, 255, 271, 286, 303, 318,
	336, 360
};

static const int gradation_shift[MAX_GAMMA_CNT][NUM_VREF] = {
/*V0 V3 V11 V23 V35 V51 V87 V151 V203 V255*/
	{0, 5, 9, 7, 3, 3, 2, 2, 3, 0},
	{0, 5, 9, 6, 3, 3, 2, 1, 2, 0},
	{0, 5, 9, 6, 3, 3, 2, 2, 3, 0},
	{0, 5, 8, 5, 3, 2, 1, 2, 3, 0},
	{0, 5, 7, 5, 2, 2, 1, 2, 3, 0},
	{0, 5, 7, 5, 2, 2, 1, 1, 2, 0},
	{0, 5, 7, 4, 2, 1, 1, 1, 2, 0},
	{0, 5, 6, 4, 2, 2, 1, 2, 3, 0},
	{0, 5, 6, 4, 2, 1, 1, 1, 2, 0},
	{0, 5, 5, 4, 2, 2, 1, 2, 3, 0},
	{0, 5, 5, 3, 2, 1, 1, 1, 2, 0},
	{0, 5, 4, 3, 1, 1, 1, 1, 2, 0},
	{0, 5, 4, 3, 1, 1, 1, 2, 3, 0},
	{0, 5, 4, 3, 1, 1, 1, 1, 2, 0},
	{0, 5, 4, 3, 1, 1, 1, 2, 3, 0},
	{0, 5, 4, 3, 1, 1, 1, 2, 3, 0},
	{0, 5, 4, 3, 2, 1, 1, 2, 3, 0},
	{0, 5, 4, 3, 2, 2, 1, 2, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 2, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 5, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 4, 4, 2, 2, 2, 3, 3, 0},
	{0, 5, 4, 3, 2, 2, 2, 3, 3, 0},
	{0, 5, 4, 3, 2, 2, 2, 3, 3, 0},
	{0, 4, 4, 3, 2, 2, 2, 3, 3, 0},
	{0, 4, 3, 3, 2, 2, 2, 3, 3, 0},
	{0, 4, 3, 3, 2, 2, 2, 3, 3, 0},
	{0, 4, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 4, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 3, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 2, 2, 2, 2, 2, 3, 3, 0},
	{0, 3, 2, 2, 2, 2, 3, 3, 3, 0},
	{0, 3, 2, 2, 2, 2, 4, 3, 3, 0},
	{0, 3, 2, 2, 2, 2, 4, 3, 3, 0},
	{0, 3, 2, 2, 2, 2, 4, 3, 3, 0},
	{0, 3, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 3, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 3, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 3, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 3, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 2, 2, 2, 2, 3, 4, 3, 2, 0},
	{0, 2, 2, 2, 2, 2, 4, 3, 2, 0},
	{0, 2, 2, 2, 2, 2, 3, 3, 2, 0},
	{0, 2, 2, 2, 2, 2, 3, 3, 2, 0},
	{0, 2, 2, 2, 2, 2, 3, 3, 1, 0},
	{0, 2, 2, 2, 2, 2, 3, 3, 1, 0},
	{0, 2, 2, 2, 2, 2, 3, 3, 1, 0},
	{0, 2, 1, 2, 2, 2, 3, 3, 1, 0},
	{0, 2, 1, 2, 2, 2, 3, 3, 1, 0},
	{0, 2, 1, 2, 2, 2, 3, 3, 1, 0},
	{0, 1, 0, 2, 2, 2, 3, 3, 1, 0},
	{0, 1, 0, 1, 2, 2, 2, 2, 1, 0},
	{0, 0, 0, 1, 2, 2, 2, 1, 1, 0},
	{0, 0, 0, 1, 1, 2, 2, 1, 0, 0},
	{0, 0, 0, 1, 1, 2, 1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset[MAX_GAMMA_CNT][RGB_COMPENSATION] = {
	/* R0 R3 R11 R23 R35 R51 R87 R151 R203 R255(RGB)*/
	{0, 0, 0, 0, 0, 0, 0, 0, 1, -14, 0, -2, -6, 0, -3, -5, 0, 4, -3, 0, 0, -1, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 0, 1, -13, 0, -4, -6, 0, -3, -4, 0, 4, -2, 0, 0, -1, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -6, 0, 0, -13, 0, -3, -5, 0, -3, -4, 0, 3, -3, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -8, 0, 0, -12, 0, -4, -5, 0, -4, -4, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -10, 0, 1, -10, 0, -2, -5, 0, -5, -3, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -12, 0, -1, -8, 0, -2, -4, 0, -3, -4, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -13, 0, -1, -9, 0, -3, -4, 0, -3, -3, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -13, 0, -1, -9, 0, -2, -3, 0, -5, -2, 0, 3, -1, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -11, 0, -1, -7, 0, -1, -3, 0, -5, -3, 0, 2, -1, 0, 1, 0, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -11, 0, -1, -5, 0, -2, -2, 0, -5, -2, 0, 3, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -11, 0, -2, -7, 0, -3, -1, 0, -4, -2, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -13, 0, -2, -5, 0, -3, -1, 0, -4, -1, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -11, 0, 0, -5, 0, -3, -1, 0, -4, -2, 0, 2, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, 1, -5, 0, -4, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -8, 0, 2, -5, 0, -3, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -7, 0, 0, -6, 0, -5, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -7, 1, -3, -6, 0, -4, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -7, 1, -3, -6, 0, -6, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, -4, -5, 0, -5, 0, 0, -5, -3, 0, 1, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, -7, -4, 0, -5, 0, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -9, 0, -6, -3, 0, -5, 1, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -9, 0, -6, -3, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -9, 0, -7, -2, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 0, -4, -2, 0, -5, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -7, 0, -2, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, -1, -2, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 0, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 1, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 3, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 4, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 0, 0, 0, 0, -4, 3, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, -4, 2, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -3, 1, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, -3, 1, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, 1, 0, 0, -4, 1, 0, -4, 0, 0, 3, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -4, 3, 0, -4, -1, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -4, 3, 0, -3, 0, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 0, 0, 0, 0, -4, 1, 0, -4, 0, 0, 2, -1, 0, -1, 0, 0, 1, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, -4, 1, 0, -3, 0, 0, 2, -1, 0, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, 1, 0, -1, 0, 0, -3, 0, 0, 0, -1, 0, 0, -1, 0, 1, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

const unsigned int vref_index[NUM_VREF] = {
	TBL_INDEX_V0,
	TBL_INDEX_V3,
	TBL_INDEX_V11,
	TBL_INDEX_V23,
	TBL_INDEX_V35,
	TBL_INDEX_V51,
	TBL_INDEX_V87,
	TBL_INDEX_V151,
	TBL_INDEX_V203,
	TBL_INDEX_V255,
};

const int vreg_element_max[NUM_VREF] = {
	0x0f,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0x1ff,
};

const struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,		.de = 860},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 72,	.de = 860},
};

const short vt_trans_volt[16] = {
	5939, 5856, 5773, 5691, 5608, 5525, 5442, 5359,
	5276, 5193, 4986, 4917, 4848, 4779, 4710, 4655
};

const short v255_trans_volt[512] = {
	5442, 5435, 5428, 5421, 5414, 5407, 5401, 5394,
	5387, 5380, 5373, 5366, 5359, 5352, 5345, 5338,
	5331, 5325, 5318, 5311, 5304, 5297, 5290, 5283,
	5276, 5269, 5262, 5256, 5249, 5242, 5235, 5228,
	5221, 5214, 5207, 5200, 5193, 5186, 5180, 5173,
	5166, 5159, 5152, 5145, 5138, 5131, 5124, 5117,
	5110, 5104, 5097, 5090, 5083, 5076, 5069, 5062,
	5055, 5048, 5041, 5035, 5028, 5021, 5014, 5007,
	5000, 4993, 4986, 4979, 4972, 4965, 4959, 4952,
	4945, 4938, 4931, 4924, 4917, 4910, 4903, 4896,
	4889, 4883, 4876, 4869, 4862, 4855, 4848, 4841,
	4834, 4827, 4820, 4814, 4807, 4800, 4793, 4786,
	4779, 4772, 4765, 4758, 4751, 4744, 4738, 4731,
	4724, 4717, 4710, 4703, 4696, 4689, 4682, 4675,
	4668, 4662, 4655, 4648, 4641, 4634, 4627, 4620,
	4613, 4606, 4599, 4593, 4586, 4579, 4572, 4565,
	4558, 4551, 4544, 4537, 4530, 4523, 4517, 4510,
	4503, 4496, 4489, 4482, 4475, 4468, 4461, 4454,
	4447, 4441, 4434, 4427, 4420, 4413, 4406, 4399,
	4392, 4385, 4378, 4372, 4365, 4358, 4351, 4344,
	4337, 4330, 4323, 4316, 4309, 4302, 4296, 4289,
	4282, 4275, 4268, 4261, 4254, 4247, 4240, 4233,
	4227, 4220, 4213, 4206, 4199, 4192, 4185, 4178,
	4171, 4164, 4157, 4151, 4144, 4137, 4130, 4123,
	4116, 4109, 4102, 4095, 4088, 4081, 4075, 4068,
	4061, 4054, 4047, 4040, 4033, 4026, 4019, 4012,
	4006, 3999, 3992, 3985, 3978, 3971, 3964, 3957,
	3950, 3943, 3936, 3930, 3923, 3916, 3909, 3902,
	3895, 3888, 3881, 3874, 3867, 3860, 3854, 3847,
	3840, 3833, 3826, 3819, 3812, 3805, 3798, 3791,
	3785, 3778, 3771, 3764, 3757, 3750, 3743, 3736,
	3729, 3722, 3715, 3709, 3702, 3695, 3688, 3681,
	3674, 3667, 3660, 3653, 3646, 3639, 3633, 3626,
	3619, 3612, 3605, 3598, 3591, 3584, 3577, 3570,
	3564, 3557, 3550, 3543, 3536, 3529, 3522, 3515,
	3508, 3501, 3494, 3488, 3481, 3474, 3467, 3460,
	3453, 3446, 3439, 3432, 3425, 3418, 3412, 3405,
	3398, 3391, 3384, 3377, 3370, 3363, 3356, 3349,
	3343, 3336, 3329, 3322, 3315, 3308, 3301, 3294,
	3287, 3280, 3273, 3267, 3260, 3253, 3246, 3239,
	3232, 3225, 3218, 3211, 3204, 3197, 3191, 3184,
	3177, 3170, 3163, 3156, 3149, 3142, 3135, 3128,
	3122, 3115, 3108, 3101, 3094, 3087, 3080, 3073,
	3066, 3059, 3052, 3046, 3039, 3032, 3025, 3018,
	3011, 3004, 2997, 2990, 2983, 2977, 2970, 2963,
	2956, 2949, 2942, 2935, 2928, 2921, 2914, 2907,
	2901, 2894, 2887, 2880, 2873, 2866, 2859, 2852,
	2845, 2838, 2831, 2825, 2818, 2811, 2804, 2797,
	2790, 2783, 2776, 2769, 2762, 2756, 2749, 2742,
	2735, 2728, 2721, 2714, 2707, 2700, 2693, 2686,
	2680, 2673, 2666, 2659, 2652, 2645, 2638, 2631,
	2624, 2617, 2610, 2604, 2597, 2590, 2583, 2576,
	2569, 2562, 2555, 2548, 2541, 2535, 2528, 2521,
	2514, 2507, 2500, 2493, 2486, 2479, 2472, 2465,
	2459, 2452, 2445, 2438, 2431, 2424, 2417, 2410,
	2403, 2396, 2389, 2383, 2376, 2369, 2362, 2355,
	2348, 2341, 2334, 2327, 2320, 2314, 2307, 2300,
	2293, 2286, 2279, 2272, 2265, 2258, 2251, 2244,
	2238, 2231, 2224, 2217, 2210, 2203, 2196, 2189,
	2182, 2175, 2168, 2162, 2155, 2148, 2141, 2134,
	2127, 2120, 2113, 2106, 2099, 2093, 2086, 2079,
	2072, 2065, 2058, 2051, 2044, 2037, 2030, 2023,
	2017, 2010, 2003, 1996, 1989, 1982, 1975, 1968,
	1961, 1954, 1948, 1941, 1934, 1927, 1920, 1913,
};

const short v3_v203_trans_volt[256] = {
	205, 208, 211, 214, 218, 221, 224, 227,
	230, 234, 237, 240, 243, 246, 250, 253,
	256, 259, 262, 266, 269, 272, 275, 278,
	282, 285, 288, 291, 294, 298, 301, 304,
	307, 310, 314, 317, 320, 323, 326, 330,
	333, 336, 339, 342, 346, 349, 352, 355,
	358, 362, 365, 368, 371, 374, 378, 381,
	384, 387, 390, 394, 397, 400, 403, 406,
	410, 413, 416, 419, 422, 426, 429, 432,
	435, 438, 442, 445, 448, 451, 454, 458,
	461, 464, 467, 470, 474, 477, 480, 483,
	486, 490, 493, 496, 499, 502, 506, 509,
	512, 515, 518, 522, 525, 528, 531, 534,
	538, 541, 544, 547, 550, 554, 557, 560,
	563, 566, 570, 573, 576, 579, 582, 586,
	589, 592, 595, 598, 602, 605, 608, 611,
	614, 618, 621, 624, 627, 630, 634, 637,
	640, 643, 646, 650, 653, 656, 659, 662,
	666, 669, 672, 675, 678, 682, 685, 688,
	691, 694, 698, 701, 704, 707, 710, 714,
	717, 720, 723, 726, 730, 733, 736, 739,
	742, 746, 749, 752, 755, 758, 762, 765,
	768, 771, 774, 778, 781, 784, 787, 790,
	794, 797, 800, 803, 806, 810, 813, 816,
	819, 822, 826, 829, 832, 835, 838, 842,
	845, 848, 851, 854, 858, 861, 864, 867,
	870, 874, 877, 880, 883, 886, 890, 893,
	896, 899, 902, 906, 909, 912, 915, 918,
	922, 925, 928, 931, 934, 938, 941, 944,
	947, 950, 954, 957, 960, 963, 966, 970,
	973, 976, 979, 982, 986, 989, 992, 995,
	998, 1002, 1005, 1008, 1011, 1014, 1018, 1021,
};

const short int_tbl_v0_v3[2] = {
	341, 683,
};

const short int_tbl_v3_v11[7] = {
	128, 256, 384, 512, 640, 768, 896,
};

const short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};

const short int_tbl_v51_v87[35] = {
	28, 57, 85, 114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};

const short int_tbl_v87_v151[63] = {
	16, 32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};

const short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const int gamma_tbl[256] = {
	0, 16, 65, 145, 258, 403, 581, 790,
	1032, 1306, 1613, 1951, 2322, 2725, 3161, 3628,
	4128, 4660, 5225, 5821, 6450, 7111, 7805, 8531,
	9288, 10079, 10901, 11756, 12643, 13562, 14513, 15497,
	16513, 17561, 18641, 19754, 20899, 22076, 23286, 24527,
	25801, 27107, 28446, 29816, 31219, 32655, 34122, 35622,
	37154, 38718, 40314, 41943, 43604, 45297, 47023, 48780,
	50570, 52393, 54247, 56134, 58053, 60004, 61987, 64003,
	66051, 68131, 70244, 72388, 74565, 76775, 79016, 81290,
	83596, 85934, 88305, 90707, 93142, 95609, 98109, 100641,
	103205, 105801, 108429, 111090, 113783, 116508, 119266, 122056,
	124878, 127732, 130618, 133537, 136488, 139471, 142487, 145535,
	148615, 151727, 154872, 158048, 161257, 164499, 167772, 171078,
	174416, 177786, 181189, 184624, 188091, 191590, 195121, 198685,
	202281, 205910, 209570, 213263, 216988, 220745, 224535, 228357,
	232211, 236097, 240015, 243966, 247949, 251965, 256012, 260092,
	264204, 268348, 272525, 276734, 280975, 285248, 289554, 293892,
	298262, 302664, 307099, 311565, 316064, 320596, 325159, 329755,
	334383, 339044, 343736, 348461, 353218, 358007, 362829, 367683,
	372569, 377487, 382438, 387421, 392436, 397483, 402563, 407675,
	412819, 417995, 423204, 428445, 433718, 439023, 444361, 449731,
	455133, 460567, 466034, 471533, 477064, 482627, 488223, 493851,
	499511, 505203, 510928, 516685, 522474, 528295, 534149, 540035,
	545953, 551903, 557886, 563901, 569948, 576027, 582139, 588283,
	594459, 600668, 606908, 613181, 619486, 625824, 632193, 638595,
	645029, 651496, 657995, 664525, 671089, 677684, 684312, 690972,
	697664, 704388, 711145, 717934, 724755, 731609, 738494, 745412,
	752362, 759345, 766359, 773406, 780486, 787597, 794741, 801917,
	809125, 816365, 823638, 830943, 838280, 845650, 853051, 860485,
	867952, 875450, 882981, 890544, 898139, 905766, 913426, 921118,
	928842, 936599, 944388, 952209, 960062, 967947, 975865, 983815,
	991797, 999812, 1007859, 1015938, 1024049, 1032192, 1040368, 1048576
};

const int gamma_multi_tbl[256] = {
	0, 6, 23, 51, 91, 142, 204, 278,
	363, 459, 567, 686, 816, 958, 1111, 1276,
	1451, 1638, 1837, 2047, 2268, 2500, 2744, 2999,
	3265, 3543, 3832, 4133, 4445, 4768, 5102, 5448,
	5805, 6174, 6554, 6945, 7347, 7761, 8186, 8623,
	9071, 9530, 10000, 10482, 10976, 11480, 11996, 12523,
	13062, 13612, 14173, 14746, 15330, 15925, 16531, 17149,
	17779, 18419, 19071, 19734, 20409, 21095, 21792, 22501,
	23221, 23952, 24695, 25449, 26214, 26991, 27779, 28578,
	29389, 30211, 31045, 31889, 32745, 33613, 34491, 35382,
	36283, 37196, 38120, 39055, 40002, 40960, 41929, 42910,
	43902, 44906, 45921, 46947, 47984, 49033, 50093, 51165,
	52247, 53342, 54447, 55564, 56692, 57832, 58982, 60145,
	61318, 62503, 63699, 64907, 66126, 67356, 68597, 69850,
	71114, 72390, 73677, 74975, 76285, 77606, 78938, 80282,
	81637, 83003, 84380, 85769, 87170, 88581, 90004, 91439,
	92884, 94341, 95810, 97289, 98780, 100283, 101796, 103321,
	104858, 106405, 107964, 109535, 111116, 112709, 114314, 115930,
	117557, 119195, 120845, 122506, 124178, 125862, 127557, 129264,
	130981, 132710, 134451, 136203, 137966, 139740, 141526, 143323,
	145132, 146951, 148783, 150625, 152479, 154344, 156221, 158108,
	160008, 161918, 163840, 165773, 167718, 169674, 171641, 173619,
	175609, 177610, 179623, 181647, 183682, 185729, 187787, 189856,
	191937, 194029, 196132, 198246, 200372, 202510, 204658, 206818,
	208990, 211172, 213366, 215571, 217788, 220016, 222255, 224506,
	226768, 229042, 231326, 233622, 235930, 238248, 240578, 242920,
	245272, 247637, 250012, 252399, 254797, 257206, 259627, 262059,
	264502, 266957, 269423, 271901, 274389, 276890, 279401, 281924,
	284458, 287003, 289560, 292128, 294708, 297299, 299901, 302514,
	305139, 307775, 310423, 313082, 315752, 318434, 321126, 323831,
	326546, 329273, 332011, 334761, 337522, 340294, 343078, 345872,
	348679, 351496, 354325, 357166, 360017, 362880, 365754, 368640
};

const unsigned char lookup_tbl[361] = {
	0, 13, 19, 23, 27, 30, 33, 36, 38, 40, 42,
	45, 47, 48, 50, 52, 54, 55, 57, 59, 60,
	62, 63, 64, 66, 67, 69, 70, 71, 72, 74,
	75, 76, 77, 78, 80, 81, 82, 83, 84, 85,
	86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 101, 102, 103, 104,
	105, 106, 107, 108, 108, 109, 110, 111, 112, 112,
	113, 114, 115, 116, 116, 117, 118, 119, 119, 120,
	121, 122, 122, 123, 124, 125, 125, 126, 127, 127,
	128, 129, 130, 130, 131, 132, 132, 133, 134, 134,
	135, 136, 136, 137, 138, 138, 139, 140, 140, 141,
	142, 142, 143, 143, 144, 145, 145, 146, 147, 147,
	148, 148, 149, 150, 150, 151, 151, 152, 153, 153,
	154, 154, 155, 156, 156, 157, 157, 158, 158, 159,
	160, 160, 161, 161, 162, 162, 163, 163, 164, 165,
	165, 166, 166, 167, 167, 168, 168, 169, 169, 170,
	171, 171, 172, 172, 173, 173, 174, 174, 175, 175,
	176, 176, 177, 177, 178, 178, 179, 179, 180, 180,
	181, 181, 182, 182, 183, 183, 184, 184, 185, 185,
	186, 186, 187, 187, 188, 188, 189, 189, 190, 190,
	191, 191, 191, 192, 192, 193, 193, 194, 194, 195,
	195, 196, 196, 197, 197, 198, 198, 198, 199, 199,
	200, 200, 201, 201, 202, 202, 202, 203, 203, 204,
	204, 205, 205, 206, 206, 206, 207, 207, 208, 208,
	209, 209, 210, 210, 210, 211, 211, 212, 212, 212,
	213, 213, 214, 214, 215, 215, 215, 216, 216, 217,
	217, 218, 218, 218, 219, 219, 220, 220, 220, 221,
	221, 222, 222, 222, 223, 223, 224, 224, 224, 225,
	225, 226, 226, 226, 227, 227, 228, 228, 228, 229,
	229, 230, 230, 230, 231, 231, 232, 232, 232, 233,
	233, 234, 234, 234, 235, 235, 235, 236, 236, 237,
	237, 237, 238, 238, 239, 239, 239, 240, 240, 240,
	241, 241, 242, 242, 242, 243, 243, 243, 244, 244,
	245, 245, 245, 246, 246, 246, 247, 247, 247, 248,
	248, 249, 249, 249, 250, 250, 250, 251, 251, 251,
	252, 252, 253, 253, 253, 254, 254, 254, 255, 255
};

static int calc_vt_volt(int gamma)
{
	int max;

	max = sizeof(vt_trans_volt) >> 2;
	if (gamma > max) {
		pr_warn("%s: exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct smart_dimming *dimming, int color)
{
	return MULTIPLE_VREGOUT;
}

static int calc_v3_volt(struct smart_dimming *dimming, int color)
{
	int ret, v11, gamma;

	gamma = dimming->gamma[V3][color];

	if (gamma > vreg_element_max[V3]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V3];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (MULTIPLE_VREGOUT << 10) -
		((MULTIPLE_VREGOUT - v11) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v11_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v23, gamma;

	gamma = dimming->gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (vt << 10) - ((vt - v23) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v23_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v35, gamma;

	gamma = dimming->gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (vt << 10) - ((vt - v35) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v35_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v51, gamma;

	gamma = dimming->gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (vt << 10) - ((vt - v51) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v51_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v87, gamma;

	gamma = dimming->gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (vt << 10) - ((vt - v87) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v87_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v151, gamma;

	gamma = dimming->gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (vt << 10) -
		((vt - v151) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v151_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v203, gamma;

	gamma = dimming->gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (vt << 10) - ((vt - v203) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v203_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v255, gamma;

	gamma = dimming->gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (vt << 10) - ((vt - v255) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v255_volt(struct smart_dimming *dimming, int color)
{
	int ret, gamma;

	gamma = dimming->gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v0_v3(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v0, v3, ratio;

	ratio = (int)int_tbl_v0_v3[gray];

	v0 = dimming->volt[TBL_INDEX_V0][color];
	v3 = dimming->volt[TBL_INDEX_V3][color];

	ret = (v0 << 10) - ((v0 - v3) * ratio);

	return ret >> 10;
}

static int calc_inter_v3_v11(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v3, v11, ratio;

	ratio = (int)int_tbl_v3_v11[gray];
	v3 = dimming->volt[TBL_INDEX_V3][color];
	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (v3 << 10) - ((v3 - v11) * ratio);

	return ret >> 10;
}

static int calc_inter_v11_v23(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = dimming->volt[TBL_INDEX_V11][color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (v11 << 10) - ((v11 - v23) * ratio);

	return ret >> 10;
}

static int calc_inter_v23_v35(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = dimming->volt[TBL_INDEX_V23][color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (v23 << 10) - ((v23 - v35) * ratio);

	return ret >> 10;
}

static int calc_inter_v35_v51(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = dimming->volt[TBL_INDEX_V35][color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (v35 << 10) - ((v35 - v51) * ratio);

	return ret >> 10;
}

static int calc_inter_v51_v87(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = dimming->volt[TBL_INDEX_V51][color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (v51 << 10) - ((v51 - v87) * ratio);

	return ret >> 10;
}

static int calc_inter_v87_v151(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = dimming->volt[TBL_INDEX_V87][color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (v87 << 10) - ((v87 - v151) * ratio);

	return ret >> 10;
}

static int calc_inter_v151_v203(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = dimming->volt[TBL_INDEX_V151][color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (v151 << 10) - ((v151 - v203) * ratio);

	return ret >> 10;
}

static int calc_inter_v203_v255(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = dimming->volt[TBL_INDEX_V203][color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (v203 << 10) - ((v203 - v255) * ratio);

	return ret >> 10;
}

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp;
	u8 pos = 0;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((data[pos] & 0x01) ? -1 : 1) * data[pos+1];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
	pr_info("%s:MTP OFFSET\n", __func__);
	for (i = V0; i <= V255; i++)
		pr_info("%d %d %d\n", dimming->mtp[i][0],
				dimming->mtp[i][1], dimming->mtp[i][2]);

	pr_debug("MTP+ Center gamma\n");
	for (i = V0; i <= V255; i++)
		pr_debug("%d %d %d\n", dimming->gamma[i][0],
			dimming->gamma[i][1], dimming->gamma[i][2]);
}

int panel_generate_volt_tbl(struct smart_dimming *dimming)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;
	int calc_seq[NUM_VREF] = {
		V0, V255, V203, V151, V87, V51, V35, V23, V11, V3};
	int (*calc_volt_point[NUM_VREF])(struct smart_dimming *, int) = {
		calc_v0_volt,
		calc_v3_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct smart_dimming *, int, int)  = {
		NULL,
		calc_inter_v0_v3,
		calc_inter_v3_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};
#ifdef SMART_DIMMING_DEBUG
	long temp[CI_MAX];
#endif
	for (i = 0; i < CI_MAX; i++)
		dimming->volt_vt[i] =
			calc_vt_volt(dimming->gamma[VT][i]);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				dimming->volt[index][i] =
					calc_volt_point[seq](dimming, i);
		}
	}

	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL)
				dimming->volt[i][j] =
				calc_inter_volt[index](dimming, gray, j);
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("============= VT Voltage ===============\n");
	for (i = 0; i < CI_MAX; i++)
		temp[i] = dimming->volt_vt[i] >> 10;

	pr_info("R : %d : %ld G : %d : %ld B : %d : %ld.\n",
				dimming->gamma[VT][0], temp[0],
				dimming->gamma[VT][1], temp[1],
				dimming->gamma[VT][2], temp[2]);

	pr_info("=================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		for (j = 0; j < CI_MAX; j++)
			temp[j] = dimming->volt[i][j] >> 10;

		pr_info("V%d R : %d : %ld G : %d : %ld B : %d : %ld\n", i,
					dimming->volt[i][0], temp[0],
					dimming->volt[i][1], temp[1],
					dimming->volt[i][2], temp[2]);
	}
#endif
	return ret;
}


static int lookup_volt_index(struct smart_dimming *dimming, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)lookup_tbl[temp];
#ifdef SMART_DIMMING_DEBUG
	pr_info("============== look up index ================\n");
	pr_info("gray : %d : %d, index : %d\n", gray, temp, index);
#endif
	exit = 1;
	i = 0;
	while (exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_GAMMA)
			index_h = MAX_GAMMA;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h)
			exit = 0;
		i++;
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("base index : %d, cnt : %d\n",
			lookup_tbl[index_l], cnt_l + cnt_h);
#endif
	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;
	temp = gamma_multi_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
	pr_info("temp : %d, gray : %d, p_delta : %d\n", temp, gray, p_delta);
#endif
	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
		pr_info("temp : %d, gray : %d, delta : %d\n",
				temp, gray, delta);
#endif
		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("ret : %d\n", ret);
#endif
	return ret;
}

static int calc_reg_v3(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V3][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V11][color] - MULTIPLE_VREGOUT;

	ret = ((t1 * fix_const[V3].de) / t2) - fix_const[V3].nu;

	return ret;
}

static int calc_reg_v11(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V11][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V23][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V11].de)) / t2) - fix_const[V11].nu;

	return ret;
}

static int calc_reg_v23(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V23][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V35][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V23].de)) / t2) - fix_const[V23].nu;

	return ret;
}

static int calc_reg_v35(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V35][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V51][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V35].de)) / t2) - fix_const[V35].nu;

	return ret;
}

static int calc_reg_v51(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V51][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V87][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V51].de)) / t2) - fix_const[V51].nu;

	return ret;
}

static int calc_reg_v87(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V87][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V151][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V87].de)) / t2) - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V151][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V203][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V151].de)) / t2) - fix_const[V151].nu;

	return ret;
}

static int calc_reg_v203(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V203][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V255][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V203].de)) / t2) - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1;

	t1 = MULTIPLE_VREGOUT - dimming->look_volt[V255][color];

	ret = ((t1 * fix_const[V255].de) / MULTIPLE_VREGOUT) -
			fix_const[V255].nu;

	return ret;
}

int panel_get_gamma(struct smart_dimming *dimming,
				int index_br, unsigned char *result)
{
	int i, j;
	int ret = 0;
	int gray, index, shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br;
	int *color_shift_table = NULL;
	int (*calc_reg[NUM_VREF])(struct smart_dimming *, int)  = {
		NULL,
		calc_reg_v3,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = ref_cd_tbl[index_br];

	if (br > MAX_GAMMA)
		br = MAX_GAMMA;

	for (i = V3; i < NUM_VREF; i++) {
		/* get reference shift value */
		shift = gradation_shift[index_br][i];
		gray = gamma_tbl[vref_index[i]] * br;
		index = lookup_volt_index(dimming, gray);
		index = index + shift;
#ifdef SMART_DIMMING_DEBUG
		pr_info("index : %d\n", index);
#endif
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				dimming->look_volt[i][j] =
					dimming->volt[index][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("volt : %d : %d\n",
					dimming->look_volt[i][j],
					dimming->look_volt[i][j] >> 10);
#endif
			}
		}
	}

	for (i = V3; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
				color_shift_table = rgb_offset[index_br];
				c_shift = color_shift_table[index];
				gamma_int[i][j] =
					(calc_reg[i](dimming, j) + c_shift) -
					dimming->mtp[i][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("gamma : %d, shift : %d\n",
						gamma_int[i][j], c_shift);
#endif
				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;

			}
		}
	}

	for (j = 0; j < CI_MAX; j++)
		gamma_int[VT][j] = dimming->gamma[VT][j] - dimming->mtp[VT][j];

	index = 0;

	for (i = V255; i >= VT; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] =
					gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else {
				result[index++] =
					(unsigned char)gamma_int[i][j];
			}
		}
	}

	return ret;
}

