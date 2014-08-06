/*
 * platform_ov5648.c: ov5648 platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/mfd/intel_mid_pmic.h>
#include <linux/vlv2_plat_clock.h>
#include "platform_camera.h"
#include "platform_ov5648.h"

/* workround - pin defined for byt */
#define CAMERA_0_RESET 126
#define CAMERA_0_PWDN 123
#ifdef CONFIG_VLV2_PLAT_CLK
#define OSC_CAM0_CLK 0x0
#define CLK_19P2MHz 0x1
#endif
#ifdef CONFIG_CRYSTAL_COVE
#define VPROG_2P8V 0x66
#define VPROG_1P8V 0x5D
#define VPROG_ENABLE 0x3
#define VPROG_DISABLE 0x2
#endif
static int camera_vprog0_on;
static int gp_camera0_power_down;
static int gp_camera0_reset;

/*
 * camera sensor - ov5648 platform data
 */

static int ov5648_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	int pin;
	printk("[DEBUG] ov5648_rear_camera_gpio_ctrl()  ++\n");
	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2) {
		if (gp_camera0_reset < 0) {
			ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 1);
			if (ret < 0)
				return ret;
			gp_camera0_reset = ret;
		}
	} else {
		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently.
		 */
		pin = CAMERA_0_RESET;
		if (gp_camera0_reset < 0) {
			ret = gpio_request(pin, "camera_0_reset");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
					__func__, pin);
				return -EINVAL;
			}
		}
		gp_camera0_reset = pin;
		ret = gpio_direction_output(pin, 1);
		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, pin);
			gpio_free(pin);
		}

		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently.
		 */
		pin = CAMERA_0_PWDN;
		if (gp_camera0_power_down < 0) {
			ret = gpio_request(pin, "camera_0_power");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
					__func__, pin);
				return ret;
			}
		}
		gp_camera0_power_down = pin;

			ret = gpio_direction_output(pin, 1);

		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, pin);
			gpio_free(pin);
			return ret;
		}
	}
	if (flag) {
		if (spid.hardware_id == BYT_TABLET_BLK_8PR0)
			gpio_set_value(gp_camera0_power_down, 0);	//intel's patch changing 0 -> 1
		else
			gpio_set_value(gp_camera0_power_down, 0);

		gpio_set_value(gp_camera0_reset, 0);
		msleep(20);
		gpio_set_value(gp_camera0_reset, 1);
	} else {//powr down
		gpio_set_value(gp_camera0_reset, 0);
		if (spid.hardware_id == BYT_TABLET_BLK_8PR0)
			gpio_set_value(gp_camera0_power_down, 1);	//intel's patch changing 1 ->  0
		else
			gpio_set_value(gp_camera0_power_down, 1);
		gpio_free(gp_camera0_reset);
		gpio_free(gp_camera0_power_down);
		gp_camera0_reset = -1;
		gp_camera0_power_down = -1;	
	}
	
	return 0;
}

static int ov5648_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_VLV2_PLAT_CLK
	if (flag) {
		int ret;
		ret = vlv2_plat_set_clock_freq(OSC_CAM0_CLK, CLK_19P2MHz);
		if (ret)
			return ret;
	}
	return vlv2_plat_configure_clock(OSC_CAM0_CLK, flag);
#endif
	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2)
		return intel_scu_ipc_osc_clk(OSC_CLK_CAM0,
					     flag ? clock_khz : 0);
	else
		return 0;
}

/*
 * The power_down gpio pin is to control OV5648's
 * internal power state.
 */
static int ov5648_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	if (flag) {
		if (!camera_vprog0_on) {
			if (intel_mid_identify_cpu() !=
			    INTEL_MID_CPU_CHIP_VALLEYVIEW2)
				ret = intel_scu_ipc_msic_vprog1(1);
#ifdef CONFIG_CRYSTAL_COVE
			/*
			 * This should call VRF APIs.
			 *
			 * VRF not implemented for BTY, so call this
			 * as WAs
			 */
			ret = camera_set_pmic_power(CAMERA_2P8V, true);
			if (ret)
				return ret;
			ret = camera_set_pmic_power(CAMERA_1P8V, true);
#endif
			if (!ret)
				camera_vprog0_on = 1;
			return ret;
		}
	} else {
		if (camera_vprog0_on) {
			if (intel_mid_identify_cpu() !=
			    INTEL_MID_CPU_CHIP_VALLEYVIEW2)
				ret = intel_scu_ipc_msic_vprog1(0);
#ifdef CONFIG_CRYSTAL_COVE
			ret = camera_set_pmic_power(CAMERA_2P8V, false);
			if (ret)
				return ret;
			ret = camera_set_pmic_power(CAMERA_1P8V, false);
#endif
			if (!ret)
				camera_vprog0_on = 0;
			return ret;
		}
	}

	return 0;
}

static int ov5648_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 2;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, LANES,
				 ATOMISP_INPUT_FORMAT_RAW_10,
				 atomisp_bayer_order_bggr, flag);
}

static struct camera_sensor_platform_data ov5648_sensor_platform_data = {
	.gpio_ctrl = ov5648_gpio_ctrl,
	.flisclk_ctrl = ov5648_flisclk_ctrl,
	.power_ctrl = ov5648_power_ctrl,
	.csi_cfg = ov5648_csi_configure,
};

void *ov5648_platform_data(void *info)
{
	gp_camera0_reset = -1;
	gp_camera0_power_down = -1;
	camera_vprog0_on = 0;
	return &ov5648_sensor_platform_data;
}
