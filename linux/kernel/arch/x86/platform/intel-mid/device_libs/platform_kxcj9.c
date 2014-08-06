/*
 * platform_kxcj9.c: kxcj9 platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:tao.xiong@intel.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/input/kionix_accel.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <asm/intel-mid.h>

#define ACCEL_INT_GPIO	133

static struct kionix_accel_platform_data kxcj9_pdata = {
	.min_interval = 5,
	.poll_interval = 200,
	.accel_direction = 1,
	.accel_irq_use_drdy =1,
	.accel_res = KIONIX_ACCEL_RES_12BIT,
	.accel_g_range = KIONIX_ACCEL_G_2G,
};


static struct i2c_board_info __initdata kxcj9_i2c_device = {
	I2C_BOARD_INFO("kxcj9", 0x0F),
	.platform_data = &kxcj9_pdata,
};

static int __init accel_i2c_init(void)
{
	int ret = 0;

	pr_info("accel i2c init begin...\n");
	ret = gpio_request(ACCEL_INT_GPIO, "kxcj9-int");
	if (ret) {
		pr_err("unable to request GPIO pin for accelerate sensor\n");
		kxcj9_i2c_device.irq = -1;
		return -ENXIO;
	} else {
		gpio_direction_input(ACCEL_INT_GPIO);
		kxcj9_i2c_device.irq = gpio_to_irq(ACCEL_INT_GPIO);
	}

	return i2c_register_board_info(5, &kxcj9_i2c_device, 1);
}

module_init(accel_i2c_init);
