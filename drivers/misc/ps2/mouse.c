/**
 * @file drivers/misc/ps2/mouse.c
 * @brief Mouse portion of PS/2 driver
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

/* Mouse port */
uint8_t mouse_port = 0;

/* Packet size */
uint8_t mouse_id = 0;
uint8_t mouse_packet_size = 0;
uint8_t mouse_enabled = 0;

/**
 * @brief Set sample rate
 */
int ps2_setMouseSampleRate(uint8_t rate) {
    if (ps2_sendDeviceACK(mouse_port, PS2_MOUSE_SET_SAMPLE_RATE)) return 1;
    if (ps2_sendDeviceACK(mouse_port, rate)) return 1;
    return 0;
}

/**
 * @brief Mouse IRQ handler
 */
int mouse_irq(void *context) {
    if (!mouse_enabled) return 0;
    uint8_t data = inportb(PS2_DATA);

    // Get the next byte in the cycle
    ps2_mouse_packet[ps2_mouse_packet_cycle] = data;
    if (!ps2_mouse_packet_cycle && !(ps2_mouse_packet[ps2_mouse_packet_cycle] & 0x08)) {
        return 0;
    }
    ps2_mouse_packet_cycle++;
    

    // TODO: Support for more advanced PS/2 mouse modes?
    if (ps2_mouse_packet_cycle < mouse_packet_size) {
        return 0;
    }

    // Reset packet cycle
    ps2_mouse_packet_cycle = 0;

    // Start building data
    int x_diff = ps2_mouse_packet[1];
    int y_diff = ps2_mouse_packet[2];
    int scroll = MOUSE_SCROLL_NONE;

    if (mouse_id == 0x03 || mouse_id == 0x04) {
        if ((int8_t)(ps2_mouse_packet[3]) < 0) {
            // Scrolling up
            scroll = MOUSE_SCROLL_UP;
        } else if ((int8_t)(ps2_mouse_packet[3]) > 0) {
            // Scrolling down
            scroll = MOUSE_SCROLL_DOWN;
        }
    }

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

    if (buttons == ps2_last_buttons && !x_diff && !y_diff && !scroll) return 0;
    periphfs_sendMouseEvent(EVENT_MOUSE_UPDATE, buttons, x_diff, y_diff, scroll);
    ps2_last_buttons = buttons;
    return 0;
}

/**
 * @brief Initialize the PS/2 mouse
 */
void mouse_init(uint8_t p) {
    mouse_port = p;

    // Do magic sequence
    ps2_setMouseSampleRate(200);
    ps2_setMouseSampleRate(100);
    ps2_setMouseSampleRate(80);
    ps2_sendDeviceACK(mouse_port, PS2_DEVCMD_IDENTIFY);
    
    uint8_t byte = ps2_readByte();
    mouse_id = byte;
    switch (byte) {
        case 0x00:
            // Standard PS/2 mouse
            mouse_packet_size = 3;
            break;

        case 0x03:
            // PS/2 mouse with scrollwheel
            mouse_packet_size = 4;
            break;
        
        case 0x04:
            // PS/2 mouse with 5 buttons
            mouse_packet_size = 4;
            break;

        default:
            LOG(ERR, "Unsupported PS/2 mouse %02x\n", byte);
            hal_unregisterInterruptHandler(PS2_MOUSE_IRQ);
            break;
    }

    LOG(DEBUG, "Mouse ID: %02x\n", mouse_id);
    mouse_enabled = 1;


    // Register IRQ
    hal_registerInterruptHandler(PS2_MOUSE_IRQ, mouse_irq, NULL);
}