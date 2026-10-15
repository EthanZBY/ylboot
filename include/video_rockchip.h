/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef VIDEO_ROCKCHIP_H
#define VIDEO_ROCKCHIP_H

#define CONFIG_DRM_ROCKCHIP_VIDEO_FRAMEBUFFER
#ifdef CONFIG_DRM_ROCKCHIP_VIDEO_FRAMEBUFFER
 #define DRM_ROCKCHIP_FB_WIDTH		1920 // 800
 #define DRM_ROCKCHIP_FB_HEIGHT		1080 // 600
 #define DRM_ROCKCHIP_FB_BPP		VIDEO_BPP32
#else
 #define DRM_ROCKCHIP_FB_WIDTH		0
 #define DRM_ROCKCHIP_FB_HEIGHT		0
 #define DRM_ROCKCHIP_FB_BPP		VIDEO_BPP32
#endif

#define MEMORY_POOL_SIZE	CONFIG_DRM_MEM_RESERVED_SIZE_MBYTES * 1024 * 1024
#define DRM_ROCKCHIP_FB_SIZE \
	VNBYTES(DRM_ROCKCHIP_FB_BPP) * DRM_ROCKCHIP_FB_WIDTH * DRM_ROCKCHIP_FB_HEIGHT

int update_show_bmp(const char *bmp,unsigned char *dat);
int rockchip_show_bmp(const char *bmp);
int rockchip_show_logo(void);
void rockchip_display_fixup(void *blob);
int rockchip_display_fb_handed_over(const void *blob);
void rockchip_display_console_off(void);

#endif
