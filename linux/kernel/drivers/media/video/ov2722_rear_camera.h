/*
 * Support for OmniVision OV2722 1080p HD camera sensor.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __OV2722_H__
#define __OV2722_H__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>

#include <linux/atomisp_platform.h>

#define OV2722_NAME		"ov272r"

/* Defines for register writes and register array processing */
#define I2C_MSG_LENGTH		0x2
#define I2C_RETRY_COUNT		5

#define OV2722_FOCAL_LENGTH_NUM	278	/*2.78mm*/
#define OV2722_FOCAL_LENGTH_DEM	100
#define OV2722_F_NUMBER_DEFAULT_NUM	26
#define OV2722_F_NUMBER_DEM	10

#define MAX_FMTS		1

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV2722_FOCAL_LENGTH_DEFAULT 0x1160064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV2722_F_NUMBER_DEFAULT 0x1a000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV2722_F_NUMBER_RANGE 0x1a0a1a0a
#define OV2720_ID	0x2720
#define OV2722_ID	0x2722

#define OV2722_FINE_INTG_TIME_MIN 0
#define OV2722_FINE_INTG_TIME_MAX_MARGIN 0
#define OV2722_COARSE_INTG_TIME_MIN 1
#define OV2722_COARSE_INTG_TIME_MAX_MARGIN (0xffff - 6)

/*
 * OV2722 System control registers
 */
#define OV2722_SW_SLEEP				0x0100
#define OV2722_SW_RESET				0x0103
#define OV2722_SW_STREAM			0x0100

#define OV2722_SC_CMMN_CHIP_ID_H		0x300A
#define OV2722_SC_CMMN_CHIP_ID_L		0x300B
#define OV2722_SC_CMMN_SCCB_ID			0x300C
#define OV2722_SC_CMMN_SUB_ID			0x302A /* process, version*/

#define OV2722_SC_CMMN_PAD_OEN0			0x3000
#define OV2722_SC_CMMN_PAD_OEN1			0x3001
#define OV2722_SC_CMMN_PAD_OEN2			0x3002
#define OV2722_SC_CMMN_PAD_OUT0			0x3008
#define OV2722_SC_CMMN_PAD_OUT1			0x3009
#define OV2722_SC_CMMN_PAD_OUT2			0x300D
#define OV2722_SC_CMMN_PAD_SEL0			0x300E
#define OV2722_SC_CMMN_PAD_SEL1			0x300F
#define OV2722_SC_CMMN_PAD_SEL2			0x3010

#define OV2722_SC_CMMN_PAD_PK			0x3011
#define OV2722_SC_CMMN_A_PWC_PK_O_13		0x3013
#define OV2722_SC_CMMN_A_PWC_PK_O_14		0x3014

#define OV2722_SC_CMMN_CLKRST0			0x301A
#define OV2722_SC_CMMN_CLKRST1			0x301B
#define OV2722_SC_CMMN_CLKRST2			0x301C
#define OV2722_SC_CMMN_CLKRST3			0x301D
#define OV2722_SC_CMMN_CLKRST4			0x301E
#define OV2722_SC_CMMN_CLKRST5			0x3005
#define OV2722_SC_CMMN_PCLK_DIV_CTRL		0x3007
#define OV2722_SC_CMMN_CLOCK_SEL		0x3020
#define OV2722_SC_SOC_CLKRST5			0x3040

#define OV2722_SC_CMMN_PLL_CTRL0		0x3034
#define OV2722_SC_CMMN_PLL_CTRL1		0x3035
#define OV2722_SC_CMMN_PLL_CTRL2		0x3039
#define OV2722_SC_CMMN_PLL_CTRL3		0x3037
#define OV2722_SC_CMMN_PLL_MULTIPLIER		0x3036
#define OV2722_SC_CMMN_PLL_DEBUG_OPT		0x3038
#define OV2722_SC_CMMN_PLLS_CTRL0		0x303A
#define OV2722_SC_CMMN_PLLS_CTRL1		0x303B
#define OV2722_SC_CMMN_PLLS_CTRL2		0x303C
#define OV2722_SC_CMMN_PLLS_CTRL3		0x303D

