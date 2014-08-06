/*
 * Copyright Â© 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Jani Nikula <jani.nikula@intel.com>
 *	   Shobhit Kumar <shobhit.kumar@intel.com>
 *
 *
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/i915_drm.h>
#include <linux/slab.h>
#include <video/mipi_display.h>
#include <asm/intel-mid.h>
#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"
#include "dsi_mod_cpt_claa080wq08.h"


/* Test key for level 2 commands */
static u8 PASSWD1[] = {
	0xF0, 0x5A, 0x5A};
static u8 PASSWD2[] = {
	0xF1, 0x5A, 0x5A};

/* Test key for level 3 commands */
static u8 PASSWD3[] = {
	0xFC, 0xA5, 0xA5};

/* LV2 OTP Reload selection */
static u8 OPT_CTRL[] = {
	0xD0, 0x00, 0x10};

/* Resolution selection */
static u8 RNLCTL[] = {
	0xB1, 0x10};

/* Source Direction selection */
static u8 IFCTL[] = {
	0xB2, 0x14, 0x22, 0x2F, 0x04};

/* ASG Timing selection */
static u8 ASG_TIMING_SELECTION_1[] = {
	0xB5, 0x01};
static u8 ASG_TIMING_SELECTION_2[] = {
	0xEE, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x1F,
	0x00};
static u8 ASG_TIMING_SELECTION_3[] = {
	0xEF, 0x56, 0x34, 0x43, 0x65, 0x90, 0x80, 0x24,
	0x81, 0x00, 0x91, 0x11, 0x11, 0x11};

/* ASG Pin Assignment */
static u8 ASG_PIN_ASSIGNMENT[] = {
	0xF7, 0x04, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
	0x0E, 0x0F, 0x16, 0x17, 0x10, 0x01, 0x01, 0x01,
	0x01, 0x04, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
	0x0E, 0x0F, 0x16, 0x17, 0x10, 0x01, 0x01, 0x01,
	0x01};

/* Vertical & Horizontal Porch control */
static u8 PORCH_CTL[] = {
	0xF2, 0x02, 0x08, 0x08, 0x40, 0x10};

/* Source output control */
static u8 SOURCE_OUTPUT_CTL[] = {
	0xF6, 0x60, 0x25, 0x26, 0x00, 0x00, 0x00};

/* Gamma */
static u8 GAMMA1[] = {
	0xFA, 0x04, 0x35, 0x07, 0x0B, 0x12, 0x0B, 0x10,
	0x16, 0x1A, 0x24, 0x2C, 0x33, 0x3B, 0x3B, 0x33,
	0x34, 0x33};
static u8 GAMMA2[] = {
	0xFB, 0x04, 0x35, 0x07, 0x0B, 0x12, 0x0B, 0x10,
	0x16, 0x1A, 0x24, 0x2C, 0x33, 0x3B, 0x3B, 0x33,
	0x34, 0x33};

/* DDI internal power control */
static u8 DDI_INTERNAL_PWR_CTL[] = {
	0xF3, 0x01, 0xC4, 0xE0, 0x62, 0xD4, 0x83, 0x37,
	0x3C, 0x24, 0x00};

/* DDI internal power sequence control */
static u8 DDI_INTERNAL_PWR_SEQ_CTL[] = {
	0xF4, 0x00, 0x02, 0x03, 0x26, 0x03, 0x02, 0x09,
	0x00, 0x07, 0x16, 0x16, 0x03, 0x00, 0x08, 0x08,
	0x03, 0x19, 0x1C, 0x12, 0x1C, 0x1D, 0x1E, 0x1A,
	0x09, 0x01, 0x04, 0x02, 0x61, 0x74, 0x75, 0x72,
	0x83, 0x80, 0x80, 0xF0};

/* Output Voltage setting & internal power sequence */
static u8 OUT_VOL_SET[] = {
	0xB0, 0x01};
static u8 INTERNAL_PWR_SEQ[] = {
	0xF5, 0x2F, 0x2F, 0x5F, 0xAB, 0x98, 0x52, 0x0F,
	0x33, 0x43, 0x04, 0x59, 0x54, 0x52, 0x05, 0x40,
	0x40, 0x5D, 0x59, 0x40};

/* Watch Dog */
static u8 WATCH_DOG1[] = {
	0xBC, 0x01, 0x4E, 0x08};
static u8 WATCH_DOG2[] = {
	0xE1, 0x03, 0x10, 0x1C, 0xA0, 0x10};

/* DDI Analog interface Setting */
static u8 DDI_ANALOG_INTERFACE_SET[] = {
	0xFD, 0x16, 0x10, 0x11, 0x20, 0x09};

/* TE */
static u8 TE[] = {0x35};

