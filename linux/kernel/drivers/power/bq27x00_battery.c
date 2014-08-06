/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <linux/power/bq27x00_battery.h>

#define DRIVER_VERSION			"1.2.0"

#define BQ27x00_REG_TEMP_L		0x02
#define BQ27x00_REG_TEMP_H		0x03
#define BQ27x00_REG_VOLT_L		0x04
#define BQ27x00_REG_VOLT_H		0x05
#define BQ27x00_REG_AI			0x10
#define BQ27x00_REG_FLAGS		0x06
#define BQ27x00_REG_NAC			0x08 /* Nominal available capacity */
#define BQ27x00_REG_LMD			0x0e /* Last measured discharge */
#define BQ27x00_REG_AE			0x0c /* Available energy */

#define BQ27x00_REG_CYCT		0x2A /* Cycle count total */
#define BQ27x00_REG_RSOC		0x1C /* Relative State-of-Charge */
#define BQ27x00_REG_TTECP		0x26
#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x0E

#define BQ27000_REG_RSOC		0x1C /* Relative State-of-Charge */
#define BQ27000_REG_ILMD		0x76 /* Initial last measured discharge */
#define BQ27000_FLAG_EDVF		BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_EDV1		BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_CI			BIT(4) /* Capacity Inaccurate flag */
#define BQ27000_FLAG_FC			BIT(5)
#define BQ27000_FLAG_CHGS		BIT(7) /* Charge state flag */

#define BQ27500_REG_SOC			0x2C
#define BQ27500_REG_DCAP		0x3C /* Design capacity */
#define BQ27500_FLAG_DSC		BIT(0)
#define BQ27500_FLAG_SOCF		BIT(1) /* State-of-Charge threshold final */
#define BQ27500_FLAG_SOC1		BIT(2) /* State-of-Charge threshold 1 */
#define BQ27500_FLAG_FC			BIT(9)

#define BQ27000_RS			20 /* Resistor sense */

#define CONFIG_BATTERY_BQ27X00_PLATFORM

struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
	int (*write)(struct bq27x00_device_info *di, u8 reg, u8 val);
};

enum bq27x00_chip { BQ27000, BQ27500 , BQ27441};

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int energy;
	int flags;
};

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
	enum bq27x00_chip	chip;

	struct bq27x00_reg_cache cache;
	int charge_design_full;

	unsigned long last_update;
	struct delayed_work work;

	struct power_supply	bat;
	int status;
	struct bq27x00_access_methods bus;

	struct mutex lock;
};

static enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static unsigned int poll_interval = 100;
module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - " \
				"0 disables polling");

/*
 * Common code for BQ27x00 devices
 */

static inline int bq27x00_read(struct bq27x00_device_info *di, u8 reg,
		bool single)
{
	return di->bus.read(di, reg, single);
}

