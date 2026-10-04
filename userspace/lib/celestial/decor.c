/**
 * @file userspace/lib/celestial/decor.c
 * @brief Decoration system
 * 
 * Includes Celestial's built-in Mercury, MINTIA, and Windows XP themes.
 * @todo Add support for loading other decorations (from shared libraries?)
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <structs/list.h>
#include <assert.h>

extern hashmap_t *celestial_window_map;

/* Built-in theme functions */
extern decor_t *celestial_loadMercury(decor_handler_t *handler, window_t *win);
extern decor_borders_t celestial_getBordersMercury(decor_handler_t *handler);
extern decor_t *celestial_loadMintia(decor_handler_t *handler, window_t *win);
extern decor_borders_t celestial_getBordersMintia(decor_handler_t *handler);

static decor_handler_t celestial_mercury_theme = {
    .theme = "mercury",
    .load = celestial_loadMercury,
    .borders = celestial_getBordersMercury, 
};

static decor_handler_t celestial_mintia_theme = {
    .theme = "mintia",
    .load = celestial_loadMintia,
    .borders = celestial_getBordersMintia,
};

static decor_handler_t *celestial_themes[] = {
    &celestial_mercury_theme,
    &celestial_mintia_theme
};

/* Default decoration handler */
decor_handler_t *celestial_default_decor = &celestial_mercury_theme;

/* Outside of internal window */
#define DECOR_IN_BORDERS(x, y) ((int)x < win->decor->borders.left_width || (int)x > (int)win->info->width - win->decor->borders.right_width || (int)y < win->decor->borders.top_height || (int)y > (int)win->info->height - win->decor->borders.bottom_height)

/* Hacks */
int decor_was_last_in_borders = 0; // Was the last mouse state in the borders?
int decor_faked_exit_event = 0; // Decor faked last mouse event?
int decor_last_clicked_in_borders = 0; // The last click was in the borders?
bool decor_had_custom_mouse = false; // Whether the mouse sprite was one for resizing
unsigned char decor_last_resize_direction = 0;

/**
 * @brief Initialize specific decorations for a window
 * @param win The window to initialize decorations on
 * @param decor The decorations to initialize on the window
 */
int celestial_initDecorations(struct window *win, decor_handler_t *decor) {
    win->decor = decor->load(decor, win);
    
    // Temporary shenanigans
    size_t width = win->width;
    size_t height = win->height;
    win->width = CELESTIAL_REAL_WIDTH(win);
    win->height = CELESTIAL_REAL_HEIGHT(win);

    // Setup the window's graphics context
    win->decor->ctx = celestial_getGraphicsContext(win);
    win->ctx = NULL;

    // Setup titlebar too
    win->decor->titlebar = "Celestial Window";

    // By default, Celestial focuses us
    win->decor->focused = 1;

    // Initialize decorations
    win->decor->init(win);
    win->decor->render(win);

    // Create new graphics context for the window
    win->ctx = malloc(sizeof(gfx_context_t));
    memset(win->ctx, 0, sizeof(gfx_context_t));
    win->ctx->width = win->decor->ctx->width;
    win->ctx->pitch = win->decor->ctx->pitch;
    win->ctx->height = win->decor->ctx->height;
    win->ctx->bpp = win->decor->ctx->bpp;
    win->ctx->flags = win->decor->ctx->flags;

    // Move window graphics context
    win->ctx->buffer = &GFX_PIXEL_REAL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
    if (win->decor->ctx->backbuffer) win->ctx->backbuffer = &GFX_PIXEL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
    win->ctx->width -= win->decor->borders.right_width + win->decor->borders.left_width;
    win->ctx->height -= win->decor->borders.bottom_height + win->decor->borders.top_height;
    
    win->width = width;
    win->height = height;
    return 0;
}

/**
 * @brief Initialize the default decorations for Celestial on a window
 * @param win The window to initialize decorations on
 */
int celestial_initDecorationsDefault(struct window *win) {
    return celestial_initDecorations(win, celestial_getDefaultDecorations());
}

/**
 * @brief Get a decoration theme
 * @param name The name of the decoration theme
 * @returns The theme or NULL
 */
decor_handler_t *celestial_getDecorationTheme(char *name) {
    if (!name) return NULL;
    size_t nthemes = sizeof(celestial_themes) / sizeof(celestial_themes[0]);
    for (unsigned i = 0; i < nthemes; i++) {
        if (!strcmp(celestial_themes[i]->theme, name)) {
            return celestial_themes[i];
        }
    }

    return NULL;
}

