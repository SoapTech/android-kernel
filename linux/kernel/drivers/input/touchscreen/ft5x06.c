/*
 * Copyright (C) 2013  wenzengc <wenzeng.chen@intel.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/input/ft5x06.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define MAX_SUPPORT_POINTS		5
#define EACH_POINT_LEN			6

#define FT5X06_REG_DEVIDE_MODE  	0x00
#define FT5X06_REG_ROW_ADDR             0x01
#define FT5X06_REG_TD_STATUS            0x02
#define FT5X06_REG_START_SCAN           0x02
#define FT5X06_REG_TOUCH_START  	0x03
#define FT5X06_REG_VOLTAGE              0x05
#define FT5X06_REG_CALB			0xA0
#define FT5X06_ID_G_PMODE		0xA5
#define FT5X06_REG_FW_VER     		0xA6
#define FT5X06_ID_G_FT5201ID            0xA8
#define FT5X06_NOISE_FILTER             0xB5
#define FT5X06_REG_POINT_RATE           0x88
#define FT5X06_REG_THGROUP              0x80

#define TOUCH_EVENT_DOWN		0x00
#define TOUCH_EVENT_UP			0x01
#define TOUCH_EVENT_ON			0x02
#define TOUCH_EVENT_RESERVED		0x03

#define MXT_WAKEUP_TIME			10	/*msec */

#define FT_1664S_NAME		"FT1664"
#define FT_3432S_NAME 		"FT3432"

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_early_suspend(struct early_suspend *es);
static void ft5x06_late_resume(struct early_suspend *es);
#endif

struct ft5x06_ts_data {
	struct i2c_client *client;
	struct input_dev *input;
	u16 num_x;
	u16 num_y;

	struct mutex mutex;
	int threshold;
	int gain;
	int offset;
	int report_rate;
	int fw_version;
#ifdef CONFIG_HAS_EARLYSUSPEND
        struct early_suspend early_suspend;
#endif

};

static int ft5x06_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int ft5x06_read_block(struct i2c_client *client, u8 reg,
			     u8 len, u8 * buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int ft5x06_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	int ret;
	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static irqreturn_t ft5x06_ts_isr(int irq, void *dev_id)
{
	struct ft5x06_ts_data *tsdata = dev_id;
	struct device *dev = &tsdata->client->dev;
	u8 rdbuf[31], number, touchpoint=0;
	int i, type, x, y, id, press;

	memset(rdbuf, 0, sizeof(rdbuf));
	mutex_lock(&tsdata->mutex);

	ft5x06_read_block(tsdata->client, FT5X06_REG_TD_STATUS,
			31, rdbuf);
	number = rdbuf[0] & 0x0f;
	if(number >= 5)
		number = 5;

	for (i = 0; i < 5; i++) {
		u8 *buf = &rdbuf[EACH_POINT_LEN * i + 1];
		bool down;

		type = (buf[0] >> 6) & 0x03;
		/* ignore Reserved events */
		if (type == TOUCH_EVENT_RESERVED)
			continue;

		x = ((buf[0] << 8) | buf[1]) & 0x0fff;
		y = ((buf[2] << 8) | buf[3]) & 0x0fff;
		id = (buf[2] >> 4) & 0x0f;
		down = (type != TOUCH_EVENT_UP);
		printk("id = %d..., x value = %d...., y value = %d......number = %d..... down = %d.....\n", id, x, y, number, down);
		input_mt_slot(tsdata->input, id);
		input_mt_report_slot_state(tsdata->input, MT_TOOL_FINGER, down);

		if (!down)
			continue;

		press = 0xff;
		touchpoint ++;
		input_report_abs(tsdata->input, ABS_MT_POSITION_X, 768-x);
		input_report_abs(tsdata->input, ABS_MT_POSITION_Y, 1024-y);
		input_report_abs(tsdata->input, ABS_MT_TOUCH_MAJOR, press);

	}

	if(touchpoint == number)
		input_report_key(tsdata->input, BTN_TOUCH, touchpoint > 0);
	else
		input_report_key(tsdata->input, BTN_TOUCH, 0);
	input_sync(tsdata->input);
	mutex_unlock(&tsdata->mutex);
	return IRQ_HANDLED;
}

static int ft5x06_ts_reset(struct i2c_client *client, int reset_pin)
{
	int error;

	if (gpio_is_valid(reset_pin)) {
		/* this pulls reset down, enabling the low active reset */
		error = gpio_request_one(reset_pin, GPIOF_OUT_INIT_LOW,
					 "ft5x06 reset");
		if (error) {
			dev_err(&client->dev,
				"Failed to request GPIO %d as reset pin, error %d\n",
				reset_pin, error);
			return error;
		}

		msleep(10);
		gpio_set_value(reset_pin, 1);
		msleep(20);
		gpio_free(reset_pin);
	}

	return 0;
}


static ssize_t ft5x06_thrsh_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	u8 value;
	u8 rdbuf[30] = {0};
	size_t count = 0;
	int i=0;

	mutex_lock(&tsdata->mutex);
	value = ft5x06_read_reg(client, FT5X06_REG_THGROUP);
	count = scnprintf(buf, PAGE_SIZE, "%x\n", value);

	mutex_unlock(&tsdata->mutex);

	return count;
}

