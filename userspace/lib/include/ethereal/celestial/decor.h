/**
 * @file userspace/lib/include/ethereal/celestial/decor.h
 * @brief Celestial window decorations
 * 
 * When using decorations, Celestial calls the load function on the window.
 * The load function then creates a new @c decor_t structure for the window. The window provides the decorations
 * with a graphics buffer @c decor_buffer and the decorations are expected to provide a window buffer for the user to draw in.
 * 
 * If 0 is returned on these, you can still setup the buffer in the window in @c init as you please.
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_DECOR_H
#define _CELESTIAL_DECOR_H

/**** INCLUDES ****/
#include <ethereal/celestial/types.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/
#define BORDER_NONE             (-1)    // No border, don't draw

#define DECOR_FLAG_DEFAULT      0x0     // Default

#define DECOR_BTN_NONE          0
#define DECOR_BTN_CLOSE         1
#define DECOR_BTN_MAXIMIZE      2
#define DECOR_BTN_MINIMIZE      3

#define DECOR_BTN_STATE_NORMAL  0
#define DECOR_BTN_STATE_HOVER   1


/**** TYPES ****/

struct window;
struct decor_handler;
struct decor;

/**
 * @brief Load decorations on window
 * @param handler The decoration handler
 * @param win The window to load decorations on
 * @returns Decoration object
 */
typedef struct decor* (*decor_load_t)(struct decor_handler *handler, struct window *win);

/**
 * @brief Get window bounds
 * @param handler The decoration handler
 */
typedef struct decor_borders (*decor_get_borders_t)(struct decor_handler *handler);

/**
 * @brief Initialize decorations
 * @param win The window to initialize decorations on
 * @returns 0 on success
 */
typedef int (*decor_init_t)(struct window *win);

/**
 * @brief Render decorations
 * @param win The window to render decorations on
 * @returns 0 on success
 */
typedef int (*decor_render_t)(struct window *win);

/**
 * @brief Check whether we're in bounds of a certain button
 * @param win The window to check
 * @param x The X coordinate
 * @param y The Y coordinate
 * @returns Button ID in bounds
 */
typedef int (*decor_in_bounds_t)(struct window *win, int32_t x, int32_t y);

/**
 * @brief Update the state of a button
 * @param win The window
 * @param btn The button ID to update
 * @param state The new state to use
 * @returns 0 on success
 */ 
typedef int (*decor_update_state_t)(struct window *win, int btn, int state);

typedef struct decor_handler {
    char *theme;                    // Theme name
    decor_load_t load;              // Load method
    decor_get_borders_t borders;    // Get borders method (for adjusting width/height) 
} decor_handler_t;

typedef struct decor_borders {
    int top_height;             // Top boundary height
    int bottom_height;          // Bottom boundary height
    int left_width;             // Left boundary width
    int right_width;            // Right boundary width
} decor_borders_t;

typedef struct decor {
    struct window *win;         // Window
    decor_handler_t *handler;   // Parent handler
    char *titlebar;             // Titlebar
    uint8_t flags;              // Flags of decoration
    gfx_context_t *ctx;         // Graphics context of the decorations

    decor_borders_t borders;    // Borders

    decor_init_t init;          // Initialize decorations
    decor_render_t render;      // Render decorations
    decor_in_bounds_t inbtn;    // Button bounds check
    decor_update_state_t state; // Update button state

    
    gfx_font_t *font;           // Font
    void *d;                    // Specific to decoration handler
} decor_t;

// Real window information
typedef struct decor_window_info {
    size_t width;               // Actual window width
    size_t height;              // Actual window height
} decor_window_info_t;



/**** FUNCTIONS ****/

/**
 * @brief Initialize specific decorations for a window
 * @param win The window to initialize decorations on
 * @param decor The decorations to initialize on the window
 */
int celestial_initDecorations(struct window *win, decor_handler_t *decor);

/**
 * @brief Initialize the default decorations for Celestial on a window
 * @param win The window to initialize decorations on
 */
int celestial_initDecorationsDefault(struct window *win);

/**
 * @brief Get default decorations
 * @returns The default decorations
 */
decor_handler_t *celestial_getDefaultDecorations();

/**
 * @brief Get boundaries for decoration
 * @param handler The decoration handler to get boundaries for
 */
decor_borders_t celestial_getDecorationBorders(decor_handler_t *handler);

/**
 * @brief Handle a mouse event
 * @param win The window the event occurred on
 * @param event The event to handle
 * @returns 1 if the event was handled and not to pass it to event handler
 */
int celestial_handleDecorationEvent(struct window *win, void *event);


#endif