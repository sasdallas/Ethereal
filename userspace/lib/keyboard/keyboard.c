/**
 * @file userspace/lib/keyboard/keyboard.c
 * @brief Keyboard library
 * 
 * This is a keyboard library, that, while good, needs work on the kernel to be better.
 * Peripheral filesystem is a garbage concept that needs to just be replaced with pipes of scancodes, nothing more.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/keyboard.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MOD(t) { if (ev->type == KEYBOARD_EVENT_PRESS) { kbd->mods |= t; } else { kbd->mods &= ~t; }; ev->mods = kbd->mods; ev->ascii = '\0'; }
#define CTRL ((kbd->mods & KEYBOARD_MOD_LEFT_CTRL) || (kbd->mods & KEYBOARD_MOD_RIGHT_CTRL))
#define SHIFT ((kbd->mods & KEYBOARD_MOD_LEFT_SHIFT) || (kbd->mods & KEYBOARD_MOD_RIGHT_SHIFT))


/* US keyboard scancodes (lower) */
static key_scancode_t kbd_us_scancodes_lower[128] = {
	0, SCANCODE_ESCAPE,
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', SCANCODE_LEFT_CTRL,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', SCANCODE_LEFT_SHIFT,
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SCANCODE_RIGHT_SHIFT,
	'*', 0, ' ', 0,
	SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5, SCANCODE_F6,
	SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
	0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0,
	SCANCODE_DEL, 0, 0, 0, SCANCODE_F11, SCANCODE_F12, 0
};

/* US keyboard scancodes (upper) */
static key_scancode_t kbd_us_scancodes_upper[128] = {
	0, SCANCODE_ESCAPE,
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	'_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', SCANCODE_LEFT_CTRL,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', SCANCODE_LEFT_SHIFT,
	'|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', SCANCODE_RIGHT_SHIFT,
	'*', 0, ' ', 0,
	SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5, SCANCODE_F6,
	SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
	0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, // TODO: Fill in these scancodes
	SCANCODE_DEL, 0, 0, 0, SCANCODE_F11, SCANCODE_F12, 0
};

/* Extended scancodes */
#define PS2_SCANCODE_PREV_TRACK         0x10
#define PS2_SCANCODE_NEXT_TRACK         0x19
#define PS2_SCANCODE_KEYPAD_ENTER       0x1C
#define PS2_SCANCODE_RIGHT_CTRL         0x1D
#define PS2_SCANCODE_RIGHT_ALT          0x38
#define PS2_SCANCODE_HOME               0x47
#define PS2_SCANCODE_UP_ARROW           0x48
#define PS2_SCANCODE_PGUP               0x49
#define PS2_SCANCODE_LEFT_ARROW         0x4B
#define PS2_SCANCODE_RIGHT_ARROW        0x4D
#define PS2_SCANCODE_END                0x4F
#define PS2_SCANCODE_DOWN_ARROW         0x50
#define PS2_SCANCODE_PGDOWN             0x51
#define PS2_SCANCODE_INSERT             0x52
#define PS2_SCANCODE_DEL                0x53
#define PS2_SCANCODE_LEFT_SUPER         0x5B
#define PS2_SCANCODE_RIGHT_SUPER        0x5C

/**
 * @brief Initialize keyboard state
 * @returns New keyboard object
 */
keyboard_t *keyboard_create() {
    keyboard_t *kbd = malloc(sizeof(keyboard_t));
    kbd->mods = 0;
    return kbd;
}

/**
 * @brief Process extended scancodes
 * @param ev The event to process in (scancode will be updated)
 */
static keyboard_event_t *keyboard_eventProcessExtended(keyboard_event_t *ev) {
    switch (ev->scancode) {
        case PS2_SCANCODE_RIGHT_CTRL:
            ev->scancode = SCANCODE_RIGHT_CTRL;
            break;
        case PS2_SCANCODE_RIGHT_ALT:
            ev->scancode = SCANCODE_RIGHT_ALT;
            break;
        case PS2_SCANCODE_HOME:
            ev->scancode = SCANCODE_HOME;
            break;
        case PS2_SCANCODE_UP_ARROW:
            ev->scancode = SCANCODE_UP_ARROW;
            break;
        case PS2_SCANCODE_PGUP:
            ev->scancode = SCANCODE_PGUP;
            break;
        case PS2_SCANCODE_LEFT_ARROW:
            ev->scancode = SCANCODE_LEFT_ARROW;
            break;
        case PS2_SCANCODE_RIGHT_ARROW:
            ev->scancode = SCANCODE_RIGHT_ARROW;
            break;
        case PS2_SCANCODE_DOWN_ARROW:
            ev->scancode = SCANCODE_DOWN_ARROW;
            break;
        case PS2_SCANCODE_PGDOWN:
            ev->scancode = SCANCODE_PGDOWN;
            break;
        case PS2_SCANCODE_DEL:
            ev->scancode = SCANCODE_DEL;
            break;
        case PS2_SCANCODE_LEFT_SUPER:
            ev->scancode = SCANCODE_LEFT_SUPER;
            break;
        case PS2_SCANCODE_RIGHT_SUPER:
            ev->scancode = SCANCODE_RIGHT_SUPER;
            break;
    }

    return ev;
}

