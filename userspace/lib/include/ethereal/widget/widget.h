/**
 * @file userspace/lib/include/ethereal/widget/widget.h
 * @brief Widget class
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_WIDGET_H
#define _WIDGET_WIDGET_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <structs/list.h>
#include <ethereal/widget/geometry.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

/* Types of widgets */
#define WIDGET_TYPE_FRAME                   0
#define WIDGET_TYPE_LABEL                   1
#define WIDGET_TYPE_BUTTON                  2

/**** TYPES ****/

struct widget;

/**
 * @brief Widget render function
 * @param widget The widget getting rendered
 * @param context The graphical context to the render the widget with
 * @param x The X to render the widget to
 * @param y The Y to render the widget to
 */
typedef void (*widget_render_t)(struct widget *widget, gfx_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Widget mouse down function
 * @param widget The widget that was clicked
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 * @param held Held buttons. These are @c CELESTIAL_MOUSE_BUTTON_... types
 */
typedef void (*widget_mouse_down_t)(struct widget *widget, gfx_context_t *ctx, int32_t x, int32_t y, int held);

/**
 * @brief Widget mouse up function
 * @param widget The widget that was released
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 * @param release Released button, @c CELESTIAL_MOUSE_BUTTON_... type
 */
typedef void (*widget_mouse_up_t)(struct widget *widget, gfx_context_t *ctx, int32_t x, int32_t y, int released);

/**
 * @brief Widget mouse enter function
 * @param widget The widget that was entered
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 */
typedef void (*widget_mouse_enter_t)(struct widget *widget, gfx_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Widget mouse exit function
 * @param widget The widget that was exited
 * @param ctx To render to
 */
typedef void (*widget_mouse_exit_t)(struct widget *widget, gfx_context_t *ctx);

/**
 * @brief Widget mouse motion function
 * @param widget The widget that was moved in
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 */
typedef void (*widget_mouse_motion_t)(struct widget *widget, gfx_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Widget structure
 */
typedef struct widget {
    int type;                       // Type of widget
    size_t width;                   // Width of the widget
    size_t height;                  // Height of the widget
    widget_geometry_t *geometry;    // Widget geometry
    list_t *children;               // List of widget children if it supports them. Will be rendered upon widget_render

    // Virtual table
    widget_render_t render;         // Render the widget
    widget_mouse_down_t down;       // Mouse down callback
    widget_mouse_up_t up;           // Mouse up callback
    widget_mouse_enter_t enter;     // Mouse enter callback
    widget_mouse_exit_t exit;       // Mouse exit callback
    widget_mouse_motion_t motion;   // Mouse motion callback

    // User callbacks

    void *impl;                     // Implementation/widget specific field
    void *user;                     // User-specific field (YOU CAN USE THIS ONE)
} widget_t;

/**** FUNCTIONS ****/

/**
 * @brief Create the @c widget_t structure
 * This creates a blank widget for you to fill in
 */
widget_t *widget_createEmpty();

/**
 * @brief Render a widget
 * @param ctx The context to render the widgets in
 * @param widget The widget to render
 * @returns 0 on success or -1 on failure with errno set
 */
int widget_render(gfx_context_t *ctx, widget_t *widget);

#endif