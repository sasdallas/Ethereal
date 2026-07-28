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
USB_STATUS usb_controlTransferDevice(USBDevice_t *dev, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length, void *data);

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
USB_STATUS usb_controlTransferInterface(USBInterface_t *intf, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length, void *data);

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
USB_STATUS usb_controlTransferEndpoint(USBEndpoint_t *endp, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length, void *data);

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
USB_STATUS usb_controlTransfer(USBDevice_t *dev, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length, void *data);


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
USB_STATUS usb_getDescriptor(USBDevice_t *dev, uint8_t request_type, uint8_t type, uint16_t index, uint16_t length, void *desc);

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
USB_TRANSFER_STATUS usb_interruptTransfer(USBDevice_t *device, USBTransfer_t *transfer);

/**
 * @brief Perform USB bulk transfer (syncronous)
 * @param device The USB device to perform the transfer on
 * @param transfer The transfer to perform
 * @returns USB transfer status
 */
USB_TRANSFER_STATUS usb_bulkTransfer(USBDevice_t *device, USBTransfer_t *transfer);

/**
 * @brief Receive bytes from a bulk transfer endpoint
 * @param device The device to receive from
 * @param endp The endpoint to receive from
 * @param data The data buffer to receive into
 * @param size The amount of data to transfer
 */
static inline USB_TRANSFER_STATUS usb_bulkReceive(USBDevice_t *device, USBEndpoint_t *endp, void *data, size_t data_size) {
    USBTransfer_t bulk_xfer = {
        .data = data,
        .endp = endp,
        .endpoint = USB_ENDP_NUMBER(endp),
        .length = data_size,
        .parameter = NULL,
        .status = USB_TRANSFER_IN_PROGRESS
    };

    return usb_bulkTransfer(device, &bulk_xfer);
}

/**
 * @brief Send bytes to a bulk transfer endpoint
 * @param device The device to send to
 * @param endp The endpoint to send to
 * @param data The data buffer to send from
 * @param size The amount of data to transfer
 */
static inline USB_TRANSFER_STATUS usb_bulkSend(USBDevice_t *device, USBEndpoint_t *endp, void *data, size_t data_size) {
    USBTransfer_t bulk_xfer = {
        .data = data,
        .endp = endp,
        .endpoint = USB_ENDP_NUMBER(endp),
        .length = data_size,
        .parameter = NULL,
        .status = USB_TRANSFER_IN_PROGRESS
    };

    return usb_bulkTransfer(device, &bulk_xfer);
}

#endif
