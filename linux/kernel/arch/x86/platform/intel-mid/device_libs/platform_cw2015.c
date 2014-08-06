/*
 * platform_bq24192.c: bq24192 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/sfi.h>
#include <linux/power/cw2015_battery.h>
#include <linux/power/bq24192_charger.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_gpadc.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/usb/otg.h>
#include "platform_cw2015.h"

/*
   note the follow array must set depend on the battery that you use
   you must send the battery to cellwise-semi the contact information:
   name: chen gan; tel:13416876079; E-mail: ben.chen@cellwise-semi.com
 */
static u8 config_info[SIZE_BATINFO] = {
	0x15, 0x42, 0x60, 0x59, 0x52,
	0x58, 0x4D, 0x48, 0x48, 0x44,
	0x44, 0x46, 0x49, 0x48, 0x32,
	0x24, 0x20, 0x17, 0x13, 0x0F,
	0x19, 0x3E, 0x51, 0x45, 0x08,
	0x76, 0x0B, 0x85, 0x0E, 0x1C,
	0x2E, 0x3E, 0x4D, 0x52, 0x52,
	0x57, 0x3D, 0x1B, 0x6A, 0x2D,
	0x25, 0x43, 0x52, 0x87, 0x8F,
	0x91, 0x94, 0x52, 0x82, 0x8C,
	0x92, 0x96, 0xFF, 0x7B, 0xBB,
	0xCB, 0x2F, 0x7D, 0x72, 0xA5,
	0xB5, 0xC1, 0x46, 0xAE
};


static struct cw_bat_platform_data cw2015_pdata = {
	.dc_det_pin      = INVALID_GPIO,
        .dc_det_level    = GPIO_LOW,

        .bat_low_pin    = INVALID_GPIO,
        .bat_low_level  = GPIO_LOW,
        .chg_ok_pin   = INVALID_GPIO,
        .chg_ok_level = GPIO_HIGH,

        .is_usb_charge = 0,
        .chg_mode_sel_pin = INVALID_GPIO,
        .chg_mode_sel_level = GPIO_HIGH,

        .cw_bat_config_info     = config_info,

	.battery_status = bq24192_get_charging_status,
};

void *cw2015_platform_data(void *info)
{
	return &cw2015_pdata;
}
