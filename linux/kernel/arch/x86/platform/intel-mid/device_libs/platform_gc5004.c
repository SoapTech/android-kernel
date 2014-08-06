/*
 * platform_gc5004.c: gc5004 platform data initilization file
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
#include "platform_gc5004.h"

/* workround - pin defined for byt */
#define CAMERA_0_RESET 126 //meng 0924 :MCSI_GPIO[09]
#define CAMERA_0_PWDN 123 //MCSI_GPIO[06]-active low to power down in P1
#define CAM28_EN 119 //ECS-BYT: MCSI_GPIO[02], camera VDD2.8 control, active high.
#define AFVCC28_EN 118 //ECS-BYT: MCSI_GPIO[01], camera VCM2.8 control, active high.
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
static int camera_reset;
static int camera_power_down;
static int camera_vprog1_on;
static int camera_p28_en;
static int camera_vcm28_en;



/*
 * camera sensor - gc5004 platform data
 */

static int gc5004_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	int pin;

	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_VALLEYVIEW2) {
		if (camera_reset < 0) {
			ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 1);
			if (ret < 0)
				return ret;
			camera_reset = ret;
		}
	} else {
		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently
		 */
		if (camera_reset < 0) {
			ret = gpio_request(CAMERA_0_RESET, "camera_0_reset");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
				__func__, CAMERA_0_RESET);
				return -EINVAL;
			}
		}
		camera_reset = CAMERA_0_RESET;
		ret = gpio_direction_output(camera_reset, 1);
		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, camera_reset);
			gpio_free(camera_reset);
		}

		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently.
		 */
		pin = CAMERA_0_PWDN;
		if (camera_power_down < 0) {
			ret = gpio_request(pin, "camera_0_power");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
					__func__, pin);
				return ret;
			}
		}
		camera_power_down = pin;
			ret = gpio_direction_output(pin, 1);

		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, pin);
			gpio_free(pin);
			return ret;
		}

		
	}
	if (flag) {
		gpio_set_value(camera_power_down, 0);
		
		gpio_set_value(camera_reset, 1);

	} else {
		gpio_set_value(camera_power_down, 1);
		gpio_set_value(camera_reset, 0);
		gpio_free(camera_power_down);
		gpio_free(camera_reset);
		camera_power_down = -1;
		camera_reset = -1;
	}

	return 0;
}

static int gc5004_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
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
 * The power_down gpio pin is to control OV5693's
 * internal power state.
 */
static int gc5004_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	if (camera_p28_en < 0) {
		ret = gpio_request(CAM28_EN, "camera_0_power");
		if (ret) {
			pr_err("%s: failed to request gpio(pin %d)\n",
				__func__, CAM28_EN);
			return ret;
		}
	}
	camera_p28_en = CAM28_EN;
	ret = gpio_direction_output(camera_p28_en, 1);
        if (ret) {
                pr_err("%s: failed to set gpio(pin %d) direction\n",
                        __func__, camera_p28_en);
                gpio_free(camera_p28_en);
		camera_p28_en = -1;
                return ret;
        }

	if (camera_vcm28_en < 0) {
                ret = gpio_request(AFVCC28_EN, "camera_vcm_power");
                if (ret) {
                        pr_err("%s: failed to request gpio(pin %d)\n",
                                __func__, AFVCC28_EN);
                        return ret;
                }
        }
        camera_vcm28_en = AFVCC28_EN;
        ret = gpio_direction_output(camera_vcm28_en, 1);
	if (ret) {
		pr_err("%s: failed to set gpio(pin %d) direction\n",
			__func__, camera_vcm28_en);
		gpio_free(camera_vcm28_en);
		camera_vcm28_en = -1;
		return ret;
	}

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
			ret = camera_set_pmic_power(CAMERA_1P8V, true);
			if (ret)
				return ret;
			ret = camera_set_pmic_power(CAMERA_2P8V, true);
#endif
			if (!ret)
				camera_vprog1_on = 1;
			gpio_set_value(camera_p28_en,1);
			gpio_set_value(camera_vcm28_en, 1);
			msleep(10);
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
			gpio_set_value(camera_p28_en,0);
			gpio_free(camera_p28_en);
			camera_p28_en = -1;
			gpio_set_value(camera_vcm28_en, 0);
			gpio_free(camera_vcm28_en);
			camera_vcm28_en = -1;
			return ret;
		}
	}
	return ret;
}

static int gc5004_csi_configure(struct v4l2_subdev *sd, int flag)
{
    static const int LANES = 2;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_gbrg, flag); 
}

static struct camera_sensor_platform_data gc5004_sensor_platform_data = {
	.gpio_ctrl	= gc5004_gpio_ctrl,
	.flisclk_ctrl	= gc5004_flisclk_ctrl,
	.power_ctrl	= gc5004_power_ctrl,
	.csi_cfg	= gc5004_csi_configure,
};

void *gc5004_platform_data(void *info)
{
	camera_reset = -1;
	camera_power_down = -1;
	camera_vprog1_on = 0;
	camera_p28_en = -1;
	camera_vcm28_en = -1;

	return &gc5004_sensor_platform_data;
}
