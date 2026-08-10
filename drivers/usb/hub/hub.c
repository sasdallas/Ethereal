/**
 * @file drivers/usb/hub/hub.c
 * @brief USB hub driver
 * 
 * This is a very core driver to the USB subsystem: without it,
 * no USB devices will be detected.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "hub.h"
#include <kernel/loader/driver.h>
#include <kernel/drivers/usb2/usb.h>
#include <kernel/debug.h>

static int hub_match(usb_device_t *device, usb_interface_t *intf);
static usb_status_t hub_attach(usb_device_t *device, usb_interface_t *intf);
static usb_status_t hub_detach(usb_device_t *device, usb_interface_t *intf);
usb_driver_t hub_driver = {
    .name = "Hub Driver",
    .ops = {
        .match = hub_match,
        .attach = hub_attach,
        .detach = hub_detach,
    }
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USBHUB", __VA_ARGS__)
/**
 * @brief Clear port feature
 */
static usb_status_t hub_clearFeature(usb_device_t *device, int port_index, uint32_t feature) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_H2D | USB_RT_CLASS | USB_RT_OTHER,
        .bRequest = USB_REQ_CLEAR_FEATURE,
        .wValue = feature,
        .wIndex = port_index,
        .wLength = 0,
    };

    return usb_request(device, &req, NULL);
}

/**
 * @brief Set port feature
 */
static usb_status_t hub_setFeature(usb_device_t *device, int port_index, uint32_t feature) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_H2D | USB_RT_CLASS | USB_RT_OTHER,
        .bRequest = USB_REQ_SET_FEATURE,
        .wValue = feature,
        .wIndex = port_index,
        .wLength = 0,
    };

    return usb_request(device, &req, NULL);
}

/**
 * @brief Get port status
 */
static usb_status_t hub_getPortStatus(usb_device_t *device, int port_index, usb_hub_port_status_t *sts) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_D2H | USB_RT_CLASS | USB_RT_OTHER,
        .bRequest = USB_REQ_GET_STATUS,
        .wValue = 0,
        .wIndex = port_index,
        .wLength = sizeof(usb_hub_port_status_t)
    };

    return usb_request(device, &req, sts);
}

#define CLEAR_CHANGE(cbit, sel) do {\
    if ((changes) & (cbit)) {\
        status = hub_clearFeature(dev, port, (sel));\
        if (USB_ERROR(status)) {\
            LOG(ERR, "Failed to clear port feature " #sel ": %s\n", usb_strerror(status));\
            return status;\
        }\
    }\
} while(0);

/**
 * @brief Acknowledge port changes
 * @param dev The device to ack
 * @param port The port to acknowledge on
 * @param changes Change bitmask
 */
static usb_status_t hub_acknowledgeChanges(usb_device_t *dev, int port, uint16_t changes) {
    usb_status_t status = USB_SUCCESS;
    CLEAR_CHANGE(
        USB_HUB_CHANGE_CONNECTION,
        USB_HUB_SEL_C_PORT_CONNECTION
    );

    CLEAR_CHANGE(
        USB_HUB_CHANGE_ENABLE,
        USB_HUB_SEL_C_PORT_ENABLE
    );

    CLEAR_CHANGE(
        USB_HUB_CHANGE_OVERCURRENT,
        USB_HUB_SEL_C_PORT_OVER_CURRENT
    );

    CLEAR_CHANGE(
        USB_HUB_CHANGE_RESET,
        USB_HUB_SEL_C_PORT_RESET
    );

    if (USB_IS_SUPERSPEED(dev->speed)) {
        CLEAR_CHANGE(
            USB_HUB_SS_CHANGE_BH_PORT_RESET,
            USB_HUB_SEL_C_BH_PORT_RESET
        );

        CLEAR_CHANGE(
            USB_HUB_SS_CHANGE_PORT_LINK_STATE,
            USB_HUB_SEL_C_PORT_LINK_STATE
        );

        CLEAR_CHANGE(
            USB_HUB_SS_CHANGE_PORT_CONFIG_ERROR,
            USB_HUB_SEL_C_PORT_CONFIG_ERROR
        );
    }

    return status;
}

/**
 * @brief Handle connection change
 */