/**
 * @brief Get default decorations
 * @returns The default decorations
 */
decor_handler_t *celestial_getDefaultDecorations() {
    char name[CELESTIAL_THEME_NAME_MAX];
    if (celestial_getServerTheme(name, sizeof(name)) == 0) {
        decor_handler_t *theme = celestial_getDecorationTheme(name);
        celestial_default_decor = theme ? theme : &celestial_mercury_theme;
    }

    // an environ can also be used to override
    decor_handler_t *theme = celestial_getDecorationTheme(getenv("CELESTIAL_THEME"));
    if (theme) return theme;
    return celestial_default_decor;
}

/**
 * @brief Actually apply window theme when everything is ready
 */
static int celestial_applyWindowTheme2(window_t *win, decor_handler_t *handler) {
    if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) || !win->decor || win->decor->handler == handler) {
        return 0;
    }

    // get the new theme and calculate the new stuff
    decor_t *old = win->decor;
    decor_borders_t borders = celestial_getDecorationBorders(handler);
    int width = (int)win->info->width - borders.left_width - borders.right_width;
    int height = (int)win->info->height - borders.top_height - borders.bottom_height;
    if (width <= 0 || height <= 0) {
        assert(0);
    }

    // !!! hacky stuff to create a new context
    size_t old_width = win->width;
    size_t old_height = win->height;
    uint32_t *pixels = malloc(old_width * old_height * sizeof(uint32_t));
    uint8_t *source = old->ctx->backbuffer ? old->ctx->backbuffer : old->ctx->buffer;
    for (size_t y = 0; y < old_height; y++) {
        memcpy(pixels + y * old_width,
               source + (y + old->borders.top_height) * old->ctx->pitch + old->borders.left_width * 4,
               old_width * sizeof(uint32_t));
    }

    decor_t *decor = handler->load(handler, win);
    assert(decor && "applyWindowTheme2 doesn't handle ts case yet");

    decor->ctx = old->ctx;
    decor->titlebar = old->titlebar;
    decor->focused = old->focused;
    decor->resizable = old->resizable;
    win->decor = decor;
    win->width = width;
    win->height = height;
    decor->init(win);
    decor->render(win);

    win->ctx->width = width;
    win->ctx->height = height;
    win->ctx->buffer = &GFX_PIXEL_REAL(decor->ctx, borders.left_width, borders.top_height);
    if (decor->ctx->backbuffer) {
        win->ctx->backbuffer = &GFX_PIXEL(decor->ctx, borders.left_width, borders.top_height);
    }

    // !!! copy all the content so that it doesn't look that weird
    size_t copy_width = old_width < (size_t)width ? old_width : (size_t)width;
    size_t copy_height = old_height < (size_t)height ? old_height : (size_t)height;
    uint8_t *target = decor->ctx->backbuffer ? decor->ctx->backbuffer : decor->ctx->buffer;
    for (size_t y = 0; y < copy_height; y++) {
        memcpy(target + (y + borders.top_height) * decor->ctx->pitch + borders.left_width * 4,
               pixels + y * old_width, copy_width * sizeof(uint32_t));
    }

    free(pixels);
    gfx_render(decor->ctx);
    celestial_flip(win);

    if (old->font) gfx_destroyFont(old->font);
    free(old);

    // Fake a resize event
    if (old_width != (size_t)width || old_height != (size_t)height) {
        celestial_event_handler_t callback = celestial_lookupEventHandler(win, CELESTIAL_EVENT_RESIZE);
        if (callback) {
            celestial_event_resize_t event = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_RESIZE,
                .size = sizeof(event),
                .wid = win->wid,
                .new_width = width,
                .new_height = height,
                .buffer_key = win->key,
            };

            callback(win, CELESTIAL_EVENT_RESIZE, &event);
        }
    }

    return 0;
}

/**
 * @brief Apply a new window theme to a window
 */
