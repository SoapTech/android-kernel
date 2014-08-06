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
#include <linux/input.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/input/gt9xx.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>

#define GT9110_RESET_PIN 60
#define GT9110_INT_PIN   (130+12)

static struct i2c_board_info __initdata gt9110_i2c_device = {
	I2C_BOARD_INFO("Goodix-TS", 0x14),
};

struct goodix_9110_platform_data  goodix9110_info = {
        .irq_pin = GT9110_INT_PIN,
        .reset= GT9110_RESET_PIN,
};

static int __init gt9110_i2c_init(void)
{

	gt9110_i2c_device.platform_data = &goodix9110_info;

	return i2c_register_board_info(6, &gt9110_i2c_device, 1);
}

module_init(gt9110_i2c_init);

MODULE_AUTHOR("jim.kuang <jim.kuang@emdoor.com>");
MODULE_DESCRIPTION("GT9110 I2C Touchscreen Platform Driver");
MODULE_LICENSE("GPL");
