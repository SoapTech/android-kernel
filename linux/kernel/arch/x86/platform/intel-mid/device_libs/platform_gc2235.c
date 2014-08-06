/*
 * platform_gc2235.c: gc2235 platform data initilization file
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
#include "platform_gc2235.h"

/* workround - pin defined for byt */
#define CAMERA_1_RESET 127
#define CAMERA_1_PWDN 124
#ifdef CONFIG_VLV2_PLAT_CLK
#define OSC_CAM1_CLK 0x1
#define CLK_19P2MHz 0x1
#endif
#ifdef CONFIG_CRYSTAL_COVE
#define VPROG_2P8V 0x66
#define VPROG_1P8V 0x5D
#define VPROG_ENABLE 0x3
#define VPROG_DISABLE 0x2
#endif
static int camera_vprog1_on;
static int gp_camera1_power_down;
static int gp_camera1_reset;

/*
 * camera sensor - gc2235 platform data
 */

static int gc2235_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	int pin;

	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2) {
		if (gp_camera1_reset < 0) {
			ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
					GPIOF_DIR_OUT, 1);
			if (ret < 0)
				return ret;
			gp_camera1_reset = ret;
		}
	} else {
		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently
		 */
		pin = CAMERA_1_RESET;
		if (gp_camera1_reset < 0) {
			ret = gpio_request(pin, "camera_1_reset");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
					__func__, pin);
				return -EINVAL;
			}
		}
		gp_camera1_reset = pin;
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
		pin = CAMERA_1_PWDN;
		if (gp_camera1_power_down < 0) {
			ret = gpio_request(pin, "camera_1_power");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
					__func__, pin);
				return ret;
			}
		}
		gp_camera1_power_down = pin;

		if (spid.hardware_id == BYT_TABLET_BLK_8PR0)
			ret = gpio_direction_output(pin, 1);
		else
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
			gpio_set_value(gp_camera1_power_down, 1);
		else
			gpio_set_value(gp_camera1_power_down, 1);

		gpio_set_value(gp_camera1_reset, 0);
		msleep(20);
		gpio_set_value(gp_camera1_reset, 1);
	} else {
		gpio_set_value(gp_camera1_reset, 0);
		if (spid.hardware_id == BYT_TABLET_BLK_8PR0)
			gpio_set_value(gp_camera1_power_down, 0);
		else
			gpio_set_value(gp_camera1_power_down, 0);
		gpio_free(gp_camera1_reset);
		gpio_free(gp_camera1_power_down);
		gp_camera1_reset = -1;
		gp_camera1_power_down = -1;	
	}

	//printk("[DEBUG] gc2235_gpio_ctrl()  gp_camera0_power_down:%d \n",gpio_get_value(gp_camera1_power_down));
	//printk("[DEBUG] gc2235_gpio_ctrl()  gp_camera0eset:%d \n",gpio_get_value(gp_camera1_reset));
	return 0;
}

static int gc2235_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_VLV2_PLAT_CLK
	if (flag) {
		int ret;
		ret = vlv2_plat_set_clock_freq(OSC_CAM1_CLK, CLK_19P2MHz);
		if (ret)
			return ret;
	}
	return vlv2_plat_configure_clock(OSC_CAM1_CLK, flag);
#endif
	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2)
		return intel_scu_ipc_osc_clk(OSC_CLK_CAM1,
			flag ? clock_khz : 0);
	else
		return 0;
}

/*
 * The power_down gpio pin is to control gc2235's
 * internal power state.
 */
static int gc2235_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	if (flag) {
		if (!camera_vprog1_on) {
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
				camera_vprog1_on = 1;
			return ret;
		}
	} else {
		if (camera_vprog1_on) {
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
				camera_vprog1_on = 0;
			return ret;
		}
	}

	return 0;
}

static int gc2235_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 1;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_gbrg, flag);
}

static struct camera_sensor_platform_data gc2235_sensor_platform_data = {
	.gpio_ctrl	= gc2235_gpio_ctrl,
	.flisclk_ctrl	= gc2235_flisclk_ctrl,
	.power_ctrl	= gc2235_power_ctrl,
	.csi_cfg	= gc2235_csi_configure,
};

void *gc2235_platform_data(void *info)
{
	gp_camera1_power_down = -1;
	gp_camera1_reset = -1;
	return &gc2235_sensor_platform_data;
}
