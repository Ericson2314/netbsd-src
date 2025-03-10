/*	$NetBSD: radeon_fb.c,v 1.10 2021/08/07 16:19:17 thorpej Exp $	*/

/*
 * Copyright © 2007 David Airlie
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
 * Authors:
 *     David Airlie
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radeon_fb.c,v 1.10 2021/08/07 16:19:17 thorpej Exp $");

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fb.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/radeon_drm.h>
#include "radeon.h"

#include <drm/drm_fb_helper.h>

#include <linux/vga_switcheroo.h>

#ifdef __NetBSD__
#include "radeondrmkmsfb.h"

#include <linux/nbsd-namespace.h>
#endif

/* object hierarchy -
   this contains a helper + a radeon fb
   the helper contains a pointer to radeon framebuffer baseclass.
*/
struct radeon_fbdev {
	struct drm_fb_helper helper;
	struct radeon_framebuffer rfb;
	struct list_head fbdev_list;
	struct radeon_device *rdev;
};

#ifndef __NetBSD__
static struct fb_ops radeonfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = drm_fb_helper_cfb_fillrect,
	.fb_copyarea = drm_fb_helper_cfb_copyarea,
	.fb_imageblit = drm_fb_helper_cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
};
#endif


int radeon_align_pitch(struct radeon_device *rdev, int width, int bpp, bool tiled)
{
	int aligned = width;
	int align_large = (ASIC_IS_AVIVO(rdev)) || tiled;
	int pitch_mask = 0;

	switch (bpp / 8) {
	case 1:
		pitch_mask = align_large ? 255 : 127;
		break;
	case 2:
		pitch_mask = align_large ? 127 : 31;
		break;
	case 3:
	case 4:
		pitch_mask = align_large ? 63 : 15;
		break;
	}

	aligned += pitch_mask;
	aligned &= ~pitch_mask;
	return aligned;
}

static void radeonfb_destroy_pinned_object(struct drm_gem_object *gobj)
{
	struct radeon_bo *rbo = gem_to_radeon_bo(gobj);
	int ret;

	ret = radeon_bo_reserve(rbo, false);
	if (likely(ret == 0)) {
		radeon_bo_kunmap(rbo);
		radeon_bo_unpin(rbo);
		radeon_bo_unreserve(rbo);
	}
	drm_gem_object_unreference_unlocked(gobj);
}

static int radeonfb_create_pinned_object(struct radeon_fbdev *rfbdev,
					 struct drm_mode_fb_cmd2 *mode_cmd,
					 struct drm_gem_object **gobj_p)
{
	struct radeon_device *rdev = rfbdev->rdev;
	struct drm_gem_object *gobj = NULL;
	struct radeon_bo *rbo = NULL;
	bool fb_tiled = false; /* useful for testing */
	u32 tiling_flags = 0;
	int ret;
	int aligned_size, size;
	int height = mode_cmd->height;
	u32 bpp, depth;

	drm_fb_get_bpp_depth(mode_cmd->pixel_format, &depth, &bpp);

	/* need to align pitch with crtc limits */
	mode_cmd->pitches[0] = radeon_align_pitch(rdev, mode_cmd->width, bpp,
						  fb_tiled) * ((bpp + 1) / 8);

	if (rdev->family >= CHIP_R600)
		height = ALIGN(mode_cmd->height, 8);
	size = mode_cmd->pitches[0] * height;
	aligned_size = ALIGN(size, PAGE_SIZE);
	ret = radeon_gem_object_create(rdev, aligned_size, 0,
				       RADEON_GEM_DOMAIN_VRAM,
				       0, true, &gobj);
	if (ret) {
		printk(KERN_ERR "failed to allocate framebuffer (%d)\n",
		       aligned_size);
		return -ENOMEM;
	}
	rbo = gem_to_radeon_bo(gobj);

	if (fb_tiled)
		tiling_flags = RADEON_TILING_MACRO;

#ifdef __BIG_ENDIAN
	switch (bpp) {
	case 32:
		tiling_flags |= RADEON_TILING_SWAP_32BIT;
		break;
	case 16:
		tiling_flags |= RADEON_TILING_SWAP_16BIT;
	default:
		break;
	}
#endif

	if (tiling_flags) {
		ret = radeon_bo_set_tiling_flags(rbo,
						 tiling_flags | RADEON_TILING_SURFACE,
						 mode_cmd->pitches[0]);
		if (ret)
			dev_err(rdev->dev, "FB failed to set tiling flags\n");
	}


