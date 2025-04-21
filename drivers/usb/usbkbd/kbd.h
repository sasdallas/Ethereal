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

/**** DEFINITIONS ****/

#define KBD_CLASS                       0x03    // HID class
#define KBD_SUBCLASS                    0x01    // Boot subclass
#define KBD_PROTOCOL                    0x01    // Keyboard protocol


/**** TYPES ****/

typedef struct usbkbd {
    USBTransfer_t transfer;         // The interrupt transfer which was setup
    USBEndpoint_t *endp;            // Endpoint selected (INTERRUPT IN)
} usbkbd_t;

typedef struct usb_kbd_data {
    uint8_t data[8];
} __attribute__((packed)) usb_kbd_data_t;

#endif