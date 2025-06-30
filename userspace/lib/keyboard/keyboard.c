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
 * @brief Convert a key event from /device/keyboard to a scancode to a keyboard event
 * @param keyboard The keyboard object
 * @param event The event object
 */
keyboard_event_t *keyboard_event(keyboard_t *kbd, void *event) {
    keyboard_event_t *ev = malloc(sizeof(keyboard_event_t));
    key_event_t *ev_src = (key_event_t*)event;

    // I hate periphfs....
    ev->scancode = ev_src->scancode;
    ev->ascii = ev_src->scancode;
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