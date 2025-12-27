/**
 * @file userspace/celestial/mouse.c
 * @brief Mouse system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <unistd.h>
#include <errno.h>
#include <kernel/fs/periphfs.h>
#include <fcntl.h>

/* Mouse file descriptor */
int __celestial_mouse_fd = 0;

/* Current mouse position */
int __celestial_mouse_x = 0;
int __celestial_mouse_y = 0;

/* Last X/Y */
static int last_mouse_x = 0;
static int last_mouse_y = 0;

/* Mouse sprite */
sprite_t *__celestial_mouse_sprite = NULL;

/* Selected window */
wm_window_t *__celestial_mouse_window = NULL;

/* Currently/previously held mouse buttons */
uint32_t __celestial_mouse_buttons = 0;
uint32_t __celestial_previous_buttons = 0;

/* Calculated on left button press (for dragging) */
static int mouse_window_off_x = 0;
static int mouse_window_off_y = 0;

/* Mouse relative */
int __celestial_mouse_relative = 0;
static int mouse_rel_x = 0;
static int mouse_rel_y = 0;

/* Convert mouse x/y to window x/y */
#define WM_MOUSE_REL_WINDOW_X (WM_MOUSEX - WM_MOUSE_WINDOW->x)
#define WM_MOUSE_REL_WINDOW_Y (WM_MOUSEY - WM_MOUSE_WINDOW->y)

/* Loaded mouse sprites */
static sprite_t *mouse_default = NULL;
static sprite_t *mouse_text = NULL;

/**
 * @brief Load a mouse sprite
 */
void mouse_load(sprite_t **sp, char *filename) {
    *sp = gfx_createSprite(0, 0);

    FILE *f = fopen(filename, "r");
    if (!f) {
        CELESTIAL_ERR("mouse: Failed to load mouse cursor \'%s\'\n", filename);
        celestial_fatal();
    }


    if (gfx_loadSprite(*sp, f)) {
        CELESTIAL_ERR("mouse: Failed to load mouse sprite (using \"%s\")\n", filename);
        celestial_fatal();
    }

    CELESTIAL_DEBUG("mouse: Loaded \"%s\"\n", filename);   
}

/**
 * @brief Initialize the mouse system
 * @returns 0 on success
 */
