/*
 * Copyright © 2013 Intel Corporation
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
 * Author: Alan Zhang <alan.zhang@intel.com>
 *	   Xi Yu <yu.xi@intel.com>
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
#include "dsi_mod_lsl080al02.h"
#include "linux/mfd/intel_mid_pmic.h"
#include <linux/gpio.h>

static void lsl080al02_get_panel_info(int pipe,
				       struct drm_connector *connector)
{
	DRM_DEBUG_KMS("\n");
		
	if (!connector)
		return;

	if (pipe == 0) {
		/* FIXME: the actual width is 94.5, height is 151.2 */
		connector->display_info.width_mm = 107;
		connector->display_info.height_mm = 172;
	}

	return;
}

bool lsl080al02_init(struct intel_dsi_device * dsi)
{
	DRM_DEBUG_KMS("\n");
	
	dsi->eotp_pkt = 1;
	dsi->operation_mode = DSI_VIDEO_MODE;
	dsi->video_mode_type = DSI_VIDEO_NBURST_SEVENT;
	dsi->pixel_format = VID_MODE_FORMAT_RGB888;
	dsi->port_bits = 0;
	dsi->turn_arnd_val = 0x30;
	dsi->rst_timer_val = 0xffff;
	dsi->hs_to_lp_count = 0x46;
	dsi->lp_byte_clk = 0x46;
	dsi->bw_timer = 0x400;
	dsi->clk_lp_to_hs_count = 0x2f;
	dsi->clk_hs_to_lp_count = 0x16;
	dsi->video_frmt_cfg_bits = IP_TG_CONFIG;
	dsi->dphy_reg = 0x300c340F;

	dsi->backlight_off_delay = 20;
	dsi->send_shutdown = true;
	dsi->shutdown_pkt_delay = 20;

	return true;
}

void lsl080al02_create_resources(struct intel_dsi_device *dsi)
{
		
}

void lsl080al02_dpms(struct intel_dsi_device *dsi, bool enable)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	
	#if 1
	DRM_INFO("lsl080al02_dpms \n");
	DRM_DEBUG_KMS("\n");
	if (enable) {
		dsi_vc_dcs_write_0(intel_dsi, 0, MIPI_DCS_EXIT_SLEEP_MODE);

		dsi_vc_dcs_write_1(intel_dsi, 0, MIPI_DCS_SET_TEAR_ON, 0x00);

		dsi_vc_dcs_write_0(intel_dsi, 0, MIPI_DCS_SET_DISPLAY_ON);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x14, 0x55);

	} else {
		dsi_vc_dcs_write_0(intel_dsi, 0, MIPI_DCS_SET_DISPLAY_OFF);
		dsi_vc_dcs_write_0(intel_dsi, 0, MIPI_DCS_ENTER_SLEEP_MODE);
	}
	#endif
}

int lsl080al02_mode_valid(struct intel_dsi_device *dsi,
			   struct drm_display_mode *mode)
{
	
	return MODE_OK;
}

bool lsl080al02_mode_fixup(struct intel_dsi_device * dsi,
			    const struct drm_display_mode * mode,
			    struct drm_display_mode * adjusted_mode)
{
		
	return true;
}

void lsl080al02_panel_reset(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	DRM_INFO("lsl080al02_reset \n");

	#if 0
	gpio_request(139,"panel_reset");
	gpio_direction_output(139,1);
	gpio_set_value(139,0);
	#endif

	#if 1
	intel_mid_pmic_writeb(0x3c, 0x20); /*set RESET to low*/
	msleep(5);
	#endif

	intel_mid_pmic_writeb(0x52, 0x00); /*disable LCD Power*/
	msleep(5);

	intel_mid_pmic_writeb(0x52, 0x01); /*Enable LCD Power*/
	msleep(10);

	#if 0
	gpio_set_value(139,1);  /*set high to enable*/
	msleep(20);
	gpio_free(139);
	#endif

	#if 1
	intel_mid_pmic_writeb(0x3c, 0x21); /*set RESET to high*/
	msleep(20);
	#endif
}

void lsl080al02_disable_panel_power(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	DRM_INFO("lsl080al02_disable_power \n");
	#if 0
	gpio_request(139,"panel_reset");
	gpio_direction_output(139,1);

	gpio_set_value(139,0);
	#endif

	intel_mid_pmic_writeb(0x3c, 0x20); /*set RESET to low*/
	msleep(5);

	intel_mid_pmic_writeb(0x52, 0x00); /*Disable LCD Power*/
	msleep(10);
}