static ssize_t ft5x06_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	u8 value;
	size_t count = 0;

	mutex_lock(&tsdata->mutex);
	value = ft5x06_read_reg(client, FT5X06_REG_POINT_RATE);
	count = scnprintf(buf, PAGE_SIZE, "%x\n", value);
	mutex_unlock(&tsdata->mutex);

	return count;

}

static ssize_t ft5x06_rawdata_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{

}

static ssize_t ft5x06_thrsh_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	u8 value;
	int error;

	mutex_lock(&tsdata->mutex);
	error = kstrtouint(buf, 0, &value);
	if (error)
		goto out;

	ft5x06_write_reg(client, FT5X06_REG_THGROUP, value);

out:
	mutex_unlock(&tsdata->mutex);
	return error ? : count;

}

static ssize_t ft5x06_rate_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);
	u8 value;
	int error;

	mutex_lock(&tsdata->mutex);
	error = kstrtouint(buf, 0, &value);
	if (error)
		goto out;

	ft5x06_write_reg(client, FT5X06_REG_POINT_RATE, value);
out:
	mutex_unlock(&tsdata->mutex);
	return error ? : count;

}

/* sysfs */
static DEVICE_ATTR(threshold, 0644, ft5x06_thrsh_show, ft5x06_thrsh_store);
static DEVICE_ATTR(report_rate, 0644, ft5x06_rate_show, ft5x06_rate_store);
static DEVICE_ATTR(rawdata, 0644, ft5x06_rawdata_show, NULL);

static struct attribute *ft5x06_attrs[] = {
	&dev_attr_threshold.attr,
	&dev_attr_report_rate.attr,
	&dev_attr_rawdata.attr,
	NULL
};

static const struct attribute_group ft5x06_attr_group = {
	.attrs = ft5x06_attrs
};

static int ft5x06_config(struct ft5x06_ts_data *tsdata)
{
	/* enable auto calibration */
	ft5x06_write_reg(tsdata->client, FT5X06_REG_CALB, 0x00);
	msleep(100);
	if (tsdata->threshold)
		ft5x06_write_reg(tsdata->client,
				 FT5X06_REG_THGROUP, tsdata->threshold);
	if (tsdata->report_rate)
		ft5x06_write_reg(tsdata->client,
				 FT5X06_REG_POINT_RATE, tsdata->report_rate);
	return 0;
}


static int ft5x06_get_defaults(struct ft5x06_ts_data *tsdata)
{
	u8 version = 0, threshold = 0, rate = 0;

	threshold = ft5x06_read_reg(tsdata->client, FT5X06_REG_THGROUP);
	rate = ft5x06_read_reg(tsdata->client, FT5X06_REG_POINT_RATE);
	version = ft5x06_read_reg(tsdata->client, FT5X06_REG_FW_VER);
	tsdata->fw_version = version;

	dev_info(&tsdata->client->dev,
		 "Rev.%02x, Thold.%x, Rate.%x\n", version, threshold, rate);

	return 0;
}