#define OV2722_SC_CMMN_MIPI_PHY_16		0x3016
#define OV2722_SC_CMMN_MIPI_PHY_17		0x3017
#define OV2722_SC_CMMN_MIPI_SC_CTRL_18		0x3018
#define OV2722_SC_CMMN_MIPI_SC_CTRL_19		0x3019
#define OV2722_SC_CMMN_MIPI_SC_CTRL_21		0x3021
#define OV2722_SC_CMMN_MIPI_SC_CTRL_22		0x3022

#define OV2722_AEC_PK_EXPO_H			0x3500
#define OV2722_AEC_PK_EXPO_M			0x3501
#define OV2722_AEC_PK_EXPO_L			0x3502
#define OV2722_AEC_MANUAL_CTRL			0x3503
#define OV2722_AGC_ADJ_H			0x3508
#define OV2722_AGC_ADJ_L			0x3509
#define OV2722_VTS_DIFF_H			0x350c
#define OV2722_VTS_DIFF_L			0x350d
#define OV2722_GROUP_ACCESS			0x3208

#define OV2722_MWB_GAIN_R_H			0x5186
#define OV2722_MWB_GAIN_R_L			0x5187
#define OV2722_MWB_GAIN_G_H			0x5188
#define OV2722_MWB_GAIN_G_L			0x5189
#define OV2722_MWB_GAIN_B_H			0x518a
#define OV2722_MWB_GAIN_B_L			0x518b

#define OV2722_H_CROP_START_H			0x3800
#define OV2722_H_CROP_START_L			0x3801
#define OV2722_V_CROP_START_H			0x3802
#define OV2722_V_CROP_START_L			0x3803
#define OV2722_H_CROP_END_H			0x3804
#define OV2722_H_CROP_END_L			0x3805
#define OV2722_V_CROP_END_H			0x3806
#define OV2722_V_CROP_END_L			0x3807
#define OV2722_H_OUTSIZE_H			0x3808
#define OV2722_H_OUTSIZE_L			0x3809
#define OV2722_V_OUTSIZE_H			0x380a
#define OV2722_V_OUTSIZE_L			0x380b

#define OV2722_START_STREAMING			0x01
#define OV2722_STOP_STREAMING			0x00

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct ov2722_resolution {
	u8 *desc;
	const struct ov2722_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	int pix_clk_freq;
	u32 skip_frames;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	u8 bin_mode;
	bool used;
};

struct ov2722_format {
	u8 *desc;
	u32 pixelformat;
	struct ov2722_reg *regs;
};

struct ov2722_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/*
 * ov2722 device structure.
 */
struct ov2722_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct mutex input_lock;

	struct camera_sensor_platform_data *platform_data;
	int vt_pix_clk_freq_mhz;
	int fmt_idx;
	int run_mode;
	int R_gain;
	int G_gain;
	int B_gain;
	int pre_digitgain;
	u8 res;
	u8 type;
};

enum ov2722_tok_type {
	OV2722_8BIT  = 0x0001,
	OV2722_16BIT = 0x0002,
	OV2722_32BIT = 0x0004,
	OV2722_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	OV2722_TOK_DELAY  = 0xfe00,	/* delay token for reg list */
	OV2722_TOK_MASK = 0xfff0
};

/**
 * struct ov2722_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct ov2722_reg {
	enum ov2722_tok_type type;
	u16 reg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

#define to_ov2722_sensor(x) container_of(x, struct ov2722_device, sd)

#define OV2722_MAX_WRITE_BUF_SIZE	30

struct ov2722_write_buffer {
	u16 addr;
	u8 data[OV2722_MAX_WRITE_BUF_SIZE];
};

struct ov2722_write_ctrl {
	int index;
	struct ov2722_write_buffer buffer;
};

static const struct i2c_device_id ov2722_id[] = {
	{OV2722_NAME, 0},
	{}
};

/*
 * Register settings for various resolution
 */
