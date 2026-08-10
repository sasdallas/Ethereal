/**
 * @file hexahedron/drivers/usb2/roothub.c
 * @brief USB root hub handler
 * 
 * Provides fake operations for the USB root hub.
 * Simulates the CONTROL endpoint garbage. The HC is required to provide the endpoint pipe.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>

/* Control pipe operations */
static usb_status_t roothub_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void roothub_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t roothub_submit(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t roothub_start(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void roothub_abort(usb_pipe_t *pipe, usb_transfer_t *transfer); 
usb_pipe_ops_t roothub_control_ops = {
    .init_transfer = roothub_init_transfer,
    .free_transfer = roothub_free_transfer,
    .submit = roothub_submit,
    .start = roothub_start,
    .abort = roothub_abort
};

typedef struct {
    usb_config_desc_t config;
    usb_interface_desc_t interface;
    usb_endpoint_desc_t endpoint;
} __attribute__((packed)) roothub_desc_t;

/* Primary root hub descriptor */
#define DECLARE_ROOTHUB_DESCRIPTOR(len,proto) \
    .config = {\
        .bLength = sizeof(usb_config_desc_t),\
        .bDescriptorType = USB_DESC_CONF,\
        .wTotalLength = sizeof(roothub_desc_t),\
        .bNumInterfaces = 1,\
        .bConfigurationValue = 0,\
        .iConfiguration = 0,\
        .bmAttributes = 0x80 | 0x40,\
        .bMaxPower = 0,\
    },\
    .interface = {\
        .bLength = sizeof(usb_interface_desc_t),\
        .bDescriptorType = USB_DESC_INTF,\
        .bInterfaceNumber = 0,\
        .bAlternateSetting = 0,\
        .bNumEndpoints = 1,\
        .bInterfaceClass = USB_CLASS_HUB,\
        .bInterfaceSubClass = USB_SUBCLASS_HUB,\
        .bInterfaceProtocol = (proto),\
        .iInterface = 0\
    },\
    .endpoint = {\
        .bLength = sizeof(usb_endpoint_desc_t),\
        .bDescriptorType = USB_DESC_ENDP,\
        .bEndpointAddress = 0x81,\
        .bmAttributes = USB_ENDP_TYPE_INT,\
        /* !!!: This is wrong on certain speeds */ \
        .wMaxPacketSize = 2,\
        .bInterval = 8,\
    }

static roothub_desc_t roothub_desc_full = { DECLARE_ROOTHUB_DESCRIPTOR(sizeof(roothub_desc_t), USB_PROTOCOL_HUB_FULLSPD) };
static roothub_desc_t roothub_desc_high = { DECLARE_ROOTHUB_DESCRIPTOR(sizeof(roothub_desc_t), USB_PROTOCOL_HUB_HIGHSPD_STT) };

// TODO: Superspeed capability
static roothub_desc_t roothub_desc_super = { DECLARE_ROOTHUB_DESCRIPTOR(sizeof(roothub_desc_t), USB_PROTOCOL_HUB_SUPERSPD) };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:ROOTHUB", __VA_ARGS__)

/**
 * @brief Initialize a transfer on the root hub
 */
static usb_status_t roothub_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    return USB_SUCCESS;
}

/**
 * @brief Free transfer
 */
static void roothub_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    // No-op
}

/**
 * @brief Submit transfer to inner pipe
 */
static usb_status_t roothub_submit(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    transfer->status = USB_IN_PROGRESS;
    return USB_SUCCESS;
}

/**
 * @brief Handle get descriptor request
 */