static void hub_handleConnectionChange(hub_internal_t *internal, int port, usb_hub_port_status_t *sts) {
    usb_port_t *uport = &internal->hub->ports[port-1];

    if (!(sts->wPortStatus & USB_HUB_STATUS_CONNECTION)) {
        LOG(DEBUG, "Device %p is not connected anymore\n", uport->device);
        if (uport->device != NULL) {
            uport->state = USB_PORT_DISCONNECTING;
            usb_removeDevice(uport->device);
        } else {
            uport->state = USB_PORT_IDLE;
        }
        
        return;
    }

    if (uport->state == USB_PORT_RESETTING) {
        LOG(ERR, "Possible bugged USB hub: received connection change while port is resetting.\n");
        return;
    }

    // Now reset the port
    LOG(DEBUG, "Resetting port %d.\n", port);
    uport->state = USB_PORT_RESETTING;
    usb_status_t status = hub_setFeature(internal->intf->device, port, USB_HUB_SEL_PORT_RESET);
    if (USB_ERROR(status)) {
        LOG(ERR, "SET_FEATURE(USB_HUB_STATUS_RESET) failed: %s\n", usb_strerror(status));
        uport->state = USB_PORT_IDLE;
        return;
    }
}

/**
 * @brief Handle reset finish
 */
static void hub_handleResetChange(hub_internal_t *internal, int port, usb_hub_port_status_t *sts) {
    usb_port_t *uport = &internal->hub->ports[port-1];
    usb_device_t *dev = internal->intf->device;

    if (uport->state != USB_PORT_RESETTING) {
        LOG(WARN, "USB port was not in resetting state, assuming possible bug\n");
        uport->state = USB_PORT_IDLE;
        return;
    }

    if ((sts->wPortStatus & USB_HUB_STATUS_CONNECTION) == 0) {
        LOG(WARN, "USB port completed reset but did not set connection?\n");
        uport->state = USB_PORT_IDLE;
        return;
    }

    LOG(INFO, "Reset complete on USB port %d\n", port);
    
    usb_speed_t spd;
    if (USB_IS_SUPERSPEED(dev->speed)) {
        uint32_t spd_raw = USB_HUB_SS_SPEED(sts->wPortStatus);

        // TODO: Detect superspeed devices
        if (spd_raw == USB_HUB_SS_SPEED_LOW) spd = USB_SPEED_LOW;
        else if (spd_raw == USB_HUB_SS_SPEED_FULL) spd = USB_SPEED_FULL;
        else if (spd_raw == USB_HUB_SS_SPEED_HIGH) spd = USB_SPEED_HIGH;
        else if (spd_raw == USB_HUB_SS_SPEED_SUPER) spd = USB_SPEED_SUPER; 
        else assert(0 && "unknown hub speed");
    } else {
        if (sts->wPortStatus & USB_HUB_STATUS_LOW_SPEED) {
            spd = USB_SPEED_LOW;
        } else if (sts->wPortStatus & USB_HUB_STATUS_HIGH_SPEED) {
            spd = USB_SPEED_HIGH;
        } else {
            spd = USB_SPEED_FULL;
        }
    }

    // Create the device
    usb_device_t *new;
    usb_status_t status = usb_createDevice(dev->bus, uport, spd, &new);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to create port USB device: %s\n", usb_strerror(status));
        uport->state = USB_PORT_IDLE;
        return;
    }

    uport->device = new;
    uport->state = USB_PORT_CONNECTED;
}

/**
 * @brief Process change on port
 */
static void hub_processChange(hub_internal_t *internal, int port) {
    usb_device_t *dev = internal->intf->device;

    usb_hub_port_status_t sts;
    usb_status_t status = hub_getPortStatus(dev, port, &sts);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to get port status: %s\n", usb_strerror(status));
        return;
    }

    LOG(DEBUG, "Processing change on port %d: wPortStatus=%04x wPortChanged=%04x\n", port, sts.wPortStatus, sts.wPortChanged);

    if (sts.wPortChanged == 0) {
        return;
    }

    if (USB_ERROR(hub_acknowledgeChanges(dev, port, sts.wPortChanged))) {
        LOG(ERR, "Failed to acknowledge changes\n");
        return;
    }

    if (sts.wPortChanged & USB_HUB_CHANGE_CONNECTION) {
        hub_handleConnectionChange(internal, port, &sts);
    }

    if (sts.wPortChanged & USB_HUB_CHANGE_RESET) {
        hub_handleResetChange(internal, port, &sts);
    }
}

/**
 * @brief Process interrupt transfer
 */