	ret = radeon_bo_reserve(rbo, false);
	if (unlikely(ret != 0))
		goto out_unref;
	/* Only 27 bit offset for legacy CRTC */
	ret = radeon_bo_pin_restricted(rbo, RADEON_GEM_DOMAIN_VRAM,
				       ASIC_IS_AVIVO(rdev) ? 0 : 1 << 27,
				       NULL);
	if (ret) {
		radeon_bo_unreserve(rbo);
		goto out_unref;
	}
	if (fb_tiled)
		radeon_bo_check_tiling(rbo, 0, 0);
	ret = radeon_bo_kmap(rbo, NULL);
	radeon_bo_unreserve(rbo);
	if (ret) {
		goto out_unref;
	}

	*gobj_p = gobj;
	return 0;
out_unref:
	radeonfb_destroy_pinned_object(gobj);
	*gobj_p = NULL;
	return ret;
}

static int radeonfb_create(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct radeon_fbdev *rfbdev =
		container_of(helper, struct radeon_fbdev, helper);
	struct radeon_device *rdev = rfbdev->rdev;
#ifndef __NetBSD__
	struct fb_info *info;
#endif
	struct drm_framebuffer *fb = NULL;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_gem_object *gobj = NULL;
	struct radeon_bo *rbo = NULL;
	int ret;
#ifndef __NetBSD__
	unsigned long tmp;
#endif

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	/* avivo can't scanout real 24bpp */
	if ((sizes->surface_bpp == 24) && ASIC_IS_AVIVO(rdev))
		sizes->surface_bpp = 32;

	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	ret = radeonfb_create_pinned_object(rfbdev, &mode_cmd, &gobj);
	if (ret) {
		DRM_ERROR("failed to create fbcon object %d\n", ret);
		return ret;
	}

	rbo = gem_to_radeon_bo(gobj);

#ifdef __NetBSD__
	ret = radeon_framebuffer_init(rdev->ddev, &rfbdev->rfb, &mode_cmd, gobj);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer %d\n", ret);
		goto out_unref;
	}

	(void)memset(rbo->kptr, 0, radeon_bo_size(rbo));

    {
	static const struct radeonfb_attach_args zero_rfa;
	struct radeonfb_attach_args rfa = zero_rfa;

	rfa.rfa_fb_helper = helper;
	rfa.rfa_fb_sizes = *sizes;
	rfa.rfa_fb_ptr = rbo->kptr;
	rfa.rfa_fb_linebytes = mode_cmd.pitches[0];

	helper->fbdev = config_found(rdev->ddev->dev, &rfa, NULL,
	    CFARGS(.iattr = "radeonfbbus"));
	if (helper->fbdev == NULL) {
		DRM_ERROR("failed to attach genfb\n");
		goto out_unref;
	}
    }
	fb = &rfbdev->rfb.base;
	rfbdev->helper.fb = fb;
#else
	/* okay we have an object now allocate the framebuffer */
	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		goto out_unref;
	}

	info->par = rfbdev;

	ret = radeon_framebuffer_init(rdev->ddev, &rfbdev->rfb, &mode_cmd, gobj);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer %d\n", ret);
		goto out_destroy_fbi;
	}

	fb = &rfbdev->rfb.base;

	/* setup helper */
	rfbdev->helper.fb = fb;

	memset_io(rbo->kptr, 0x0, radeon_bo_size(rbo));

	strcpy(info->fix.id, "radeondrmfb");

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);

	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
	info->fbops = &radeonfb_ops;

	tmp = radeon_bo_gpu_offset(rbo) - rdev->mc.vram_start;
	info->fix.smem_start = rdev->mc.aper_base + tmp;
	info->fix.smem_len = radeon_bo_size(rbo);
	info->screen_base = rbo->kptr;
	info->screen_size = radeon_bo_size(rbo);

	drm_fb_helper_fill_var(info, &rfbdev->helper, sizes->fb_width, sizes->fb_height);

	/* setup aperture base/size for vesafb takeover */
	info->apertures->ranges[0].base = rdev->ddev->mode_config.fb_base;
	info->apertures->ranges[0].size = rdev->mc.aper_size;

	/* Use default scratch pixmap (info->pixmap.flags = FB_PIXMAP_SYSTEM) */

	if (info->screen_base == NULL) {
		ret = -ENOSPC;
		goto out_destroy_fbi;
	}

	DRM_INFO("fb mappable at 0x%lX\n",  info->fix.smem_start);
	DRM_INFO("vram apper at 0x%lX\n",  (unsigned long)rdev->mc.aper_base);
	DRM_INFO("size %lu\n", (unsigned long)radeon_bo_size(rbo));
	DRM_INFO("fb depth is %d\n", fb->depth);
	DRM_INFO("   pitch is %d\n", fb->pitches[0]);

	vga_switcheroo_client_fb_set(rdev->ddev->pdev, info);
#endif
	return 0;

#ifndef __NetBSD__
out_destroy_fbi:
	drm_fb_helper_release_fbi(helper);