/**
 * @brief Convert a key event from /device/keyboard to a scancode to a keyboard event
 * @param keyboard The keyboard object
 * @param event The event object
 */
keyboard_event_t *keyboard_event(keyboard_t *kbd, void *event) {
    keyboard_event_t *ev = malloc(sizeof(keyboard_event_t));
    key_event_t *ev_src = (key_event_t*)event;

    // PS/2 extended keycode?
    if (ev_src->scancode == 0xE0) {
        kbd->extension = 1;
        ev->scancode = ev_src->scancode;
        ev->type = EVENT_KEY_RELEASE;
        ev->ascii = '\0';
        ev->type = ev_src->event_type == EVENT_KEY_PRESS ? KEYBOARD_EVENT_PRESS : KEYBOARD_EVENT_RELEASE;
        ev->mods = kbd->mods;
        return ev;
    }

    // Is this part of an extension?
    if (kbd->extension) {
        kbd->extension = 0;
        
        ev->type = ev_src->event_type == EVENT_KEY_PRESS ? KEYBOARD_EVENT_PRESS : KEYBOARD_EVENT_RELEASE;
        ev->scancode = ev_src->scancode;
        if (ev->type == KEYBOARD_EVENT_RELEASE) ev->scancode -= 0x80;

        ev->ascii = '\0';
        ev->mods = kbd->mods;
        return keyboard_eventProcessExtended(ev);
    }

    // I hate periphfs....
    if (ev_src->scancode >= 0x80) ev_src->scancode -= 0x80; // Adjust for key release
    ev->scancode = SHIFT ? kbd_us_scancodes_upper[ev_src->scancode] : kbd_us_scancodes_lower[ev_src->scancode];
    ev->ascii = ev->scancode;
    ev->type = ev_src->event_type == EVENT_KEY_PRESS ? KEYBOARD_EVENT_PRESS : KEYBOARD_EVENT_RELEASE;
    ev->mods = kbd->mods;

    ev->ascii &= 0x7F;
    switch (ev->scancode) {
        case SCANCODE_ESCAPE:
            ev->ascii = '\033';
            break;
        case SCANCODE_LEFT_CTRL:
            MOD(KEYBOARD_MOD_LEFT_CTRL);
            break;
        case SCANCODE_LEFT_SHIFT:
            MOD(KEYBOARD_MOD_LEFT_SHIFT);
            break;
        case SCANCODE_LEFT_ALT:
            MOD(KEYBOARD_MOD_LEFT_ALT);
            break;
        case SCANCODE_LEFT_SUPER:
            MOD(KEYBOARD_MOD_LEFT_SUPER);
            break;
        case SCANCODE_RIGHT_CTRL:
            MOD(KEYBOARD_MOD_RIGHT_CTRL);
            break;
        case SCANCODE_RIGHT_SHIFT:
            MOD(KEYBOARD_MOD_RIGHT_SHIFT);
            break;
        case SCANCODE_RIGHT_ALT:
            MOD(KEYBOARD_MOD_RIGHT_ALT);
            break;
        case SCANCODE_RIGHT_SUPER:
            MOD(KEYBOARD_MOD_RIGHT_SUPER);
            break;

        case SCANCODE_F1:
        case SCANCODE_F2:
        case SCANCODE_F3:
        case SCANCODE_F4:
        case SCANCODE_F5:
        case SCANCODE_F6:
        case SCANCODE_F7:
        case SCANCODE_F8:
        case SCANCODE_F9:
        case SCANCODE_F10:
        case SCANCODE_F11:
        case SCANCODE_F12:
        case SCANCODE_PGUP:
        case SCANCODE_PGDOWN:
        case SCANCODE_HOME:
        case SCANCODE_DEL:
            break; // Nothing needs to be done for these

        default:
            // We should just have a normal ASCII character
            if (CTRL) {
                // Let's convert this one
                // https://github.com/klange/toaruos/blob/master/lib/kbd.c#L154
                char ch = ev->ascii;
                if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
                if (ch == '-') ch = '_';
                if (ch == '`') ch = '@';

                int o = (int)(ch - 0x40);
                if (o < 0) {
                    // ev->ascii remains unchanged
                    // TODO: Keyboard layouts here!
                } else {
                    ev->ascii = o;
                }
            } else {
                // ev->ascii remains unchanged
                // TODO: Keyboard layouts here!
            }

            return ev;
    }


    ev->ascii = 0;
    return ev;
}

/**
 * @brief Convert a raw scancode into a key event
 * @param keyboard The keyboard object
 * @param scancode The scancode 
 */
keyboard_event_t *keyboard_scancode(keyboard_t *kbd, key_scancode_t scancode) {
    return NULL; // TODO
}