static struct ov2722_reg const ov2722_QVGA_30fps[] = {
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x4a}, /* H crop start: 330 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x03}, /* V crop start: 3 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xe1}, /* H crop end:  1761 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x47}, /* V crop end:  1096 */
	{OV2722_8BIT, 0x3808, 0x01},
	{OV2722_8BIT, 0x3809, 0x50}, /* H output size: 336 */
	{OV2722_8BIT, 0x380a, 0x01},
	{OV2722_8BIT, 0x380b, 0x00}, /* V output size: 256 */

	/* H blank timing */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00}, /* H total size: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa0}, /* V total size: 1184 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x05}, /* H window offset: 5 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /* flip isp*/
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3000, 0xff},
	{OV2722_8BIT, 0x3001, 0xff},
	{OV2722_8BIT, 0x3002, 0xf0},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1}, /* v_en, h_en, blc_en */
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf},
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0}, /* AWB red */
	{OV2722_8BIT, 0x5184, 0xb0}, /* AWB green */
	{OV2722_8BIT, 0x5185, 0xb0}, /* AWB blue */
	{OV2722_8BIT, 0x5180, 0x03}, /* AWB manual mode */
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x4800, 0x24}, /* clk lane gate enable */
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26},
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},

	/* Added for power optimization */
	{OV2722_8BIT, 0x3000, 0x00},
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x46},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x10},
	{OV2722_TOK_TERM, 0, 0},
};

static struct ov2722_reg const ov2722_VGA_30fps[] = {
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x4a}, /* H crop start: 330 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x03}, /* V crop start: 3 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xe1}, /* H crop end:  1761 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x47}, /* V crop end:  1096 */
	{OV2722_8BIT, 0x3808, 0x02},
	{OV2722_8BIT, 0x3809, 0x90}, /* H output size: 656 */
	{OV2722_8BIT, 0x380a, 0x01},
	{OV2722_8BIT, 0x380b, 0xF0}, /* V output size: 496 */

	/* H blank timing */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00}, /* H total size: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa0}, /* V total size: 1184 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x05}, /* H window offset: 5 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /* flip isp*/
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3000, 0xff},
	{OV2722_8BIT, 0x3001, 0xff},
	{OV2722_8BIT, 0x3002, 0xf0},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1}, /* v_en, h_en, blc_en */
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf},
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0}, /* AWB red */
	{OV2722_8BIT, 0x5184, 0xb0}, /* AWB green */
	{OV2722_8BIT, 0x5185, 0xb0}, /* AWB blue */
	{OV2722_8BIT, 0x5180, 0x03}, /* AWB manual mode */
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x4800, 0x24}, /* clk lane gate enable */
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26},
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},

	/* Added for power optimization */
	{OV2722_8BIT, 0x3000, 0x00},
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x46},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x10},
	{OV2722_TOK_TERM, 0, 0},
};
static struct ov2722_reg const ov2722_QCIF_30fps[] = {
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x4a}, /* H crop start: 330 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x03}, /* V crop start: 3 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xe1}, /* H crop end:  1761 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x47}, /* V crop end:  1096 */
	{OV2722_8BIT, 0x3808, 0x00},
	{OV2722_8BIT, 0x3809, 0xc0}, /* H output size: 192 */
	{OV2722_8BIT, 0x380a, 0x00},
	{OV2722_8BIT, 0x380b, 0xa0}, /* V output size: 160 */

	/* H blank timing */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00}, /* H total size: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa0}, /* V total size: 1184 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x05}, /* H window offset: 5 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /* flip isp*/
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3000, 0xff},
	{OV2722_8BIT, 0x3001, 0xff},
	{OV2722_8BIT, 0x3002, 0xf0},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1}, /* v_en, h_en, blc_en */
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf},
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0}, /* AWB red */
	{OV2722_8BIT, 0x5184, 0xb0}, /* AWB green */
	{OV2722_8BIT, 0x5185, 0xb0}, /* AWB blue */
	{OV2722_8BIT, 0x5180, 0x03}, /* AWB manual mode */
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x4800, 0x24}, /* clk lane gate enable */
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26},
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},

	/* Added for power optimization */
	{OV2722_8BIT, 0x3000, 0x00},
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x46},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x10},
	{OV2722_TOK_TERM, 0, 0},
};

