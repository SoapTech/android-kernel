/*
 * bq24192_charger.h - Charger driver for TI BQ24190/191/192/192I
 *
 * Copyright (C) 2012 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __BQ24192_CHARGER_H_
#define __BQ24192_CHARGER_H_

#define TEMP_NR_RNG	4
#define BATTID_STR_LEN	8
#define RANGE	25
/* User limits for sysfs charge enable/disable */
#define USER_SET_CHRG_DISABLE	0
#define USER_SET_CHRG_LMT1	1
#define USER_SET_CHRG_LMT2	2
#define USER_SET_CHRG_LMT3	3
#define USER_SET_CHRG_NOLMT	4

#define INPUT_CHRG_CURR_0	0
#define INPUT_CHRG_CURR_100	100
#define INPUT_CHRG_CURR_500	500
#define INPUT_CHRG_CURR_950	950
#define INPUT_CHRG_CURR_1500	1500
/* Charger Master Temperature Control Register */
#define MSIC_CHRTMPCTRL         0x18E
/* Higher Temprature Values*/
#define CHRTMPCTRL_TMPH_60      (3 << 6)
#define CHRTMPCTRL_TMPH_55      (2 << 6)
#define CHRTMPCTRL_TMPH_50      (1 << 6)
#define CHRTMPCTRL_TMPH_45      (0 << 6)

/* Lower Temprature Values*/
#define CHRTMPCTRL_TMPL_15      (3 << 4)
#define CHRTMPCTRL_TMPL_10      (2 << 4)
#define CHRTMPCTRL_TMPL_05      (1 << 4)
#define CHRTMPCTRL_TMPL_00      (0 << 4)

enum bq24192_bat_chrg_mode {
	BATT_CHRG_FULL = 0,
	BATT_CHRG_NORMAL = 1,
	BATT_CHRG_MAINT = 2,
	BATT_CHRG_NONE = 3
};

static const char *usbevt_str[] = {
	"USB_EVENT_NONE",
	"USB_EVENT_VBUS",
	"USB_EVENT_ID",
	"USB_EVENT_CHARGER",
	"USB_EVENT_ENUMERATED",
	"USB_EVENT_DRIVE_VBUS",
};

static const char *psp_str[] = {
	"POWER_SUPPLY_PROP_STATUS",
	"POWER_SUPPLY_PROP_CHARGE_TYPE",
	"POWER_SUPPLY_PROP_HEALTH",
	"POWER_SUPPLY_PROP_PRESENT",
	"POWER_SUPPLY_PROP_ONLINE",
	"POWER_SUPPLY_PROP_TECHNOLOGY",
	"POWER_SUPPLY_PROP_CYCLE_COUNT",
	"POWER_SUPPLY_PROP_VOLTAGE_MAX",
	"POWER_SUPPLY_PROP_VOLTAGE_MIN",
	"POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN",
	"POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN",
	"POWER_SUPPLY_PROP_VOLTAGE_NOW",
	"POWER_SUPPLY_PROP_VOLTAGE_AVG",
	"POWER_SUPPLY_PROP_VOLTAGE_OCV",
	"POWER_SUPPLY_PROP_CURRENT_MAX",
	"POWER_SUPPLY_PROP_CURRENT_NOW",
	"POWER_SUPPLY_PROP_CURRENT_AVG",
	"POWER_SUPPLY_PROP_POWER_NOW",
	"POWER_SUPPLY_PROP_POWER_AVG",
	"POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN",
	"POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN",
	"POWER_SUPPLY_PROP_CHARGE_FULL",
	"POWER_SUPPLY_PROP_CHARGE_EMPTY",
	"POWER_SUPPLY_PROP_CHARGE_NOW",
	"POWER_SUPPLY_PROP_CHARGE_AVG",
	"POWER_SUPPLY_PROP_CHARGE_COUNTER",
	"POWER_SUPPLY_CHARGE_CURRENT_LIMIT",
	"POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT",
	"POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX",
	"POWER_SUPPLY_PROP_CHARGE_CURRENT",
	"POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT",
	"POWER_SUPPLY_PROP_CHARGE_VOLTAGE",
	"POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE",
	"POWER_SUPPLY_PROP_INLMT",
	"POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN",
	"POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN",
	"POWER_SUPPLY_PROP_ENERGY_FULL",
	"POWER_SUPPLY_PROP_ENERGY_EMPTY",
	"POWER_SUPPLY_PROP_ENERGY_NOW",
	"POWER_SUPPLY_PROP_ENERGY_AVG",
	"POWER_SUPPLY_PROP_CAPACITY", /* in percents! */
	"POWER_SUPPLY_PROP_CAPACITY_LEVEL",
	"POWER_SUPPLY_PROP_TEMP",
	"POWER_SUPPLY_PROP_MAX_TEMP",
	"POWER_SUPPLY_PROP_MIN_TEMP",
	"POWER_SUPPLY_PROP_TEMP_AMBIENT",
	"POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW",
	"POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG",
	"POWER_SUPPLY_PROP_TIME_TO_FULL_NOW",
	"POWER_SUPPLY_PROP_TIME_TO_FULL_AVG",
	"POWER_SUPPLY_PROP_TYPE", /* use power_supply.type instead */
	"POWER_SUPPLY_PROP_CHARGE_TERM_CUR",
	"POWER_SUPPLY_PROP_ENABLE_CHARGING",
	"POWER_SUPPLY_PROP_ENABLE_CHARGER",
	"POWER_SUPPLY_PROP_CABLE_TYPE",
	"POWER_SUPPLY_PROP_PRIORITY",
	"POWER_SUPPLY_PROP_SCOPE",
	/* Local extensions */
	"POWER_SUPPLY_PROP_USB_HC",
	"POWER_SUPPLY_PROP_USB_OTG",
	"POWER_SUPPLY_PROP_CHARGE_ENABLED",
	/* Properties of type `const char *' */
	"POWER_SUPPLY_PROP_MODEL_NAME",
	"POWER_SUPPLY_PROP_MANUFACTURER",
	"POWER_SUPPLY_PROP_SERIAL_NUMBER",
};

