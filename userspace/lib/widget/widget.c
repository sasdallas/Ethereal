/**
 * @file userspace/lib/widget/widget.c
 * @brief Main widget code
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
#include <string.h>
#include <errno.h>

/**
 * @brief Create the @c widget_t structure
 * This creates a blank widget for you to fill in
 */
widget_t *widget_createEmpty() {
    widget_t *ret = malloc(sizeof(widget_t));
    memset(ret, 0, sizeof(widget_t));
    return ret;
}

/**
 * @brief Render a widget
 * @param ctx The context to render the widgets in
 * @param widget The widget to render
 * @returns 0 on success or -1 on failure with errno set
 */
int widget_render(gfx_context_t *ctx, widget_t *widget) {
    if (!ctx || !widget || !widget->geometry) {
        errno = EINVAL;
        return -1;
    }

    // First get the widget geometry
    if (widget->geometry->type == GEOMETRY_TYPE_XY) {
        // XY geometry! Easy!
        widget_geometry_xy_t *geometry = (widget_geometry_xy_t*)widget->geometry;
        widget->render(widget, ctx, geometry->x, geometry->y); 
    } else {
        errno = ENOSYS;
        return -1;
    }

    // Any children?
    if (widget->children) {
        foreach(child_node, widget->children) {
            widget_t *child = (widget_t*)child_node->value;
            widget_render(ctx, child);
        }
    }

    return 0;
}