static struct ov2722_reg const ov2722_CIF_30fps[] = {
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x4a}, /* H crop start: 330 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x03}, /* V crop start: 3 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xe1}, /* H crop end:  1761 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x47}, /* V crop end:  1096 */
	{OV2722_8BIT, 0x3808, 0x01},
	{OV2722_8BIT, 0x3809, 0x70}, /* H output size: 368 */
	{OV2722_8BIT, 0x380a, 0x01},
	{OV2722_8BIT, 0x380b, 0x30}, /* V output size: 304 */

	/* H blank timing */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00},	/* H total size: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa0},	/* V total size: 1184 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x05},	/* H window offset: 5 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02},	/* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06},	/* flip isp */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3000, 0xff},
	{OV2722_8BIT, 0x3001, 0xff},
	{OV2722_8BIT, 0x3002, 0xf0},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2},	/* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},	/* v_en, h_en, blc_en */
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf},
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},	/* AWB red */
	{OV2722_8BIT, 0x5184, 0xb0},	/* AWB green */
	{OV2722_8BIT, 0x5185, 0xb0},	/* AWB blue */
	{OV2722_8BIT, 0x5180, 0x03},	/* AWB manual mode */
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x4800, 0x24},	/* clk lane gate enable */
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26},
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},

	/* Added for power optimization */
	{OV2722_8BIT, 0x3000, 0x00},
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x46},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x10},
	{OV2722_TOK_TERM, 0, 0},
};

static struct ov2722_reg const ov2722_1452_1092_30fps[] = {
	{OV2722_8BIT, 0x3021, 0x03}, /* For stand wait for
				a whole frame complete.(vblank) */
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x00},
	{OV2722_8BIT, 0x3801, 0xF8}, /* H crop start: 248 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x01}, /* V crop start: 1 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xab}, /* H crop end: 1707 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x45}, /* V crop end: 1093 */
	{OV2722_8BIT, 0x3808, 0x05},
	{OV2722_8BIT, 0x3809, 0xac}, /* H output size: 1452 */
	{OV2722_8BIT, 0x380a, 0x04},
	{OV2722_8BIT, 0x380b, 0x44}, /* V output size: 1092 */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0xd4}, /* H timing: 2260 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xdc}, /* V timing: 1244 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x03}, /* H window offset: 3 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /*  mirror */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf}, /* manual 3a */
	{OV2722_8BIT, 0x301d, 0xf0}, /* enable group hold */
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},
	{OV2722_8BIT, 0x5184, 0xb0},
	{OV2722_8BIT, 0x5185, 0xb0},
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x2c}, /* 422.4 MHz */
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},
	{OV2722_8BIT, 0x3000, 0x00}, /* added for power optimization */
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07}, /* manual 3a */
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x3F},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x00},
	{OV2722_TOK_TERM, 0, 0}
};
static struct ov2722_reg const ov2722_1M3_30fps[] = {
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x4a},	/* H crop start: 330 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x03},	/* V crop start: 3 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0xe1},	/* H crop end:  1761 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x47},	/* V crop end:  1095 */
	{OV2722_8BIT, 0x3808, 0x05},
	{OV2722_8BIT, 0x3809, 0x88},	/* H output size: 1416 */
	{OV2722_8BIT, 0x380a, 0x04},
	{OV2722_8BIT, 0x380b, 0x0a},	/* V output size: 1034 */

	/* H blank timing */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00},	/* H total size: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa0},	/* V total size: 1184 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x05},	/* H window offset: 5 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02},	/* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06},	/* flip isp */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3000, 0xff},
	{OV2722_8BIT, 0x3001, 0xff},
	{OV2722_8BIT, 0x3002, 0xf0},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2},	/* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},	/* v_en, h_en, blc_en */
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf},
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},	/* AWB red */
	{OV2722_8BIT, 0x5184, 0xb0},	/* AWB green */
	{OV2722_8BIT, 0x5185, 0xb0},	/* AWB blue */
	{OV2722_8BIT, 0x5180, 0x03},	/* AWB manual mode */
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x4800, 0x24},	/* clk lane gate enable */
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26},
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},

	/* Added for power optimization */
	{OV2722_8BIT, 0x3000, 0x00},
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x46},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x10},
	{OV2722_TOK_TERM, 0, 0},
};

