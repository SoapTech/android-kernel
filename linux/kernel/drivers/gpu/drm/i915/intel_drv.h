/*
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef __INTEL_DRV_H__
#define __INTEL_DRV_H__

#include <linux/i2c.h>
#include "i915_drm.h"
#include "i915_drv.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"
#include "drm_fb_helper.h"
#include "drm_dp_helper.h"

#define _wait_for(COND, MS, W) ({ \
	unsigned long timeout__ = jiffies + msecs_to_jiffies(MS);	\
	int ret__ = 0;							\
	while (!(COND)) {						\
		if (time_after(jiffies, timeout__)) {			\
			ret__ = -ETIMEDOUT;				\
			break;						\
		}							\
		if (W && drm_can_sleep()) msleep(W);	\
	}								\
	ret__;								\
})

#define wait_for_atomic_us(COND, US) ({ \
	unsigned long timeout__ = jiffies + usecs_to_jiffies(US);	\
	int ret__ = 0;							\
	while (!(COND)) {						\
		if (time_after(jiffies, timeout__)) {			\
			ret__ = -ETIMEDOUT;				\
			break;						\
		}							\
		cpu_relax();						\
	}								\
	ret__;								\
})

#define wait_for(COND, MS) _wait_for(COND, MS, 1)
#define wait_for_atomic(COND, MS) _wait_for(COND, MS, 0)

#define KHz(x) (1000*x)
#define MHz(x) KHz(1000*x)

/*
 * Display related stuff
 */

/* store information about an Ixxx DVO */
/* The i830->i865 use multiple DVOs with multiple i2cs */
/* the i915, i945 have a single sDVO i2c bus - which is different */
#define MAX_OUTPUTS 6
/* maximum connectors per crtcs in the mode set */
#define INTELFB_CONN_LIMIT 4

#define INTEL_I2C_BUS_DVO 1
#define INTEL_I2C_BUS_SDVO 2

/* these are outputs from the chip - integrated only
   external chips are via DVO or SDVO output */
#define INTEL_OUTPUT_UNUSED 0
#define INTEL_OUTPUT_ANALOG 1
#define INTEL_OUTPUT_DVO 2
#define INTEL_OUTPUT_SDVO 3
#define INTEL_OUTPUT_LVDS 4
#define INTEL_OUTPUT_TVOUT 5
#define INTEL_OUTPUT_HDMI 6
#define INTEL_OUTPUT_DISPLAYPORT 7
#define INTEL_OUTPUT_EDP 8
#define INTEL_OUTPUT_DSI 9
#define INTEL_OUTPUT_UNKNOWN 10

#define INTEL_DVO_CHIP_NONE 0
#define INTEL_DVO_CHIP_LVDS 1
#define INTEL_DVO_CHIP_TMDS 2
#define INTEL_DVO_CHIP_TVOUT 4

#define INTEL_DSI_COMMAND_MODE 0
#define INTEL_DSI_VIDEO_MODE   1

/* drm_display_mode->private_flags */
#define INTEL_MODE_PIXEL_MULTIPLIER_SHIFT (0x0)
#define INTEL_MODE_PIXEL_MULTIPLIER_MASK (0xf << INTEL_MODE_PIXEL_MULTIPLIER_SHIFT)
#define INTEL_MODE_DP_FORCE_6BPC (0x10)
/* This flag must be set by the encoder's mode_fixup if it changes the crtc
 * timings in the mode to prevent the crtc fixup from overwriting them.
 * Currently only lvds needs that. */
#define INTEL_MODE_CRTC_TIMINGS_SET (0x20)

#define DDC_SHORT_READ_SIZE 12
#define DDC_SEGMENT_OFFSET_MFGID 0


static inline void
intel_mode_set_pixel_multiplier(struct drm_display_mode *mode,
				int multiplier)
{
	mode->clock *= multiplier;
	mode->private_flags |= multiplier;
}

static inline int
intel_mode_get_pixel_multiplier(const struct drm_display_mode *mode)
{
	return (mode->private_flags & INTEL_MODE_PIXEL_MULTIPLIER_MASK) >> INTEL_MODE_PIXEL_MULTIPLIER_SHIFT;
}