static void hub_processChanges(usb_transfer_t *transfer) {
    hub_internal_t *internal = transfer->priv;
    if (transfer->status != USB_SUCCESS) {
        // Internal cannot be trusted, as it may have been freed
        LOG(ERR, "Interrupt request failed with error: %s\n", usb_strerror(transfer->status));
        usb_freeTransfer(transfer);
        return;
    }

    // the lock must be taken in order to avoid racing with the init sequence
    spinlock_acquireRaw(&internal->lock);

    for (unsigned i = 1; i <= internal->num_ports; i++) {
        if (bitmap_test(internal->bitmap, i)) {
            hub_processChange(internal, i);
        }
    }

    if (bitmap_test(internal->bitmap, 0)) {
        LOG(DEBUG, "Hub change detected (no impl)\n");
    }


    spinlock_releaseRaw(&internal->lock);

    // Restart USB transfer
    bitmap_fill(internal->bitmap, 0, internal->num_ports+1);
    usb_prepareTransfer(transfer, internal->int_pipe, internal->bitmap, transfer->length, USB_TRANSFER_ALLOW_SHORT, hub_processChanges, 0, internal);
    usb_transfer(transfer);
}

/**
 * @brief Get the hub's descriptor
 */
static usb_status_t hub_getDescriptor(usb_device_t *device, uint8_t type, void *buffer, size_t length) {
    usb_device_request_t req = {
        .bmRequestType = USB_RT_D2H | USB_RT_CLASS | USB_RT_DEV,
        .bRequest = USB_REQ_GET_DESC,
        .wValue = (type << 8),
        .wIndex = 0,
        .wLength = length
    };

    return usb_request(device, &req, buffer);
}

/**
 * @brief Match USB hub
 */
static int hub_match(usb_device_t *device, usb_interface_t *intf) {
    if (intf == NULL) return USB_MATCH_NONE;

    if (intf->desc.bInterfaceClass != USB_CLASS_HUB) {
        return USB_MATCH_NONE;
    }

    if (intf->desc.bInterfaceSubClass != USB_SUBCLASS_HUB) {
        return USB_MATCH_NONE;
    }

    if (
        intf->desc.bInterfaceProtocol != USB_PROTOCOL_HUB_HIGHSPD_MTT &&
        intf->desc.bInterfaceProtocol != USB_PROTOCOL_HUB_HIGHSPD_STT &&
        intf->desc.bInterfaceProtocol != USB_PROTOCOL_HUB_FULLSPD &&
        intf->desc.bInterfaceProtocol != USB_PROTOCOL_HUB_SUPERSPD
    ) {
        LOG(WARN, "Detected a device with a hub class/subclass but invalid protocol 0x%x?\n", intf->desc.bInterfaceProtocol);
        return USB_MATCH_NONE;
    }

    return USB_MATCH_INTF_CLASS_SUBCLASS_PROTO;
}

/**
 * @brief Attach USB hub
 */