/*********************************************************************
 * SFI table entries Structures
 ********************************************************************/
/*********************************************************************
 *		Platform Data Section
 *********************************************************************/
/* Battery Thresholds info which need to get from SMIP area */
struct platform_batt_safety_param {
	u8 smip_rev;
	u8 fpo;		/* fixed implementation options */
	u8 fpo1;	/* fixed implementation options1 */
	u8 rsys;	/* System Resistance for Fuel gauging */

	/* Minimum voltage necessary to
	 * be able to safely shut down */
	short int vbatt_sh_min;

	/* Voltage at which the battery driver
	 * should report the LEVEL as CRITICAL */
	short int vbatt_crit;

	short int itc;		/* Charge termination current */
	short int temp_high;	/* Safe Temp Upper Limit */
	short int temp_low;	/* Safe Temp lower Limit */
	u8 brd_id;		/* Unique Board ID */
} __packed;

/* Parameters defining the range */
struct platform_temp_mon_table {
	short int temp_up_lim;
	short int temp_low_lim;
	short int rbatt;
	short int full_chrg_vol;
	short int full_chrg_cur;
	short int maint_chrg_vol_ll;
	short int maint_chrg_vol_ul;
	short int maint_chrg_cur;
} __packed;

struct platform_batt_profile {
	char batt_id[BATTID_STR_LEN];
	unsigned short int voltage_max;
	unsigned int capacity;
	u8 battery_type;
	u8 temp_mon_ranges;
	struct platform_temp_mon_table temp_mon_range[TEMP_NR_RNG];

} __packed;

struct bq24192_platform_data {
	bool slave_mode;
	short int temp_low_lim;
	bool sfi_tabl_present;
	short int safetemp;
	struct platform_batt_profile batt_profile;
	struct platform_batt_safety_param safety_param;
	struct power_supply_throttle *throttle_states;

	char **supplied_to;
	size_t	num_supplicants;
	size_t num_throttle_states;
	unsigned long supported_cables;
	/* Function pointers for platform specific initialization */
	int (*init_platform_data)(void);
	int (*get_irq_number)(void);
	int (*get_usbid_irq_number)(void);
	int (*query_otg)(void *, void *);
	int (*drive_vbus)(bool);
	int (*get_battery_pack_temp)(int *);
	void (*free_platform_data)(void);
};

#ifdef CONFIG_CHARGER_BQ24192
extern int bq24192_slave_mode_enable_charging(int volt, int cur, int ilim);
extern int bq24192_slave_mode_disable_charging(void);
extern int bq24192_query_battery_status(void);
extern int bq24192_get_battery_pack_temp(int *temp);
extern int bq24192_get_battery_health(void);
extern bool bq24192_is_volt_shutdown_enabled(void);
extern int bq24192_get_charging_status(void);
#else
static int bq24192_get_battery_health(void)
{
	return 0;
}
#endif
#endif /* __BQ24192_CHARGER_H_ */