static int celestial_applyWindowTheme(window_t *win, decor_handler_t *handler) {
    if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) || !win->decor || win->decor->handler == handler) {
        return 0;
    }

    decor_borders_t borders = celestial_getDecorationBorders(handler);
    size_t outer_width = win->width + borders.left_width + borders.right_width;
    size_t outer_height = win->height + borders.top_height + borders.bottom_height;

    if (outer_width == win->info->width && outer_height == win->info->height) {
        // same stuff? apply it in place rather than doing a resize first
        return celestial_applyWindowTheme2(win, handler);
    }

    celestial_req_resize_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_RESIZE,
        .size = sizeof(req),
        .wid = win->wid,
        .width = outer_width,
        .height = outer_height,
    };

    win->theme_pending = handler;
    if (celestial_sendRequest(&req, sizeof(req)) < 0) {
        win->theme_pending = NULL;
        return -1;
    }

    celestial_resp_resize_t *resp = celestial_getResponse(CELESTIAL_REQ_RESIZE);
    if (!resp) {
        win->theme_pending = NULL;
        return -1;
    }

    if (resp->magic == CELESTIAL_MAGIC_ERROR) {
        errno = -((celestial_resp_error_t*)resp)->error;
        free(resp);
        win->theme_pending = NULL;
        return -1;
    }

    free(resp);

    if (win->theme_pending) {
        win->theme_pending = NULL;
        return celestial_applyWindowTheme2(win, handler);
    }

    return 0;
}

/**
 * @brief Set a window theme
 * @param win The window to set the theme on
 * @param name Name of the theme (or NULL to use default)
 * @returns 0 on success
 */
int celestial_setWindowTheme(struct window *win, char *name) {
    if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED)) {
        return -1;
    }

    if (!name) {
        char server_theme[CELESTIAL_THEME_NAME_MAX];
        if (celestial_getServerTheme(server_theme, sizeof(server_theme)) == 0) {
            decor_handler_t *default_theme = celestial_getDecorationTheme(server_theme);
            celestial_default_decor = default_theme ? default_theme : &celestial_mintia_theme;
        }
    }

    decor_handler_t *theme = name ? celestial_getDecorationTheme(name) : celestial_default_decor;
    if (!theme) {
        return -1;
    }

    decor_handler_t *old_override = win->theme_override;
    win->theme_override = name ? theme : NULL;
    if (celestial_applyWindowTheme(win, theme) < 0) {
        win->theme_override = old_override;
        return -1;
    }

    return 0;
}

/**
 * @brief Update default theme, applying to all windows currently
 */
void celestial_updateDefaultTheme(char *name) {
    decor_handler_t *theme = celestial_getDecorationTheme(name);
    celestial_default_decor = theme ? theme : &celestial_mercury_theme;
    if (!celestial_window_map) return; // too early to do anything

    list_t *windows = hashmap_values(celestial_window_map);
    foreach(node, windows) {
        window_t *win = node->value;
        if (win->state != CELESTIAL_STATE_CLOSED && !win->theme_override) {
            celestial_applyWindowTheme(win, celestial_default_decor);
        }
    }

    list_destroy(windows, false);
}

/**
 * @brief Get boundaries for decoration
 * @param handler The decoration handler to get boundaries for
 */
decor_borders_t celestial_getDecorationBorders(decor_handler_t *handler) {
    return handler->borders(handler);
}

/**
 * @brief Convert coordinates to scale direction
 */
static int celestial_getScaleDirection(window_t *win, int x, int y) {
#define ON_LEFT ((x <= win->decor->borders.left_width+4))
#define ON_RIGHT ((x >= (int)win->info->width - win->decor->borders.right_width - 8))
#define ON_TOP ((y <= win->decor->borders.top_height))
#define ON_BOTTOM ((y >= (int)win->info->height - win->decor->borders.bottom_height - 6))

    if (ON_TOP && ON_LEFT) return CELESTIAL_RESIZE_TOP_LEFT;
    if (ON_TOP && ON_RIGHT) return CELESTIAL_RESIZE_TOP_RIGHT;
    if (ON_BOTTOM && ON_RIGHT) return CELESTIAL_RESIZE_BOTTOM_RIGHT;
    if (ON_BOTTOM && ON_LEFT) return CELESTIAL_RESIZE_BOTTOM_LEFT;
    if (ON_TOP && y < 8) return CELESTIAL_RESIZE_TOP;
    if (ON_RIGHT) return CELESTIAL_RESIZE_RIGHT;
    if (ON_BOTTOM) return CELESTIAL_RESIZE_BOTTOM;
    if (ON_LEFT) return CELESTIAL_RESIZE_LEFT;

    return -1; // Not on resize border

#undef ON_LEFT
#undef ON_RIGHT
#undef ON_TOP
#undef ON_BOTTOM
}

