/**
 * @file userspace/lib/widget/geometry.c
 * @brief Widget geometry manager
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/widget.h>
#include <stdlib.h>

/**
 * @brief Render widget to X and Y coordinates
 * @param widget The widget to render
 * @param x The X coordinate in the window to render
 * @param y The Y coordinate in the window to render
 * @returns 0 on success, -1 on failure (errno set)
 */
int widget_renderAtCoordinates(struct widget *widget, int32_t x, int32_t y) {
    if (widget->geometry) {
        free(widget->geometry);
    }

    widget_geometry_xy_t *geometry = malloc(sizeof(widget_geometry_xy_t));
    geometry->type = GEOMETRY_TYPE_XY;
    geometry->x = x;
    geometry->y = y;

    widget->geometry = (widget_geometry_t*)geometry;
    return 0;
}