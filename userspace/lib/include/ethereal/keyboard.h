/**
 * @file userspace/lib/include/ethereal/keyboard.h
 * @brief Keyboard library for Ethereal
 * 
 * This is kinda a jumbled mess of periphfs + keyboard library
 * @todo Make PeriphFS not expose /device/stdin because it is trash
 * @todo Move scancode translation from PS/2 to here for keyboard layouts and more
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

/**** INCLUDES ****/
#include <kernel/fs/periphfs.h>

/**** DEFINITIONS ****/

#define KEYBOARD_MOD_LEFT_SHIFT         0x01
#define KEYBOARD_MOD_RIGHT_SHIFT        0x02
#define KEYBOARD_MOD_LEFT_CTRL          0x04
#define KEYBOARD_MOD_RIGHT_CTRL         0x08
#define KEYBOARD_MOD_LEFT_ALT           0x10
#define KEYBOARD_MOD_RIGHT_ALT          0x20
#define KEYBOARD_MOD_LEFT_SUPER         0x40
#define KEYBOARD_MOD_RIGHT_SUPER        0x80

#define KEYBOARD_EVENT_RELEASE          0x01
#define KEYBOARD_EVENT_PRESS            0x02

/**** TYPES ****/

typedef unsigned long key_modifiers_t;
typedef char key_event_type_t;

typedef struct keyboard_event {
    key_scancode_t scancode;            // Raw scancode from /device/keyboard
    char ascii;                         // ASCII code of the key
    key_modifiers_t mods;               // Modifiers
    key_event_type_t type;              // Event type
} keyboard_event_t;

typedef struct keyboard {
    key_modifiers_t mods;               // Modifiers
} keyboard_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize keyboard state
 * @returns New keyboard object
 */
keyboard_t *keyboard_create();

/**
 * @brief Convert a key event from /device/keyboard to a scancode to a keyboard event
 * @param keyboard The keyboard object
 * @param event The event object
 */
keyboard_event_t *keyboard_event(keyboard_t *kbd, void *event);

/**
 * @brief Convert a raw scancode into a key event
 * @param keyboard The keyboard object
 * @param scancode The scancode 
 */
keyboard_event_t *keyboard_scancode(keyboard_t *kbd, key_scancode_t scancode);

#endif