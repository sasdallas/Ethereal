/**
 * @file hexahedron/include/kernel/drivers/usb/api.h
 * @brief USB driver API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_API_H
#define DRIVERS_USB_API_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/usb/status.h>
#include <kernel/drivers/usb/dev.h>
#include <kernel/drivers/usb/desc.h>

/**** FUNCTIONS ****/

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
USB_STATUS usb_controlTransferDevice(USBDevice_t *dev, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data);

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
USB_STATUS usb_controlTransferInterface(USBInterface_t *intf, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data);

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
USB_STATUS usb_controlTransferEndpoint(USBEndpoint_t *endp, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data);

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
USB_STATUS usb_controlTransfer(USBDevice_t *dev, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data);


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
USB_STATUS usb_getDescriptor(USBDevice_t *dev, uintptr_t request_type, uintptr_t type, uintptr_t index, uintptr_t length, void *desc);

/**
 * @brief Read a string from the USB device
 * @param dev The device to read the string from
 * @param idx The index of the string to read
 * @param lang The language code
 * @param buffer The output buffer
 * @param length The maximum length of the string
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_getStringDevice(USBDevice_t *device, int idx, uint16_t lang, char *buffer, size_t length);

/**
 * @brief Configure the endpoint of a USB device
 * 
 * You are expected to do this when you want to use a specific endpoint
 * 
 * @param device The device to configure the endoint for
 * @param endp The endpoint to configure
 * @returns USB_SUCCESS on success
 */
USB_STATUS usb_configureEndpoint(USBDevice_t *device, USBEndpoint_t *endp);


/**
 * @brief Perform USB interrupt transfer
 * @param device The USB device to perform the transfer on
 * @param transfer The transfer to perform
 * @returns USB transfer status
 */
int usb_interruptTransfer(USBDevice_t *device, USBTransfer_t *transfer);

#endif