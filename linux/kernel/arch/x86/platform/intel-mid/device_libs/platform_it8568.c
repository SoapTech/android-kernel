/*
 * platform_ft5x06.c: FT5x06 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Alan Zhang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <linux/power/byt_ulpmc_battery.h>

void * it8568_platform_data(void *info)
{
	static struct ulpmc_platform_data it8568_data = {
		.gpio = 148,
		.extcon_devname = "ulpmc-battery",
		.volt_sh_min = 35000000,
		.cc_lim0 = 1200000,	/* default charge current */
		.cc_lim1 = 100000,	/* charge current limit 1 */
		.cc_lim2 = 500000,	/* charge current limit 2 */
		.cc_lim3 = 1200000,	/* charge current limit 3 */
		.rbatt = 1000,	/* Rbatt in milli Ohms */
		.temp_ul = 50,	/* temp upper limit */
		.temp_ll = 0,	/* temp lower limit */
		.version = BYTULPMCFGV4,
	};

	return &it8568_data;
}