/* Sleep out */
static u8 SLEEP_OUT[] = {0x11};


/* BC_CTRL Enable (Power IC Enable) */
static u8 BC_CTRL_ENABLE[] = {0xC3, 0x40, 0x00, 0x28};

/* Display on */
static u8 DISPLAY_ON[] = {0x29};


/*******************    Sleep   *****************/
/* Display off */
static u8 DISPLAY_OFF[] = {0x28};

/* BC_CTRL Disable (Power IC Disable) */
static u8 BC_CTRL_DISABLE[] = {0xC3, 0x40, 0x00, 0x20};

/* Sleep in	*/
static u8 SLEEP_IN[] = {0x10};


static void  claa080wq08_get_panel_info(int pipe,
					struct drm_connector *connector)
{
	if (!connector) {
		DRM_DEBUG_KMS("Cpt: Invalid input to get_info\n");
		return;
	}

	if (pipe == 0) {
		connector->display_info.width_mm = 192;
		connector->display_info.height_mm = 120;
	}

	return;
}

static void claa080wq08_destroy(struct intel_dsi_device *dsi)
{
}

static void claa080wq08_dump_regs(struct intel_dsi_device *dsi)
{
}

static void claa080wq08_create_resources(struct intel_dsi_device *dsi)
{
}

static struct drm_display_mode *claa080wq08_get_modes(
	struct intel_dsi_device *dsi)
{
	struct drm_display_mode *mode = NULL;

	/* Allocate */
	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode) {
		DRM_DEBUG_KMS("Cpt panel: No memory\n");
		return NULL;
	}

	/* Hardcode 800*1280 */
	/*HFP = 16, HSYNC = 5, HBP = 59 */
	/*VFP = 8, VSYNC = 5, VBP = 3 */
	mode->hdisplay = 800;
	mode->hsync_start = mode->hdisplay + 16;
	mode->hsync_end = mode->hsync_start + 5;
	mode->htotal = mode->hsync_end + 59;

	mode->vdisplay = 1280;
	mode->vsync_start = mode->vdisplay + 8;
	mode->vsync_end = mode->vsync_start + 5;
	mode->vtotal = mode->vsync_end + 3;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal *
		mode->htotal / 1000;

	/* Configure */
	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}


static bool claa080wq08_get_hw_state(struct intel_dsi_device *dev)
{
	return true;
}

static enum drm_connector_status claa080wq08_detect(
					struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	return connector_status_connected;
}

static bool claa080wq08_mode_fixup(struct intel_dsi_device *dsi,
		    const struct drm_display_mode *mode,
		    struct drm_display_mode *adjusted_mode) {
	return true;
}

void claa080wq08_panel_reset(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	intel_gpio_nc_write32(dev_priv, 0x4030, 0x2000CC00);
	intel_gpio_nc_write32(dev_priv, 0x4038, 0x00000004);
	usleep_range(2000, 2500);
	intel_gpio_nc_write32(dev_priv, 0x4038, 0x00000005);
	msleep(20);
}

static int claa080wq08_mode_valid(struct intel_dsi_device *dsi,
		   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void claa080wq08_dpms(struct intel_dsi_device *dsi, bool enable)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");
}
static void claa080wq08_enable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	DRM_DEBUG_KMS("\n");
	dsi_vc_dcs_write(intel_dsi, 0, PASSWD1, 3);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);
	msleep(31);
	dsi_vc_dcs_write(intel_dsi, 0, BC_CTRL_ENABLE, 4);
	msleep(151);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);

}
static void claa080wq08_disable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x28);
	msleep(81);
	dsi_vc_dcs_write(intel_dsi, 0, BC_CTRL_DISABLE, 4);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x10);
	msleep(35);
}

bool claa080wq08_init(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* create private data, slam to dsi->dev_priv. could support many panels
	 * based on dsi->name. This panal supports both command and video mode,
	 * so check the type. */

	/* where to get all the board info style stuff:
	 *
	 * - gpio numbers, if any (external te, reset)
	 * - pin config, mipi lanes
	 * - dsi backlight? (->create another bl device if needed)
	 * - esd interval, ulps timeout
	 *
	 */

	DRM_DEBUG_KMS("Init: CPT panel\n");

	if (!dsi) {
		DRM_DEBUG_KMS("Init: Invalid input to claa080wq08_init\n");
		return false;
	}

	dsi->eotp_pkt = 1;
	dsi->operation_mode = DSI_VIDEO_MODE;
	dsi->video_mode_type = DSI_VIDEO_NBURST_SEVENT;
	dsi->pixel_format = VID_MODE_FORMAT_RGB888;
	dsi->port_bits = 0;
	dsi->turn_arnd_val = 0x14;
	dsi->rst_timer_val = 0xffff;
	dsi->bw_timer = 0x820;
	/*b044*/
	dsi->hs_to_lp_count = 0x18;
	/*b060*/
	dsi->lp_byte_clk = 0x03;
	/*b080*/
	dsi->dphy_reg = 0x170d340b;
	/* b088 high 16bits */
	dsi->clk_lp_to_hs_count = 0x1e;
	/* b088 low 16bits */
	dsi->clk_hs_to_lp_count = 0x0d;
	/* BTA sending at the last blanking line of VFP is disabled */
	dsi->video_frmt_cfg_bits = 1<<3;


	dsi->backlight_off_delay = 20;
	dsi->send_shutdown = true;
	dsi->shutdown_pkt_delay = 20;
	dev_priv->mipi.panel_bpp = 24;

	return true;
}