static usb_status_t hub_attach(usb_device_t *device, usb_interface_t *intf) {
    // locate the first interrupt IN endpoint
    usb_endpoint_t *int_endp = NULL;
    for (unsigned i = 0; i < intf->num_endpoints; i++) {
        usb_endpoint_t *endp = intf->endpoints[i];
        if (USB_ENDP_TYPE(endp->desc.bmAttributes) == USB_ENDP_TYPE_INT) {
            if (USB_ENDP_DIRECTION(endp->desc.bEndpointAddress) == USB_ENDP_DIR_IN) {
                int_endp = endp;
                break;
            }
        }
    }

    if (int_endp == NULL) {
        LOG(ERR, "Hub has no interrupt IN endpoint\n");
        return USB_INVALID;
    }

    usb_pipe_t *pipe;
    usb_status_t status = usb_openPipe(int_endp, USB_PIPE_DEFAULT, &pipe);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error opening interrupt pipe: %s\n", usb_strerror(status));
        return status;
    }

    // Retrieve hub descriptor
    // TODO: None of the extra fields in the hub SS desc are used
    usb_hub_desc_ss_t d;
    uint8_t desc_type = USB_IS_SUPERSPEED(device->speed) ? USB_DESC_HUB_SUPERSPEED : USB_DESC_HUB;
    size_t desc_size = (desc_type == USB_DESC_HUB) ? sizeof(usb_hub_desc_t) : sizeof(usb_hub_desc_ss_t);
    status = hub_getDescriptor(device, desc_type, &d, desc_size);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error getting hub descriptor: %s\n", usb_strerror(status));
        return status;
    }

    LOG(INFO, "Detected a USB hub with %d ports\n", d.bNbrPorts);

    // Configure the hub depth
    if (USB_IS_SUPERSPEED(device->speed)) {
        usb_device_request_t req = {
            .bmRequestType = USB_RT_CLASS | USB_RT_H2D | USB_RT_DEV,
            .bRequest = USB_HUB_REQ_SET_HUB_DEPTH,
            .wValue = device->depth,
            .wIndex = 0,
            .wLength = 0
        };

        status = usb_request(device, &req, NULL);
        if (USB_ERROR(status)) {
            LOG(ERR, "Error setting hub depth: %s\n", usb_strerror(status));
            return status;
        }
    }

    // Create the hub internal
    hub_internal_t *internal = kmalloc(sizeof(hub_internal_t));
    SPINLOCK_INIT(&internal->lock);
    internal->intf = intf;
    internal->num_ports = d.bNbrPorts;
    internal->int_pipe = pipe;
    internal->bitmap = kzalloc(BITMAP_TO_SIZE(d.bNbrPorts+1));
    internal->hub = usb_allocateObject(USB_OBJECT_TYPE_HUB);
    intf->driver_priv = internal;

    // TODO: Make an API call for this
    assert(device->attached_hub == NULL);
    device->attached_hub = internal->hub;

    // Setup each port
    internal->hub->self = device;
    internal->hub->ports = kmalloc(sizeof(usb_port_t) * d.bNbrPorts);
    internal->hub->num_ports = d.bNbrPorts;

    for (unsigned i = 0; i < internal->hub->num_ports; i++) {
        usb_port_t *port = &internal->hub->ports[i];
        port->speed = device->speed;
        port->hub = internal->hub;
        port->device = NULL;
        port->number = i+1;
        port->state = USB_PORT_IDLE;
    }

    // The transfer must be sent but it must also not interfere with the existing hub transport

    // Create the interrupt transfer
    size_t sz = (d.bNbrPorts + 1 + 7) / 8;
    usb_transfer_t *transfer = usb_allocateTransfer(device);
    internal->intr_transfer = transfer;

    // Prepare it for sending
    usb_prepareTransfer(transfer, pipe, internal->bitmap, sz, USB_TRANSFER_ALLOW_SHORT, hub_processChanges, 0, internal);

    // Power on each port
    spinlock_acquireRaw(&internal->lock);
    for (unsigned i = 1; i <= internal->num_ports; i++) {
        usb_hub_port_status_t sts;
        status = hub_getPortStatus(device, i, &sts);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to get status for port %d: %s\n", i, usb_strerror(status));
            continue;
        }

        bool is_powered = false;
        if (USB_IS_SUPERSPEED(device->speed)) {
            is_powered = !!(sts.wPortStatus & USB_HUB_SS_STATUS_POWER);
        } else {
            is_powered = !!(sts.wPortStatus & USB_HUB_STATUS_POWER);
        }

        if (!is_powered) {
            LOG(DEBUG, "Powering port %d\n", i);
            status = hub_setFeature(device, i, USB_HUB_SEL_PORT_POWER);
            if (USB_ERROR(status)) {
                LOG(ERR, "Failed to power on port %d: %s\n", i, usb_strerror(status));
                continue;
            }
        }

        // bootstrap hub ports here
        if (sts.wPortChanged != 0) {
            hub_processChange(internal, i);
        } else if (sts.wPortStatus & USB_HUB_STATUS_CONNECTION) {
            hub_handleConnectionChange(internal, i, &sts);
        }
    } 

    spinlock_releaseRaw(&internal->lock);

    // Off it goes
    status = usb_transfer(transfer);
    if (USB_ERROR2(status)) {
        LOG(ERR, "Error starting interrupt transfer: %s\n", usb_strerror(status));
        // TODO: Cleanup
        return status;
    }


    return USB_SUCCESS;
}

/**
 * @brief Detach USB hub
 */
static usb_status_t hub_detach(usb_device_t *device, usb_interface_t *intf) {
    hub_internal_t *internal = intf->driver_priv;

    // the transfer will already be aborted + freed
    device->attached_hub = NULL;
    for (unsigned i = 0; i < internal->num_ports; i++) {
        usb_port_t *port = &internal->hub->ports[i];
        if (port->state == USB_PORT_CONNECTED) {
            usb_removeDevice(port->device);
        }
    }

    kfree(internal->hub->ports);
    kfree(internal->bitmap);
    usb_freeObject(USB_OBJECT_TYPE_HUB, internal->hub);
    usb_closePipe(internal->int_pipe);
    kfree(internal);
    return USB_SUCCESS;
}

/**
 * @brief Driver init
 */
int driver_init(int argc, char *argv[]) {
    usb_registerDriver(&hub_driver);
    return DRIVER_STATUS_SUCCESS;
}

/**
 * @brief Driver deinit
 */
int driver_deinit() {
    return DRIVER_STATUS_SUCCESS;
}

struct driver_metadata driver_metadata = {
    .name = "USB Hub Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