static struct ov2722_reg const ov2722_1620p_30fps[] = {
	{OV2722_8BIT, 0x3021, 0x03}, /* For stand wait for
				a whole frame complete.(vblank) */
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x00},
	{OV2722_8BIT, 0x3801, 0x9c}, /* H crop start: 156 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x01}, /* V crop start: 1 */
	{OV2722_8BIT, 0x3804, 0x07},
	{OV2722_8BIT, 0x3805, 0x07}, /* H crop end: 1799 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x45}, /* V crop end: 1093 */
	{OV2722_8BIT, 0x3808, 0x06},
	{OV2722_8BIT, 0x3809, 0x64}, /* H output size: 1636 */
	{OV2722_8BIT, 0x380a, 0x04},
	{OV2722_8BIT, 0x380b, 0x44}, /* V output size: 1092 */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0xd4}, /* H timing: 2260 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xdc}, /* V timing: 1244 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x03}, /* H window offset: 3 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /*  mirror */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf}, /* manual 3a */
	{OV2722_8BIT, 0x301d, 0xf0}, /* enable group hold */
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},
	{OV2722_8BIT, 0x5184, 0xb0},
	{OV2722_8BIT, 0x5185, 0xb0},
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x2c}, /* 422.4 MHz */
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},
	{OV2722_8BIT, 0x3000, 0x00}, /* added for power optimization */
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07}, /* manual 3a */
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x3F},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x00},
	{OV2722_TOK_TERM, 0, 0}
};

static struct ov2722_reg const ov2722_1080p_30fps[] = {
	{OV2722_8BIT, 0x3021, 0x03}, /* For stand wait for
				a whole frame complete.(vblank) */
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x00},
	{OV2722_8BIT, 0x3801, 0x08}, /* H crop start: 8 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0x01}, /* V crop start: 1 */
	{OV2722_8BIT, 0x3804, 0x07},
	{OV2722_8BIT, 0x3805, 0x9b}, /* H crop end: 1947 */
	{OV2722_8BIT, 0x3806, 0x04},
	{OV2722_8BIT, 0x3807, 0x45}, /* V crop end: 1093 */
	{OV2722_8BIT, 0x3808, 0x07},
	{OV2722_8BIT, 0x3809, 0x8c}, /* H output size: 1932 */
	{OV2722_8BIT, 0x380a, 0x04},
	{OV2722_8BIT, 0x380b, 0x44}, /* V output size: 1092 */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0xd4}, /* H timing: 2260 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xdc}, /* V timing: 1244 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x03}, /* H window offset: 3 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /*  mirror */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf}, /* manual 3a */
	{OV2722_8BIT, 0x301d, 0xf0}, /* enable group hold */
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},
	{OV2722_8BIT, 0x5184, 0xb0},
	{OV2722_8BIT, 0x5185, 0xb0},
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x2c}, /* 422.4 MHz */
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},
	{OV2722_8BIT, 0x3000, 0x00}, /* added for power optimization */
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07}, /* manual 3a */
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x3F},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x00},
	{OV2722_TOK_TERM, 0, 0}
};

