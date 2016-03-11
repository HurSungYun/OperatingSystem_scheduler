/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _MDNIE_LITE_H_
#define _MDNIE_LITE_H_

typedef u8 mdnie_t;

#define MAX_DATA	150

enum mdnie_mode {
	MODE_DYNAMIC,
	MODE_STANDARD,
	MODE_NATURAL,
	MODE_MOVIE,
	MODE_AUTO,
	MODE_MAX,
};

enum mdnie_accessibility {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	GRAYSCALE,
	GRAYSCALE_NEGATIVE,
	ACCESSIBILITY_MAX
};

enum mdnie_scenario {
	SCENARIO_UI,
	SCENARIO_GALLERY,
	SCENARIO_VIDEO,
	SCENARIO_VTCALL,
	SCENARIO_CAMERA = 4,
	SCENARIO_BROWSER,
	SCENARIO_NEGATIVE,
	SCENARIO_EMAIL,
	SCENARIO_EBOOK,
	SCEANRIO_GRAY,
	SCENARIO_MAX,
};

enum mdnie_outdoor {
	OUTDOOR_OFF = 0,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

enum MDNIE_CMD {
	LEVEL1_KEY_UNLOCK,
	MDNIE_CMD1,
	LEVEL1_KEY_LOCK,
	MDNIE_CMD_MAX,
};

typedef struct lcm_init_code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
} lcm_init_code;

struct mdnie_command {
	lcm_init_code *sequence;
	unsigned int size;
	unsigned int sleep;
};

struct mdnie_table {
	char *name;
	struct mdnie_command tune[MDNIE_CMD_MAX];
};

#define MDNIE_SET(id)   \
{					\
	.name	= #id,			\
	.tune	= {			\
		{.sequence = LEVEL1_UNLOCK, .size = ARRAY_SIZE(LEVEL1_UNLOCK), .sleep = 0,}, \
		{.sequence = id##_1, .size = ARRAY_SIZE(id##_1), .sleep = 0,}, \
		{.sequence = LEVEL1_LOCK, .size = ARRAY_SIZE(LEVEL1_LOCK), .sleep = 0,}, \
	}	\
}

struct mdnie_ops {
	int (*write)(void *data, struct mdnie_command *seq, u32 len);
	int (*read)(void *data, u8 addr, mdnie_t *buf, u32 len);
	int (*color)(void *data, u32 *co_ord);
};

typedef int (*mdnie_w)(void *devdata, struct mdnie_command *seq, u32 len);
typedef int (*mdnie_r)(void *devdata, u8 addr, u8 *buf, u32 len);
typedef int (*mdnie_c)(void *devdata, u32 *co_ord);

struct mdnie_lite_device {
	enum mdnie_mode 		mode;
	enum mdnie_scenario		scenario;
	enum mdnie_outdoor	 	outdoor;
	enum mdnie_accessibility 	accessibility;
	struct device		*dev;
	bool 			tuning;
	unsigned int 		enable;
	struct mdnie_ops	ops;
	struct notifier_block	fb_notif;
	struct mutex		dev_lock;
	struct mutex		lock;
	char 			path[50];
	unsigned int co_ordinates[2];
	unsigned int color_correction
};

#endif /*_MDNIE_LITE_H_*/
