/*
 * Android R&D Group 3.
 *
 * drivers/gpio/gpio_dvs/sc7727_gpio_dvs.c - Read GPIO info. from SC7715
 *
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <asm/io.h>
#include <mach/gpio.h>
#include <soc/sprd/pinmap.h>

/********************* Fixed Code Area !***************************/
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>
#define GET_RESULT_GPIO(a, b, c)    \
    (((a)<<10 & 0xFC00) |((b)<<4 & 0x03F0) | ((c) & 0x000F))

#define GET_GPIO_IO(value)  \
    (unsigned char)((0xFC00 & (value)) >> 10)
#define GET_GPIO_PUPD(value)    \
    (unsigned char)((0x03F0 & (value)) >> 4)
#define GET_GPIO_LH(value)  \
    (unsigned char)(0x000F & (value))
/****************************************************************/

typedef struct {
    unsigned int gpio_num;
    unsigned int ctrl_offset;
} gpio_ctrl_info;

static gpio_ctrl_info available_gpios_sc7727[] = {
	{ 10, REG_PIN_U0TXD },
	{ 11, REG_PIN_U0RXD },
	{ 12, REG_PIN_U0CTS },
	{ 13, REG_PIN_U0RTS },
	{ 14, REG_PIN_U1TXD },
	{ 15, REG_PIN_U1RXD },
	{ 16, REG_PIN_U2TXD },
	{ 17, REG_PIN_U2RXD },
	{ 18, REG_PIN_U3TXD },
	{ 19, REG_PIN_U3RXD },
	{ 20, REG_PIN_U3CTS },
	{ 21, REG_PIN_U3RTS },
	{ 27, REG_PIN_CP2_RFCTL0 },
	{ 28, REG_PIN_CP2_RFCTL1 },
	{ 29, REG_PIN_CP2_RFCTL2 },
	{ 30, REG_PIN_RFSDA0 },
	{ 31, REG_PIN_RFSCK0 },
	{ 32, REG_PIN_RFSEN0 },
	{ 44, REG_PIN_CP1_RFCTL8 },
	{ 45, REG_PIN_CP1_RFCTL9 },
	{ 46, REG_PIN_CP1_RFCTL10 },
	{ 47, REG_PIN_CP1_RFCTL11 },
	{ 48, REG_PIN_CP1_RFCTL12 },
	{ 49, REG_PIN_CP1_RFCTL13 },
	{ 50, REG_PIN_CP1_RFCTL14 },
	{ 51, REG_PIN_CP1_RFCTL15 },
	{ 52, REG_PIN_CP0_RFCTL0 },
	{ 53, REG_PIN_CP0_RFCTL1 },
	{ 54, REG_PIN_CP0_RFCTL2 },
	{ 55, REG_PIN_CP0_RFCTL3 },
	{ 56, REG_PIN_CP0_RFCTL4 },
	{ 57, REG_PIN_CP0_RFCTL5 },
	{ 58, REG_PIN_CP0_RFCTL6 },
	{ 59, REG_PIN_CP0_RFCTL7 },
	{ 60, REG_PIN_XTLEN },
	{ 65, REG_PIN_SCL3 },
	{ 66, REG_PIN_SDA3 },
	{ 67, REG_PIN_SPI0_CSN	},
	{ 68, REG_PIN_SPI0_DO },
	{ 69, REG_PIN_SPI0_DI },
	{ 70, REG_PIN_SPI0_CLK },
	{ 71, REG_PIN_EXTINT0 },
	{ 72, REG_PIN_EXTINT1 },
	{ 73, REG_PIN_SCL1	},
	{ 74, REG_PIN_SDA1 },
	{ 75, REG_PIN_SIMCLK0 },
	{ 76, REG_PIN_SIMDA0 },
	{ 77, REG_PIN_SIMRST0 },
	{ 78, REG_PIN_SIMCLK1 },
	{ 79, REG_PIN_SIMDA1 },
	{ 80, REG_PIN_SIMRST1 },
	{ 81, REG_PIN_SIMCLK2 },
	{ 82, REG_PIN_SIMDA2 },
	{ 83, REG_PIN_SIMRST2 },
	{ 84, REG_PIN_MEMS_MIC_CLK0 },
	{ 85, REG_PIN_MEMS_MIC_DATA0 },
	{ 86, REG_PIN_MEMS_MIC_CLK1 },
	{ 87, REG_PIN_MEMS_MIC_DATA1 },
	{ 88, REG_PIN_SD1_CLK },
	{ 89, REG_PIN_SD1_CMD },
	{ 90, REG_PIN_SD1_D0 },
	{ 91, REG_PIN_SD1_D1 },
	{ 92, REG_PIN_SD1_D2 },
	{ 93, REG_PIN_SD1_D3 },
	{ 94, REG_PIN_SD0_CLK0 },
	{ 95, REG_PIN_SD0_CLK1 },
	{ 96, REG_PIN_SD0_CMD },
	{ 97, REG_PIN_SD0_D0 },
	{ 98, REG_PIN_SD0_D1 },
	{ 99, REG_PIN_SD0_D2 },
	{ 100, REG_PIN_SD0_D3 },
	{ 101, REG_PIN_LCD_CSN1 },
	{ 102, REG_PIN_LCD_CSN0 },
	{ 103, REG_PIN_LCD_RSTN },
	{ 105, REG_PIN_LCD_FMARK },
	{ 136, REG_PIN_EMMC_CLK },
	{ 137, REG_PIN_EMMC_CMD },
	{ 138, REG_PIN_EMMC_D0 },
	{ 139, REG_PIN_EMMC_D1 },
	{ 140, REG_PIN_EMMC_D2 },
	{ 141, REG_PIN_EMMC_D3 },
	{ 142, REG_PIN_EMMC_D4 },
	{ 143, REG_PIN_EMMC_D5 },
	{ 144, REG_PIN_EMMC_D6 },
	{ 145, REG_PIN_EMMC_D7 },
	{ 146, REG_PIN_EMMC_RST },
	{ 147, REG_PIN_NFWPN },
	{ 148, REG_PIN_NFRB },
	{ 149, REG_PIN_NFCLE },
	{ 150, REG_PIN_NFALE },
	{ 151, REG_PIN_NFCEN0 },
	{ 152, REG_PIN_NFCEN1 },
	{ 153, REG_PIN_NFREN },
	{ 154, REG_PIN_NFWEN },
	{ 155, REG_PIN_NFD0 },
	{ 156, REG_PIN_NFD1 },
	{ 157, REG_PIN_NFD2 },
	{ 158, REG_PIN_NFD3 },
	{ 159, REG_PIN_NFD4 },
	{ 160, REG_PIN_NFD5 },
	{ 161, REG_PIN_NFD6 },
	{ 162, REG_PIN_NFD7 },
	{ 163, REG_PIN_NFD8 },
	{ 164, REG_PIN_NFD9 },
	{ 165, REG_PIN_NFD10 },
	{ 166, REG_PIN_NFD11 },
	{ 167, REG_PIN_NFD12 },
	{ 168, REG_PIN_NFD13 },
	{ 169, REG_PIN_NFD14 },
	{ 170, REG_PIN_NFD15 },
	{ 171, REG_PIN_CCIRCK0 },
	{ 172, REG_PIN_CCIRCK1 },
	{ 173, REG_PIN_CCIRMCLK },
	{ 174, REG_PIN_CCIRHS },
	{ 175, REG_PIN_CCIRVS },
	{ 178, REG_PIN_CCIRD2 },
	{ 179, REG_PIN_CCIRD3 },
	{ 180, REG_PIN_CCIRD4 },
	{ 181, REG_PIN_CCIRD5 },
	{ 182, REG_PIN_CCIRD6 },
	{ 183, REG_PIN_CCIRD7 },
	{ 184, REG_PIN_CCIRD8 },
	{ 185, REG_PIN_CCIRD9 },
	{ 186, REG_PIN_CCIRRST },
	{ 187, REG_PIN_CCIRPD1 },
	{ 188, REG_PIN_CCIRPD0 },
	{ 189, REG_PIN_SCL0 },
	{ 190, REG_PIN_SDA0 },
	{ 191, REG_PIN_KEYOUT0 },
	{ 192, REG_PIN_KEYOUT1 },
	{ 193, REG_PIN_KEYOUT2 },
	{ 199, REG_PIN_KEYIN0 },
	{ 200, REG_PIN_KEYIN1 },
	{ 201, REG_PIN_KEYIN2 },
	{ 207, REG_PIN_SCL2 },
	{ 208, REG_PIN_SDA2 },
	{ 209, REG_PIN_CLK_AUX0 },
	{ 210, REG_PIN_IIS0DI },
	{ 211, REG_PIN_IIS0DO },
	{ 212, REG_PIN_IIS0CLK },
	{ 213, REG_PIN_IIS0LRCK },
	{ 214, REG_PIN_IIS0MCK },
	{ 215, REG_PIN_IIS1DI },
	{ 216, REG_PIN_IIS1DO },
	{ 217, REG_PIN_IIS1CLK },
	{ 218, REG_PIN_IIS1LRCK },
	{ 219, REG_PIN_IIS1MCK },
	{ 225, REG_PIN_MTDO },
	{ 226, REG_PIN_MTDI },
	{ 227, REG_PIN_MTCK },
	{ 228, REG_PIN_MTMS },
	{ 229, REG_PIN_MTRST_N },
	{ 230, REG_PIN_TRACECLK },
	{ 231, REG_PIN_TRACECTRL },
	{ 232, REG_PIN_TRACEDAT0 },
	{ 233, REG_PIN_TRACEDAT1 },
	{ 234, REG_PIN_TRACEDAT2 },
	{ 235, REG_PIN_TRACEDAT3 },
	{ 236, REG_PIN_TRACEDAT4 },
	{ 237, REG_PIN_TRACEDAT5 },
	{ 238 , REG_PIN_TRACEDAT6 },
	{ 239 , REG_PIN_TRACEDAT7} ,
};

