/**
 * @file hexahedron/drivers/usb2/hid.c
 * @brief HID parser
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
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>
#include <kernel/init.h>

/* Drivers */
mutex_t hid_drivers_lock = MUTEX_INITIALIZER;
SLIST_HEAD(hid_drivers, hid_driver_t);

/* Devices */
mutex_t hid_devices_lock = MUTEX_INITIALIZER;
SLIST_HEAD(hid_devices, hid_device_t);

/* HID device driver */
static int hid_match(usb_device_t *dev, usb_interface_t *intf);
static usb_status_t hid_attach(usb_device_t *dev, usb_interface_t *intf);
static usb_status_t hid_detach(usb_device_t *dev, usb_interface_t *intf);
usb_driver_t hid_driver = {
    .name = "HID driver",
    .ops = {
        .match = hid_match,
        .attach = hid_attach,
        .detach = hid_detach
    }
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID", __VA_ARGS__)

/**
 * @brief Check match against driver
 */
static bool hid_compatible(hid_collection_t *collection, hid_driver_t *driver) {
    for (unsigned i = 0; i < driver->num_matches; i++) {
        hid_match_t *match = &driver->matches[i];

        if (match->usage_id != HID_MATCH_ANY_USAGE_ID) {
            if (collection->usage_id != match->usage_id) {
                continue;
            }
        }

        if (match->usage_page != HID_MATCH_ANY_USAGE_PAGE) {
            if (collection->usage_page != match->usage_page) {
                continue;
            }
        }

        bool ret = driver->ops.probe(collection);
        if (ret) {
            return true;
        }
    }

    return false;
}

/**
 * @brief HID match
 */
static int hid_match(usb_device_t *dev, usb_interface_t *intf) {
    if (intf == NULL) return USB_MATCH_NONE;

    if (intf->desc.bInterfaceClass == HID_CLASS) {
        return USB_MATCH_INTF_CLASS_GENERIC;
    }

    return USB_MATCH_NONE;
}

/**
 * @brief HID interrupt in callback
 */
static void hid_callback(usb_transfer_t *transfer) {
    if (USB_ERROR(transfer->status)) {
        LOG(ERR, "HID interrupt transfer failed: %s\n", usb_strerror(transfer->status));
        usb_freeTransfer(transfer);
        return;
    }
    
    hid_device_t *hid_dev = (hid_device_t*)transfer->priv;
    hid_process(hid_dev, transfer->buffer, transfer->actual_length);

    usb_prepareTransfer(hid_dev->transfer, hid_dev->in_pipe, hid_dev->buffer, transfer->length, USB_TRANSFER_ALLOW_SHORT, hid_callback, USB_NO_TIMEOUT, hid_dev);
    usb_status_t status = usb_transfer(transfer);
    if (USB_ERROR2(status)) {
        LOG(ERR, "Error restarting HID transfer: %s\n", usb_strerror(status));
        usb_freeTransfer(transfer);
    }
}

/**
 * @brief HID attach
 */
static usb_status_t hid_attach(usb_device_t *dev, usb_interface_t *intf) {
    LOG(INFO, "Starting HID attach\n");

    // Locate the HID descriptor
    // TODO: store these in device parser instead of this
    size_t full_length = intf->config->desc.wTotalLength;
    uint8_t *full_desc = kmalloc(full_length);
    usb_status_t status = usb_getDescriptor(dev, USB_DESC_CONF, intf->config->index, full_desc, full_length);
    if (USB_ERROR(status)) {
        kfree(full_desc);
        return status;
    }

    usb_hid_desc_t *hid_desc = NULL;
    uint8_t *desc_end = full_desc + full_length;
    uint8_t *desc_ptr = full_desc + intf->config->desc.bLength;
    bool target_interface = false;

    while (desc_ptr + sizeof(usb_descriptor_t) <= desc_end) {
        usb_descriptor_t *iter = (usb_descriptor_t*)desc_ptr;
        size_t remaining = desc_end - desc_ptr;

        if (iter->bLength < sizeof(usb_descriptor_t) || iter->bLength > remaining) {
            LOG(ERR, "Malformed descriptor while locating HID descriptor for interface %d\n", intf->desc.bInterfaceNumber);
            kfree(full_desc);
            return USB_BAD_DESCRIPTOR;
        }

        if (iter->bDescriptorType == USB_DESC_INTF) {
            if (iter->bLength < sizeof(usb_interface_desc_t)) {
                LOG(ERR, "Malformed interface descriptor while locating HID descriptor\n");
                kfree(full_desc);
                return USB_BAD_DESCRIPTOR;
            }

            usb_interface_desc_t *interface_desc = (usb_interface_desc_t*)iter;
            if (interface_desc->bInterfaceNumber == intf->desc.bInterfaceNumber) {
                if (interface_desc->bAlternateSetting == intf->desc.bAlternateSetting) {
                    target_interface = true;
                }
            }
        } else if (target_interface && iter->bDescriptorType == HID_DESC_TYPE) {
            if (iter->bLength < sizeof(usb_hid_desc_t)) {
                LOG(ERR, "Malformed HID descriptor for interface %d\n", intf->desc.bInterfaceNumber);
                kfree(full_desc);
                return USB_BAD_DESCRIPTOR;
            }

            usb_hid_desc_t *candidate = (usb_hid_desc_t*)iter;
            size_t hid_length = sizeof(usb_hid_desc_t) + candidate->bNumDescriptors * sizeof(usb_hid_optional_desc_t);
            if (candidate->bLength < hid_length) {
                LOG(ERR, "Truncated HID descriptor for interface %d\n", intf->desc.bInterfaceNumber);
                kfree(full_desc);
                return USB_BAD_DESCRIPTOR;
            }

            hid_desc = candidate;
            break;
        }

        desc_ptr += iter->bLength;
    }

    if (hid_desc == NULL) {
        LOG(ERR, "HID descriptor not found for interface %d alternate setting %d\n", intf->desc.bInterfaceNumber, intf->desc.bAlternateSetting);
        kfree(full_desc);
        return USB_INTERNAL_ERROR;
    }

    if (intf->desc.bInterfaceSubClass == 1) {
        // Currently in boot protocol
        usb_device_request_t req = {
            .bmRequestType = USB_RT_H2D | USB_RT_CLASS | USB_RT_INTF,
            .bRequest = HID_REQ_SET_PROTOCOL,
            .wValue = 1,
            .wIndex = intf->desc.bInterfaceNumber,
            .wLength = 0
        };

        status = usb_request(dev, &req, NULL);
        if (USB_ERROR(status)) {
            LOG(ERR, "HID_REQ_SET_PROTOCOL failed: %s\n", usb_strerror(status));
            kfree(full_desc);
            return status;
        }
    }

    // Create the HID device
    hid_device_t *hid_dev = kzalloc(sizeof(hid_device_t));
    STAILQ_INIT(&hid_dev->collections);
    hid_dev->intf = intf;
    hid_dev->device = dev;
    hid_dev->uses_report_id = false;
    intf->driver_priv = hid_dev;

    // parse the HID descriptor
    status = hid_parse(hid_dev, hid_desc);
    kfree(full_desc);
    if (USB_ERROR(status)) {
        // TODO: leaks memory!
        LOG(ERR, "Error parsing HID descriptor\n");
        kfree(hid_dev);
        return status;
    }
    
    LOG(INFO, "HID parser completed.\n");

    // find the endpoints
    usb_endpoint_t *intr_in_ep = NULL;
    usb_endpoint_t *intr_out_ep = NULL;

    for (unsigned i = 0; i < intf->num_endpoints; i++) {
        usb_endpoint_t *ep = intf->endpoints[i];
        if (USB_ENDP_TYPE(ep->desc.bmAttributes) == USB_ENDP_TYPE_INT) {
            if (USB_ENDP_DIRECTION(ep->desc.bEndpointAddress) == USB_ENDP_DIR_IN) {
                intr_in_ep = ep;
            } else {
                intr_out_ep = ep;
            }
        }
    }

    if (!intr_in_ep && !intr_out_ep) {
        LOG(ERR, "No INTERRUPT IN endpoint or INTERRUPT OUT endpoint?\n");
        LOG(ERR, "This could indicate a kernel bug!\n");
    }

    if (intr_in_ep != NULL) {
        status = usb_openPipe(intr_in_ep, USB_PIPE_DEFAULT, &hid_dev->in_pipe);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to open INTERRUPT IN pipe: %s\n", usb_strerror(status));
        } else {
            // prepare the interrupt transfer
            size_t mps = intr_in_ep->desc.wMaxPacketSize & 0x7FF;
            hid_dev->buffer = kmalloc(mps);
            hid_dev->transfer = usb_allocateTransfer(dev);
            usb_prepareTransfer(hid_dev->transfer, hid_dev->in_pipe, hid_dev->buffer, mps, USB_TRANSFER_ALLOW_SHORT, hid_callback, USB_NO_TIMEOUT, hid_dev);
            
            status = usb_transfer(hid_dev->transfer);
            if (USB_ERROR2(status)) {
                LOG(ERR, "Error starting INTERRUPT IN transfer: %s\n", usb_strerror(status));
                usb_freeTransfer(hid_dev->transfer);
                kfree(hid_dev->buffer);
                hid_dev->buffer = NULL;
                hid_dev->transfer = NULL;
            }
        }
    }

    if (intr_out_ep != NULL) {
        status = usb_openPipe(intr_out_ep, USB_PIPE_DEFAULT, &hid_dev->out_pipe);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to open INTERRUPT OUT pipe: %s\n", usb_strerror(status));
        }
    }

    // Device lock is held by hold register and this
    mutex_acquire(&hid_devices_lock);

    // Insert into device list
    SLIST_INSERT_HEAD(&hid_devices, hid_dev, node);
    
    // Attempt to locate a driver for each collection
    hid_collection_t *col = STAILQ_FIRST(&hid_dev->collections);
    while (col) {
        // Only appliaction collections can be bound
        if (col->type != HID_COLLECTION_TYPE_APPLICATION) {
            goto _next_collection;
        }

        // Locate a suitable driver for the collection
        mutex_acquire(&hid_drivers_lock);

        hid_driver_t *driver = SLIST_FIRST(&hid_drivers);
        while (driver) {
            if (hid_compatible(col, driver)) {
                // TODO: prioritization n stuff
                break;
            }

            driver = SLIST_NEXT(driver, node);
        }

        mutex_release(&hid_drivers_lock);

        if (driver == NULL) {
            // No driver found for this collection
            goto _next_collection;
        }

        // Driver located, attach it to the device
        LOG(INFO, "Attaching driver \"%s\" to collection\n", driver->name);
        driver->ops.attach(col);
        
        col->driver = driver;

    _next_collection:
        col = STAILQ_NEXT(col, node);
    }
    mutex_release(&hid_devices_lock);

    return USB_SUCCESS;
}

