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

#include <kernel/arch/arch.h>

#include "ps2.h"
#include <kernel/loader/driver.h>
#include <kernel/fs/periphfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>
#include <stdio.h>
#include <string.h>

uint8_t keyboard_port = 0;

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

	int event_type = (ch >= 0x80) ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS;

    // Determine the scancode we should print
    key_scancode_t sc = ch;
	periphfs_sendKeyboardEvent(event_type, sc);
    return 0;
}


/**
 * @brief Initialize the PS/2 keyboard
 */
void kbd_init(uint8_t port) {
    keyboard_port = port;

    // Setup the keyboard to do translation with scancode mode 2
    ps2_sendDeviceACK(keyboard_port, PS2_KEYBOARD_SET_SCANCODE);
    ps2_sendDeviceACK(keyboard_port, 2);

    // Register IRQ
    hal_registerInterruptHandler(PS2_KEYBOARD_IRQ, ps2_keyboardIRQ, NULL);
}