#define GPIO_CTRL_ADDR  (SPRD_PIN_BASE)
#define GPIO_DATA_ADDR  (SPRD_GPIO_BASE)

#define GPIODATA_OFFSET     0x0
#define GPIODIR_OFFSET      0x8

#define GetBit(dwData, i)   (dwData & (0x1 << i))
#define SetBit(dwData, i)   (dwData | (0x1 << i))
#define ClearBit(dwData, i) (dwData & ~(0x1 << i))

#define GPIO_COUNT  (ARRAY_SIZE(available_gpios_sc7727))

/****************************************************************/
/* Define value in accordance with
    the specification of each BB vendor. */
#define AP_GPIO_COUNT   GPIO_COUNT
/****************************************************************/


/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static uint16_t checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result_t gpiomap_result = {
    .init = checkgpiomap_result[PHONE_INIT],
    .sleep = checkgpiomap_result[PHONE_SLEEP]
};

#ifdef SECGPIO_SLEEP_DEBUGGING
static struct sleepdebug_gpiotable sleepdebug_table;
#endif
/****************************************************************/

unsigned int get_gpio_io(unsigned int value)
{
    switch(value) {
    case 0x0: /* in fact, this is hi-z */
        return GDVS_IO_FUNC; //GDVS_IO_HI_Z;
    case 0x1:
        return GDVS_IO_OUT;
    case 0x2:
        return GDVS_IO_IN;
    default:
        return GDVS_IO_ERR;
    }
}

