/**
 * @file drivers/usb/usbkbd/kbd.h
 * @brief USB keyboard driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KBD_H
#define KBD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/task/process.h>

/**** DEFINITIONS ****/

#define KBD_CLASS                       0x03    // HID class
#define KBD_SUBCLASS                    0x01    // Boot subclass
#define KBD_PROTOCOL                    0x01    // Keyboard protocol

/* Modifiers */
#define KBD_MOD_LEFT_CTRL               0x01
#define KBD_MOD_LEFT_SHIFT              0x02
#define KBD_MOD_LEFT_ALT                0x04
#define KBD_MOD_LEFT_SUPER              0x08
#define KBD_MOD_RIGHT_CTRL              0x10
#define KBD_MOD_RIGHT_SHIFT             0x20
#define KBD_MOD_RIGHT_ALT               0x40
#define KBD_MOD_RIGHT_SUPER             0x80

/* Default wait (before we start repeating a long pressed key) */
#define KBD_DEFAULT_WAIT                10

/**** TYPES ****/

typedef struct usb_kbd_data {
    uint8_t data[8];
} __attribute__((packed)) usb_kbd_data_t;

typedef struct usbkbd {
    USBInterface_t *intf;           // Interface
    USBTransfer_t transfer;         // The interrupt transfer which was setup
    USBEndpoint_t *endp;            // Endpoint selected (INTERRUPT IN)
    usb_kbd_data_t data;            // Data
    usb_kbd_data_t last_data;       // Last data read (for other keys)
    process_t *proc;                // Polling thread
    int auto_repeat_wait;           // Auto repeat wait count. Decremented each time a duplicate is detected
                                    // Why does USB not include the PS/2 method of building this into hardware?
} usbkbd_t;

#endif