static int ft5x06_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	const struct ft5x06_platform_data *pdata = client->dev.platform_data;
	struct ft5x06_ts_data *tsdata;
	struct input_dev *input;
	int error;
	dev_info(&client->dev, "ft5x06....probe......\n");

	if (!pdata) {
		dev_err(&client->dev, "no platform data?\n");
		return -EINVAL;
	}
	error = ft5x06_ts_reset(client, pdata->reset_pin);
	if (error)
		return error;

	if (gpio_is_valid(pdata->irq_pin)) {
		error = gpio_request_one(pdata->irq_pin,
					 GPIOF_IN, "ft5x06 irq");
		if (error) {
			dev_err(&client->dev,
				"Failed to request GPIO %d, error %d\n",
				pdata->irq_pin, error);
			return error;
		}
	}

	tsdata = kzalloc(sizeof(*tsdata), GFP_KERNEL);
	input = input_allocate_device();
	if (!tsdata || !input) {
		dev_err(&client->dev, "failed to allocate driver data.\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	mutex_init(&tsdata->mutex);
	tsdata->client = client;
	tsdata->input = input;
	tsdata->num_x = pdata->x_max;
	tsdata->num_y = pdata->y_max;
	tsdata->threshold = pdata->threshold;
	tsdata->report_rate = pdata->report_rate;

	ft5x06_get_defaults(tsdata);
	ft5x06_config(tsdata);

	input->name = client->name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);
	__set_bit(BTN_TOUCH, input->keybit);
	input_mt_init_slots(input, MAX_SUPPORT_POINTS);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, tsdata->num_x, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, tsdata->num_y, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_set_drvdata(input, tsdata);
	i2c_set_clientdata(client, tsdata);
	client->irq = gpio_to_irq(pdata->irq_pin);
	//client->irq = 69;
	dev_info(&client->dev, "ft5x06 client irq = %d......\n", client->irq);
	gpio_direction_input(pdata->irq_pin);
	error = request_threaded_irq(client->irq, NULL, ft5x06_ts_isr,
				     IRQF_TRIGGER_FALLING |IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				     client->name, tsdata);
	if (error) {
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		goto err_free_mem;
	}
	error = input_register_device(input);
	if (error)
		goto err_free_irq;

	device_init_wakeup(&client->dev, 1);

	error = sysfs_create_group(&client->dev.kobj, &ft5x06_attr_group);
	if (error) {
		dev_err(&client->dev, "fail to export sysfs entires\n");
		goto err_free_irq;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
        tsdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        tsdata->early_suspend.suspend = ft5x06_early_suspend;
        tsdata->early_suspend.resume = ft5x06_late_resume;
        register_early_suspend(&tsdata->early_suspend);
#endif
	dev_info(&client->dev,
		 "EDT FT5x06 initialized: IRQ pin %d, Reset pin %d.\n",
		 pdata->irq_pin, pdata->reset_pin);
	return 0;

err_free_irq:
	free_irq(client->irq, tsdata);
err_free_mem:
	input_free_device(input);
	kfree(tsdata);

	if (gpio_is_valid(pdata->irq_pin))
		gpio_free(pdata->irq_pin);

	return error;
}

static int ft5x06_ts_remove(struct i2c_client *client)
{
	const struct ft5x06_platform_data *pdata =
	    dev_get_platdata(&client->dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &ft5x06_attr_group);
	free_irq(client->irq, tsdata);
	input_unregister_device(tsdata->input);

	if (gpio_is_valid(pdata->irq_pin))
		gpio_free(pdata->irq_pin);
	kfree(tsdata);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ft5x06_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_ts_data *tsdata = i2c_get_clientdata(client);

	mutex_lock(&tsdata->mutex);
	disable_irq(client->irq);
	/*set to hibernate mode. */
	ft5x06_write_reg(client, FT5X06_ID_G_PMODE, 0x03);
	mutex_unlock(&tsdata->mutex);
	return 0;
}

static int ft5x06_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x06_platform_data *pdata = client->dev.platform_data;

	/*reset to enter active mode. */
	ft5x06_ts_reset(client, pdata->reset_pin);
	enable_irq(client->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ft5x06_ts_pm_ops, ft5x06_ts_suspend, ft5x06_ts_resume);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_early_suspend(struct early_suspend *es)
{
	struct ft5x06_ts_data *tsdata;
	tsdata =container_of(es, struct ft5x06_ts_data, early_suspend);

	mutex_lock(&tsdata->mutex);
	disable_irq(tsdata->client->irq);
        /*set to hibernate mode. */
	ft5x06_write_reg(tsdata->client, FT5X06_ID_G_PMODE, 0x03);
        mutex_unlock(&tsdata->mutex);
}

static void ft5x06_late_resume(struct early_suspend *es)
{
	struct ft5x06_ts_data *tsdata;
	struct ft5x06_platform_data *pdata;

	tsdata =container_of(es, struct ft5x06_ts_data, early_suspend);
	pdata = tsdata->client->dev.platform_data;

	/*reset to enter active mode */
	ft5x06_ts_reset(tsdata->client, pdata->reset_pin);
	enable_irq(tsdata->client->irq);
}
#endif

static const struct i2c_device_id ft5x06_ts_id[] = {
	{"ft5x06", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

static struct acpi_device_id ft5x06_acpi_match[] = {
	{FT_1664S_NAME, 0},
	{FT_3432S_NAME, 0},
	{},
};

static struct i2c_driver ft5x06_ts_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "ft5x06",
		   .pm = &ft5x06_ts_pm_ops,
		   .acpi_match_table = ACPI_PTR(ft5x06_acpi_match),
		   },
	.id_table = ft5x06_ts_id,
	.probe = ft5x06_ts_probe,
	.remove = ft5x06_ts_remove,
};

static int __init ft5x06_init(void)
{
	int ret;
	ret = i2c_add_driver(&ft5x06_ts_driver);
	if (ret)
		printk(KERN_ERR "Unable to register ft5x06 i2c driver\n");

	return ret;
}

late_initcall(ft5x06_init);

static void __exit ft5x06_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}

module_exit(ft5x06_exit);

MODULE_AUTHOR("Wenzeng Chen <wenzeng.chen@intel.com>");
MODULE_DESCRIPTION("FT5x06 I2C Touchscreen Driver");
MODULE_LICENSE("GPL");