static void roothub_handleGetDescriptor(usb_transfer_t *transfer) {
    usb_device_t *dev = transfer->pipe->device;
    usb_device_request_t *req = &transfer->req;
    uint8_t desc_type = (req->wValue >> 8) & 0xFF;
    uint8_t desc_index = (req->wValue & 0xFF);

    if (desc_type == USB_DESC_DEVICE && desc_index == 0) {
        // must be constructed dynamically
        uint16_t bcdUsb;
        switch (dev->speed) {
            case USB_SPEED_FULL: bcdUsb = 0x0100; break; // !!! if actually USB 1.1 this loses it
            case USB_SPEED_HIGH: bcdUsb = 0x0200; break;
            case USB_SPEED_SUPER: bcdUsb = 0x0300; break;
            case USB_SPEED_SUPER_PLUS: bcdUsb = 0x0310; break;
            default:
                LOG(ERR, "Unhandled case for speed 0x%x\n", dev->speed);
                bcdUsb = 0x0200;
                break;
        }

        uint8_t bProtocol;
        switch (dev->speed) {
            case USB_SPEED_FULL: 
                bProtocol = USB_PROTOCOL_HUB_FULLSPD;
                break;
            case USB_SPEED_HIGH:
                bProtocol = USB_PROTOCOL_HUB_HIGHSPD_STT;
                break;
            case USB_SPEED_SUPER:
            case USB_SPEED_SUPER_PLUS:
                bProtocol = USB_PROTOCOL_HUB_SUPERSPD;
                break;
            default:
                LOG(ERR, "Unhandled case for speed 0x%x\n", dev->speed);
                bProtocol = USB_PROTOCOL_HUB_HIGHSPD_STT;
                break;
        }

        usb_device_desc_t desc = {
            .bLength = sizeof(usb_device_desc_t),
            .bDescriptorType = USB_DESC_DEVICE,
            .bcdUSB = bcdUsb,
            .bDeviceClass = USB_CLASS_HUB,
            .bDeviceSubClass = USB_SUBCLASS_HUB,
            .bDeviceProtocol = bProtocol,
            .bMaxPacketSize0 = transfer->pipe->endp->mps,
            .idVendor = 0x0000,
            .idProduct = 0x0000,
            .bcdDevice = 0x0100,
            .iManufacturer = 1,
            .iProduct = 2,
            .iSerialNumber = 0,
            .bNumConfigurations = 1,
        };

        size_t to_copy = min(transfer->length, sizeof(usb_device_desc_t));
        memcpy(transfer->buffer, &desc, to_copy);
        transfer->actual_length = to_copy;
        transfer->status = USB_SUCCESS;
    } else if (desc_type == USB_DESC_STRING) {
        char *fill;
        switch (desc_index) {
            case 1: fill = "Ethereal Inc."; break; // very good name
            case 2: fill = "Root Hub"; break;
            default:
                LOG(ERR, "Invalid string descriptor %i\n", desc_index);
                transfer->status = USB_INVALID;
                return;
        }

        usb_string_desc_t desc = { 0 };
        size_t len = strlen(fill);
        if (len > USB_MAX_STRING_LENGTH) len = USB_MAX_STRING_LENGTH;

        // convert it to unicode
        for (unsigned i = 0; i < len; i++) {
            desc.bString[i] = fill[i];
        }

        desc.bLength = sizeof(usb_descriptor_t) + (len*2);
        desc.bDescriptorType = USB_DESC_STRING;

        size_t to_copy = min(desc.bLength, transfer->length);
        memcpy(transfer->buffer, &desc, to_copy);
        transfer->actual_length = to_copy;
        transfer->status = USB_SUCCESS;
    } else if (desc_type == USB_DESC_CONF) {
        if (desc_index != 0) {
            LOG(ERR, "Invalid config index %d\n", desc_index);
            transfer->status = USB_INVALID;
            return;
        }

        roothub_desc_t *target = NULL;
        switch (dev->speed) {
            case USB_SPEED_FULL: target = &roothub_desc_full; break;
            case USB_SPEED_HIGH: target = &roothub_desc_high; break;

            case USB_SPEED_SUPER:
            case USB_SPEED_SUPER_PLUS:
                target = &roothub_desc_super;
                break;

            default:
                LOG(ERR, "Invalid device speed 0x%x, assuming high speed\n", dev->speed);
                target = &roothub_desc_high;
                break;
        }

        size_t to_copy = min(transfer->length, target->config.wTotalLength);
        memcpy(transfer->buffer, target, to_copy);
        transfer->actual_length = to_copy;
        transfer->status = USB_SUCCESS;
    } else if (desc_type == USB_DESC_HUB || desc_type == USB_DESC_HUB_SUPERSPEED) {
        transfer->status = dev->bus->ops->root_hub_control(dev->bus, transfer);
    } else {
        LOG(ERR, "Unhandled GET_DESCRIPTOR request for type=0x%02x index=0x%02x\n", desc_type, desc_index);
        transfer->status = USB_INVALID;
    }
}

/**
 * @brief Handle a roothub transfer
 */
static void roothub_handle(usb_transfer_t *transfer) {
    usb_device_request_t *req = &transfer->req;
    usb_device_t *dev = transfer->pipe->device;

#if 0
    LOG(DEBUG, "roothub_handle bmRequestType=%02x bRequest=%02x wValue=%04x wIndex=%04x wLength=%04x\n", req->bmRequestType, req->bRequest, req->wValue, req->wIndex, req->wLength);
#endif

    if (req->bRequest == USB_REQ_GET_STATUS) {
        transfer->status = dev->bus->ops->root_hub_control(dev->bus, transfer);
    } else if (req->bRequest == USB_REQ_CLEAR_FEATURE) {
        transfer->status = dev->bus->ops->root_hub_control(dev->bus, transfer);    
    } else if (req->bRequest == USB_REQ_SET_FEATURE) {
        transfer->status = dev->bus->ops->root_hub_control(dev->bus, transfer);
    } else if (req->bRequest == USB_REQ_SET_ADDR) {
        transfer->status = USB_SUCCESS;
    } else if (req->bRequest == USB_REQ_GET_DESC) {
        roothub_handleGetDescriptor(transfer);
    } else if (req->bRequest == USB_REQ_SET_CONF) {
        if (req->wValue != 0) {
            LOG(ERR, "Invalid configuration %d\n", req->wValue);
            transfer->status = USB_INVALID;
        } else {
            transfer->status = USB_SUCCESS;
        }
    } else if (req->bRequest == USB_HUB_REQ_SET_HUB_DEPTH) {
        transfer->status = USB_SUCCESS;
    } else {
        LOG(ERR, "Unknown/unhandled request type 0x%02x\n", req->bRequest);
        transfer->status = USB_INVALID;
    }
}

/**
 * @brief Start transfer
 */
static usb_status_t roothub_start(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    roothub_handle(transfer);
    usb_transferCompleteLocked(transfer);
    return USB_SUCCESS;
}

/**
 * @brief Abort transfer
 */
static void roothub_abort(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    // roothub transfers complete the moment they start...
    assert(0 && "cannot abort roothub transfer?");
}