int mouse_init() {
    WM_MOUSE = open("/device/mouse", O_RDONLY);
    if (WM_MOUSE < 0) {
        CELESTIAL_PERROR("open");
        celestial_fatal();
    }

    mouse_load(&mouse_default, "/usr/share/cursors/default.bmp");
    mouse_load(&mouse_text, "/usr/share/cursors/text.bmp");
    __celestial_mouse_sprite = mouse_default;

    // Default to center of screen
    WM_MOUSEX = GFX_WIDTH(WM_GFX) / 2;
    WM_MOUSEY = GFX_HEIGHT(WM_GFX) / 2;

    gfx_createClip(WM_GFX, WM_MOUSEX, WM_MOUSEY, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
    gfx_renderSprite(WM_GFX, WM_MOUSE_SPRITE, WM_MOUSEX, WM_MOUSEY);

    return 0;
}

/**
 * @brief Send any mouse events
 */
void mouse_events() {

    if (WM_MOUSE_RELATIVE) {
        if (WM_MOUSEX != last_mouse_x || WM_MOUSEY != last_mouse_y) {
            // Is this just a motion event or a drag event?
            // TODO: Maybe its both?
                celestial_event_mouse_motion_rel_t motion = {
                    .magic = CELESTIAL_MAGIC_EVENT,
                    .type = CELESTIAL_EVENT_MOUSE_MOTION_REL,
                    .size = sizeof(celestial_event_mouse_motion_rel_t),
                    .wid = WM_MOUSE_WINDOW->id,
                    .x = mouse_rel_x,
                    .y = mouse_rel_y,
                    .buttons = WM_MOUSE_BUTTONS,
                };

                event_send(WM_MOUSE_WINDOW, &motion);
            
        }
        return;
    }

    // Check to make sure still in window
    if (WM_MOUSE_WINDOW) {
        // Are we dragging the mouse window?
        if (WM_MOUSE_WINDOW->state == WINDOW_STATE_DRAGGING) {
            gfx_rect_t old = { .x = WM_MOUSE_WINDOW->x, .y = WM_MOUSE_WINDOW->y, .width = WM_MOUSE_WINDOW->width, .height = WM_MOUSE_WINDOW->height };
        
            // Yes, calculate bounds and update position
            int32_t new_x = WM_MOUSEX + mouse_window_off_x;
            int32_t new_y = WM_MOUSEY + mouse_window_off_y;
        
            // Make sure window is within boundaries
            if (new_x < 0) new_x = 0;
            if (new_x + WM_MOUSE_WINDOW->width >= GFX_WIDTH(WM_GFX)) new_x = GFX_WIDTH(WM_GFX) - WM_MOUSE_WINDOW->width - 1;
            if (new_y < 0) new_y = 0;
            if (new_y + WM_MOUSE_WINDOW->height >= GFX_HEIGHT(WM_GFX)) new_y = GFX_HEIGHT(WM_GFX) - WM_MOUSE_WINDOW->height - 1;
            WM_MOUSE_WINDOW->x = new_x;
            WM_MOUSE_WINDOW->y = new_y;

            // Remember to hold the mouse cursor in place if we're hitting the edge.
            WM_MOUSEX = WM_MOUSE_WINDOW->x - mouse_window_off_x;
            WM_MOUSEY = WM_MOUSE_WINDOW->y - mouse_window_off_y;
            
            // Do we collide with our rectangles?
            gfx_rect_t collide = { .x = WM_MOUSE_WINDOW->x, .y = WM_MOUSE_WINDOW->y, .width = WM_MOUSE_WINDOW->width, .height = WM_MOUSE_WINDOW->height };
            
            // TODO: We need to undisable this and add the ability to specify windows with alpha and windows without
            if (GFX_RECT_COLLIDES(WM_GFX, old, collide) &&  0) {
                // Yes, both rectangles collide.
                // Let's get the rectangles at which we need to redraw them
                if (GFX_RECT_TOP(WM_GFX, old) < GFX_RECT_TOP(WM_GFX, collide)) {
                    // We dragged downward
                    gfx_rect_t top = { .x = old.x, .y = old.y, .width = old.width, .height = collide.y - old.y };
                    
                #ifdef GFX_DEBUG_DRAGS
                    gfx_createClip(WM_GFX, top.x, top.y, top.width, top.height);
                    gfx_drawRectangleFilled(WM_GFX, &top, GFX_RGB(0,255,0));
                #endif

                    window_updateRegionIgnoring(top, WM_MOUSE_WINDOW);
                }

                if (GFX_RECT_BOTTOM(WM_GFX, old) > GFX_RECT_BOTTOM(WM_GFX, collide)) {
                    // We dragged up
                    gfx_rect_t bottom = { .x = old.x, .y = collide.y + collide.height, .width = old.width, .height = GFX_RECT_BOTTOM(WM_GFX, old) - GFX_RECT_BOTTOM(WM_GFX, collide) };

                #ifdef GFX_DEBUG_DRAGS
                    gfx_createClip(WM_GFX, bottom.x, bottom.y, bottom.width, bottom.height);
                    gfx_drawRectangleFilled(WM_GFX, &bottom, GFX_RGB(255,0,0));
                #endif

                    window_updateRegionIgnoring(bottom, WM_MOUSE_WINDOW);
                }

                if (GFX_RECT_LEFT(WM_GFX, old) < GFX_RECT_LEFT(WM_GFX, collide)) {
                    // We dragged to the right
                    gfx_rect_t left  = { .x = old.x, .y = old.y, .width = collide.x - old.x, .height = old.height };
                    
                #ifdef GFX_DEBUG_DRAGS
                    gfx_createClip(WM_GFX, left.x, left.y, left.width, left.height);
                    gfx_drawRectangleFilled(WM_GFX, &left, GFX_RGB(255,0xa5,0));
                #endif

                    window_updateRegionIgnoring(left, WM_MOUSE_WINDOW);
                }

                if (GFX_RECT_RIGHT(WM_GFX, old) > GFX_RECT_RIGHT(WM_GFX, collide)) {
                    // We dragged to the left
                    gfx_rect_t right = { .x = GFX_RECT_RIGHT(WM_GFX, collide), .y = old.y, .width = GFX_RECT_RIGHT(WM_GFX, old) - GFX_RECT_RIGHT(WM_GFX, collide), .height = old.height };
                    
                #ifdef GFX_DEBUG_DRAGS
                    gfx_createClip(WM_GFX, right.x, right.y, right.width, right.height);
                    gfx_drawRectangleFilled(WM_GFX, &right, GFX_RGB(0,0,255));
                #endif

                    window_updateRegionIgnoring(right, WM_MOUSE_WINDOW);
                }
            } else {
                // Nah, update the whole region
                window_updateRegion(old);
            }

            gfx_rect_t upd = { .x = 0, .y = 0, .width = WM_MOUSE_WINDOW->width, .height = WM_MOUSE_WINDOW->height };
            window_update(WM_MOUSE_WINDOW, upd);

            if (!((WM_MOUSE_BUTTONS & __celestial_previous_buttons) < __celestial_previous_buttons) && !(WM_MOUSE_BUTTONS == __celestial_previous_buttons)) {
                return;
            }
        }

        if (!(WM_MOUSE_WINDOW->x <= WM_MOUSEX) || !((int)(WM_MOUSE_WINDOW->x + WM_MOUSE_WINDOW->width) > WM_MOUSEX) ||
                !(WM_MOUSE_WINDOW->y <= WM_MOUSEY) || !((int)(WM_MOUSE_WINDOW->y + WM_MOUSE_WINDOW->height) > WM_MOUSEY)) {
           
            // Send exit event
            celestial_event_mouse_exit_t exit = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_EXIT,
                .size = sizeof(celestial_event_mouse_exit_t),
                .wid = WM_MOUSE_WINDOW->id
            };

            event_send(WM_MOUSE_WINDOW, &exit);
            WM_MOUSE_WINDOW = NULL;
        }

        // Check if we exited
        wm_window_t *win = window_top(WM_MOUSEX, WM_MOUSEY);
        if (WM_MOUSE_WINDOW != win) {
            // Send exit event
            celestial_event_mouse_exit_t exit = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_EXIT,
                .size = sizeof(celestial_event_mouse_exit_t),
                .wid = WM_MOUSE_WINDOW->id
            };

            event_send(WM_MOUSE_WINDOW, &exit);
            window_updateRegion(GFX_RECT(WM_MOUSE_WINDOW->x, WM_MOUSE_WINDOW->y, WM_MOUSE_WINDOW->width, WM_MOUSE_WINDOW->height));
            WM_MOUSE_WINDOW = win;

            // Send new event
            celestial_event_mouse_enter_t enter = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_ENTER,
                .size = sizeof(celestial_event_mouse_enter_t),
                .wid = WM_MOUSE_WINDOW->id,
                .x = WM_MOUSE_REL_WINDOW_X,
                .y = WM_MOUSE_REL_WINDOW_Y,
            };

            event_send(WM_MOUSE_WINDOW, &enter);
        }
    }

    // Send corresponding event
    if (!WM_MOUSE_WINDOW) {
        // Try to get a new window
        WM_MOUSE_WINDOW = window_top(WM_MOUSEX, WM_MOUSEY);
        if (WM_MOUSE_WINDOW) {
            // Send new event
            celestial_event_mouse_enter_t enter = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_ENTER,
                .size = sizeof(celestial_event_mouse_enter_t),
                .wid = WM_MOUSE_WINDOW->id,
                .x = WM_MOUSE_REL_WINDOW_X,
                .y = WM_MOUSE_REL_WINDOW_Y,
            };

            event_send(WM_MOUSE_WINDOW, &enter);
        }
    } else {
        // Check if we got a press
        if (WM_MOUSE_BUTTONS != __celestial_previous_buttons) {
            if ((WM_MOUSE_BUTTONS & __celestial_previous_buttons) < __celestial_previous_buttons) {
                CELESTIAL_DEBUG("mouse: Button released\n");

                // Determine released button
                uint32_t btn_released = __celestial_previous_buttons & ~(WM_MOUSE_BUTTONS);

                if (btn_released == 0x3 || btn_released > CELESTIAL_MOUSE_BUTTON_MIDDLE) {
                    // Debug check to make sure we can't hit two at once
                    CELESTIAL_ERR("mouse: Released two buttons at the same time (0x%x)! Forgetting about event.\n", btn_released);
                } else {
                    // Send!
                    celestial_event_mouse_button_up_t up = {
                        .magic = CELESTIAL_MAGIC_EVENT,
                        .type = CELESTIAL_EVENT_MOUSE_BUTTON_UP,
                        .size = sizeof(celestial_event_mouse_button_up_t),
                        .wid = WM_MOUSE_WINDOW->id,
                        .x = WM_MOUSE_REL_WINDOW_X,
                        .y = WM_MOUSE_REL_WINDOW_Y,
                        .released = btn_released
                    };

                    event_send(WM_MOUSE_WINDOW, &up);
                }
            } else {
                CELESTIAL_DEBUG("mouse: Button pressed\n");

                // Focus window if it is not focused already
                if (WM_FOCUSED_WINDOW != WM_MOUSE_WINDOW && WM_MOUSE_WINDOW->z_array == CELESTIAL_Z_DEFAULT) {
                    // Notify that we have unfocused the previous window
                    if (WM_FOCUSED_WINDOW) {
                        celestial_event_unfocused_t unfocus = {
                            .magic = CELESTIAL_MAGIC_EVENT,
                            .size = sizeof(celestial_event_unfocused_t),
                            .type = CELESTIAL_EVENT_UNFOCUSED,
                            .wid = WM_FOCUSED_WINDOW->id,
                        };

                        event_send(WM_FOCUSED_WINDOW, &unfocus);
                    }
                    // Reorder
                    WM_FOCUSED_WINDOW = WM_MOUSE_WINDOW;

                    // TODO: Store node
                    list_delete(WM_WINDOW_LIST, list_find(WM_WINDOW_LIST, WM_MOUSE_WINDOW));
                    list_append(WM_WINDOW_LIST, WM_MOUSE_WINDOW);

                    celestial_event_focused_t focus = {
                        .magic = CELESTIAL_MAGIC_EVENT,
                        .size = sizeof(celestial_event_focused_t),
                        .type = CELESTIAL_EVENT_FOCUSED,
                        .wid = WM_FOCUSED_WINDOW->id,
                    };

                    event_send(WM_FOCUSED_WINDOW, &focus);
                }

                // Determine pressed button
                uint32_t btn_pressed = WM_MOUSE_BUTTONS & ~(__celestial_previous_buttons);
                if (btn_pressed == 0x3 || btn_pressed > CELESTIAL_MOUSE_BUTTON_MIDDLE) {
                    // Debug check to make sure we can't hit two at once
                    CELESTIAL_ERR("mouse: Pressed two buttons at the same time (0x%x)! Forgetting about event.\n", btn_pressed);
                } else {

                    if (btn_pressed == CELESTIAL_MOUSE_BUTTON_LEFT) {
                        mouse_window_off_x = WM_MOUSE_WINDOW->x - WM_MOUSEX;
                        mouse_window_off_y = WM_MOUSE_WINDOW->y - WM_MOUSEY;
                    }

                    // Send!
                    celestial_event_mouse_button_down_t down = {
                        .magic = CELESTIAL_MAGIC_EVENT,
                        .type = CELESTIAL_EVENT_MOUSE_BUTTON_DOWN,
                        .size = sizeof(celestial_event_mouse_button_down_t),
                        .wid = WM_MOUSE_WINDOW->id,
                        .x = WM_MOUSE_REL_WINDOW_X,
                        .y = WM_MOUSE_REL_WINDOW_Y,
                        .held = btn_pressed,
                    };

                    event_send(WM_MOUSE_WINDOW, &down);
                }
            }
        }

        if (WM_MOUSE_WINDOW->state == WINDOW_STATE_DRAGGING && !(WM_MOUSE_BUTTONS & CELESTIAL_MOUSE_BUTTON_LEFT)) {
            // Release
            // !!!: Maybe make usermode apps do this?
            WM_MOUSE_WINDOW->state = WINDOW_STATE_NORMAL;
        } else if (WM_MOUSE_WINDOW->state == WINDOW_STATE_DRAGGING) {
            return; // Only stop dragging when button released
        }

        // Send motion event if there was any
        if (WM_MOUSEX != last_mouse_x || WM_MOUSEY != last_mouse_y) {
            // Is this just a motion event or a drag event?
            // TODO: Maybe its both?

            if (WM_MOUSE_BUTTONS & CELESTIAL_MOUSE_BUTTON_LEFT) {
                celestial_event_mouse_drag_t drag = {
                    .magic = CELESTIAL_MAGIC_EVENT,
                    .type = CELESTIAL_EVENT_MOUSE_DRAG,
                    .size = sizeof(celestial_event_mouse_drag_t),
                    .wid = WM_MOUSE_WINDOW->id,
                    .x = WM_MOUSE_REL_WINDOW_X,
                    .y = WM_MOUSE_REL_WINDOW_Y,
                    .win_x = WM_MOUSE_WINDOW->x,
                    .win_y = WM_MOUSE_WINDOW->y,
                };

                event_send(WM_MOUSE_WINDOW, &drag);
            } else {
                celestial_event_mouse_motion_t motion = {
                    .magic = CELESTIAL_MAGIC_EVENT,
                    .type = CELESTIAL_EVENT_MOUSE_MOTION,
                    .size = sizeof(celestial_event_mouse_motion_t),
                    .wid = WM_MOUSE_WINDOW->id,
                    .x = WM_MOUSE_REL_WINDOW_X,
                    .y = WM_MOUSE_REL_WINDOW_Y,
                    .buttons = WM_MOUSE_BUTTONS,
                };

                event_send(WM_MOUSE_WINDOW, &motion);
            }
        }
    }
}

