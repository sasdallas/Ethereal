/**
 * @file drivers/misc/ps2/kbd.c
 * @brief PS/2 keyboard handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include "ps2.h"
#include <kernel/loader/driver.h>
#include <kernel/fs/periphfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>
#include <stdio.h>
#include <string.h>

/* Holding shift key */
static int held_shift_key = 0;

/* Scancode (lower) */
static key_scancode_t ps2_keyboard_scancodes_lower[128] = {
	0, 27,
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', SCANCODE_LEFT_CTRL,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', SCANCODE_LEFT_SHIFT,
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SCANCODE_RIGHT_SHIFT,
	'*', 0, ' ', 0,
	SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5, SCANCODE_F6,
	SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
	0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0,
	SCANCODE_DEL,
	0, 0, 0,
	SCANCODE_F11, SCANCODE_F12,
	0, /* everything else */
};

/* Scancode (upper) */
static key_scancode_t ps2_keyboard_scancodes_upper[128] = {
	0, 27,
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	'_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', SCANCODE_LEFT_SHIFT,
	'|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', SCANCODE_RIGHT_SHIFT,
	'*', 0, ' ', 0,
	SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5, SCANCODE_F6,
	SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
	0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0,
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};


/**
 * @brief Send something to the keyboard PS/2 port (PORT1)
 * @param data What to send
 * @returns Usually an ACK value (0xFA)
 */
uint8_t ps2_writeKeyboard(uint8_t data) {
    ps2_waitForInputClear();
    outportb(PS2_DATA, data);
    ps2_waitForOutput();
    return inportb(PS2_DATA);
}



/**
 * @brief PS/2 keyboard IRQ
 */
int ps2_keyboardIRQ(void *context) {
	// Get character from PS/2
	uint8_t ch = inportb(PS2_DATA); 
	hal_endInterrupt(PS2_KEYBOARD_IRQ); // End IRQ so more can come in

	// TODO: TEMPORARY
    if (ch == 0x2A || ch == 0x36) {
        held_shift_key = 1;
        return 0;
    } else if (ch == 0xAA || ch == 0xB6) {
        held_shift_key = 0;
        return 0;
    }

	int event_type = (ch >= 0x80) ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS;
	if (ch >= 0x80) ch -= 0x80;

    // Determine the scancode we should print
    char ascii = 0;
    if (held_shift_key) {
        ascii = ps2_keyboard_scancodes_upper[ch];
    } else {
        ascii = ps2_keyboard_scancodes_lower[ch];
    }

	periphfs_sendKeyboardEvent(event_type, ascii);
    return 0;
}


/**
 * @brief Initialize the PS/2 keyboard
 */
void kbd_init() {
    // Setup the keyboard to do translation with scancode mode 2
    ps2_writeKeyboard(PS2_KEYBOARD_SET_SCANCODE);
    ps2_writeKeyboard(2);

    // Register IRQ
    hal_registerInterruptHandlerContext(PS2_KEYBOARD_IRQ, ps2_keyboardIRQ, NULL);
}