unsigned int get_gpio_pull_value(unsigned int value)
{
    switch(value) {
    case 0x0:
        return GDVS_PUPD_NP;
    case 0x1:
        return GDVS_PUPD_PD;
    case 0x2:
        return GDVS_PUPD_PU;
    default:
        return GDVS_PUPD_ERR;
    }
}

unsigned int get_gpio_data(unsigned int value)
{
    if (value == 0)
        return GDVS_HL_L;
    else
        return GDVS_HL_H;
}

void get_gpio_group(unsigned int num, unsigned int* grp_offset, unsigned int* bit_pos)
{
    if (num < 16) {
        *grp_offset = 0x0;
        *bit_pos = num;
    } else if (num < 32) {
        *grp_offset = 0x80;
        *bit_pos = num - 16;
    } else if (num < 48) {
        *grp_offset = 0x100;
        *bit_pos = num - 32;
    } else if (num < 64) {
        *grp_offset = 0x180;
        *bit_pos = num - 48;
    } else if (num < 80) {
        *grp_offset = 0x200;
        *bit_pos = num - 64;
    } else if (num < 96) {
        *grp_offset = 0x280;
        *bit_pos = num - 80;
    } else if (num < 112) {
        *grp_offset = 0x300;
        *bit_pos = num - 96;
    } else if (num < 128) {
        *grp_offset = 0x380;
        *bit_pos = num - 112;
    } else if (num < 144) {
        *grp_offset = 0x400;
        *bit_pos = num - 128;
    } else if (num < 160) {
        *grp_offset = 0x480;
        *bit_pos = num - 144;
    } else if (num < 176) {
        *grp_offset = 0x500;
        *bit_pos = num - 160;
    } else if (num < 192) {
        *grp_offset = 0x580;
        *bit_pos = num - 176;
    } else if (num < 208) {
        *grp_offset = 0x600;
        *bit_pos = num - 192;
    } else if (num < 224) {
        *grp_offset = 0x680;
        *bit_pos = num - 208;
    } else if (num < 240) {
        *grp_offset = 0x700;
        *bit_pos = num - 224;
    } else {
        *grp_offset = 0x780;
        *bit_pos = num - 240;
    }
}