void lsl080al02_send_otp_cmds(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	DRM_DEBUG_KMS("\n");
	DRM_INFO("lsl080al02_enable \n");

	const static unsigned char ucData1[] = { 0xf0, 0x5a, 0x5a };
	const static unsigned char ucData2[] = { 0xf1, 0x5a, 0x5a };
	const static unsigned char ucData3[] = { 0xfc, 0xa5, 0xa5 };
	const static unsigned char ucData4[] = { 0xd0, 0x00, 0x10 };
	const static unsigned char ucData5[] = { 0xc3, 0x40, 0x00, 0x28 };
	const static unsigned char ucData6[] = { 0x36, 0x04 };
	const static unsigned char ucData7[] = { 0xf6, 0x63, 0x20, 0x86, 0x00, 0x00, 0x10 };
	const static unsigned char ucData8[] = { 0x36, 0x00 };
	const static unsigned char ucData9[] = { 0xf0, 0xa5, 0xa5 };
	const static unsigned char ucData10[] = { 0xf1, 0xa5, 0xa5 };
	const static unsigned char ucData11[] = { 0xfc, 0x5a, 0x5a };

	dsi_vc_dcs_write(intel_dsi, 0, ucData1, sizeof(ucData1));
	dsi_vc_dcs_write(intel_dsi, 0, ucData2, sizeof(ucData2));
	dsi_vc_dcs_write(intel_dsi, 0, ucData3, sizeof(ucData3));
	dsi_vc_dcs_write(intel_dsi, 0, ucData4, sizeof(ucData4));
	dsi_vc_dcs_write(intel_dsi, 0, ucData5, sizeof(ucData5));
	msleep(20);
	dsi_vc_dcs_write(intel_dsi, 0, ucData6, sizeof(ucData6));
	dsi_vc_dcs_write(intel_dsi, 0, ucData7, sizeof(ucData7));
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);//dsi_vc_dcs_write_1(intel_dsi, 0, 0x11, 0);
	msleep(120);
	dsi_vc_dcs_write(intel_dsi, 0, ucData8, sizeof(ucData8));
	dsi_vc_dcs_write(intel_dsi, 0, ucData9, sizeof(ucData9));
	dsi_vc_dcs_write(intel_dsi, 0, ucData10, sizeof(ucData10));
	dsi_vc_dcs_write(intel_dsi, 0, ucData11, sizeof(ucData11));
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);//dsi_vc_dcs_write_1(intel_dsi, 0, 0x29, 0);
	msleep(200);
}
void lsl080al02_enable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");

	dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);	/*exit_sleep_mode */
	msleep(120);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);	/*set_display_on */
}

void lsl080al02_disable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	dsi_vc_dcs_write_0(intel_dsi, 0, 0x28);	/*set_display_off */
	msleep(20);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x10);	/*enter_sleep_mode */
	msleep(80);
}

enum drm_connector_status lsl080al02_detect(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	return connector_status_connected;
}

bool lsl080al02_get_hw_state(struct intel_dsi_device * dev)
{
	return true;
}

struct drm_display_mode *lsl080al02_get_modes(struct intel_dsi_device *dsi)
{
	
	u32 hblank = 160;
	u32 vblank = 16;
	u32 hsync_offset = 10;//60
	u32 hsync_width = 30;//60
	u32 vsync_offset = 8;
	u32 vsync_width = 4;
	struct drm_display_mode *mode = NULL;

	DRM_INFO("lsl080al02_get_modes \n");
	/* Allocate */
	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode) {
		DRM_DEBUG_KMS("LSL080AL02 8inch Panel: No memory\n");
		return NULL;
	}

	/* Hardcode 800x1200 */
	strncpy(mode->name, "800x1280", sizeof(mode->name) - 1);
	mode->hdisplay = 800;
	mode->vdisplay = 1280;
	mode->vrefresh = 60;
	mode->clock = 75000;

	/* Calculate */
	mode->hsync_start = mode->hdisplay + hsync_offset;
	mode->hsync_end = mode->hdisplay + hsync_offset + hsync_width;
	mode->htotal = mode->hdisplay + hblank;
	mode->vsync_start = mode->vdisplay + vsync_offset;
	mode->vsync_end = mode->vdisplay + vsync_offset + vsync_width;
	mode->vtotal = mode->vdisplay + vblank;

	/* Configure */
	mode->flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;
	mode->type |= DRM_MODE_TYPE_PREFERRED;
	mode->status = MODE_OK;

	return mode;
}

void lsl080al02_dump_regs(struct intel_dsi_device *dsi)
{
}

void lsl080al02_destroy(struct intel_dsi_device *dsi)
{
}

/* Callbacks. We might not need them all. */
struct intel_dsi_dev_ops jdi_lsl080al02_dsi_display_ops = {
	.init = lsl080al02_init,
	.get_info = lsl080al02_get_panel_info,
	.create_resources = lsl080al02_create_resources,
	.dpms = lsl080al02_dpms,
	.mode_valid = lsl080al02_mode_valid,
	.mode_fixup = lsl080al02_mode_fixup,
	.panel_reset = lsl080al02_panel_reset,
	.disable_panel_power = lsl080al02_disable_panel_power,
	.send_otp_cmds = lsl080al02_send_otp_cmds,
	.detect = lsl080al02_detect,
	.get_hw_state = lsl080al02_get_hw_state,
	.get_modes = lsl080al02_get_modes,
	.destroy = lsl080al02_destroy,
	.dump_regs = lsl080al02_dump_regs,
};

