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

    int geometry_x = 0;
    int geometry_y = 0;
    widget_getCoordinates(widget, &geometry_x, &geometry_y);
    widget->render(widget, ctx, geometry_x, geometry_y); 

    // Any children?
    if (widget->children) {
        foreach(child_node, widget->children) {
            widget_t *child = (widget_t*)child_node->value;
            
            // TODO: Create sub-context or rebase X and Y. Critical!
            widget_render(ctx, child);
        }
    }

    return 0;
}

/**
 * @brief Set widget event handler
 * @param widget The widget to set the event handler of
 * @param event The event
 * @param handler The handler
 * @param d Data variable
 */
int widget_setHandler(widget_t *widget, uint32_t event, void *handler, void *d) {
    switch (event) {
        case WIDGET_EVENT_CLICK:
            widget->user.click.fn = handler;
            widget->user.click.d = d;
            break;

        case WIDGET_EVENT_RIGHT_CLICK:
            widget->user.right_click.fn = handler;
            widget->user.right_click.d = d;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}

/**
 * @brief Internal method
 */
static int widget_updateReal(widget_t *widget, gfx_context_t *ctx) {
    // Calculate tick amount
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t ticks = tv.tv_sec * 1000000 + tv.tv_usec;

    int r = 0;

    if (widget->update) {
        int t = widget->update(widget, ctx, ticks);
        if (!r) r = t;
    }

    if (widget->children) {    
        foreach(child, widget->children) {
            int t = widget_updateReal((widget_t*)child->value, ctx);
            if (!r) r = t;
        }
    }

    return r;
}

/**
 * @brief Update widget and all of its children
 * @param widget The widget to update
 * @param ctx Graphics context
 * @returns 1 on updates
 */
int widget_update(widget_t *widget, gfx_context_t *ctx) {
    int r = widget_updateReal(widget, ctx);
    if (r) gfx_render(ctx);
    return r;
}