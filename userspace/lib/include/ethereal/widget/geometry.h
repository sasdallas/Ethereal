/**
 * @file userspace/lib/include/ethereal/widget/geometry.h
 * @brief Widget geometry manager
 * 
 * Ethereal supports managing widget geometry, via grid/pack/raw X + Y cordinates.
 * 
 * @todo GEOMETRY_TYPE_PACK
 * @todo GEOMETRY_TYPE_GRID
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_GEOMETRY_H
#define _WIDGET_GEOMETRY_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/
#define GEOMETRY_TYPE_XY                0

/**** TYPES ****/

/**
 * @brief Blank geometry header. Typecast to desired geometry
 */
typedef struct widget_geometry {
    int type;
    char data[];
} widget_geometry_t;

typedef struct widget_geometry_xy {
    int type;                       // Type of geometry
    int32_t x;                      // X coordinate to render to
    int32_t y;                      // Y coordinate to render to
} widget_geometry_xy_t;

struct widget;

/**** FUNCTIONS ****/

/**
 * @brief Render widget to X and Y coordinates
 * @param widget The widget to render
 * @param x The X coordinate in the window to render
 * @param y The Y coordinate in the window to render
 * @returns 0 on success, -1 on failure (errno set)
 */
int widget_renderAtCoordinates(struct widget *widget, int32_t x, int32_t y);

#endif