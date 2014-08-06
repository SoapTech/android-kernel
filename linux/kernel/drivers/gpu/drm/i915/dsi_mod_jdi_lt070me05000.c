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
#include "dsi_mod_jdi_lt070me05000.h"
#include "linux/mfd/intel_mid_pmic.h"


static void lt070me05000_get_panel_info(int pipe,
				struct drm_connector *connector)
{
	DRM_DEBUG_KMS("\n");
	if (!connector)
		return;

	if (pipe == 0) {
		/* FIXME: the actual width is 94.5, height is 151.2 */
		connector->display_info.width_mm = 95;
		connector->display_info.height_mm = 151;
	}

	return;
}

bool lt070me05000_init(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	dsi->eotp_pkt = 1;
	dsi->operation_mode = DSI_VIDEO_MODE;
	dsi->video_mode_type = DSI_VIDEO_NBURST_SEVENT;
	dsi->pixel_format = VID_MODE_FORMAT_RGB888;
	dsi->port_bits = 0;
	dsi->turn_arnd_val = 0x30;
	dsi->rst_timer_val = 0xffff;
	dsi->hs_to_lp_count = 0x2f;
	dsi->lp_byte_clk = 7;
	dsi->bw_timer = 0x820;
	dsi->clk_lp_to_hs_count = 0x2f;
	dsi->clk_hs_to_lp_count = 0x16;
	dsi->video_frmt_cfg_bits = 0x8;
	dsi->dphy_reg = 0x2a18681f;

	dsi->backlight_off_delay = 20;
	dsi->send_shutdown = true;
	dsi->shutdown_pkt_delay = 20;
	dev_priv->mipi.panel_bpp = 24;

	return true;
}

void lt070me05000_create_resources(struct intel_dsi_device *dsi) { }

void lt070me05000_dpms(struct intel_dsi_device *dsi, bool enable)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

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
}

int lt070me05000_mode_valid(struct intel_dsi_device *dsi,
		   struct drm_display_mode *mode)
{
	return MODE_OK;
}

bool lt070me05000_mode_fixup(struct intel_dsi_device *dsi,
		    const struct drm_display_mode *mode,
		    struct drm_display_mode *adjusted_mode) {
	return true;
}

void lt070me05000_panel_reset(struct intel_dsi_device *dsi)
{
	#if 0
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_gpio_nc_write32(dev_priv, 0x4160, 0x2000CC00);
	intel_gpio_nc_write32(dev_priv, 0x4168, 0x00000004);
	usleep_range(2000, 2500);
	intel_gpio_nc_write32(dev_priv, 0x4168, 0x00000005);
	msleep(20);
	#endif

	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);
	intel_mid_pmic_writeb(0x3c, 0x20);
	intel_mid_pmic_writeb(0x52, 0x01);
	msleep(20);

	intel_mid_pmic_writeb(0x3c, 0x21);
	msleep(10);

	intel_mid_pmic_writeb(0x52, 0x00);
	msleep(20);
	intel_mid_pmic_writeb(0x52, 0x01);
	msleep(20);

}
static int sus_res_id = 0;

void  lt070me05000_disable_panel_power(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	#if 0
	intel_gpio_nc_write32(dev_priv, 0x4160, 0x2000CC00);
	intel_gpio_nc_write32(dev_priv, 0x4168, 0x00000004);
	usleep_range(2000, 2500);
	#endif
	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);
	#if 0
	intel_mid_pmic_writeb(0x3c, 0x20);
	intel_mid_pmic_writeb(0x52, 0x00);
	msleep(25);
	#endif

	#if 1
	/* Follow the spec to entry DSTB mode */
	{
		unsigned char ucData[] = {0xb0, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 2);
	}
	{
		unsigned char ucData[] = {0xb1, 0x01};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 2);
	}
	msleep(25);
	intel_mid_pmic_writeb(0x3c, 0x20);
	intel_mid_pmic_writeb(0x52, 0x00);
	msleep(25);
	#endif

}