#endif
out_unref:
	if (rbo) {

	}
	if (fb && ret) {
		drm_gem_object_unreference(gobj);
		drm_framebuffer_unregister_private(fb);
		drm_framebuffer_cleanup(fb);
		kfree(fb);
	}
	return ret;
}

void radeon_fb_output_poll_changed(struct radeon_device *rdev)
{
	drm_fb_helper_hotplug_event(&rdev->mode_info.rfbdev->helper);
}

static int radeon_fbdev_destroy(struct drm_device *dev, struct radeon_fbdev *rfbdev)
{
	struct radeon_framebuffer *rfb = &rfbdev->rfb;
#ifdef __NetBSD__
	int ret;
#endif

#ifdef __NetBSD__
	/* XXX errno NetBSD->Linux */
	ret = -config_detach(rfbdev->helper.fbdev, DETACH_FORCE);
	if (ret)
		DRM_ERROR("failed to detach radeonfb: %d\n", ret);
	rfbdev->helper.fbdev = NULL;
#else
	drm_fb_helper_unregister_fbi(&rfbdev->helper);
	drm_fb_helper_release_fbi(&rfbdev->helper);
#endif

	if (rfb->obj) {
		radeonfb_destroy_pinned_object(rfb->obj);
		rfb->obj = NULL;
	}
	drm_fb_helper_fini(&rfbdev->helper);
	drm_framebuffer_unregister_private(&rfb->base);
	drm_framebuffer_cleanup(&rfb->base);

	return 0;
}

static const struct drm_fb_helper_funcs radeon_fb_helper_funcs = {
	.gamma_set = radeon_crtc_fb_gamma_set,
	.gamma_get = radeon_crtc_fb_gamma_get,
	.fb_probe = radeonfb_create,
};

int radeon_fbdev_init(struct radeon_device *rdev)
{
	struct radeon_fbdev *rfbdev;
	int bpp_sel = 32;
	int ret;

	/* select 8 bpp console on RN50 or 16MB cards */
	if (ASIC_IS_RN50(rdev) || rdev->mc.real_vram_size <= (32*1024*1024))
		bpp_sel = 8;

	rfbdev = kzalloc(sizeof(struct radeon_fbdev), GFP_KERNEL);
	if (!rfbdev)
		return -ENOMEM;

	rfbdev->rdev = rdev;
	rdev->mode_info.rfbdev = rfbdev;

	drm_fb_helper_prepare(rdev->ddev, &rfbdev->helper,
			      &radeon_fb_helper_funcs);

	ret = drm_fb_helper_init(rdev->ddev, &rfbdev->helper,
				 rdev->num_crtc,
				 RADEONFB_CONN_LIMIT);
	if (ret)
		goto free;

	ret = drm_fb_helper_single_add_all_connectors(&rfbdev->helper);
	if (ret)
		goto fini;

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(rdev->ddev);

	ret = drm_fb_helper_initial_config(&rfbdev->helper, bpp_sel);
	if (ret)
		goto fini;

	return 0;

fini:
	drm_fb_helper_fini(&rfbdev->helper);
free:
	kfree(rfbdev);
	return ret;
}

void radeon_fbdev_fini(struct radeon_device *rdev)
{
	if (!rdev->mode_info.rfbdev)
		return;

	radeon_fbdev_destroy(rdev->ddev, rdev->mode_info.rfbdev);
	kfree(rdev->mode_info.rfbdev);
	rdev->mode_info.rfbdev = NULL;
}

void radeon_fbdev_set_suspend(struct radeon_device *rdev, int state)
{
#ifndef __NetBSD__		/* XXX radeon fb suspend */
	fb_set_suspend(rdev->mode_info.rfbdev->helper.fbdev, state);
#endif
}

bool radeon_fbdev_robj_is_fb(struct radeon_device *rdev, struct radeon_bo *robj)
{
	if (robj == gem_to_radeon_bo(rdev->mode_info.rfbdev->rfb.obj))
		return true;
	return false;
}

void radeon_fb_add_connector(struct radeon_device *rdev, struct drm_connector *connector)
{
	drm_fb_helper_add_one_connector(&rdev->mode_info.rfbdev->helper, connector);
}

void radeon_fb_remove_connector(struct radeon_device *rdev, struct drm_connector *connector)
{
	drm_fb_helper_remove_one_connector(&rdev->mode_info.rfbdev->helper, connector);
}

void radeon_fbdev_restore_mode(struct radeon_device *rdev)
{
	struct radeon_fbdev *rfbdev = rdev->mode_info.rfbdev;
	struct drm_fb_helper *fb_helper;
	int ret;

	if (!rfbdev)
		return;

	fb_helper = &rfbdev->helper;

	ret = drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper);
	if (ret)
		DRM_DEBUG("failed to restore crtc mode\n");
}
