/**
 * @file hexahedron/include/kernel/gfx/color.h
 * @brief Color definitions for 32 bits per pixel
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_GFX_COLOR_H
#define KERNEL_GFX_COLOR_H

typedef union _color {
    unsigned long rgb;
    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
    } c;
} color_t;

#define RGB_R(color) (color.c.r & 255)
#define RGB_G(color) (color.c.g & 255)
#define RGB_B(color) (color.c.b & 255)
#define RGB(red, green, blue) (color_t){.c.r = red, .c.g = green, .c.b = blue}

#define COLOR_BLACK             RGB(0, 0, 0)
#define COLOR_BLUE              RGB(0, 0, 170)
#define COLOR_GREEN             RGB(0, 170, 0)
#define COLOR_CYAN              RGB(0, 170, 170)
#define COLOR_RED               RGB(170, 0, 0)
#define COLOR_PURPLE            RGB(170, 0, 170)
#define COLOR_BROWN             RGB(170, 85, 0)
#define COLOR_GRAY              RGB(170, 170, 170)
#define COLOR_DARK_GRAY         RGB(85, 85, 85)
#define COLOR_LIGHT_BLUE        RGB(85, 85, 255)
#define COLOR_LIGHT_GREEN       RGB(85, 255, 85)
#define COLOR_LIGHT_CYAN        RGB(85, 255, 255)
#define COLOR_LIGHT_RED         RGB(255, 85, 85)
#define COLOR_LIGHT_PURPLE      RGB(255, 85, 255)
#define COLOR_YELLOW            RGB(255, 255, 85)
#define COLOR_WHITE             RGB(255, 255, 255)

#endif