static struct ov2722_reg const ov2722_720p_30fps[] = {
	{OV2722_8BIT, 0x3021, 0x03},
	{OV2722_8BIT, 0x3718, 0x10},
	{OV2722_8BIT, 0x3702, 0x24},
	{OV2722_8BIT, 0x373a, 0x60},
	{OV2722_8BIT, 0x3715, 0x01},
	{OV2722_8BIT, 0x3703, 0x2e},
	{OV2722_8BIT, 0x3705, 0x10},
	{OV2722_8BIT, 0x3730, 0x30},
	{OV2722_8BIT, 0x3704, 0x62},
	{OV2722_8BIT, 0x3f06, 0x3a},
	{OV2722_8BIT, 0x371c, 0x00},
	{OV2722_8BIT, 0x371d, 0xc4},
	{OV2722_8BIT, 0x371e, 0x01},
	{OV2722_8BIT, 0x371f, 0x0d},
	{OV2722_8BIT, 0x3708, 0x61},
	{OV2722_8BIT, 0x3709, 0x12},
	{OV2722_8BIT, 0x3800, 0x01},
	{OV2722_8BIT, 0x3801, 0x40}, /* H crop start: 320 */
	{OV2722_8BIT, 0x3802, 0x00},
	{OV2722_8BIT, 0x3803, 0xb1}, /* V crop start: 177 */
	{OV2722_8BIT, 0x3804, 0x06},
	{OV2722_8BIT, 0x3805, 0x55}, /* H crop end: 1621 */
	{OV2722_8BIT, 0x3806, 0x03},
	{OV2722_8BIT, 0x3807, 0x95}, /* V crop end: 918 */
	{OV2722_8BIT, 0x3808, 0x05},
	{OV2722_8BIT, 0x3809, 0x10}, /* H output size: 0x0788==1928 */
	{OV2722_8BIT, 0x380a, 0x02},
	{OV2722_8BIT, 0x380b, 0xe0}, /* output size: 0x02DE==734 */
	{OV2722_8BIT, 0x380c, 0x08},
	{OV2722_8BIT, 0x380d, 0x00}, /* H timing: 2048 */
	{OV2722_8BIT, 0x380e, 0x04},
	{OV2722_8BIT, 0x380f, 0xa3}, /* V timing: 1187 */
	{OV2722_8BIT, 0x3810, 0x00},
	{OV2722_8BIT, 0x3811, 0x03}, /* H window offset: 3 */
	{OV2722_8BIT, 0x3812, 0x00},
	{OV2722_8BIT, 0x3813, 0x02}, /* V window offset: 2 */
	{OV2722_8BIT, 0x3820, 0x80},
	{OV2722_8BIT, 0x3821, 0x06}, /* mirror */
	{OV2722_8BIT, 0x3814, 0x11},
	{OV2722_8BIT, 0x3815, 0x11},
	{OV2722_8BIT, 0x3612, 0x0b},
	{OV2722_8BIT, 0x3618, 0x04},
	{OV2722_8BIT, 0x3a08, 0x01},
	{OV2722_8BIT, 0x3a09, 0x50},
	{OV2722_8BIT, 0x3a0a, 0x01},
	{OV2722_8BIT, 0x3a0b, 0x18},
	{OV2722_8BIT, 0x3a0d, 0x03},
	{OV2722_8BIT, 0x3a0e, 0x03},
	{OV2722_8BIT, 0x4520, 0x00},
	{OV2722_8BIT, 0x4837, 0x1b},
	{OV2722_8BIT, 0x3600, 0x08},
	{OV2722_8BIT, 0x3621, 0xc0},
	{OV2722_8BIT, 0x3632, 0xd2}, /* added for power opt */
	{OV2722_8BIT, 0x3633, 0x23},
	{OV2722_8BIT, 0x3634, 0x54},
	{OV2722_8BIT, 0x3f01, 0x0c},
	{OV2722_8BIT, 0x5001, 0xc1},
	{OV2722_8BIT, 0x3614, 0xf0},
	{OV2722_8BIT, 0x3630, 0x2d},
	{OV2722_8BIT, 0x370b, 0x62},
	{OV2722_8BIT, 0x3706, 0x61},
	{OV2722_8BIT, 0x4000, 0x02},
	{OV2722_8BIT, 0x4002, 0xc5},
	{OV2722_8BIT, 0x4005, 0x08},
	{OV2722_8BIT, 0x404f, 0x84},
	{OV2722_8BIT, 0x4051, 0x00},
	{OV2722_8BIT, 0x5000, 0xcf}, /* manual 3a */
	{OV2722_8BIT, 0x301d, 0xf0}, /* enable group hold */
	{OV2722_8BIT, 0x3a18, 0x00},
	{OV2722_8BIT, 0x3a19, 0x80},
	{OV2722_8BIT, 0x3503, 0x07},
	{OV2722_8BIT, 0x4521, 0x00},
	{OV2722_8BIT, 0x5183, 0xb0},
	{OV2722_8BIT, 0x5184, 0xb0},
	{OV2722_8BIT, 0x5185, 0xb0},
	{OV2722_8BIT, 0x370c, 0x0c},
	{OV2722_8BIT, 0x3035, 0x00},
	{OV2722_8BIT, 0x3036, 0x26}, /* {0x3036, 0x2c}, //422.4 MHz */
	{OV2722_8BIT, 0x3037, 0xa1},
	{OV2722_8BIT, 0x303e, 0x19},
	{OV2722_8BIT, 0x3038, 0x06},
	{OV2722_8BIT, 0x3018, 0x04},
	{OV2722_8BIT, 0x3000, 0x00}, /* added for power optimization */
	{OV2722_8BIT, 0x3001, 0x00},
	{OV2722_8BIT, 0x3002, 0x00},
	{OV2722_8BIT, 0x3a0f, 0x40},
	{OV2722_8BIT, 0x3a10, 0x38},
	{OV2722_8BIT, 0x3a1b, 0x48},
	{OV2722_8BIT, 0x3a1e, 0x30},
	{OV2722_8BIT, 0x3a11, 0x90},
	{OV2722_8BIT, 0x3a1f, 0x10},
	{OV2722_8BIT, 0x3503, 0x07}, /* manual 3a */
	{OV2722_8BIT, 0x3500, 0x00},
	{OV2722_8BIT, 0x3501, 0x3F},
	{OV2722_8BIT, 0x3502, 0x00},
	{OV2722_8BIT, 0x3508, 0x00},
	{OV2722_8BIT, 0x3509, 0x00},
	{OV2722_TOK_TERM, 0, 0},
};