/**
 * @brief Free report
 */
static void hid_freeReport(hid_report_t *r) {
    for (unsigned i = 0; i < r->num_fields; i++) {
        hid_field_t *f = &r->fields[i];
        kfree(f->last_state);
        kfree(f->current_state);
    }

    kfree(r->fields);
    kfree(r);
}

/**
 * @brief HID detach
 */
static usb_status_t hid_detach(usb_device_t *usbdev, usb_interface_t *intf) {
    hid_device_t *dev = intf->driver_priv;
    
    // !!! lol, racey
    mutex_acquire(&hid_devices_lock);
    SLIST_REMOVE(&hid_devices, dev, hid_device_t, node);
    mutex_release(&hid_devices_lock);
    
    if (dev->in_pipe) {
        usb_closePipe(dev->in_pipe);
    }

    if (dev->out_pipe) {
        usb_closePipe(dev->out_pipe);
    }

    if (dev->buffer) {
        kfree(dev->buffer);
    }

    hid_collection_t *col = SLIST_FIRST(&dev->collections);
    while (col) {
        if (col->driver) col->driver->ops.remove(col);
        hid_collection_t *nxt = SLIST_NEXT(col, node);
        kfree(col);
        col = nxt;
    }

    for (unsigned i = 0; i < 256; i++) {
        if (dev->input_reports[i]) hid_freeReport(dev->input_reports[i]);
        if (dev->output_reports[i]) hid_freeReport(dev->output_reports[i]);
        if (dev->feature_reports[i]) hid_freeReport(dev->feature_reports[i]);
    }

    return USB_SUCCESS;
}

