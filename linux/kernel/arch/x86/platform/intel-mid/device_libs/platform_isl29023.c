#include <linux/input/isl29023.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <asm/intel-mid.h>

#define ALS_INT_GPIO	3

static struct isl29023_i2c_platform_data isl29023_pdata;

static struct i2c_board_info __initdata isl29023_i2c_device = {
	I2C_BOARD_INFO("isl29023", 0x44),
	.platform_data = &isl29023_pdata,
};

static int __init als_i2c_init(void)
{
	int ret = 0;

	pr_warn("<tao>als isl29023 i2c init begin...\n");
	ret = gpio_request(ALS_INT_GPIO, "isl29023-int");
	if (ret) {
		pr_err("unable to request GPIO pin for als sensor\n");
		isl29023_pdata.irq = -1;
		return -ENXIO;
	} else {
		gpio_direction_input(ALS_INT_GPIO);
		isl29023_pdata.irq_gpio = ALS_INT_GPIO;
		isl29023_i2c_device.irq = gpio_to_irq(ALS_INT_GPIO);
	}

	return i2c_register_board_info(5, &isl29023_i2c_device, 1);
}

module_init(als_i2c_init);
