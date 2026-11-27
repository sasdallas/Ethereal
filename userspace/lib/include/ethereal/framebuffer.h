/**
 * @file userspace/lib/include/ethereal/framebuffer.h
 * @brief Ethereal framebuffer API
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _ETHEREAL_FRAMEBUFFER_H
#define _ETHEREAL_FRAMEBUFFER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define IO_FB_GET_INFO          0x4000
#define IO_FB_SET_INFO          0x4001

/**** TYPES ****/

// this is a larp of Linux's fb_var_screeninfo mixed with fb_fix_screeninfo
typedef struct fb_info {
    char name[16];

    // xres and yres are the resolution the display shows to the user
    uint32_t xres, yres;

    // xres_virtual and yres_virtual are the actual resolution of the framebuffer in memory
    uint32_t xres_virtual, yres_virtual;

    // xoffset and yoffset are the offsets of the virtual to physical resolution
    uint32_t xoffset, yoffset;
    
    // bits-per-pixel
    uint32_t bpp;

    // horizontal stride
    uint32_t pitch;
    
    // memory length from mmap
    size_t mmio_length;

    // physical memory base (ignore this and use mmap)
    char *mmio_start;
} fb_info_t;


#endif
