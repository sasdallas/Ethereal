/**
 * @file hexahedron/include/kernel/drivers/usb/hid/hid.h
 * @brief USB Human Interface Device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_USB_HID_HID_H
#define DRIVERS_USB_HID_HID_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef struct USBHidOptionalDescriptor {
    uint8_t bDescriptorType;
    uint16_t wItemLength;
} USBHidOptionalDescriptor_t;

typedef struct USBHidDescriptor {
    uint8_t bLength;                    // Size of this descriptor in bytes
    uint8_t bDescriptorType;            // USB_DESC_HID
    uint16_t bcdHid;                    // HID class specification release number 
    uint8_t bCountryCode;               // Country code
    uint8_t bNumDescriptors;            // Number of optional descriptors
    USBHidOptionalDescriptor_t desc[];  // Optional descriptors
} USBHidDescriptor_t;

/**** FUNCTIONS ****/

/**
 * @brief Register and initialize HID drivers
 */
void hid_init();

#endif