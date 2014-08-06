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
#include <linux/power/bq24192_charger.h>
#include <linux/lnw_gpio.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>
#include <asm/intel-mid.h>
#include <asm/spid.h>
#include "platform_bq24192.h"
#include <linux/usb/otg.h>

static struct ps_pse_mod_prof ps_pse_mod_prof;
static struct ps_batt_chg_prof ps_batt_chrg_prof;
static struct bq24192_platform_data platform_data;
static struct power_supply_throttle bq24192_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24192_CHRG_CUR_NOLIMIT,

	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24192_CHRG_CUR_HIGH,

	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24192_CHRG_CUR_MEDIUM,

	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGER,
	},

};

char *bq24192_supplied_to[] = {
	"max170xx_battery",
	"max17042_battery",
	"max17047_battery",
	"bq27441",
};

static struct i2c_board_info __initdata bq24192_i2c_device = {
        I2C_BOARD_INFO("bq24192", 0x6b),
};

static int platform_get_irq_number(void)
{
	int irq;
	pr_debug("%s:\n", __func__);
	irq = gpio_to_irq(CHGR_INT_N);
	pr_debug("%s:%d:irq = %d\n", __func__, __LINE__, irq);
	return irq;
}

static int platform_drive_vbus(bool onoff)
{
	/* In BYT, don't use charger to supply OTG power*/
	pr_info("driver vbus, should not call this.....\n");
	return 0;
}

int platform_get_battery_pack_temp(int *temp)
{
	return 250;
}

void *bq24192_platform_data(void *info)
{
	pr_debug("%s:\n", __func__);
		
	platform_data.sfi_tabl_present = false;

	platform_data.throttle_states = bq24192_throttle_states;
	platform_data.supplied_to = bq24192_supplied_to;
	platform_data.num_throttle_states = ARRAY_SIZE(bq24192_throttle_states);
	platform_data.num_supplicants = ARRAY_SIZE(bq24192_supplied_to);
	platform_data.supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB;
	platform_data.get_irq_number = platform_get_irq_number;
	platform_data.drive_vbus = platform_drive_vbus;
	platform_data.get_battery_pack_temp = NULL;
	platform_data.query_otg = NULL;
	platform_data.slave_mode = 0;

	return &platform_data;
}

static void platform_get_batt_charge_profile()
{
	struct ps_temp_chg_table temp_mon_range[BATT_TEMP_NR_RNG];

	char batt_str[] = "INTN0001";

	memcpy(ps_pse_mod_prof.batt_id, batt_str, strlen(batt_str));

	ps_pse_mod_prof.battery_type = 0x2;
	ps_pse_mod_prof.capacity = 0x2C52;
	ps_pse_mod_prof.voltage_max = 4350;
	ps_pse_mod_prof.chrg_term_mA = 300;
	ps_pse_mod_prof.low_batt_mV = 3400;
	ps_pse_mod_prof.disch_tmp_ul = 55;
	ps_pse_mod_prof.disch_tmp_ll = 0;
	ps_pse_mod_prof.temp_mon_ranges = 5;

	temp_mon_range[0].temp_up_lim = 55;
	temp_mon_range[0].full_chrg_vol = 4100;
	temp_mon_range[0].full_chrg_cur = 1800;
	temp_mon_range[0].maint_chrg_vol_ll = 4000;
	temp_mon_range[0].maint_chrg_vol_ul = 4100;
	temp_mon_range[0].maint_chrg_cur = 1800;

	temp_mon_range[1].temp_up_lim = 45;
	temp_mon_range[1].full_chrg_vol = 4350;
	temp_mon_range[1].full_chrg_cur = 1800;
	temp_mon_range[1].maint_chrg_vol_ll = 4250;
	temp_mon_range[1].maint_chrg_vol_ul = 4350;
	temp_mon_range[1].maint_chrg_cur = 1800;

	temp_mon_range[2].temp_up_lim = 23;
	temp_mon_range[2].full_chrg_vol = 4350;
	temp_mon_range[2].full_chrg_cur = 1400;
	temp_mon_range[2].maint_chrg_vol_ll = 4250;
	temp_mon_range[2].maint_chrg_vol_ul = 4350;
	temp_mon_range[2].maint_chrg_cur = 1400;

	temp_mon_range[3].temp_up_lim = 10;
	temp_mon_range[3].full_chrg_vol = 4350;
	temp_mon_range[3].full_chrg_cur = 1000;
	temp_mon_range[3].maint_chrg_vol_ll = 4250;
	temp_mon_range[3].maint_chrg_vol_ul = 4350;
	temp_mon_range[3].maint_chrg_cur = 1000;

	temp_mon_range[4].temp_up_lim = 0;
	temp_mon_range[4].full_chrg_vol = 0;
	temp_mon_range[4].full_chrg_cur = 0;
	temp_mon_range[4].maint_chrg_vol_ll = 0;
	temp_mon_range[4].maint_chrg_vol_ul = 0;
	temp_mon_range[4].maint_chrg_vol_ul = 0;
	temp_mon_range[4].maint_chrg_cur = 0;

	memcpy(ps_pse_mod_prof.temp_mon_range,
		temp_mon_range,
		BATT_TEMP_NR_RNG * sizeof(struct ps_temp_chg_table));

	ps_pse_mod_prof.temp_low_lim = 0;

	ps_batt_chrg_prof.chrg_prof_type = PSE_MOD_CHRG_PROF;
	ps_batt_chrg_prof.batt_prof = &ps_pse_mod_prof;
	battery_prop_changed(POWER_SUPPLY_BATTERY_INSERTED, &ps_batt_chrg_prof);
}
static int __init bq24192_platform_init(void)
{
	platform_get_batt_charge_profile();
	bq24192_i2c_device.platform_data = bq24192_platform_data(NULL);
	return i2c_register_board_info(1, &bq24192_i2c_device, 1);
}

module_init(bq24192_platform_init);
