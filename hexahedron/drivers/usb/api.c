/**
 * @file hexahedron/drivers/usb/api.c
 * @brief API for drivers
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/api.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:API", __VA_ARGS__)

/**
 * @brief Perform a control transfer on a device
 * @param dev The device to do the transfer on
 * @param type The request type. See USB_RT_... - this corresponds to bmRequestType
 * @param request The request to send. See USB_REQ_... - this corresponds to bRequest
 * @param value Optional parameter for the request - this corresponds to wValue
 * @param index Optional index for the request - this corresponds to wIndex
 * @param length The length of the output data
 * @param data The output data
 * 
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_controlTransferDevice(USBDevice_t *dev, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data) {
    if (!dev) return USB_FAILURE;

    if (usb_requestDevice(dev, type | USB_RT_DEV, request, value, index, length, data) != USB_TRANSFER_SUCCESS) {
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Perform a control transfer on an interface
 * @param intf The interface to do the transfer on
 * @param type The request type. See USB_RT_... - this corresponds to bmRequestType
 * @param request The request to send. See USB_REQ_... - this corresponds to bRequest
 * @param value Optional parameter for the request - this corresponds to wValue
 * @param index Optional index for the request - this corresponds to wIndex
 * @param length The length of the output data
 * @param data The output data
 * 
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_controlTransferInterface(USBInterface_t *intf, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data) {
    if (!intf || !intf->dev) return USB_FAILURE;

    if (usb_requestDevice(intf->dev, type | USB_RT_INTF, request, value, index | intf->desc.bInterfaceNumber, length, data) != USB_TRANSFER_SUCCESS) {
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Perform a control transfer on an endpoint
 * @param endp The endpoint to do the transfer on
 * @param type The request type. See USB_RT_... - this corresponds to bmRequestType
 * @param request The request to send. See USB_REQ_... - this corresponds to bRequest
 * @param value Optional parameter for the request - this corresponds to wValue
 * @param index Optional index for the request - this corresponds to wIndex
 * @param length The length of the output data
 * @param data The output data
 * 
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_controlTransferEndpoint(USBEndpoint_t *endp, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data) {
    if (!endp || !endp->intf || !endp->intf->dev) return USB_FAILURE;

    if (usb_requestDevice(endp->intf->dev, type | USB_RT_ENDP, request, value, index | endp->desc.bEndpointAddress, length, data) != USB_TRANSFER_SUCCESS) {
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Perform a control transfer
 * @param dev The device to do the transfer on
 * @param type The request type. See USB_RT_... - this corresponds to bmRequestType
 * @param request The request to send. See USB_REQ_... - this corresponds to bRequest
 * @param value Optional parameter for the request - this corresponds to wValue
 * @param index Optional index for the request - this corresponds to wIndex
 * @param length The length of the output data
 * @param data The output data
 * 
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_controlTransfer(USBDevice_t *dev, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data){
    if (!dev) return USB_FAILURE;

    if (usb_requestDevice(dev, type, request, value, index, length, data) != USB_TRANSFER_SUCCESS) {
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Read a descriptor from a device
 * @param dev The device to read the descriptor from
 * @param request_type The request type (USB_RT_STANDARD or USB_RT_CLASS mainly)
 * @param type The type of the descriptor to get
 * @param index The index of the descriptor to get (default 0)
 * @param length The length of how much to read
 * @param desc The output descriptor
 * 
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_getDescriptor(USBDevice_t *dev, uintptr_t request_type, uintptr_t type, uintptr_t index, uintptr_t length, void *desc) {
    return usb_controlTransferDevice(dev, USB_RT_D2H | USB_RT_DEV | request_type, USB_REQ_GET_DESC, type, index, length, desc);
}

/**
 * @brief Read a string from the USB device
 * @param dev The device to read the string from
 * @param idx The index of the string to read
 * @param lang The language code
 * @param buffer The output buffer
 * @param length The maximum length of the string
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_getStringDevice(USBDevice_t *device, int idx, uint16_t lang, char *buffer, size_t length) {
    if (idx == 0) {
        // String index #0 is reserved for languages - this usually means that a driver attempted to get a nonexistant string ID
        LOG(WARN, "Tried to access string ID #0 - nonfatal\n");
        return USB_FAILURE;
    }

    // Request the descriptor
    uint8_t bLength;

    if (usb_getDescriptor(device, USB_RT_STANDARD, (USB_DESC_STRING << 8) | idx, lang, 1, &bLength) != USB_TRANSFER_SUCCESS)
    {
        LOG(WARN, "Failed to get string index %i for device\n", idx);
        return USB_FAILURE;
    }


    // Now read the full descriptor
    USBStringDescriptor_t *desc = kmalloc(bLength);

    if (usb_getDescriptor(device, USB_RT_STANDARD, (USB_DESC_STRING << 8) | idx, lang, bLength, desc) != USB_TRANSFER_SUCCESS)
    {
        LOG(WARN, "Failed to get string index %i for device\n", idx);
        kfree(desc);
        return USB_FAILURE;
    }

    size_t total_string_length = ((bLength - 2) / 2); 
    if (total_string_length > length-1) total_string_length = length;

    // Convert it to ASCII
    size_t i = 0;
    while ((i/2) < total_string_length) {
        buffer[i/2] = desc->bString[i];
        i += 2;
    }

    buffer[total_string_length-1] = 0;

    kfree(desc);

    return USB_SUCCESS;
}
