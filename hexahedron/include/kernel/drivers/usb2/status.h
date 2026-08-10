/**
 * @file hexahedron/include/kernel/drivers/usb2/status.h
 * @brief USB status
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_USB_STATUS_H
#define DRIVERS_USB_STATUS_H

/**** TYPES ****/

typedef enum usb_status {
    USB_SUCCESS,
    USB_IN_PROGRESS,
    
    // The below are errors:
    USB_NOT_STARTED,
    USB_INVALID,
    USB_NO_MEMORY,
    USB_BAD_ADDRESS,
    USB_SHORT_TRANSFER,
    USB_INTERNAL_ERROR,
    USB_STALLED,
    USB_TIMED_OUT,
    USB_ABORTED,
    USB_BAD_DESCRIPTOR,
    USB_ERROR_MAX
} usb_status_t;

/**** MACROS ****/

#define USB_ERROR(status) ((status) != USB_SUCCESS)
#define USB_ERROR2(status) ((status) >= USB_NOT_STARTED)

/**** FUNCTIONS ****/

/**
 * @brief Convert a USB status into error description
 * @param status The status to convert
 */
static inline const char *usb_strerror(usb_status_t status) {
    switch (status) {
        case USB_SUCCESS: return "Success (USB_SUCCESS)";
        case USB_IN_PROGRESS: return "In progress (USB_IN_PROGRESS)";
        case USB_NOT_STARTED: return "Not started (USB_NOT_STARTED)";
        case USB_INVALID: return "Invaild parameter (USB_INVALID)";
        case USB_NO_MEMORY: return "No memory available (USB_NO_MEMORY)";
        case USB_BAD_ADDRESS: return "Bad endpoint address (USB_BAD_ADDRESS)";
        case USB_SHORT_TRANSFER: return "Short transfer (USB_SHORT_TRANSFER)";
        case USB_INTERNAL_ERROR: return "Internal error (USB_INTERNAL_ERROR)";
        case USB_STALLED: return "Transfer stalled (USB_STALLED)";
        case USB_TIMED_OUT: return "Transfer timed out (USB_TIMED_OUT)";
        case USB_ABORTED: return "Transfer was aborted (USB_ABORTED)";
        case USB_BAD_DESCRIPTOR: return "Descriptor is corrupt (USB_BAD_DESCRIPTOR)";
        default: return "Unknown error (invalid status)";
    }
}

#endif