struct intel_framebuffer {
	struct drm_framebuffer base;
	struct drm_i915_gem_object *obj;
};

struct intel_fbdev {
	struct drm_fb_helper helper;
	struct intel_framebuffer ifb;
	struct list_head fbdev_list;
	struct drm_display_mode *our_mode;
};

struct intel_encoder {
	struct drm_encoder base;
	int type;
	bool needs_tv_clock;
	/*
	 * Intel hw has only one MUX where encoders could be clone, hence a
	 * simple flag is enough to compute the possible_clones mask.
	 */
	bool cloneable;
	void (*hot_plug)(struct intel_encoder *);
	int crtc_mask;
	int port;
};

struct intel_connector {
	struct drm_connector base;
	struct intel_encoder *encoder;
};

struct intel_crtc {
	struct drm_crtc base;
	enum pipe pipe;
	enum plane plane;
	bool rotate180;
	u8 lut_r[256], lut_g[256], lut_b[256];
	int dpms_mode;
	bool active; /* is the crtc on? independent of the dpms mode */
	bool disp_suspend_state;
	bool primary_disabled; /* is the crtc obscured by a plane? */
	bool lowfreq_avail;
	struct intel_overlay *overlay;
	struct intel_unpin_work *unpin_work;
	struct intel_unpin_work *sprite_unpin_work;
	int fdi_lanes;

	/* Display surface base address adjustement for pageflips. Note that on
	 * gen4+ this only adjusts up to a tile, offsets within a tile are
	 * handled in the hw itself (with the TILEOFF register). */
	unsigned long dspaddr_offset;

	struct drm_i915_gem_object *cursor_bo;
	uint32_t cursor_addr;
	int16_t cursor_x, cursor_y;
	int16_t cursor_width, cursor_height;
	bool cursor_visible;
	unsigned int bpp;

	bool primary_alpha;
	bool sprite0_alpha;
	bool sprite1_alpha;

	/* We can share PLLs across outputs if the timings match */
	struct intel_pch_pll *pch_pll;
};

struct intel_plane {
	struct drm_plane base;
	int plane;
	enum pipe pipe;
	struct drm_i915_gem_object *obj, *old_obj;
	int max_downscale;
	bool rotate180;
	u32 lut_r[1024], lut_g[1024], lut_b[1024];
	void (*update_plane)(struct drm_plane *plane,
			     struct drm_framebuffer *fb,
			     struct drm_i915_gem_object *obj,
			     int crtc_x, int crtc_y,
			     unsigned int crtc_w, unsigned int crtc_h,
			     uint32_t x, uint32_t y,
			     uint32_t src_w, uint32_t src_h,
			     struct drm_pending_vblank_event *event);
	void (*disable_plane)(struct drm_plane *plane);
	int (*update_colorkey)(struct drm_plane *plane,
			       struct drm_intel_sprite_colorkey *key);
	void (*get_colorkey)(struct drm_plane *plane,
			     struct drm_intel_sprite_colorkey *key);
};

struct vlv_MA_component_enabled {
	union {
		u8 component;
		struct {
			u8 EnPlane:1;
			u8 EnSprite:1;
			u8 EnCursor:1;
			u8 reserved:5;
		};
	};
};

struct intel_watermark_params {
	unsigned long fifo_size;
	unsigned long max_wm;
	unsigned long default_wm;
	unsigned long guard_size;
	unsigned long cacheline_size;
};

struct cxsr_latency {
	int is_desktop;
	int is_ddr3;
	unsigned long fsb_freq;
	unsigned long mem_freq;
	unsigned long display_sr;
	unsigned long display_hpll_disable;
	unsigned long cursor_sr;
	unsigned long cursor_hpll_disable;
};

#define to_intel_crtc(x) container_of(x, struct intel_crtc, base)
#define to_intel_connector(x) container_of(x, struct intel_connector, base)
#define to_intel_encoder(x) container_of(x, struct intel_encoder, base)
#define to_intel_framebuffer(x) container_of(x, struct intel_framebuffer, base)
#define to_intel_plane(x) container_of(x, struct intel_plane, base)

#define DIP_HEADER_SIZE	5

