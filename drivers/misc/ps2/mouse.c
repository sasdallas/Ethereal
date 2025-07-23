/**
 * @file drivers/misc/ps2/mouse.c
 * @brief Mouse portion of PS/2 driver
 * 
 * @todo Support for scrollwheel 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
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
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>
#include <stdio.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:PS2", __VA_ARGS__)

/* Currently in progress mouse packet */
uint8_t ps2_mouse_packet[4];
int ps2_mouse_packet_cycle = 0;

/* Last pressed buttons */
uint32_t ps2_last_buttons = 0;

/**
 * @brief Send something to the mouse port
 * @param data The data to write to the mouse port
 * @returns Whatever data was read from the mouse port. Usually an ACK
 */
uint8_t ps2_writeMouse(uint8_t data) {
    ps2_sendCommandParameter(PS2_COMMAND_WRITE_PORT2, data);
    ps2_waitForOutput();
    return inportb(PS2_DATA);
}

/**
 * @brief Mouse IRQ handler
 */
int mouse_irq(void *context) {
    // Get the next byte in the cycle
    ps2_mouse_packet[ps2_mouse_packet_cycle] = inportb(PS2_DATA);

    // End IRQ
    hal_endInterrupt(PS2_MOUSE_IRQ);
    

    if (!ps2_mouse_packet_cycle && !(ps2_mouse_packet[ps2_mouse_packet_cycle] & 0x08)) {
        return 0;
    }
    ps2_mouse_packet_cycle++;
    

    // TODO: Support for more advanced PS/2 mouse modes?
    if (ps2_mouse_packet_cycle < 3) {
        return 0;
    }

    // Reset packet cycle
    ps2_mouse_packet_cycle = 0;

    // Start building data
    int x_diff = ps2_mouse_packet[1];
    int y_diff = ps2_mouse_packet[2];

    // Check for signatures
    if (x_diff && ps2_mouse_packet[0] & (1 << 4)) x_diff = x_diff - 0x100;
    if (y_diff && ps2_mouse_packet[0] & (1 << 5)) y_diff = y_diff - 0x100;

    // Check for overflow
    if (ps2_mouse_packet[0] & PS2_MOUSE_DATA_X_OVERFLOW) x_diff = 0;
    if (ps2_mouse_packet[0] & PS2_MOUSE_DATA_Y_OVERFLOW) y_diff = 0;

    // Get buttons
    uint32_t buttons =  (ps2_mouse_packet[0] & PS2_MOUSE_DATA_LEFTBTN ? MOUSE_BUTTON_LEFT : 0) | 
                        (ps2_mouse_packet[0] & PS2_MOUSE_DATA_RIGHTBTN ? MOUSE_BUTTON_RIGHT : 0) | 
                        (ps2_mouse_packet[0] & PS2_MOUSE_DATA_MIDDLEBTN ? MOUSE_BUTTON_MIDDLE : 0);

    if (buttons == ps2_last_buttons && !x_diff && !y_diff) return 0;
    periphfs_sendMouseEvent(EVENT_MOUSE_UPDATE, buttons, x_diff, y_diff);
    ps2_last_buttons = buttons;
    return 0;
}

/**
 * @brief Initialize the PS/2 mouse
 */
void mouse_init() {
    // First, enable mouse defaults.
    ps2_writeMouse(PS2_MOUSE_SET_DEFAUTS);
    
    // Now enable data reporting
    ps2_writeMouse(PS2_MOUSE_ENABLE_DATA_REPORTING);
    
    // Register IRQ
    hal_registerInterruptHandler(PS2_MOUSE_IRQ, mouse_irq, NULL);
}