void claa080wq08_send_otp_cmds(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");
	dsi_vc_dcs_write(intel_dsi, 0, PASSWD1, 3);
	dsi_vc_dcs_write(intel_dsi, 0, PASSWD2, 3);
	dsi_vc_dcs_write(intel_dsi, 0, PASSWD3, 3);

	dsi_vc_dcs_write(intel_dsi, 0, OPT_CTRL, 3);
	dsi_vc_dcs_write_1(intel_dsi, 0, 0xb1, 0x10);
	dsi_vc_dcs_write(intel_dsi, 0, IFCTL, 5);
	dsi_vc_dcs_write_1(intel_dsi, 0, 0xb5, 0x01);
	dsi_vc_dcs_write(intel_dsi, 0, ASG_TIMING_SELECTION_2, 9);
	dsi_vc_dcs_write(intel_dsi, 0, ASG_TIMING_SELECTION_3, 14);
	dsi_vc_dcs_write(intel_dsi, 0, ASG_PIN_ASSIGNMENT, 33);
	dsi_vc_dcs_write(intel_dsi, 0, PORCH_CTL, 6);
	dsi_vc_dcs_write(intel_dsi, 0, SOURCE_OUTPUT_CTL, 7);
	dsi_vc_dcs_write(intel_dsi, 0, GAMMA1, 18);
	dsi_vc_dcs_write(intel_dsi, 0, GAMMA2, 18);
	dsi_vc_dcs_write(intel_dsi, 0, DDI_INTERNAL_PWR_CTL, 11);
	dsi_vc_dcs_write(intel_dsi, 0, DDI_INTERNAL_PWR_SEQ_CTL, 36);
	dsi_vc_dcs_write_1(intel_dsi, 0, 0xb0, 0x01);
	dsi_vc_dcs_write(intel_dsi, 0, INTERNAL_PWR_SEQ, 20);
	dsi_vc_dcs_write(intel_dsi, 0, WATCH_DOG1, 4);
	dsi_vc_dcs_write(intel_dsi, 0, WATCH_DOG2, 6);
	dsi_vc_dcs_write(intel_dsi, 0, DDI_ANALOG_INTERFACE_SET, 6);
	dsi_vc_dcs_write_1(intel_dsi, 0, 0x35, 0x00);

}

void claa080wq08_enable_panel_power(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	intel_gpio_nc_write32(dev_priv, 0x40B0, 0x2000CC00);
	intel_gpio_nc_write32(dev_priv, 0x40B8, 0x00000005);
	usleep_range(1000, 1500);
}

void claa080wq08_disable_panel_power(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	intel_gpio_nc_write32(dev_priv, 0x40B0, 0x2000CC00);
	intel_gpio_nc_write32(dev_priv, 0x40B8, 0x00000004);
	usleep_range(1000, 1500);
}

/* Callbacks. We might not need them all. */
struct intel_dsi_dev_ops cpt_claa080wq08_dsi_display_ops = {
	.init = claa080wq08_init,
	.get_info = claa080wq08_get_panel_info,
	.create_resources = claa080wq08_create_resources,
	.dpms = claa080wq08_dpms,
	.mode_valid = claa080wq08_mode_valid,
	.mode_fixup = claa080wq08_mode_fixup,
	.panel_reset = claa080wq08_panel_reset,
	.detect = claa080wq08_detect,
	.get_hw_state = claa080wq08_get_hw_state,
	.get_modes = claa080wq08_get_modes,
	.destroy = claa080wq08_destroy,
	.dump_regs = claa080wq08_dump_regs,
	.enable = claa080wq08_enable,
	.disable = claa080wq08_disable,
	.send_otp_cmds = claa080wq08_send_otp_cmds,
	.enable_panel_power = claa080wq08_enable_panel_power,
	.disable_panel_power = claa080wq08_disable_panel_power,
};
