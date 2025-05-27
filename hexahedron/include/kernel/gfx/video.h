/**
 * @file hexahedron/include/kernel/gfx/video.h
 * @brief Video interface
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_GFX_VIDEO_H
#define KERNEL_GFX_VIDEO_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* ioctl() calls */
#define IO_VIDEO_GET_INFO           0x3000  // Get video mode information
#define IO_VIDEO_SET_INFO           0x3001  // Set video mode information
#define IO_VIDEO_GET_FRAMEBUFFER    0x3002  // Get framebuffer call
#define IO_VIDEO_FLUSH_FRAMEBUFFER  0x3004  // Flush the framebuffer, as the kernel implements double buffering

/**** TYPES ****/

/**
 * @brief Returned video information structure
 */
typedef struct video_info {
    uintptr_t screen_width;         // Width
    uintptr_t screen_height;        // Height
    uintptr_t screen_pitch;         // Pitch
    uintptr_t screen_bpp;           // BPP
    int graphics;                   // Old
} video_info_t;

#endif