#define DIP_TYPE_AVI    0x82
#define DIP_VERSION_AVI 0x2
#define DIP_LEN_AVI     13
#define DIP_AVI_PR_1    0
#define DIP_AVI_PR_2    1
#define DIP_AVI_RGB_QUANT_RANGE_DEFAULT	(0 << 2)
#define DIP_AVI_RGB_QUANT_RANGE_LIMITED	(1 << 2)
#define DIP_AVI_RGB_QUANT_RANGE_FULL	(2 << 2)
#define DIP_AVI_IT_CONTENT	(1 << 7)
#define DIP_AVI_BAR_BOTH	(3 << 2)
#define DIP_AVI_COLOR_ITU601	(1 << 6)
#define DIP_AVI_COLOR_ITU709	(2 << 6)

#define DIP_TYPE_SPD	0x83
#define DIP_VERSION_SPD	0x1
#define DIP_LEN_SPD	25
#define DIP_SPD_UNKNOWN	0
#define DIP_SPD_DSTB	0x1
#define DIP_SPD_DVDP	0x2
#define DIP_SPD_DVHS	0x3
#define DIP_SPD_HDDVR	0x4
#define DIP_SPD_DVC	0x5
#define DIP_SPD_DSC	0x6
#define DIP_SPD_VCD	0x7
#define DIP_SPD_GAME	0x8
#define DIP_SPD_PC	0x9
#define DIP_SPD_BD	0xa
#define DIP_SPD_SCD	0xb

struct dip_infoframe {
	uint8_t type;		/* HB0 */
	uint8_t ver;		/* HB1 */
	uint8_t len;		/* HB2 - body len, not including checksum */
	uint8_t ecc;		/* Header ECC */
	uint8_t checksum;	/* PB0 */
	union {
		struct {
			/* PB1 - Y 6:5, A 4:4, B 3:2, S 1:0 */
			uint8_t Y_A_B_S;
			/* PB2 - C 7:6, M 5:4, R 3:0 */
			uint8_t C_M_R;
			/* PB3 - ITC 7:7, EC 6:4, Q 3:2, SC 1:0 */
			uint8_t ITC_EC_Q_SC;
			/* PB4 - VIC 6:0 */
			uint8_t VIC;
			/* PB5 - YQ 7:6, CN 5:4, PR 3:0 */
			uint8_t YQ_CN_PR;
			/* PB6 to PB13 */
			uint16_t top_bar_end;
			uint16_t bottom_bar_start;
			uint16_t left_bar_end;
			uint16_t right_bar_start;
		} __attribute__ ((packed)) avi;
		struct {
			uint8_t vn[8];
			uint8_t pd[16];
			uint8_t sdi;
		} __attribute__ ((packed)) spd;
		uint8_t payload[27];
	} __attribute__ ((packed)) body;
} __attribute__((packed));

struct intel_hdmi {
	struct intel_encoder base;
	struct edid *edid;
	u32 sdvox_reg;
	int ddc_bus;
	int ddi_port;
	uint32_t color_range;
	bool has_hdmi_sink;
	bool has_audio;
	enum hdmi_force_audio force_audio;
	enum panel_fitter pfit;
	void (*write_infoframe)(struct drm_encoder *encoder,
				struct dip_infoframe *frame);
	void (*set_infoframes)(struct drm_encoder *encoder,
			       struct drm_display_mode *adjusted_mode);
	uint32_t edid_mode_count;
};

/*VLV clock bending*/
#define VLV_ACCUMULATOR_SIZE	249
#define ACCURACY_MULTIPLIER	1000000
#define BENDADJUST_MULT		10000000
#define PPM_MULTIPLIER		1000000
#define NANOSEC_MULTIPLIER	1000000000
#define INVERSE_BEND_RESOLUTION	(VLV_ACCUMULATOR_SIZE*48*128)

struct intel_program_clock_bending {
	u32 dotclock;
	u32 referenceclk;
	u32 targetclk;
	bool is_enable;
};

#define DP_RECEIVER_CAP_SIZE		0xf
#define DP_LINK_CONFIGURATION_SIZE	9
#define EDP_PSR_RECEIVER_CAP_SIZE	2

