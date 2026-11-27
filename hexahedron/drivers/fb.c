/**
 * @file hexahedron/drivers/fb.c
 * @brief Framebuffer driver
 * 
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
#include <kernel/init.h>
#include <kernel/gfx/video.h>

/* Framebuffer disable switch */
/* Set to true upon mmap of framebuffer device, TODO setup a VT system like Linux's */
bool fb_is_disabled = false;

/* Framebuffer list */
fb_device_t *fb_devices[FB_MAX_DEVICES] = { NULL };

/* Active devices */
int fb_active_devices = 0;

/* Some hackery */
static int fb_map_count = 0;

/* Device filesystem */
int fb_ioctl(devfs_node_t *node, unsigned long request, void *argp);
int fb_mmap_prepare(devfs_node_t *node, vmm_memory_range_t *range);
int fb_mmap(devfs_node_t *node, void *addr, size_t len, off_t offset, mmu_flags_t flags);
int fb_munmap(devfs_node_t *node, void *addr, size_t len, off_t offset);
devfs_ops_t fb_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = fb_ioctl,
    .lseek = NULL,
    .poll = NULL,
    .poll_events = NULL,
    .mmap = fb_mmap,
    .mmap_prepare = fb_mmap_prepare,
    .munmap = fb_munmap,
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FRAMEBUFFER", __VA_ARGS__)

/**
 * @brief ioctl handler
 */
int fb_ioctl(devfs_node_t *node, unsigned long request, void *argp) {
    fb_device_t *dev = node->priv;

    if (request == IO_VIDEO_GET_INFO) {
        // Compatibility ioctl with the old video API
        video_info_t info = {
            .screen_width = dev->info.xres,
            .screen_height = dev->info.yres,
            .screen_pitch = dev->info.pitch,
            .screen_bpp = dev->info.bpp,
            .graphics = 1,
        };

        memcpy(argp, &info, sizeof(video_info_t));
        return 0;
    } else if (request == IO_VIDEO_SET_INFO) {
        // IO_VIDEO_SET_INFO is not implemented
        return -EINVAL;
    } else if (request == IO_FB_GET_INFO) {
        fb_info_t snapshot;
        spinlock_acquire(&dev->fb_lock);
        memcpy(&snapshot, &dev->info, sizeof(fb_info_t));
        spinlock_release(&dev->fb_lock);
        
        memcpy(argp, &snapshot, sizeof(fb_info_t));
        return 0;
    } else if (request == IO_FB_SET_INFO) {
        fb_info_t info = *(fb_info_t*)argp;

        // info is not allowed to change anything static
        // static fields are for now name, mmio_length, and mmio_start
        if (dev->info.mmio_start != info.mmio_start || dev->info.mmio_length != info.mmio_length || memcmp(dev->info.name, info.name, sizeof(info.name))) {
            return -EINVAL;
        }

        if (dev->ops.fb_validate == NULL || dev->ops.fb_set == NULL) {
            return -ENOSYS;
        }

        // make sure the info is valid
        if (dev->ops.fb_validate(dev, &info) == false) {
            return -ENOTSUP;
        }

        // do atomic switch
        spinlock_acquire(&dev->fb_lock);
        int rval = dev->ops.fb_set(dev, &info);
        if (rval != 0) {
            spinlock_release(&dev->fb_lock);
            return rval;
        }

        memcpy(&dev->info, &info, sizeof(fb_info_t));
        spinlock_release(&dev->fb_lock);

        return 0;
    } else {
        return -ENOTTY;
    }
}

/**
 * @brief mmap prepare
 */
int fb_mmap_prepare(devfs_node_t *node, vmm_memory_range_t *range) {
    range->vmm_flags |= VM_FLAG_DEVICE;
    return 0;
}

/**
 * @brief mmap
 */
int fb_mmap(devfs_node_t *node, void *addr, size_t len, off_t offset, mmu_flags_t flags) {
    fb_device_t *dev = node->priv;
    if (offset < 0 || (size_t)offset >= dev->info.mmio_length) return -EINVAL;
    if (len > dev->info.mmio_length - (size_t)offset) len = dev->info.mmio_length - (size_t)offset;

    if (dev->ops.fb_mmap) {
        int r = dev->ops.fb_mmap(dev, addr, len, offset, flags);
        if (r != 0) {
            return r;
        }
    } else {
        for (uintptr_t i = 0; i < len; i += PAGE_SIZE) {
            arch_mmu_map(NULL, (uintptr_t)addr + i, (uintptr_t)dev->info.mmio_start + i + offset, flags | MMU_FLAG_WC);
        }
    }

    fb_map_count += 1;
    fb_is_disabled = true;
    return 0;
}

/**
 * @brief munmap
 */
int fb_munmap(devfs_node_t *node, void *addr, size_t len, off_t offset) {
    fb_device_t *dev = node->priv;
    
    if (dev->ops.fb_munmap) {
        dev->ops.fb_munmap(dev, addr, len, offset);
    } else {
        size_t bsize = dev->info.mmio_length;
        if (len > bsize) len = bsize;
        for (uintptr_t i = (uintptr_t)addr; i < (uintptr_t)addr + len; i += PAGE_SIZE) {
            arch_mmu_unmap(NULL, i);
        }
    }

    fb_map_count -= 1;
    fb_is_disabled = (fb_map_count != 0);
    return 0;
}

/**
 * @brief Register a new framebuffer device
 * @param dev The framebuffer device to register
 */
void fb_register(fb_device_t *dev) {
    LOG(INFO, "Registering new framebuffer device \"%s\"\n", dev->info.name);

    assert(fb_active_devices < FB_MAX_DEVICES);
    dev->id = fb_active_devices++;
    fb_devices[dev->id] = dev;
}

/**
 * @brief Plot a pixel
 * @param dev The device to plot the pixel on
 * @param x The X coordinate to plot at
 * @param y The Y coordinate to plot at
 * @param color The color to plot
 * 
 * @warning This API is unstable and should be removed
 */
void fb_plotPixel(fb_device_t *dev, int x, int y, color_t color) {
    if (!dev || fb_is_disabled) return;
    if ((uint32_t)x >= dev->info.xres || (uint32_t)y >= dev->info.yres) return;

    uint32_t *pixel = (uint32_t*)(dev->screen_base + (size_t)y * dev->info.pitch + (size_t)x * 4);
    *pixel = color.rgb;
}

/**
 * @brief Clear a framebuffer
 * @param dev The device to clear
 * @warning This API is not recommended for use
 */
void fb_clearScreen(fb_device_t *dev, color_t color) {
    if (!dev || fb_is_disabled) return;

    for (uint32_t y = 0; y < dev->info.yres; y++) {
        uint32_t *row = (uint32_t*)(dev->screen_base + (size_t)y * dev->info.pitch);
        for (uint32_t x = 0; x < dev->info.xres; x++) {
            row[x] = color.rgb;
        }
    }
}

/**
 * @brief Framebuffer device init
 */
static int fb_init() {
    for (int i = 0; i < FB_MAX_DEVICES; i++) {
        if (fb_devices[i] != NULL) {
            char name[64];
            snprintf(name, 64, "fb%d", fb_devices[i]->id);
            assert(devfs_register(devfs_root, name, VFS_BLOCKDEVICE, &fb_dev_ops, DEVFS_MAJOR_VIDEO, fb_devices[i]->id, fb_devices[i]));
        }
    }

    return 0;
}

FS_INIT_ROUTINE(fb, INIT_FLAG_DEFAULT, fb_init, devfs);
