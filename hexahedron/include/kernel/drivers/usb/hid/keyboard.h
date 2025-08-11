/**
 * @file hexahedron/include/kernel/drivers/usb/hid/keyboard.h
 * @brief Generic HID keyboard driver 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_USB_HID_KEYBOARD_H
#define DRIVERS_USB_HID_KEYBOARD_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef struct USBHidKeyboardState {
    uint8_t last_keyboard_state[8];
    uint8_t current_keyboard_state[8];
    uint8_t idx;
    uint8_t last_modifiers;
    uint8_t modifiers;
} USBHidKeyboardState_t;

/**** DEFINITIONS ****/

#define HID_KEYBOARD_USAGE_PAGE         0x1
#define HID_KEYBOARD_USAGE_ID           0x6

#define HID_KEYBOARD_MODIFIER_LCTRL     0
#define HID_KEYBOARD_MODIFIER_LSHIFT    1
#define HID_KEYBOARD_MODIFIER_LALT      2
#define HID_KEYBOARD_MODIFIER_LSUPER    3
#define HID_KEYBOARD_MODIFIER_RCTRL     4
#define HID_KEYBOARD_MODIFIER_RSHIFT    5
#define HID_KEYBOARD_MODIFIER_RALT      6
#define HID_KEYBOARD_MODIFIER_RSUPER    7


/**** FUNCTIONS ****/

/**
 * @brief Initialize generic USB HID keyboard driver
 */
void usb_keyboardInit();

#endif