struct intel_dp {
	struct intel_encoder base;
	uint32_t output_reg;
	uint32_t DP;
	uint8_t  link_configuration[DP_LINK_CONFIGURATION_SIZE];
	bool has_audio;
	enum hdmi_force_audio force_audio;
	enum panel_fitter pfit;
	enum port port;
	uint32_t color_range;
	int dpms_mode;
	uint8_t link_bw;
	uint8_t lane_count;
	uint8_t dpcd[DP_RECEIVER_CAP_SIZE];
	uint8_t psr_dpcd[EDP_PSR_RECEIVER_CAP_SIZE];
	struct i2c_adapter adapter;
	struct i2c_algo_dp_aux_data algo;
	bool is_pch_edp;
	uint8_t train_set[4];
	int panel_power_up_delay;
	int panel_power_down_delay;
	int panel_power_cycle_delay;
	int backlight_on_delay;
	int backlight_off_delay;
	struct drm_display_mode *panel_fixed_mode;  /* for eDP */
	struct delayed_work panel_vdd_work;
	bool want_panel_vdd;
	struct edid *edid; /* cached EDID for eDP */
	int edid_mode_count;
	uint8_t psr_setup;
};

static inline struct drm_crtc *
intel_get_crtc_for_pipe(struct drm_device *dev, int pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	return dev_priv->pipe_to_crtc_mapping[pipe];
}

static inline struct drm_crtc *
intel_get_crtc_for_plane(struct drm_device *dev, int plane)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	return dev_priv->plane_to_crtc_mapping[plane];
}

struct intel_unpin_work {
	struct work_struct work;
	struct drm_device *dev;
	struct drm_i915_gem_object *old_fb_obj;
	struct drm_i915_gem_object *pending_flip_obj;
	struct drm_pending_vblank_event *event;
	int pending;
	bool enable_stall_check;
};

struct intel_fbc_work {
	struct delayed_work work;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb;
	int interval;
};

int intel_ddc_get_modes(struct drm_connector *c, struct i2c_adapter *adapter);
void intel_cleanup_modes(struct drm_connector *connector);


extern void intel_attach_force_audio_property(struct drm_connector *connector);
extern void intel_attach_broadcast_rgb_property(struct drm_connector *connector);
extern void intel_attach_force_pfit_property(struct drm_connector *connector);

extern void intel_crt_init(struct drm_device *dev);
extern void intel_hdmi_init(struct drm_device *dev,
			    int sdvox_reg, enum port port);
extern struct intel_hdmi *enc_to_intel_hdmi(struct drm_encoder *encoder);
extern void intel_dip_infoframe_csum(struct dip_infoframe *avi_if);
extern bool intel_sdvo_init(struct drm_device *dev, uint32_t sdvo_reg,
			    bool is_sdvob);
extern void intel_dvo_init(struct drm_device *dev);
extern void intel_tv_init(struct drm_device *dev);
extern void intel_mark_busy(struct drm_device *dev);
extern void intel_mark_idle(struct drm_device *dev);
extern void intel_mark_fb_busy(struct drm_i915_gem_object *obj);
extern void intel_mark_fb_idle(struct drm_i915_gem_object *obj);
extern bool intel_lvds_init(struct drm_device *dev);
extern void intel_dp_init(struct drm_device *dev, int output_reg,
			  enum port port);
extern bool intel_dsi_init(struct drm_device *dev);
void
intel_dp_set_m_n(struct drm_crtc *crtc, struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode);
extern bool intel_dpd_is_edp(struct drm_device *dev);
extern void intel_edp_link_config(struct intel_encoder *, int *, int *);
extern int intel_edp_target_clock(struct intel_encoder *,
				  struct drm_display_mode *mode);
extern bool intel_encoder_is_pch_edp(struct drm_encoder *encoder);
extern int intel_plane_init(struct drm_device *dev, enum pipe pipe, int plane);
extern void intel_flush_display_plane(struct drm_i915_private *dev_priv,
				      enum plane plane);

/* intel_panel.c */
extern void intel_fixed_panel_mode(struct drm_display_mode *fixed_mode,
				   struct drm_display_mode *adjusted_mode);
extern void intel_pch_panel_fitting(struct drm_device *dev,
				    int fitting_mode,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode);
