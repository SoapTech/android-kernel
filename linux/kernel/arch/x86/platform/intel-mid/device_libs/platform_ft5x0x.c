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
#include <linux/input/ft5x0x_ts.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>

#define FT5X0X_RESET_PIN 60
#define FT5X0X_INT_PIN   (130+12)

static struct i2c_board_info __initdata ft5x0x_i2c_device = {
	I2C_BOARD_INFO("ft5x0x", 0x38),
};

static struct ft5x0x_platform_data ft5x0x_platform_data = {
	.irq = FT5X0X_INT_PIN,
	.reset = FT5X0X_RESET_PIN,
	.x_max = 800,
	.y_max = 1280,
};

static int __init ft5x0x_i2c_init(void)
{

	ft5x0x_i2c_device.platform_data = &ft5x0x_platform_data;

	return i2c_register_board_info(6, &ft5x0x_i2c_device, 1);
}

module_init(ft5x0x_i2c_init);

MODULE_AUTHOR("Wenzeng Chen <wenzeng.chen@intel.com>");
MODULE_DESCRIPTION("FT5x0x I2C Touchscreen Platform Driver");
MODULE_LICENSE("GPL");
