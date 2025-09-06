/**
 * @file userspace/lib/include/ethereal/desktop.h
 * @brief Ethereal desktop API
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DESKTOP_H
#define _DESKTOP_H

/**** INCLUDES ****/
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

#define TRAY_WIDGET_STATE_DISABLED          0
#define TRAY_WIDGET_STATE_IDLE              1
#define TRAY_WIDGET_STATE_HIGHLIGHTED       2
#define TRAY_WIDGET_STATE_HELD              3
#define TRAY_WIDGET_STATE_ACTIVE            4

/**** TYPES ****/

struct desktop_tray_widget;
struct desktop_tray_widget_data;

/**
 * @brief Init method, called on widget load
 */
typedef int (*twidget_init_t)(struct desktop_tray_widget *widget);

/**
 * @brief Deinit method, called on widget unload
 */
typedef int (*twidget_deinit_t)(struct desktop_tray_widget *widget);

/**
 * @brief Icon method, called every update loop
 * 
 * Draw your icon in @c widget->ctx
 */
typedef int (*twidget_icon_t)(struct desktop_tray_widget *widget);

/**
 * @brief Mouse enter method
 */
typedef void (*twidget_enter_t)(struct desktop_tray_widget *widget);

/**
 * @brief Mouse exit method
 */
typedef void (*twidget_exit_t)(struct desktop_tray_widget *widget);

/**
 * @brief Set state method
 */
typedef void (*twidget_set_t)(struct desktop_tray_widget *widget, int state);


/**
 * @brief Actual desktop tray widget
 * 
 * This is passed to functions
 */
typedef struct desktop_tray_widget {
    struct desktop_tray_widget_data *data;      // Widget data
    size_t width;                               // Width of the widget. IMPORTANT: This is supposed to be constant for bounding!!
    size_t height;                              // Height of the widget. IMPORTANT: This is supposed to be constant for bounding!!
    size_t padded_left;                         // Left padding
    size_t padded_right;                        // Right padding
    size_t padded_top;                          // Top padding
    size_t padded_bottom;                       // Bottom padding

    uint8_t state;                              // State
    gfx_rect_t rect;                            // Tray icon rectangle
    gfx_context_t *ctx;                         // Tray icon context
    void *dso;                                  // DSO handle
    void *d;                                    // Specific to widget instance
} desktop_tray_widget_t;

/**
 * @brief Desktop tray widget
 * 
 * You must declare this in your program under the name of "this_widget"
 * This will be used to create a copy of this and add it to the main widget list.
 */
typedef struct desktop_tray_widget_data {
    char *name;                 // Name of the tray widget
    twidget_init_t init;        // Tray widget init method
    twidget_deinit_t deinit;    // Tray widget deinit method
    twidget_icon_t icon;        // Tray widget icon method
    twidget_enter_t enter;      // Tray widget enter method
    twidget_exit_t exit;        // Tray widget exit method
    twidget_set_t set;          // Tray widget set method
} desktop_tray_widget_data_t;

#endif