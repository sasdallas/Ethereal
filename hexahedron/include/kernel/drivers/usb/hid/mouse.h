/**
 * @file hexahedron/include/kernel/drivers/usb/hid/mouse.h
 * @brief Generic HID mouse driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_USB_HID_MOUSE_H
#define DRIVERS_USB_HID_MOUSE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>

/**** TYPES ****/

typedef struct USBHidMouseState {
    uint32_t rel_x;
    uint32_t rel_y; 

    struct {
        bool valid;
        uint32_t min_x;
        uint32_t max_x;
        uint32_t abs_x;
        uint32_t min_y;
        uint32_t max_y;
        uint32_t abs_y; 
    } abs;

    uint8_t buttons;
    int scroll;
} USBHidMouseState_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize generic HID mouse driver
 */
void usb_mouseInit();

#endif