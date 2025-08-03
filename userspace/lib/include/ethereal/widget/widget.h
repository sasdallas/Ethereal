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

    void *impl;                     // Implementation/widget specific field
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