void get_gpio_registers(unsigned char phonestate)
{
	unsigned int i, status;
	unsigned int ctrl_reg, dir_reg;
	unsigned int PIN_NAME_sel;
	unsigned int temp_io, temp_pud, temp_lh;
	unsigned int grp_offset, bit_pos;
	unsigned int PIN_NAME_wpus;
	for (i = 0; i < GPIO_COUNT; i++) {
		ctrl_reg = readl((void __iomem*)GPIO_CTRL_ADDR + available_gpios_sc7727[i].ctrl_offset);
		PIN_NAME_sel = ((GetBit(ctrl_reg,5)|GetBit(ctrl_reg,4)) >> 4);
		PIN_NAME_wpus = (GetBit(ctrl_reg,12) >> 5);

		if (phonestate == PHONE_SLEEP)
			temp_pud = get_gpio_pull_value((GetBit(ctrl_reg,3)|GetBit(ctrl_reg,2)) >> 2);
		else
			temp_pud = get_gpio_pull_value(((GetBit(ctrl_reg,7)|PIN_NAME_wpus)|GetBit(ctrl_reg,6)) >> 6);

		if (PIN_NAME_sel == 0x3) {  // GPIO mode
			get_gpio_group(available_gpios_sc7727[i].gpio_num, &grp_offset, &bit_pos);
			if (phonestate == PHONE_SLEEP) {
				temp_io = get_gpio_io(GetBit(ctrl_reg,1)| GetBit(ctrl_reg,0));
				if (temp_io == 0)
					temp_io = GDVS_IO_HI_Z;
			} else {
				dir_reg = readl((void __iomem*)GPIO_DATA_ADDR + grp_offset + GPIODIR_OFFSET);
				temp_io = GDVS_IO_IN + (GetBit(dir_reg, bit_pos) >> bit_pos);
			}

			status = gpio_request(available_gpios_sc7727[i].gpio_num, NULL);
			temp_lh = gpio_get_value(available_gpios_sc7727[i].gpio_num); /* 0: L, 1: H */
			if (!status)
				gpio_free(available_gpios_sc7727[i].gpio_num);
		} else {    // Func mode
			temp_io = GDVS_IO_FUNC;
			temp_lh = GDVS_HL_UNKNOWN;
		}
		checkgpiomap_result[phonestate][i] = GET_RESULT_GPIO(temp_io, temp_pud, temp_lh);
	}
}

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void check_gpio_status(unsigned char phonestate)
{
    pr_info("[GPIO_DVS][%s] ++\n", __func__);

    get_gpio_registers(phonestate);

    pr_info("[GPIO_DVS][%s] --\n", __func__);

    return;
}
/****************************************************************/