static int resize_cursors[] = {
    CELESTIAL_MOUSE_VERTICAL,
    CELESTIAL_MOUSE_HORIZONTAL,
    CELESTIAL_MOUSE_HORIZONTAL,
    CELESTIAL_MOUSE_VERTICAL,
    CELESTIAL_MOUSE_DIAG_DESCEND,
    CELESTIAL_MOUSE_DIAG_ASCEND,
    CELESTIAL_MOUSE_DIAG_DESCEND,
    CELESTIAL_MOUSE_DIAG_ASCEND
};

/**
 * @brief Process mouse event (resize-specific garbage)
 */
static void celestial_handleDecorEventResize(window_t *win, void *event) {
    celestial_event_header_t *hdr = (celestial_event_header_t*)event;
    if (hdr->type == CELESTIAL_EVENT_MOUSE_MOTION) {
        celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t *)hdr; 
        int resize_dir = celestial_getScaleDirection(win, motion->x, motion->y);
        if (resize_dir == -1) {
            if (decor_had_custom_mouse) {
                celestial_setMouseCursor(CELESTIAL_MOUSE_DEFAULT);
                decor_had_custom_mouse = false;
            }
            return; // no resize direction
        }
        
        decor_last_resize_direction = resize_dir;

        celestial_setMouseCursor(resize_cursors[resize_dir]);
        decor_had_custom_mouse = true;
    } else if (hdr->type == CELESTIAL_EVENT_MOUSE_EXIT) {
        if (decor_had_custom_mouse) {
            celestial_setMouseCursor(CELESTIAL_MOUSE_DEFAULT);
            decor_had_custom_mouse = false;
        }
    } else if (hdr->type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN) {
        if (decor_had_custom_mouse) {
            celestial_startResizing(win, decor_last_resize_direction);
        }
    } else if (hdr->type == CELESTIAL_EVENT_MOUSE_BUTTON_UP) {
        if (decor_had_custom_mouse) {
            celestial_stopResizing(win);
        }
    }
}

/**
 * @brief Handle a mouse event
 * @param win The window the event occurred on
 * @param event The event to handle
 * @returns 1 if the event was handled and not to pass it to event handler
 */
