/**
 * @file userspace/celestialv2/input_generic.c
 * @brief Generic input library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <ethereal/celestial.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

sprite_t *mouse_sprites[2];
unsigned char mouse_type = CELESTIAL_MOUSE_DEFAULT;
static int input_ready = 0;

int mw_off_x = 0;
int mw_off_y = 0;

#define TO_REL_X(_x) ((_x) - (SERVER->mouse_window->x))
#define TO_REL_Y(_y) ((_y) - (SERVER->mouse_window->y))
#define BUTTON_PRESSED(btn) ((new_btns & (btn)) && ((SERVER->last_buttons & btn) == 0))
#define BUTTON_RELEASED(btn) ((new_btns & (btn)) == 0 && ((SERVER->last_buttons & btn)))
#define BUTTON_HELD(btn) ((new_btns & (btn)) && (SERVER->last_buttons & (btn)))
#define NEWLY_PRESSED (new_btns & ~(SERVER->last_buttons))
#define NEWLY_RELEASED (SERVER->last_buttons & ~(new_btns))

#if 0
#define CONVERT_MOUSE_BUTTONS(btns) mouse_toCelestialButtons(btns)
inline uint32_t mouse_toCelestialButtons(uint32_t mbuttons) {
    return ((mbuttons & MOUSE_BUTTON_LEFT) ? CELESTIAL_MOUSE_BUTTON_LEFT : 0) |
            ((mbuttons & MOUSE_BUTTON_RIGHT) ? CELESTIAL_MOUSE_BUTTON_RIGHT : 0) |
            ((mbuttons & MOUSE_BUTTON_MIDDLE) ? CELESTIAL_MOUSE_BUTTON_MIDDLE : 0); 
} 
#else
// the bitmasks are the same between kernel and Celestial
#define CONVERT_MOUSE_BUTTONS(btns) btns
#endif

static inline gfx_rect_t input_cursorRect(int x, int y) {
    sprite_t *sp = mouse_sprites[mouse_type];
    return GFX_RECT(x, y, sp->width, sp->height);
}

static inline bool input_rectsIntersect(gfx_rect_t a, gfx_rect_t b) {
    return !((int)(a.x + a.width) <= (int)b.x ||
             (int)(a.y + a.height) <= (int)b.y ||
             (int)a.x >= (int)(b.x + b.width) ||
             (int)a.y >= (int)(b.y + b.height));
}

void mouse_check_events(int new_x, int new_y, int scroll, uint32_t new_btns) {
    // Clamp
    if (new_x < 0) new_x = 0;
    else if (new_x >= (int)renderer_getWidth()) new_x = renderer_getWidth() - 1;
    if (new_y < 0) new_y = 0;
    else if (new_y >= (int)renderer_getHeight()) new_y = renderer_getHeight() - 1;

    if (SERVER->mouse_window && SERVER->mouse_window->state == WINDOW_STATE_DRAGGING) {
        // drag the window
        wm_window_t *mw = SERVER->mouse_window;

        int old_mouse_x = SERVER->mouse_x;
        int old_mouse_y = SERVER->mouse_y;

        pthread_mutex_lock(&SERVER->window_lock);
        int old_x = mw->x;
        int old_y = mw->y;

        mw->x = new_x - mw_off_x;
        mw->y = new_y - mw_off_y;

        if (mw->x < 0) {
            mw->x = 0;
        } else if (mw->x + mw->width > (int)renderer_getWidth()) {
            mw->x = renderer_getWidth() - mw->width;
        }

        if (mw->y < 0) {
            mw->y = 0;
        } else if (mw->y + mw->height > (int)renderer_getHeight()) {
            mw->y = renderer_getHeight() - mw->height;
        }

        SERVER->mouse_x = mw->x + mw_off_x;
        SERVER->mouse_y = mw->y + mw_off_y;
        pthread_mutex_unlock(&SERVER->window_lock);

        damage_lock();
        damage_add(GFX_RECT(old_mouse_x, old_mouse_y, mouse_sprites[mouse_type]->width + 1, mouse_sprites[mouse_type]->height + 1));
        damage_add(GFX_RECT(SERVER->mouse_x, SERVER->mouse_y, mouse_sprites[mouse_type]->width + 1, mouse_sprites[mouse_type]->height + 1));
        damage_move(mw, old_x, old_y);
        damage_unlock();

        if (!BUTTON_HELD(MOUSE_BUTTON_LEFT)) {
            WINDOW_CHANGE_STATE(mw, WINDOW_STATE_NORMAL);
        } else {
            return;
        }
    }

    wm_window_t *top = window_top(new_x, new_y);

    // Check if we are still in the mouse window
    if (SERVER->mouse_window && (SERVER->mouse_window != top)) {
        // Send the exit event
        EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_exit_t, CELESTIAL_EVENT_MOUSE_EXIT);
        SERVER->mouse_window = top;

        if (SERVER->mouse_window) {
            // Now send an entry event
            EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_enter_t, CELESTIAL_EVENT_MOUSE_ENTER, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y));
        }
    }


    if (!SERVER->mouse_window) {
        // Try and set one
        SERVER->mouse_window = top;
        if (!SERVER->mouse_window) {

            int old_x = SERVER->mouse_x;
            int old_y = SERVER->mouse_y;
            SERVER->last_buttons = new_btns;
            SERVER->mouse_x = new_x;
            SERVER->mouse_y = new_y;
            damage_lock();
            damage_add(input_cursorRect(old_x, old_y));
            damage_add(input_cursorRect(SERVER->mouse_x, SERVER->mouse_y));
            damage_unlock();
            return;
        }

        EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_enter_t, CELESTIAL_EVENT_MOUSE_ENTER, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y));
    }


    if (NEWLY_PRESSED) {
        if (BUTTON_PRESSED(MOUSE_BUTTON_LEFT)) {
            mw_off_x = new_x - SERVER->mouse_window->x;
            mw_off_y = new_y - SERVER->mouse_window->y;
        }

        if (BUTTON_PRESSED(MOUSE_BUTTON_LEFT) && SERVER->mouse_window != SERVER->focused) {
            window_focus(SERVER->mouse_window);
        }

        EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_button_down_t, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y), .held = CONVERT_MOUSE_BUTTONS(NEWLY_PRESSED));
    } else if (NEWLY_RELEASED) {
        // use old X and Y coordinates
        TRACE_DEBUG("Sending button up event.\n");
        EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_button_up_t, CELESTIAL_EVENT_MOUSE_BUTTON_UP, .x = TO_REL_X(SERVER->mouse_x), .y = TO_REL_Y(SERVER->mouse_y), .released = CONVERT_MOUSE_BUTTONS(NEWLY_RELEASED));
    } else if ((SERVER->mouse_x != new_x || SERVER->mouse_y != new_y)  && SERVER->mouse_window->state != WINDOW_STATE_DRAGGING) {
        // otherwise, as long as the window is not dragging, send it mouse events
        if (BUTTON_HELD(MOUSE_BUTTON_LEFT)) {
            // dragging the window
            EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_drag_t, CELESTIAL_EVENT_MOUSE_DRAG, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y), .win_x = SERVER->mouse_window->x, .win_y = SERVER->mouse_window->y);
        } else {
            EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_motion_t, CELESTIAL_EVENT_MOUSE_MOTION, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y), .buttons = CONVERT_MOUSE_BUTTONS(new_btns));
        }
    } else if (scroll != 0) {
        // send a scroll event
        EVENT_SEND(SERVER->mouse_window, celestial_event_mouse_scroll_t, CELESTIAL_EVENT_MOUSE_SCROLL, .x = TO_REL_X(new_x), .y = TO_REL_Y(new_y), .direction = (scroll == 1) ? CELESTIAL_MOUSE_SCROLL_UP : CELESTIAL_MOUSE_SCROLL_DOWN);
    }

    // Update
    int old_x = SERVER->mouse_x;
    int old_y = SERVER->mouse_y;
    SERVER->last_buttons = new_btns;
    SERVER->mouse_x = new_x;
    SERVER->mouse_y = new_y;
    damage_lock();
    damage_add(input_cursorRect(old_x, old_y));
    damage_add(input_cursorRect(SERVER->mouse_x, SERVER->mouse_y));
    damage_unlock();
}

void keyboard_process_event(key_event_t event) {
    if (SERVER->focused) {
        EVENT_SEND(SERVER->focused, celestial_event_key_t, CELESTIAL_EVENT_KEY_EVENT, .ev = event);
    }
}

void input_loadMouseSprite(int sprite, char *filename) {
    sprite_t *sp = gfx_createSprite(0,0);
    FILE *f = fopen(filename, "r");
    if (!f) {
        FATAL("Error loading mouse cursor \'%s\'\n", filename);
    }

    if (gfx_loadSprite(sp, f)) {
        FATAL("Error loading mouse cursor sprite \'%s\'", filename);
    }

    fclose(f);
    mouse_sprites[sprite] = sp;
}

void input_draw() {
    if (!input_ready) return;
    gfx_renderSprite(RENDERER->ctx, mouse_sprites[mouse_type], SERVER->mouse_x, SERVER->mouse_y);
}

void input_draw_at(int x, int y) {
    if (!input_ready) return;
    gfx_renderSprite(RENDERER->ctx, mouse_sprites[mouse_type], x, y);
}

bool input_frameCursor(render_request_t *frame, int *x, int *y) {
    if (!input_ready) return false;

    *x = SERVER->mouse_x;
    *y = SERVER->mouse_y;
    return true;
}

void input_get_mouse_pos(int *x, int *y) {
    *x = SERVER->mouse_x;
    *y = SERVER->mouse_y;
}

void input_set_mouse(int mouse) {
    if (!input_ready) return;
    if (mouse > CELESTIAL_MOUSE_TEXT || mouse_sprites[mouse] == NULL) {
        TRACE_ERROR("input_set_mouse unknown mouse type %d\n", mouse);
        return;
    }

    damage_lock();
    damage_add(input_cursorRect(SERVER->mouse_x, SERVER->mouse_y));
    mouse_type = mouse;
    damage_add(input_cursorRect(SERVER->mouse_x, SERVER->mouse_y));
    damage_unlock();
}

int input_init() {
    input_loadMouseSprite(CELESTIAL_MOUSE_DEFAULT, "/usr/share/cursors/default.bmp");
    input_loadMouseSprite(CELESTIAL_MOUSE_TEXT, "/usr/share/cursors/text.bmp");

    SERVER->mouse_window = NULL;
    SERVER->mouse_x = (renderer_getWidth() - mouse_sprites[0]->width) / 2; 
    SERVER->mouse_y = (renderer_getHeight() - mouse_sprites[0]->height) / 2; 

    input_init_backend();

    input_ready = 1;
    return 0;
}