#ifdef SECGPIO_SLEEP_DEBUGGING
/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
void setgpio_for_sleepdebug(int gpionum, uint16_t  io_pupd_lh)
{
    unsigned char temp_io, temp_pupd, temp_lh, ctrl_reg;

    if (gpionum >= GPIO_COUNT) {
        pr_info("[GPIO_DVS][%s] gpio num is out of boundary.\n", __func__);
        return;
    }

    pr_info("[GPIO_DVS][%s] gpionum=%d, io_pupd_lh=0x%x\n", __func__, gpionum, io_pupd_lh);

    temp_io = GET_GPIO_IO(io_pupd_lh);
    temp_pupd = GET_GPIO_PUPD(io_pupd_lh);
    temp_lh = GET_GPIO_LH(io_pupd_lh);

    pr_info("[GPIO_DVS][%s] io=%d, pupd=%d, lh=%d\n", __func__, temp_io, temp_pupd, temp_lh);

    /* in case of 'INPUT', set PD/PU */
    if (temp_io == GDVS_IO_IN) {
        ctrl_reg = readl((void __iomem*)GPIO_CTRL_ADDR + available_gpios_sc7727[gpionum].ctrl_offset);
        ctrl_reg = ClearBit(ctrl_reg, 3);
        ctrl_reg = ClearBit(ctrl_reg, 2);

        /* 0x0:NP, 0x1:PD, 0x2:PU */
        if (temp_pupd == GDVS_PUPD_NP)
            temp_pupd = 0x0;
        else if (temp_pupd == GDVS_PUPD_PD)
            ctrl_reg = SetBit(ctrl_reg, 2);
        else if (temp_pupd == GDVS_PUPD_PU)
            ctrl_reg = SetBit(ctrl_reg, 3);

        writel(ctrl_reg, (void __iomem*)GPIO_CTRL_ADDR + available_gpios_sc7727[gpionum].ctrl_offset);
        pr_info("[GPIO_DVS][%s] %d gpio set IN_PUPD to %d\n",
                __func__, available_gpios_sc7727[gpionum].gpio_num, temp_pupd);
    } else if (temp_io == GDVS_IO_OUT) { /* in case of 'OUTPUT', set L/H */
        unsigned int grp_offset, bit_pos, data_reg1, data_reg2;
        get_gpio_group(available_gpios_sc7727[gpionum].gpio_num, &grp_offset, &bit_pos);
        data_reg1 = readl((void __iomem*)GPIO_DATA_ADDR + grp_offset + GPIODATA_OFFSET);

        gpio_set_value(available_gpios_sc7727[gpionum].gpio_num, temp_lh);

        data_reg2 = readl((void __iomem*)GPIO_DATA_ADDR + grp_offset + GPIODATA_OFFSET);
        if(data_reg1 != data_reg2)
            pr_info("[GPIO_DVS][%s] %d gpio set OUT_LH to %d\n",
                __func__, available_gpios_sc7727[gpionum].gpio_num, temp_lh);
        else
            pr_info("[GPIO_DVS][%s] %d gpio failed to set OUT_LH to %d\n",
                __func__, available_gpios_sc7727[gpionum].gpio_num, temp_lh);
    }
}
/****************************************************************/

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void undo_sleepgpio(void)
{
    int i;
    int gpio_num;

    pr_info("[GPIO_DVS][%s] ++\n", __func__);

    for (i = 0; i < sleepdebug_table.gpio_count; i++) {
        gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;
        /*
         * << Caution >>
         * If it's necessary,
         * change the following function to another appropriate one
         * or delete it
         */
        setgpio_for_sleepdebug(gpio_num, gpiomap_result.sleep[gpio_num]);
    }

    pr_info("[GPIO_DVS][%s] --\n", __func__);
    return;
}
/****************************************************************/
#endif

/********************* Fixed Code Area !***************************/
#ifdef SECGPIO_SLEEP_DEBUGGING
static void set_sleepgpio(void)
{
    int i;
    int gpio_num;
    uint16_t set_data;

    pr_info("[GPIO_DVS][%s] ++, cnt=%d\n",
        __func__, sleepdebug_table.gpio_count);

    for (i = 0; i < sleepdebug_table.gpio_count; i++) {
        pr_info("[GPIO_DVS][%d] gpio_num(%d), io(%d), pupd(%d), lh(%d)\n",
            i, sleepdebug_table.gpioinfo[i].gpio_num,
            sleepdebug_table.gpioinfo[i].io,
            sleepdebug_table.gpioinfo[i].pupd,
            sleepdebug_table.gpioinfo[i].lh);

        gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;

        // to prevent a human error caused by "don't care" value
        if( sleepdebug_table.gpioinfo[i].io == 1)       /* IN */
            sleepdebug_table.gpioinfo[i].lh =
                GET_GPIO_LH(gpiomap_result.sleep[gpio_num]);
        else if( sleepdebug_table.gpioinfo[i].io == 2)      /* OUT */
            sleepdebug_table.gpioinfo[i].pupd =
                GET_GPIO_PUPD(gpiomap_result.sleep[gpio_num]);

        set_data = GET_RESULT_GPIO(
            sleepdebug_table.gpioinfo[i].io,
            sleepdebug_table.gpioinfo[i].pupd,
            sleepdebug_table.gpioinfo[i].lh);

        setgpio_for_sleepdebug(gpio_num, set_data);
    }

    pr_info("[GPIO_DVS][%s] --\n", __func__);
    return;
}
#endif

static struct gpio_dvs_t gpio_dvs = {
    .result = &gpiomap_result,
    .count = AP_GPIO_COUNT,
    .check_init = false,
    .check_sleep = false,
    .check_gpio_status = check_gpio_status,
#ifdef SECGPIO_SLEEP_DEBUGGING
    .sdebugtable = &sleepdebug_table,
    .set_sleepgpio = set_sleepgpio,
    .undo_sleepgpio = undo_sleepgpio,
#endif
};

static struct platform_device secgpio_dvs_device = {
    .name   = "secgpio_dvs",
    .id     = -1,
    .dev.platform_data = &gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
    &secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
    return platform_add_devices(
        secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
/****************************************************************/