static struct ov2722_resolution ov2722_res_preview[] = {
	{
		.desc = "ov2722_1452_1092_30fps",
		.width = 1452,
		.height = 1092,
		.fps = 30,
		.pix_clk_freq = 85,
		.used = 0,
		.pixels_per_line = 2260,
		.lines_per_frame = 1244,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1452_1092_30fps,
	},
	{
		.desc = "ov2722_1080P_30fps",
		.width = 1932,
		.height = 1092,
		.pix_clk_freq = 85,
		.fps = 30,
		.used = 0,
		.pixels_per_line = 2260,
		.lines_per_frame = 1244,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1080p_30fps,
	},
};
#define N_RES_PREVIEW (ARRAY_SIZE(ov2722_res_preview))

static struct ov2722_resolution ov2722_res_still[] = {

	{
		.desc = "ov2722_1M3_30fps",
		.width = 1416,
		.height = 1034,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1M3_30fps,
	},
	{
		.desc = "ov2722_1080P_30fps",
		.width = 1932,
		.height = 1092,
		.fps = 30,
		.pix_clk_freq = 85,
		.used = 0,
		.pixels_per_line = 2260,
		.lines_per_frame = 1244,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1080p_30fps,
	},
};
#define N_RES_STILL (ARRAY_SIZE(ov2722_res_still))

static struct ov2722_resolution ov2722_res_video[] = {
	{
		.desc = "ov2722_QCIF_30fps",
		.width = 192,
		.height = 160,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_QCIF_30fps,
	},
	{
		.desc = "ov2722_QVGA_30fps",
		.width = 336,
		.height = 256,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_QVGA_30fps,
	},
	{
		.desc = "ov2722_CIF_30fps",
		.width = 368,
		.height = 304,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_CIF_30fps,
	},
	{
		.desc = "ov2722_VGA_30fps",
		.width = 656,
		.height = 496,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_VGA_30fps,
	},
	{
		.desc = "ov2722_720p_30fps",
		.width = 1296,
		.height = 736,
		.fps = 30,
		.pix_clk_freq = 75,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1187,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_720p_30fps,
	},
	{
		.desc = "ov2722_1M3_30fps",
		.width = 1416,
		.height = 1034,
		.fps = 30,
		.pix_clk_freq = 73,
		.used = 0,
		.pixels_per_line = 2048,
		.lines_per_frame = 1184,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1M3_30fps,
	},
	{
		.desc = "ov2722_1620P_30fps",
		.width = 1636,
		.height = 1092,
		.fps = 30,
		.pix_clk_freq = 85,
		.used = 0,
		.pixels_per_line = 2260,
		.lines_per_frame = 1244,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1620p_30fps,
	},
	{
		.desc = "ov2722_1080P_30fps",
		.width = 1932,
		.height = 1092,
		.fps = 30,
		.pix_clk_freq = 85,
		.used = 0,
		.pixels_per_line = 2260,
		.lines_per_frame = 1244,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.bin_mode = 0,
		.skip_frames = 3,
		.regs = ov2722_1080p_30fps,
	},
};
#define N_RES_VIDEO (ARRAY_SIZE(ov2722_res_video))

static struct ov2722_resolution *ov2722_res = ov2722_res_preview;
static int N_RES = N_RES_PREVIEW;
#endif
