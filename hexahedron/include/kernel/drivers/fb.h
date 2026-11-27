/**
 * @file hexahedron/include/kernel/drivers/fb.h
 * @brief Framebuffer driver
 * 
 * This framebuffer driver is a larp of Linux's framebuffer driver
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_FB_H
#define DRIVERS_FB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <ethereal/framebuffer.h>
#include <kernel/misc/spinlock.h>
#include <stdbool.h>
#include <kernel/gfx/color.h>
#include <sys/types.h>
#include <kernel/mm/arch_mmu.h>

/**** DEFINITIONS ****/

#define FB_MAX_DEVICES      32

/**** TYPES ****/

typedef struct _fb_device {
    fb_info_t info;

    // /device/fbX
    int id;

    // this is a virtual buffer, the physical one is in fb_info_t
    char *screen_base;

    // ioctl control with fb_set
    spinlock_t fb_lock;
    struct {
        // retrurn true if new_info is ok
        bool (*fb_validate)(struct _fb_device*, fb_info_t *new_info);

        // upon success, dev->info will be updated atomically
        int (*fb_set)(struct _fb_device*, fb_info_t *new_info);
    
        // destroy framebuffer
        void (*fb_destroy)(struct _fb_device *);
        
        // memory map (optional)
        int (*fb_mmap)(struct _fb_device*, void *addr, size_t len, off_t offset, mmu_flags_t flags);
        void (*fb_munmap)(struct _fb_device*, void *addr, size_t len, off_t offset);
    } ops;

    void *priv;
} fb_device_t;

/**** VARIABLES ****/

extern fb_device_t *fb_devices[FB_MAX_DEVICES];
extern int fb_active_devices;
extern bool fb_is_disabled;

/**** FUNCTIONS ****/

/**
 * @brief Register a new framebuffer device
 * @param dev The framebuffer device to register
 */
void fb_register(fb_device_t *dev);

/**
 * @brief Plot a pixel
 * @param dev The device to plot the pixel on
 * @param x The X coordinate to plot at
 * @param y The Y coordinate to plot at
 * @param color The color to plot
 * 
 * @warning This API is unstable and should be removed
 */
void fb_plotPixel(fb_device_t *dev, int x, int y, color_t color);

/**
 * @brief Clear a framebuffer
 * @param dev The device to clear
 * @warning This API is not recommended for use
 */
void fb_clearScreen(fb_device_t *dev, color_t color);

/**
 * @brief Get the default framebuffer device
 * @warning This API is a temporary placeholder because a lot of the kernel revolves around the idea of
 *          one framebuffer only (fb.c replaces the old video.c driver which was effectively that idea).
 *          It always points to fb_devices[0]
 */
static inline fb_device_t *fb_getDefaultDevice() {
    return fb_devices[0];
}


#endif