static int bq27x00_write(struct bq27x00_device_info *di, u8 reg, u8 val)
{
	return di->bus.write(di, reg, val);
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_rsoc(struct bq27x00_device_info *di)
{
	int rsoc;

	if (di->chip == BQ27500)
		rsoc = bq27x00_read(di, BQ27500_REG_SOC, false);
	else
		rsoc = bq27x00_read(di, BQ27x00_REG_RSOC, true);

	if (rsoc < 0)
		dev_dbg(di->dev, "error reading relative State-of-Charge\n");

	return rsoc;
}

/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_charge(struct bq27x00_device_info *di, u8 reg)
{
	int charge;

	charge = bq27x00_read(di, reg, false);
	if (charge < 0) {
		dev_dbg(di->dev, "error reading charge register %02x: %d\n",
			reg, charge);
		return charge;
	}

	return charge;
}

/*
 * Return the battery Nominal available capaciy in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_nac(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_NAC)*1000;
}

/*
 * Return the battery Last measured discharge in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_lmd(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_LMD)*1000;
}

/*
 * Return the battery Initial last measured discharge in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_ilmd(struct bq27x00_device_info *di)
{
	int ilmd;

	ilmd = bq27x00_read(di, BQ27500_REG_DCAP, false);

	if (ilmd < 0) {
		dev_dbg(di->dev, "error reading initial last measured discharge\n");
		return ilmd;
	}

	return ilmd * 1000;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_energy(struct bq27x00_device_info *di)
{
	int ae;

	ae = bq27x00_read(di, BQ27x00_REG_AE, false);
	if (ae < 0) {
		dev_dbg(di->dev, "error reading available energy\n");
		return ae;
	}

	return ae*1000;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_temperature(struct bq27x00_device_info *di)
{
	int temp;
	u8 temp_l, temp_h;

	temp_l = bq27x00_read(di, BQ27x00_REG_TEMP_L, true);
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return temp;
	}

	temp_h = bq27x00_read(di, BQ27x00_REG_TEMP_H, true);
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return temp;
	}

	temp = (temp_h << 8) | temp_l;
	return temp/10;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27x00_read(di, BQ27x00_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_time(struct bq27x00_device_info *di, u8 reg)
{
	int tval;

	tval = bq27x00_read(di, reg, false);
	if (tval < 0) {
		dev_dbg(di->dev, "error reading time register %02x: %d\n",
			reg, tval);
		return tval;
	}

	if (tval == 65535)
		return -ENODATA;

	return tval * 60;
}

static void bq27x00_update(struct bq27x00_device_info *di)
{
	struct bq27x00_reg_cache cache = {0, };
	bool is_bq27500 = di->chip == BQ27500;

	cache.flags = bq27x00_read(di, BQ27x00_REG_FLAGS, 0);
	if (cache.flags >= 0) {
		cache.capacity = bq27x00_battery_read_rsoc(di);
		cache.energy = bq27x00_battery_read_energy(di);
		/*bq27441 don't this
		cache.time_to_empty = bq27x00_battery_read_time(di, BQ27x00_REG_TTE);
		cache.time_to_empty_avg = bq27x00_battery_read_time(di, BQ27x00_REG_TTECP);
		cache.time_to_full = bq27x00_battery_read_time(di, BQ27x00_REG_TTF);
		cache.cycle_count = bq27x00_battery_read_cyct(di);
		*/
		cache.charge_full = bq27x00_battery_read_lmd(di);
		cache.temperature = bq27x00_battery_read_temperature(di);
		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27x00_battery_read_ilmd(di);
	}

	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0) {
		di->cache = cache;
		power_supply_changed(&di->bat);
	}
	di->last_update = jiffies;
}

static void bq27x00_battery_poll(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, work.work);

	bq27x00_update(di);

	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&di->work, poll_interval * HZ);
	}
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int curr;
	int flags;

	curr = bq27x00_read(di, BQ27x00_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}
	if (curr > 10000)
		curr = curr - 0xFFFF;
	val->intval = curr*1000;
	return 0;
}