void lt070me05000_send_otp_cmds(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");

	#if 1
	/* Revised by reference from MTK code */
	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x01);
	usleep_range(5000, 7000);
	{
		unsigned char ucData[] = {0xb0, 0x04};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 2);
	}
	{
		unsigned char ucData[] = {0xb3, 0x14, 0x00, 0x00, 0x00, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0xb6, 0x3a, 0xc3};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x51, 0xe6);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x53, 0x2c);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x3A, 0x77);

	{
		unsigned char ucData[] = {0x2A, 0x00, 0x00, 0x04, 0xAF};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0x2B, 0x00, 0x00, 0x07, 0x7F};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
		// dsi_vc_dcs_write_0(intel_dsi, 0, 0x2c);
		// dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);
		// msleep(120);
		dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);
		msleep(100);
	#endif


	#if 0
	/* Implement from SPEC */
	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x01);
	usleep_range(5000, 7000);
	{
		unsigned char ucData[] = {0xb0, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 2);
	}
	{
		unsigned char ucData[] = {0xb3, 0x04, 0x08, 0x00, 0x22, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0xb4, 0x0c};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0xb6, 0x3a, 0xD3};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x51, 0xe6);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x53, 0x2c);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x3A, 0x77);

	{
		unsigned char ucData[] = {0x2A, 0x00, 0x00, 0x04, 0xAF};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0x2B, 0x00, 0x00, 0x07, 0x7F};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
		dsi_vc_dcs_write_0(intel_dsi, 0, 0x2c);
		dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);
		msleep(120);
		dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);

	#endif

	#if 0
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x10);
	msleep(50);

	dsi_vc_dcs_write_0(intel_dsi, 0, 0x01);
	usleep_range(5000, 7000);
	{
		unsigned char ucData[] = {0xb0, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 2);
	}
	{
		unsigned char ucData[] = {0xb3, 0x14, 0x08, 0x00, 0x22, 0x00};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0xb4, 0x0c};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0xb6, 0x3a, 0xD3};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x3A, 0x77);
		dsi_vc_dcs_write_1(intel_dsi, 0, 0x36, 0xC0);
	{
		unsigned char ucData[] = {0x2A, 0x00, 0x00, 0x04, 0xAF};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	{
		unsigned char ucData[] = {0x2B, 0x00, 0x00, 0x07, 0x7F};
		dsi_vc_generic_write(intel_dsi, 0, ucData, 6);
	}
	/* Add sleep-out, and display-on */
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x2c);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);
	msleep(120);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);

	#endif
}

void lt070me05000_enable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);

	DRM_DEBUG_KMS("\n");
	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);

	#if 0
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x11);
	msleep(120);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x29);
	#endif
}

void lt070me05000_disable(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");
	pr_info("[intel] %s:%d.\n", __FUNCTION__, __LINE__);

	dsi_vc_dcs_write_0(intel_dsi, 0, 0x28);
	msleep(20);
	dsi_vc_dcs_write_0(intel_dsi, 0, 0x10);
	msleep(80);
}

enum drm_connector_status lt070me05000_detect(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	return connector_status_connected;
}

bool lt070me05000_get_hw_state(struct intel_dsi_device *dev)
{
	return true;
}

struct drm_display_mode *lt070me05000_get_modes(struct intel_dsi_device *dsi)
{
	struct drm_display_mode *mode;

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	/* beta = 00, alpha = 45 */
	/* from renesas spec alpha + beta <= 45 */
	mode->hdisplay = 1200;
	mode->hsync_start = 1300;
	mode->hsync_end = 1340;
	mode->htotal = 1380;


	/* Added more vblank so more time for frame update */
	mode->vdisplay = 1920;
	mode->vsync_start = 1925;
	mode->vsync_end = 1930;
	mode->vtotal = 1935;

	mode->vrefresh = 60;

	mode->clock =  (mode->vrefresh * mode->vtotal *
		mode->htotal) / 1000;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

void lt070me05000_dump_regs(struct intel_dsi_device *dsi) { }

void lt070me05000_destroy(struct intel_dsi_device *dsi) { }

/* Callbacks. We might not need them all. */
struct intel_dsi_dev_ops jdi_lt070me05000_dsi_display_ops = {
	.init = lt070me05000_init,
	.get_info = lt070me05000_get_panel_info,
	.create_resources = lt070me05000_create_resources,
	.dpms = lt070me05000_dpms,
	.mode_valid = lt070me05000_mode_valid,
	.mode_fixup = lt070me05000_mode_fixup,
	.panel_reset = lt070me05000_panel_reset,
	.disable_panel_power = lt070me05000_disable_panel_power,
	.send_otp_cmds = lt070me05000_send_otp_cmds,
	.enable = lt070me05000_enable,
	.disable = lt070me05000_disable,
	.detect = lt070me05000_detect,
	.get_hw_state = lt070me05000_get_hw_state,
	.get_modes = lt070me05000_get_modes,
	.destroy = lt070me05000_destroy,
	.dump_regs = lt070me05000_dump_regs,
};
