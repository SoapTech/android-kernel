
/*
 * platform_byt_ecs_charger.c: platform data initilization file
 *                               for baytrail ecs charger
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/power/bq24192_charger.h>
#include <linux/power/cw2015_battery.h>
#include "platform_bq24192.h"
#include "platform_cw2015.h"

static struct bq24192_platform_data *bq24192_pdata;
static struct cw2015_platform_data *cw2015_pdata;

static struct i2c_board_info __initdata bq24192_i2c_device = {
	I2C_BOARD_INFO("bq24192", 0x6b),
};

static struct i2c_board_info __initdata cw2015_i2c_device = {
	I2C_BOARD_INFO("cw201x", 0x62),
};

/* Provide WA to register charger & FG in BYT-ECS */
static int __init byt_ecs_charger_i2c_init(void)
{
	int ret = 0;

	bq24192_pdata = bq24192_platform_data(NULL);

	if (bq24192_pdata == NULL) {
		pr_err("%s: unable to get the charger platform data\n",
				__func__);
		return -ENODEV;
	}

	bq24192_i2c_device.platform_data = bq24192_pdata;

	ret = i2c_register_board_info(1, &bq24192_i2c_device, 1);
	if (ret < 0) {
		pr_err("%s: unable to register charger(%d)\n", __func__, ret);
		return ret;
	}

	cw2015_pdata = cw2015_platform_data(NULL);
	cw2015_i2c_device.platform_data = cw2015_pdata;

	ret = i2c_register_board_info(1, &cw2015_i2c_device, 1);
	if (ret < 0) {
		pr_err("%s: unable to register fuel gauge(%d)\n", __func__, ret);
		return ret;
	}

	return ret;
}
module_init(byt_ecs_charger_i2c_init);