static int bq27x00_battery_healthy(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int status;
	if (!di->bus.read)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	status = bq27x00_read(di, 0x07, true);
	if (status & 0x40)
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	
	if (status & 0x80)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;

	return 0;
}
static int bq27x00_battery_status(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int status;

	if (di->chip == BQ27500) {
		if (di->cache.flags & BQ27500_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27500_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (di->cache.flags & BQ27000_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27000_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if (power_supply_am_i_supplied(&di->bat))
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (di->cache.flags & 0x01)
		status =  POWER_SUPPLY_STATUS_DISCHARGING;
	else
		status =  POWER_SUPPLY_STATUS_CHARGING;

	val->intval = status;

	return 0;
}

static int bq27x00_battery_capacity_level(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int level;

	if (di->cache.flags & BQ27500_FLAG_FC)
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (di->cache.flags & BQ27500_FLAG_SOC1)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (di->cache.flags & BQ27500_FLAG_SOCF)
		level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	val->intval = level;

	return 0;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	u8 volt_h, volt_l;

	volt_l = bq27x00_read(di, BQ27x00_REG_VOLT_L, true);
	if (volt_l < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt_l;
	}

	volt_h = bq27x00_read(di, BQ27x00_REG_VOLT_H, true);
	if (volt_h < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt_h;
	}

	val->intval = (volt_h << 8) | volt_l;
	val->intval *= 1000;
	return 0;
}

static int bq27x00_simple_value(int value,
	union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);


static int bq27x00_battery_set_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    const union power_supply_propval *val)
{
        int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

        mutex_lock(&di->lock);
        switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
                di->status = val->intval;
                break;
        default:
                ret = -EINVAL;
                break;
        }

        mutex_unlock(&di->lock);

        return ret;
}

static int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27x00_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		/*current we use the charger stauts instead of battery status*/
		//ret = bq27x00_battery_status(di, val);
		val->intval = di->status;
		break;
	#if 0
	case POWER_SUPPLY_PROP_HEALTH:
		//ret = bq27x00_battery_healthy(di, val);
		val->intval = 1;
		break;
	#endif
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->cache.flags < 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27x00_battery_current(di, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27x00_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = bq27x00_battery_capacity_level(di, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_simple_value(di->cache.temperature, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_simple_value(bq27x00_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27x00_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_simple_value(di->charge_design_full, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_simple_value(di->cache.energy, val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static int bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = bq27x00_battery_props;
	di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
	di->bat.get_property = bq27x00_battery_get_property;
	di->bat.set_property = bq27x00_battery_set_property;
	di->bat.external_power_changed = bq27x00_external_power_changed;

	INIT_DELAYED_WORK(&di->work, bq27x00_battery_poll);
	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	bq27x00_update(di);

	return 0;
}

static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{
	/*
	 * power_supply_unregister call bq27x00_battery_get_property which
	 * call bq27x00_battery_poll.
	 * Make sure that bq27x00_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(&di->bat);

	mutex_destroy(&di->lock);
}

static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

static int bq27x00_write_reg(struct bq27x00_device_info *di, u8 reg, u8 val)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[0];
	int ret = 0;
	#if 0
	u8 data[2] = {reg, val};
	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &data[0];
	msg[0].len = 2;
	printk("fdsssssssssssssss\n");
	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		return ret;
	#endif
	i2c_smbus_write_byte_data(client, reg, val);
	return 0;
}


static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static int bq27x00_enter_cfg_mode(struct bq27x00_device_info *di)
{
	unsigned char val;
	bq27x00_write(di, 0x00, 0x00);
	bq27x00_write(di, 0x01, 0x80);

	bq27x00_write(di, 0x00, 0x00);
	bq27x00_write(di, 0x01, 0x80);
		
	bq27x00_write(di, 0x00, 0x13);
	bq27x00_write(di, 0x01, 0x00);

	msleep(100);
	val = bq27x00_read(di, 0x06, true);
	/* check bit4=1 to confirm cofiguration mode is entered */
	if ((val & 0x10) == 0) 
		pr_err("fail to enter bq27441 configure mode\n");
	return 0;
}

static int bq27x00_exit_cfg_mode(struct bq27x00_device_info *di)
{
	unsigned char val;
	bq27x00_write(di, 0x00, 0x42);
	bq27x00_write(di, 0x01, 0x00);

	msleep(1500);
	val = bq27x00_read(di, 0x06, true);
	/*check if bit4=0 to confirm cofiguration mode is exit*/
	if (val & 0x10)
		pr_err("fail to exit bq27441 configure mode\n");
	bq27x00_write(di, 0x00, 0x20);
	bq27x00_write(di, 0x01, 0x00);
	return 0;
}

static void bq27x00_cfg_data(struct bq27x00_device_info *di)
{	
	u8 val;

	#ifdef CONFIG_E2T_HYD_PL3565144P

	pr_info("[intel] %s:%d. Checkpoint.\n", __FUNCTION__, __LINE__);
	
	bq27x00_write(di, 0x61, 0x00); // enable BlockData() to access RAM

	bq27x00_write(di, 0x3e, 0x24);
	bq27x00_write(di, 0x3f, 0x00);

	bq27x00_write(di, 0x40, 0x00);
	bq27x00_write(di, 0x41, 0x19);
	bq27x00_write(di, 0x42, 0x28);
	bq27x00_write(di, 0x43, 0x63);
	bq27x00_write(di, 0x44, 0x5f);
	bq27x00_write(di, 0x45, 0xff);
	bq27x00_write(di, 0x46, 0x62);
	bq27x00_write(di, 0x47, 0x00);
	bq27x00_write(di, 0x48, 0xc8);

	bq27x00_write(di, 0x60, 0xd3);
	usleep(200);
	bq27x00_write(di, 0x3e, 0x40);
	bq27x00_write(di, 0x3f, 0x00);

	bq27x00_write(di, 0x40, 0x25);
	bq27x00_write(di, 0x41, 0xe8);
	bq27x00_write(di, 0x42, 0x0f);
	bq27x00_write(di, 0x43, 0x48);
	bq27x00_write(di, 0x44, 0x00);
	bq27x00_write(di, 0x45, 0x14);
	bq27x00_write(di, 0x46, 0x04);
	bq27x00_write(di, 0x47, 0x00);
	bq27x00_write(di, 0x48, 0x09);

	bq27x00_write(di, 0x60, 0x7a);
	usleep(200);

	bq27x00_write(di, 0x3e, 0x51);
	bq27x00_write(di, 0x3f, 0x00);

	bq27x00_write(di, 0x40, 0x00);
	bq27x00_write(di, 0x41, 0xc8);
	bq27x00_write(di, 0x42, 0x00);
	bq27x00_write(di, 0x43, 0xfa);
	bq27x00_write(di, 0x44, 0x02);
	bq27x00_write(di, 0x45, 0xf8);
	bq27x00_write(di, 0x46, 0x00);
	bq27x00_write(di, 0x47, 0x3c);
	bq27x00_write(di, 0x48, 0x3c);
	bq27x00_write(di, 0x49, 0x03);
	bq27x00_write(di, 0x4a, 0xb3);
	bq27x00_write(di, 0x4b, 0xb3);
	bq27x00_write(di, 0x4c, 0x01);
	bq27x00_write(di, 0x4d, 0x90);

	bq27x00_write(di, 0x60, 0xd1);
	usleep(200);

	bq27x00_write(di, 0x3e, 0x52);
	bq27x00_write(di, 0x3f, 0x00);

	bq27x00_write(di, 0x40, 0x42);
	bq27x00_write(di, 0x41, 0xf7);
	bq27x00_write(di, 0x42, 0x00);
	bq27x00_write(di, 0x43, 0x00);
	bq27x00_write(di, 0x44, 0x00);
	bq27x00_write(di, 0x45, 0x01);
	bq27x00_write(di, 0x46, 0x0e);
	bq27x00_write(di, 0x47, 0xdb);
	bq27x00_write(di, 0x48, 0x0e);
	bq27x00_write(di, 0x49, 0xa8);
	bq27x00_write(di, 0x4a, 0x0e);
	bq27x00_write(di, 0x4b, 0xd8);
	bq27x00_write(di, 0x4c, 0x36);
	bq27x00_write(di, 0x4d, 0xec);
	bq27x00_write(di, 0x4e, 0x05);
	bq27x00_write(di, 0x4f, 0x3c);
	bq27x00_write(di, 0x50, 0x0d);
	bq27x00_write(di, 0x51, 0xac);
	bq27x00_write(di, 0x52, 0x00);
	bq27x00_write(di, 0x53, 0xc8);
	bq27x00_write(di, 0x54, 0x00);
	bq27x00_write(di, 0x55, 0x32);
	bq27x00_write(di, 0x56, 0x00);
	bq27x00_write(di, 0x57, 0x14);
	bq27x00_write(di, 0x58, 0x03);
	bq27x00_write(di, 0x59, 0xe8);
	bq27x00_write(di, 0x5a, 0x01);
	bq27x00_write(di, 0x5b, 0x00);
	bq27x00_write(di, 0x5c, 0xc8);
	bq27x00_write(di, 0x5d, 0x10);
	bq27x00_write(di, 0x5e, 0x04);
	bq27x00_write(di, 0x5f, 0x00);

	bq27x00_write(di, 0x60, 0x4e);
	usleep(200);

	bq27x00_write(di, 0x3e, 0x52);
	bq27x00_write(di, 0x3f, 0x01);

	bq27x00_write(di, 0x40, 0x1e);
	bq27x00_write(di, 0x41, 0x10);
	bq27x00_write(di, 0x42, 0x61);
	bq27x00_write(di, 0x43, 0xff);
	bq27x00_write(di, 0x44, 0xe7);
	bq27x00_write(di, 0x45, 0xff);
	bq27x00_write(di, 0x46, 0xe6);
	bq27x00_write(di, 0x47, 0x00);
	bq27x00_write(di, 0x48, 0x03);
	bq27x00_write(di, 0x49, 0x02);
	bq27x00_write(di, 0x4a, 0xbc);

	bq27x00_write(di, 0x60, 0xe4);
	usleep(200);

	bq27x00_write(di, 0x3e, 0x59);
	bq27x00_write(di, 0x3f, 0x00);

	bq27x00_write(di, 0x40, 0x00);
	bq27x00_write(di, 0x41, 0x57);
	bq27x00_write(di, 0x42, 0x00);
	bq27x00_write(di, 0x43, 0x57);
	bq27x00_write(di, 0x44, 0x00);
	bq27x00_write(di, 0x45, 0x58);
	bq27x00_write(di, 0x46, 0x00);
	bq27x00_write(di, 0x47, 0x61);
	bq27x00_write(di, 0x48, 0x00);
	bq27x00_write(di, 0x49, 0x49);
	bq27x00_write(di, 0x4a, 0x00);
	bq27x00_write(di, 0x4b, 0x45);
	bq27x00_write(di, 0x4c, 0x00);
	bq27x00_write(di, 0x4d, 0x4e);
	bq27x00_write(di, 0x4e, 0x00);
	bq27x00_write(di, 0x4f, 0x55);
	bq27x00_write(di, 0x50, 0x00);
	bq27x00_write(di, 0x51, 0x51);
	bq27x00_write(di, 0x52, 0x00);
	bq27x00_write(di, 0x53, 0x4b);
	bq27x00_write(di, 0x54, 0x00);
	bq27x00_write(di, 0x55, 0x5d);
	bq27x00_write(di, 0x56, 0x00);
	bq27x00_write(di, 0x57, 0x67);
	bq27x00_write(di, 0x58, 0x00);
	bq27x00_write(di, 0x59, 0xc7);
	bq27x00_write(di, 0x5a, 0x02);
	bq27x00_write(di, 0x5b, 0x01);
	bq27x00_write(di, 0x5c, 0x03);
	bq27x00_write(di, 0x5d, 0x30);

	bq27x00_write(di, 0x60, 0x0a);
	usleep(200);

	#else
	
	bq27x00_write(di, 0x61, 0x00); // enable BlockData() to access RAM

	//1st we access class 82 to update DesignCap, DesignEng and TaperRate
	bq27x00_write(di, 0x3E, 0x52); //choose class 82 offset 0
	bq27x00_write(di, 0x3F, 0x00);

	bq27x00_write(di, 0x40, 0x43); //Qmax=0x4324=17188 after learning
	bq27x00_write(di, 0x41, 0x24);
	bq27x00_write(di, 0x4A, 0x10); //DesignCap=4300mA=0x10CC
	bq27x00_write(di, 0x4B, 0xCC);
	bq27x00_write(di, 0x4C, 0x3E); //DesignEnerge=4300mAx3.7v=15910mWh=0x3E26
	bq27x00_write(di, 0x4D, 0x26);
	bq27x00_write(di, 0x5B, 0x00); //TaperRate=43000/391=110=0x006E
	bq27x00_write(di, 0x5C, 0x6E);
	bq27x00_write(di, 0x60, 0xEF); //new checksum=0x16 according to TI

	//2nd we access class 81 to update DsgCurTh, ChgCurTh and QuitCur
	bq27x00_write(di, 0x3E, 0x51); //choose class 81 offset 0
	bq27x00_write(di, 0x3F, 0x00);
	
	bq27x00_write(di, 0x40, 0x01); //DsgCurTh=43000/86=500=0x01F4
	bq27x00_write(di, 0x41, 0xF4);
	bq27x00_write(di, 0x42, 0x01); //ChgCurTh also 0x01F4
	bq27x00_write(di, 0x43, 0xF4);
	bq27x00_write(di, 0x44, 0x03); //QuitCur=43000/43=1000=0x03E8
	bq27x00_write(di, 0x45, 0xE8);
	bq27x00_write(di, 0x60, 0xBA); //new checksum=0xBA according to TI
	
	//3rd we access class 89 to update Ra...
	bq27x00_write(di, 0x3E, 0x59); //choose class 89 offset 0
	bq27x00_write(di, 0x3F, 0x00);
	
	bq27x00_write(di, 0x40, 0x00);
	bq27x00_write(di, 0x41, 0x41);//Ra_0=0041
	bq27x00_write(di, 0x42, 0x00);
	bq27x00_write(di, 0x43, 0x41);//Ra_1=0041
	bq27x00_write(di, 0x44, 0x00);
	bq27x00_write(di, 0x45, 0x45);//Ra_2=0045
	bq27x00_write(di, 0x46, 0x00);
	bq27x00_write(di, 0x47, 0x50);//Ra_3=0050
	bq27x00_write(di, 0x48, 0x00);
	bq27x00_write(di, 0x49, 0x3c);//Ra_4=003c
	bq27x00_write(di, 0x4A, 0x00);
	bq27x00_write(di, 0x4B, 0x3a);//Ra_5=003a
	bq27x00_write(di, 0x4C, 0x00);
	bq27x00_write(di, 0x4D, 0x42);//Ra_6=0042
	bq27x00_write(di, 0x4E, 0x00);
	bq27x00_write(di, 0x4F, 0x48);//Ra_7=0048
	bq27x00_write(di, 0x50, 0x00);
	bq27x00_write(di, 0x51, 0x46);//Ra_8=0046
	bq27x00_write(di, 0x52, 0x00);
	bq27x00_write(di, 0x53, 0x42);//Ra_9=0042
	bq27x00_write(di, 0x54, 0x00);
	bq27x00_write(di, 0x55, 0x5a);//Ra_10=005a
	bq27x00_write(di, 0x56, 0x00);
	bq27x00_write(di, 0x57, 0x66);//Ra_11=0066
	bq27x00_write(di, 0x58, 0x00);
	bq27x00_write(di, 0x59, 0xc9);//Ra_12=00c9
	bq27x00_write(di, 0x5A, 0x02);
	bq27x00_write(di, 0x5B, 0x04);//Ra_13=0204
	bq27x00_write(di, 0x5C, 0x03);
	bq27x00_write(di, 0x5D, 0x41);//Ra_14=0341
	bq27x00_write(di, 0x60, 0x8D);//new checksum=0xBA according to TI
	#endif

}

static bq27x00_calibration(struct bq27x00_device_info *di)
{
	u8 val_1, val_2;
	val_1 = bq27x00_read(di, 0x06, true); //check Flag[5], ITPOR=1 if a cold-reset happens to FG
	val_2 = bq27x00_read(di, 0x3c, true);
	#if 0
	if ((val_1 & 0x20) || (val_2 != 0xcc)){
		pr_info("battery remove, need to calibrate  0x%x..0x%x.\n", val_1, val_2);
		bq27x00_enter_cfg_mode(di);
		bq27x00_cfg_data(di);
		bq27x00_exit_cfg_mode(di);
	}
	else {
		pr_info("[intel] NO NEED to fill out new battery data!!!\n");
	}
	#endif

	#if 1
	pr_info("battery remove, need to calibrate	0x%x..0x%x.\n", val_1, val_2);
	bq27x00_enter_cfg_mode(di);
	bq27x00_cfg_data(di);
	bq27x00_exit_cfg_mode(di);
	#endif
} 

static int bq27x00_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	int num;
	int retval = 0;
	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->bat.name = "bq27441";
	di->bus.read = &bq27x00_read_i2c;
	di->bus.write = &bq27x00_write_reg;
	i2c_set_clientdata(client, di);

	bq27x00_calibration(di);
	if (bq27x00_powersupply_init(di))
		goto batt_failed_3;


	return 0;

batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	bq27x00_powersupply_unregister(di);

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);

	return 0;
}

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27200", BQ27000 },	/* bq27200 is same as bq27000, but with i2c */
	{ "bq27500", BQ27500 },
	{ "bq27441", BQ27441},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27x00_id);

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
};

static inline int bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");

	return ret;
}

static inline void bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

static int __init bq27x00_battery_init(void)
{
	int ret;

	ret = bq27x00_battery_i2c_init();
	if (ret)
		return ret;

	return ret;
}
module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_i2c_exit();
}
module_exit(bq27x00_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
