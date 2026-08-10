/**
 * @file hexahedron/drivers/usb2/util.c
 * @brief Utility wrappers for 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>

/**
 * @brief Perform a control request on a device with flags
 * @param device The device to perform the request on
 * @param req The request to perform
 * @param data Output buffer pointer
 * @param flags The flags to use for the transfer
 * @returns USB_SUCCESS on success
 * 
 * @note Syncronous transfer
 */
usb_status_t usb_requestFlags(usb_device_t *device, usb_device_request_t *req, void *data, uint32_t flags) {
    usb_transfer_t *transfer = usb_allocateTransfer(device);
    if (!transfer) return USB_NO_MEMORY;
    usb_prepareControlTransfer(transfer, device, req, data, req->wLength, flags, NULL, USB_NO_TIMEOUT, NULL);

    usb_status_t status = usb_transferSync(transfer);

    usb_freeTransfer(transfer);

    return status;
}

/**
 * @brief Perform a control request on a device
 * @param device The device to perform the request on
 * @param req The request to perform
 * @param data Output buffer pointer
 * @returns USB_SUCCESS on success
 * 
 * @note Syncronous transfer
 */
usb_status_t usb_request(usb_device_t *device, usb_device_request_t *req, void *data) {
    return usb_requestFlags(device, req, data, USB_TRANSFER_DEFAULT);
}

/**
 * @brief Retrieve a descriptor with flags
 * @param device The device to get the descriptor from
 * @param type The type of descriptor to get
 * @param index The index of the descriptor
 * @param desc Output pointer for descriptor
 * @param desc_size Size of descriptor
 * @param flags The flags to use
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getDescriptorFlags(usb_device_t *device, int type, int index, void *desc, size_t desc_size, uint32_t flags) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
        .bRequest = USB_REQ_GET_DESC,
        .wValue = (type << 8) | index,
        .wIndex = 0,
        .wLength = desc_size
    };

    return usb_requestFlags(device, &req, desc, flags);
}

/**
 * @brief Retrieve a descriptor
 * @param device The device to get the descriptor from
 * @param type The type of descriptor to get
 * @param index The index of the descriptor
 * @param desc Output pointer for descriptor
 * @param desc_size Size of descriptor
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getDescriptor(usb_device_t *device, int type, int index, void *desc, size_t desc_size) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
        .bRequest = USB_REQ_GET_DESC,
        .wValue = (type << 8) | index,
        .wIndex = 0,
        .wLength = desc_size
    };

    return usb_request(device, &req, desc);
}

/**
 * @brief Get string from device
 * @param dev The device to get the string from
 * @param index The index of the string
 * @param buf The buffer to store the string in (at least USB_MAX_STRING_LENGTH long)
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getString(usb_device_t *device, int index, char *buf) {
    if (index == 0) {
        *buf = 0;
        return USB_SUCCESS;
    }

    usb_string_desc_t desc;
    usb_status_t status = usb_getDescriptorFlags(device, USB_DESC_STRING, index, &desc, sizeof(usb_string_desc_t), USB_TRANSFER_ALLOW_SHORT);
    if (USB_ERROR(status)) {
        return status;
    }

    if (desc.bLength == 0) {
        *buf = 0;
        return USB_SUCCESS;
    }

    int length = min(desc.bLength, USB_MAX_STRING_LENGTH-1);
    for (int i = 0; i < (length-2)/2; i++) {
        buf[i] = (desc.bString[i] & 0xFF);
    }

    buf[(length-2)/2] = 0;

    return USB_SUCCESS;
}


/**
 * @brief Set configuration for device
 * @param dev The device to set the configuration for
 * @param config The config to select
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_setConfiguration(usb_device_t *device, usb_configuration_t *config) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEV,
        .bRequest = USB_REQ_SET_CONF,
        .wValue = config->desc.bConfigurationValue,
        .wIndex = 0,
        .wLength = 0
    };

    usb_status_t status = usb_request(device, &req, NULL);
    if (USB_ERROR(status)) return status;

    device->selected = config;
    return USB_SUCCESS;
}
