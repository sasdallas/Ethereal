/**
 * @file hexahedron/include/kernel/drivers/usb/status.h
 * @brief USB status codes
 * 
 * @todo Why does this need its own header?
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_STATUS_H
#define DRIVERS_USB_STATUS_H

/**** TYPES ****/

/**
 * @brief Response codes
 * 
 * This is an enum so you know which devices return a different status code.
 */
typedef enum USB_STATUS {
    USB_SUCCESS = 0,
    USB_FAILURE = 1,
} USB_STATUS;

#endif