/**
 * @brief Update and redraw the mouse (non-blocking)
 */
int mouse_update() {
    // Read mouse event
    mouse_event_t event;

    ssize_t r = read(WM_MOUSE, &event, sizeof(mouse_event_t));
    if (r < 0) {
        CELESTIAL_PERROR("read");
        CELESTIAL_ERR("mouse: Error while getting event\n");
        celestial_fatal();
    }

    if (!r) return 0;

    // Parse the mouse event
    if (event.event_type != EVENT_MOUSE_UPDATE) return 0;

    last_mouse_x = WM_MOUSEX;
    last_mouse_y = WM_MOUSEY;
    mouse_rel_x = event.x_difference;
    mouse_rel_y = event.y_difference;

    // Update X and Y
    WM_MOUSEX += event.x_difference * 3;
    WM_MOUSEY -= event.y_difference * 3; // TODO: Maybe add kernel flag to invert this or do it in driver

    // Update buttons
    if (WM_MOUSE_BUTTONS != event.buttons) {
        // Translate event.buttons
        uint32_t btns = (event.buttons & MOUSE_BUTTON_LEFT ? CELESTIAL_MOUSE_BUTTON_LEFT : 0) |
                            (event.buttons & MOUSE_BUTTON_RIGHT ? CELESTIAL_MOUSE_BUTTON_RIGHT : 0) |
                            (event.buttons & MOUSE_BUTTON_MIDDLE ? CELESTIAL_MOUSE_BUTTON_MIDDLE : 0);
        
        __celestial_previous_buttons = WM_MOUSE_BUTTONS;
        WM_MOUSE_BUTTONS = btns;
    } else {
        __celestial_previous_buttons = WM_MOUSE_BUTTONS; // To stop spamming mouse press events
    }

    // Send event for scroll
    if (WM_MOUSE_WINDOW && event.scroll != MOUSE_SCROLL_NONE) {
        celestial_event_mouse_scroll_t scroll = {
            .magic = CELESTIAL_MAGIC_EVENT,
            .type = CELESTIAL_EVENT_MOUSE_SCROLL,
            .size = sizeof(celestial_event_mouse_scroll_t),
            .wid = WM_MOUSE_WINDOW->id,
            .direction = event.scroll == MOUSE_SCROLL_UP ? CELESTIAL_MOUSE_SCROLL_UP : CELESTIAL_MOUSE_SCROLL_DOWN
        };

        event_send(WM_MOUSE_WINDOW, &scroll);
    }

    // Do we need to adjust?
    if (WM_MOUSEX < 0) WM_MOUSEX = 0;
    if (WM_MOUSEY < 0) WM_MOUSEY = 0;
    if ((size_t)WM_MOUSEX >= GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width-1) WM_MOUSEX = GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width;
    if ((size_t)WM_MOUSEY >= GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height) WM_MOUSEY = GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height;

    if (abs(WM_MOUSEX - last_mouse_x) >= 100 || abs(WM_MOUSEY - last_mouse_y) >= 100) {
        CELESTIAL_LOG("WARNING: Suspicious mouse event (dx=%d dy=%d)\n", WM_MOUSEX-last_mouse_x, WM_MOUSEY-last_mouse_y);
    }

    // Did things change?
    if (last_mouse_x != WM_MOUSEX || last_mouse_y != WM_MOUSEY || WM_MOUSE_BUTTONS != __celestial_previous_buttons) {
        mouse_events();
        window_updateRegion((gfx_rect_t){ .x = last_mouse_x, .y = last_mouse_y, .width = WM_MOUSE_SPRITE->width-1, .height = WM_MOUSE_SPRITE->height-1 });
    }

    // Make clips
    gfx_createClip(WM_GFX, WM_MOUSEX, WM_MOUSEY, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
    return 1;
}

/**
 * @brief Render the mouse
 */
void mouse_render() {
    if (!WM_MOUSE_RELATIVE) {
        if (WM_GFX->clip) gfx_renderSprite(WM_GFX, WM_MOUSE_SPRITE, WM_MOUSEX, WM_MOUSEY);
    }
}


/**
 * @brief Change mouse sprite
 * @param target The target sprite to change to
 */
void mouse_change(int target) {
    switch (target) {
        case CELESTIAL_MOUSE_TEXT:
            WM_MOUSE_SPRITE = mouse_text;
            break;
        case CELESTIAL_MOUSE_DEFAULT:
        default:
            WM_MOUSE_SPRITE = mouse_default;
            break;
    }
}