extern u32 intel_panel_get_max_backlight(struct drm_device *dev);
extern void intel_panel_set_backlight(struct drm_device *dev, u32 level);
extern void intel_panel_actually_set_backlight(struct drm_device *dev,
						u32 level);
extern int intel_panel_setup_backlight(struct drm_device *dev);
extern void intel_panel_enable_backlight(struct drm_device *dev,
					 enum pipe pipe);
extern void intel_panel_disable_backlight(struct drm_device *dev);
extern void intel_panel_destroy_backlight(struct drm_device *dev);
extern enum drm_connector_status intel_panel_detect(struct drm_device *dev);

extern void intel_crtc_load_lut(struct drm_crtc *crtc);
extern void intel_encoder_prepare(struct drm_encoder *encoder);
extern void intel_encoder_commit(struct drm_encoder *encoder);
extern void intel_encoder_noop(struct drm_encoder *encoder);
extern void intel_encoder_destroy(struct drm_encoder *encoder);
extern void intel_hdmi_simulate_hpd(struct drm_device *dev, int hpd_on);
extern void i9xx_crtc_disable(struct drm_crtc *crtc);

static inline struct intel_encoder *intel_attached_encoder(struct drm_connector *connector)
{
	return to_intel_connector(connector)->encoder;
}


extern void intel_connector_attach_encoder(struct intel_connector *connector,
					   struct intel_encoder *encoder);
extern struct drm_encoder *intel_best_encoder(struct drm_connector *connector);

extern struct drm_display_mode *intel_crtc_mode_get(struct drm_device *dev,
						    struct drm_crtc *crtc);
int intel_get_pipe_from_crtc_id(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern void intel_wait_for_vblank(struct drm_device *dev, int pipe);
extern void intel_wait_for_pipe_off(struct drm_device *dev, int pipe);
extern int intel_enable_csc(struct drm_device *dev, void *csc_params,
		struct drm_file *file_priv);

struct intel_load_detect_pipe {
	struct drm_framebuffer *release_fb;
	bool load_detect_temp;
	int dpms_mode;
};
extern bool intel_get_load_detect_pipe(struct intel_encoder *intel_encoder,
				       struct drm_connector *connector,
				       struct drm_display_mode *mode,
				       struct intel_load_detect_pipe *old);
extern void intel_release_load_detect_pipe(struct intel_encoder *intel_encoder,
					   struct drm_connector *connector,
					   struct intel_load_detect_pipe *old);

extern void intelfb_restore(void);
extern void intel_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
				    u16 blue, int regno);
extern void intel_crtc_fb_gamma_get(struct drm_crtc *crtc, u16 *red, u16 *green,
				    u16 *blue, int regno);
extern void intel_enable_clock_gating(struct drm_device *dev);

extern int intel_pin_and_fence_fb_obj(struct drm_device *dev,
				      struct drm_i915_gem_object *obj,
				      struct intel_ring_buffer *pipelined);
extern void intel_unpin_fb_obj(struct drm_i915_gem_object *obj);

extern int intel_framebuffer_init(struct drm_device *dev,
				  struct intel_framebuffer *ifb,
				  struct drm_mode_fb_cmd2 *mode_cmd,
				  struct drm_i915_gem_object *obj);
extern int intel_fbdev_init(struct drm_device *dev);
extern void intel_fbdev_fini(struct drm_device *dev);
extern void intel_fbdev_set_suspend(struct drm_device *dev, int state);
extern void intel_prepare_page_flip(struct drm_device *dev, int plane);
extern void intel_finish_page_flip(struct drm_device *dev, int pipe);
extern void intel_finish_page_flip_plane(struct drm_device *dev, int plane);
extern void intel_prepare_sprite_page_flip(struct drm_device *dev,
						int plane);
extern void intel_finish_sprite_page_flip(struct drm_device *dev,
						int plane);