int celestial_handleDecorationEvent(struct window *win, void *event) {
    celestial_event_header_t *hdr = (celestial_event_header_t*)event;

    // TODO: Adjust event position to ignore borders
    switch (hdr->type) {
        case CELESTIAL_EVENT_MOUSE_BUTTON_DOWN:
            // Depending on bounds
            celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t*)hdr;

            if (decor_had_custom_mouse && down->held & CELESTIAL_MOUSE_BUTTON_LEFT) {
                celestial_handleDecorEventResize(win,event);
                return 0;
            }

            if (DECOR_IN_BORDERS(down->x, down->y) && down->held & CELESTIAL_MOUSE_BUTTON_LEFT) {
                decor_last_clicked_in_borders = 1;
                int in_borders = (DECOR_IN_BORDERS(down->x, down->y));
                int b = in_borders ? win->decor->inbtn(win, down->x, down->y) : DECOR_BTN_NONE;
                if (b == DECOR_BTN_CLOSE) {
                    celestial_closeWindow(win);
                    return 0;
                }
            } else {
                decor_last_clicked_in_borders = 0;
                down->x -= win->decor->borders.left_width;
                down->y -= win->decor->borders.top_height;
                return 1;
            }
            return 0;

        case CELESTIAL_EVENT_MOUSE_BUTTON_UP:
            // Depending on bounds
            celestial_event_mouse_button_up_t *up = (celestial_event_mouse_button_up_t*)hdr;
            if (decor_had_custom_mouse) {
                celestial_handleDecorEventResize(win,hdr);
                return 0;
            }

            if (DECOR_IN_BORDERS(up->x, up->y)) {
                celestial_stopDragging(win);
                return 0;
            } else {
                up->x -= win->decor->borders.left_width;
                up->y -= win->decor->borders.top_height;
                return 1;
            }

        case CELESTIAL_EVENT_MOUSE_MOTION:
            // Depending on bounds
            celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t*)hdr;

            if (DECOR_IN_BORDERS(motion->x, motion->y) || decor_was_last_in_borders) {
                // Check if in bounods
                int in_borders = (DECOR_IN_BORDERS(motion->x, motion->y));
                int b = in_borders ? win->decor->inbtn(win, motion->x, motion->y) : DECOR_BTN_NONE;

                // TODO: Find a better way to do this without spamming state()
                if (b == DECOR_BTN_CLOSE) win->decor->state(win, DECOR_BTN_CLOSE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_CLOSE, DECOR_BTN_STATE_NORMAL);

                if (b == DECOR_BTN_MAXIMIZE) win->decor->state(win, DECOR_BTN_MAXIMIZE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_MAXIMIZE, DECOR_BTN_STATE_NORMAL);

                if (b == DECOR_BTN_MINIMIZE) win->decor->state(win, DECOR_BTN_MINIMIZE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_MINIMIZE, DECOR_BTN_STATE_NORMAL);

                // Fix hack
                if (!in_borders) {
                    // Look for event handler
                    celestial_event_handler_t h = celestial_lookupEventHandler(win, CELESTIAL_EVENT_MOUSE_ENTER);
                        
                    if (h) {
                        // Fake mouse enter event
                        celestial_event_mouse_enter_t e = {
                            .magic = CELESTIAL_MAGIC_EVENT,
                            .size = sizeof(celestial_event_mouse_exit_t),
                            .type = CELESTIAL_EVENT_MOUSE_ENTER,
                            .wid = win->wid,
                            .x = motion->x - win->decor->borders.left_width,
                            .y = motion->y - win->decor->borders.top_height,
                        };

                        h(win, CELESTIAL_EVENT_MOUSE_ENTER, &e);
                    }
                    
                    decor_was_last_in_borders = 0;
                } else {
                    if (!decor_was_last_in_borders) {
                        // Look for event handler 
                        celestial_event_handler_t h = celestial_lookupEventHandler(win, CELESTIAL_EVENT_MOUSE_EXIT);
                        
                        if (h) {
                            celestial_event_mouse_exit_t e = {
                                .magic = CELESTIAL_MAGIC_EVENT,
                                .size = sizeof(celestial_event_mouse_exit_t),
                                .type = CELESTIAL_EVENT_MOUSE_EXIT,
                                .wid = win->wid,
                            };

                            h(win, CELESTIAL_EVENT_MOUSE_EXIT, &e);
                        }

                        decor_faked_exit_event = 1;
                    }
                    
                    decor_was_last_in_borders = 1;
                }

                if (win->decor->resizable) {
                    celestial_handleDecorEventResize(win, event);
                }

                return 0;
            } else {
                if (decor_had_custom_mouse) {
                    celestial_setMouseCursor(CELESTIAL_MOUSE_DEFAULT);
                    decor_had_custom_mouse = false;
                }

                // Update motion X
                motion->x -= win->decor->borders.left_width;
                motion->y -= win->decor->borders.top_height;
                return 1;
            }
            

        case CELESTIAL_EVENT_MOUSE_ENTER:
            decor_was_last_in_borders = 1;
            return 0;

        case CELESTIAL_EVENT_MOUSE_EXIT:
            if (win->decor->resizable) {
                celestial_handleDecorEventResize(win, event);
            }
        
            if (!decor_faked_exit_event) {
                return 1;
            }

            decor_faked_exit_event = 0;
            return 0;

        case CELESTIAL_EVENT_MOUSE_DRAG:
            // Depending on bounds
            celestial_event_mouse_drag_t *drag = (celestial_event_mouse_drag_t*)hdr;
            if (DECOR_IN_BORDERS(drag->x, drag->y) && decor_last_clicked_in_borders) {
                celestial_startDragging(win);
                return 0;
            } else {
                // Update coordinates
                drag->x -= win->decor->borders.left_width;
                drag->y -= win->decor->borders.top_height;
                drag->win_x += win->decor->borders.left_width;
                drag->win_y += win->decor->borders.top_height;
                return 1;
            }

        case CELESTIAL_EVENT_FOCUSED:
            win->decor->focused = 1;
            win->decor->render(win);
            return 1; // Pass this event along

        case CELESTIAL_EVENT_UNFOCUSED:
            win->decor->focused = 0;
            win->decor->render(win);
            return 1; // Pass this event along

        default:
            return 1;
    }
}

/**
 * @brief Adjust actual X/Y coordinates to be inner window X/Y coordinates
 * Uses decoration bounds to adjust X/Y
 * 
 * @param win The window 
 * @param x X that corresponds to the global window
 * @param y Y that corresponds to the global window
 * @param x_out Output X
 * @param y_out Output Y
 */
void celestial_adjustCoordinates(struct window *win, int32_t x, int32_t y, int32_t *x_out, int32_t *y_out) {
    // Adjust X and Y
    *x_out = x - win->decor->borders.left_width;
    *y_out = y - win->decor->borders.top_height;
}