/**
 * @brief Register an HID device driver
 * @param driver The driver to register
 */
void hid_register(hid_driver_t *driver) {
    mutex_acquire(&hid_drivers_lock);
    SLIST_INSERT_HEAD(&hid_drivers, driver, node);
    mutex_release(&hid_drivers_lock);

    // Try to attach collections to the driver
    mutex_acquire(&hid_devices_lock);
    hid_device_t *dev = SLIST_FIRST(&hid_devices);
    while (dev != NULL) {
        hid_collection_t *col = STAILQ_FIRST(&dev->collections);
        while (col != NULL) {
            if (col->type != HID_COLLECTION_TYPE_APPLICATION || col->driver != NULL) {
                // Collection is not application or already has driver (prio not impl)
                goto _next_collection;
            }

            if (hid_compatible(col, driver) == true) {
                LOG(INFO, "Attaching driver \"%s\" to collection\n", driver->name);
                driver->ops.attach(col);
                col->driver = driver;
            }

        _next_collection:
            col = STAILQ_NEXT(col, node);
        }

        dev = SLIST_NEXT(dev, node);
    }
    mutex_release(&hid_devices_lock);
}

/**
 * @brief Initialize HID subsystem
 */
static int hid_init() {
    usb_registerDriver(&hid_driver);
    return 0;
}

KERN_EARLY_INIT_ROUTINE(hid, INIT_FLAG_DEFAULT, hid_init, usb);