extern void intel_setup_overlay(struct drm_device *dev);
extern void intel_cleanup_overlay(struct drm_device *dev);
extern int intel_overlay_switch_off(struct intel_overlay *overlay);
extern int intel_overlay_put_image(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
extern int intel_overlay_attrs(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);

extern void intel_fb_output_poll_changed(struct drm_device *dev);
extern void intel_fb_restore_mode(struct drm_device *dev);

extern void assert_pipe(struct drm_i915_private *dev_priv, enum pipe pipe,
			bool state);
#define assert_pipe_enabled(d, p) assert_pipe(d, p, true)
#define assert_pipe_disabled(d, p) assert_pipe(d, p, false)

extern void intel_init_clock_gating(struct drm_device *dev);
extern void intel_write_eld(struct drm_encoder *encoder,
			    struct drm_display_mode *mode);
extern void intel_cpt_verify_modeset(struct drm_device *dev, int pipe);
extern void intel_prepare_ddi(struct drm_device *dev);
extern void hsw_fdi_link_train(struct drm_crtc *crtc);
extern void intel_ddi_init(struct drm_device *dev, enum port port);

/* For use by IVB LP watermark workaround in intel_sprite.c */
extern void intel_update_watermarks(struct drm_device *dev);
extern void intel_update_sprite_watermarks(struct drm_device *dev, int pipe,
					   uint32_t sprite_width,
					   int pixel_size);
extern void intel_update_linetime_watermarks(struct drm_device *dev, int pipe,
			 struct drm_display_mode *mode);

extern unsigned long intel_gen4_compute_page_offset(int *x, int *y,
						    unsigned int tiling_mode,
						    unsigned int bpp,
						    unsigned int pitch);

extern int intel_sprite_set_colorkey(struct drm_device *dev, void *data,
				     struct drm_file *file_priv);
extern int intel_sprite_get_colorkey(struct drm_device *dev, void *data,
				     struct drm_file *file_priv);

extern u32 intel_dpio_read(struct drm_i915_private *dev_priv, u32 reg);

/* Power-related functions, located in intel_pm.c */
extern void intel_init_pm(struct drm_device *dev);
extern bool vlv_rs_initialize(struct drm_device *dev);
extern void vlv_rs_sleepstateinit(struct drm_device *dev,
				 bool   bdisable_rs);

extern void vlv_rs_setstate(struct drm_device *dev,
				bool enable);

extern bool vlv_turbo_initialize(struct drm_device *dev);
extern void vlv_turbo_disable(struct drm_device *dev);

/* FBC */
extern bool intel_fbc_enabled(struct drm_device *dev);
extern void intel_enable_fbc(struct drm_crtc *crtc, unsigned long interval);
extern void intel_update_fbc(struct drm_device *dev);
/* IPS */
extern void intel_gpu_ips_init(struct drm_i915_private *dev_priv);
extern void intel_gpu_ips_teardown(void);

extern void intel_init_power_wells(struct drm_device *dev);
extern void intel_enable_gt_powersave(struct drm_device *dev);
extern void intel_disable_gt_powersave(struct drm_device *dev);
extern void gen6_gt_check_fifodbg(struct drm_i915_private *dev_priv);
extern void ironlake_teardown_rc6(struct drm_device *dev);

extern void intel_ddi_dpms(struct drm_encoder *encoder, int mode);
extern void intel_ddi_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
/* intel_dp.c */
extern void intel_edp_psr_ctl_ioctl(struct drm_device *device, void *data,
					struct drm_file *file_priv);
extern void intel_edp_psr_exit_ioctl(struct drm_device *device, void *data,
					struct drm_file *file_priv);
extern void intel_edp_get_psr_support(struct drm_device *device, void *data,
					struct drm_file *file);

/* VLV LP clock bending */
extern void valleyview_program_clock_bending(struct drm_i915_private *dev_priv,
		struct intel_program_clock_bending *clockbendargs);

extern ssize_t display_runtime_suspend(struct drm_device *drm_dev);
extern ssize_t display_runtime_resume(struct drm_device *drm_dev);
bool is_plane_enabled(struct drm_i915_private *dev_priv,
			enum plane plane);

bool is_sprite_enabled(struct drm_i915_private *dev_priv,
			enum pipe pipe, enum plane plane);
bool is_cursor_enabled(struct drm_i915_private *dev_priv,
			enum pipe pipe);
bool is_maxfifo_needed(struct drm_i915_private *dev_priv);

extern void intel_unpin_work_fn(struct work_struct *__work);
extern void intel_unpin_sprite_work_fn(struct work_struct *__work);
#endif /* __INTEL_DRV_H__ */