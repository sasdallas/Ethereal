/**
 * @file hexahedron/drivers/fb_linux.c
 * @brief Compatibility drivers for the Linux framebuffer
 * 
 * This is here so my xorg-server can just use the default xf86-video-fbdev
 * driver instead of mess with other bs.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/fb.h>
#include <kernel/mm/vmm.h>
#include <kernel/fs/devfs.h>
#include <kernel/debug.h>
#include <kernel/init.h>

/* Linux definitions */
#define FBIOGET_VSCREENINFO	0x4600
#define FBIOPUT_VSCREENINFO	0x4601
#define FBIOGET_FSCREENINFO	0x4602

struct fb_fix_screeninfo {
    char id[16];
    char *smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t type_aux;
    uint32_t visual;
    uint16_t xpanstep;
    uint16_t ypanstep;
    uint16_t ywrapstep;
    uint32_t line_length;
    char *mmio_start;
    uint32_t mmio_len;
    uint32_t accel;
    uint32_t reserved[3];
};

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;

    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;

    uint32_t nonstd;
    uint32_t activate;
    uint32_t height;
    uint32_t width;

    uint32_t accel_flags;

    uint32_t pixclock;
    uint32_t left_margin;
    uint32_t right_margin;
    uint32_t upper_margin;
    uint32_t lower_margin;
    uint32_t hsync_len;
    uint32_t vsync_len;
    uint32_t sync;
    uint32_t vmode;
    uint32_t reserved[6];
};

/* Device ops */
static int linuxfb_ioctl(devfs_node_t *node, unsigned long request, void *argp);
extern int fb_mmap_prepare(devfs_node_t *node, vmm_memory_range_t *range);
extern int fb_mmap(devfs_node_t *node, void *addr, size_t len, off_t offset, mmu_flags_t flags);
extern int fb_munmap(devfs_node_t *node, void *addr, size_t len, off_t offset);
static devfs_ops_t linux_fb_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = linuxfb_ioctl,
    .lseek = NULL,
    .poll = NULL,
    .poll_events = NULL,
    .mmap = fb_mmap,
    .mmap_prepare = fb_mmap_prepare,
    .munmap = fb_munmap,
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FB:LINUX", __VA_ARGS__)

/**
 * @brief ioctl linux fb
 */
static int linuxfb_ioctl(devfs_node_t *node, unsigned long request, void *argp) {
    fb_device_t *dev = node->priv;
    if (request == FBIOGET_VSCREENINFO) {
        struct fb_var_screeninfo vinfo;

        vinfo.xres = dev->info.xres;
        vinfo.yres = dev->info.yres;
        vinfo.xres_virtual = dev->info.xres_virtual;
        vinfo.yres_virtual = dev->info.yres_virtual;
        vinfo.bits_per_pixel = dev->info.bpp;
        vinfo.red.msb_right = 1;
        vinfo.green.msb_right = 1;
        vinfo.blue.msb_right = 1;
        vinfo.transp.msb_right = 1;
        vinfo.width = -1;
        vinfo.height = -1;
        vinfo.red.offset = 16;
        vinfo.red.length = 8;
        vinfo.green.offset = 8;
        vinfo.green.length = 8;
        vinfo.blue.offset = 0;
        vinfo.blue.length = 8;

        memcpy(argp, &vinfo, sizeof(struct fb_var_screeninfo));
    } else if (request == FBIOPUT_VSCREENINFO) {
        LOG(DEBUG, "FBIOPUT_VSCREENINFO is not supported\n");
    } else if (request == FBIOGET_FSCREENINFO) {
        struct fb_fix_screeninfo finfo = { 0 };
        strncpy(finfo.id, dev->info.name, sizeof(finfo.id)-1);
        finfo.smem_len = dev->info.mmio_length;
        finfo.mmio_len = dev->info.mmio_length;
        finfo.line_length = dev->info.pitch;
        finfo.visual = 2; // FB_VISUAL_TRUECOLOR
        finfo.type = 0; // FB_TYPE_PACKED_PIXELS
        memcpy(argp, &finfo, sizeof(struct fb_fix_screeninfo));
    } else {
        LOG(WARN, "Unrecognized ioctl: 0x%x\n", request);
        return -ENOTTY;
    }

    return 0;
}

/**
 * @brief Initialize
 */
static int linuxfb_init() {
    for (int i = 0; i < FB_MAX_DEVICES; i++) {
        if (fb_devices[i] != NULL) {
            char name[64];
            snprintf(name, 64, "linuxfb%d", fb_devices[i]->id);
            assert(devfs_register(devfs_root, name, VFS_BLOCKDEVICE, &linux_fb_dev_ops, DEVFS_MAJOR_VIDEO, fb_devices[i]->id, fb_devices[i]));
        }
    }

    return 0;
}

FS_INIT_ROUTINE(linuxfb, INIT_FLAG_DEFAULT, linuxfb_init, devfs);
