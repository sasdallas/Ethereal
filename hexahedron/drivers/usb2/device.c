/**
 * @file hexahedron/drivers/usb2/device.c
 * @brief Handles device initialization
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
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:DEVICE", __VA_ARGS__)

/**
 * @brief Probe a specific configuration
 * @param dev The device to probe on
 * @param config The configuration to probe
 */
static usb_status_t usb_probeConfiguration(usb_device_t *dev, usb_configuration_t *config) {
    size_t full_length = config->desc.wTotalLength;

    if (full_length <= sizeof(usb_config_desc_t)) {
        // configuration has no interfaces
        return USB_SUCCESS;
    }

    // Read the full descriptor into RAM
    uint8_t *full_desc = kmalloc(full_length);
    usb_status_t status = usb_getDescriptor(dev, USB_DESC_CONF, config->index, full_desc, full_length);
    if (USB_ERROR(status)) {
        kfree(full_desc);
        return status;
    }

    // Allocate an array for the interfaces, assuming that bNumInterfaces is the maximum
    // Assertions will catch if it isnt
    config->interfaces = kmalloc(sizeof(usb_interface_t*) * config->desc.bNumInterfaces);
    config->num_interfaces = 0;

    usb_descriptor_t *iter = (usb_descriptor_t*)(full_desc + config->desc.bLength);
    usb_interface_t *interface = NULL;
    while ((uint8_t*)iter < (full_desc + full_length)) {
        if (iter->bDescriptorType == USB_DESC_INTF) {
            usb_interface_desc_t *desc = (usb_interface_desc_t*)iter;
            if (desc->bAlternateSetting != 0) {
                LOG(INFO, "Ignoring interface descriptor (bAlternateSetting = %d) as it is unsupported\n", desc->bAlternateSetting);
                interface = NULL;
                goto _next_descriptor;
            }

            if (config->num_interfaces >= config->desc.bNumInterfaces) {
                LOG(ERR, "Extra interface class=0x%x subclass=0x%x protocol=0x%x endpoints=%d (BAD)\n", desc->bInterfaceClass, desc->bInterfaceSubClass, desc->bInterfaceProtocol, desc->bNumEndpoints);
                interface = NULL;
                goto _next_descriptor;
            }

            LOG(INFO, "Found interface class=0x%x subclass=0x%x protocol=0x%x endpoints=%d\n", desc->bInterfaceClass, desc->bInterfaceSubClass, desc->bInterfaceProtocol, desc->bNumEndpoints);

            interface = usb_allocateObject(USB_OBJECT_TYPE_INTERFACE);
            memcpy(&interface->desc, desc, min(desc->bLength, sizeof(usb_interface_desc_t)));
            interface->device = dev;
            interface->config = config;
            interface->num_endpoints = 0;
            interface->endpoints = kmalloc(sizeof(usb_endpoint_t*) * desc->bNumEndpoints);
            interface->driver = NULL;

            config->interfaces[config->num_interfaces++] = interface;
        } else if (iter->bDescriptorType == USB_DESC_ENDP) {
            usb_endpoint_desc_t *desc = (usb_endpoint_desc_t*)iter;
            if (interface == NULL) {
                LOG(WARN, "Orphaned endpoint with address=0x%x attributes=0x%x max packet size=0x%x\n", desc->bEndpointAddress, desc->bmAttributes, desc->wMaxPacketSize);
                goto _next_descriptor;
            }
            
            if (interface->num_endpoints >= interface->desc.bNumEndpoints) {
                LOG(ERR, "\tExtra endpoint with address=0x%x attributes=0x%x max packet size=0x%x (BAD)\n", desc->bEndpointAddress, desc->bmAttributes, desc->wMaxPacketSize);
                goto _next_descriptor;
            }

            LOG(INFO, "\tFound endpoint with address=0x%x attributes=0x%x max packet size=0x%x\n", desc->bEndpointAddress, desc->bmAttributes, desc->wMaxPacketSize);

            usb_endpoint_t *endp = usb_allocateObject(USB_OBJECT_TYPE_ENDPOINT);
            memcpy(&endp->desc, desc, min(sizeof(usb_endpoint_desc_t), desc->bLength));
            endp->interface = interface;
            endp->mps = desc->wMaxPacketSize;
            endp->pipe = NULL;
            endp->toggle = 0;

            interface->endpoints[interface->num_endpoints++] = endp;
        } else {
            LOG(INFO, "Found vendor-specific descriptor with descriptor type 0x%x\n", iter->bDescriptorType);
        }

    _next_descriptor:
        iter = USB_DESC_NEXT(iter);
    }

    return USB_SUCCESS;
}

/**
 * @brief Probe device configurations
 * @param dev The device to probe
 */
static usb_status_t usb_probeDeviceConfigurations(usb_device_t *dev) {
    if (dev->desc.bNumConfigurations == 0) {
        LOG(DEBUG, "Device has no configurations\n");
        return USB_SUCCESS;
    }

    dev->configs = kzalloc(sizeof(usb_configuration_t*) * dev->desc.bNumConfigurations);
    dev->num_configs = 0;

    for (unsigned i = 0; i < dev->desc.bNumConfigurations; i++) {
        usb_configuration_t *config = usb_allocateObject(USB_OBJECT_TYPE_CONFIGURATION);
        config->index = i;

        usb_status_t status = usb_getDescriptor(dev, USB_DESC_CONF, i, &config->desc, sizeof(usb_config_desc_t));
        if (USB_ERROR(status)) {
            LOG(ERR, "Error reading config %d descriptor: %s\n", i, usb_strerror(status));
            usb_freeObject(USB_OBJECT_TYPE_CONFIGURATION, config);

            // TODO: if one config fails to read dont scrap entire device
            return status;
        }

        LOG(DEBUG, "Configuration %d has %d interfaces\n", i, config->desc.bNumInterfaces);
    
        status = usb_probeConfiguration(dev, config);
        if (USB_ERROR(status)) {
            LOG(ERR, "Error probing config %d: %s\n", i, usb_strerror(status));
            usb_freeObject(USB_OBJECT_TYPE_CONFIGURATION, config);

            // TODO: if one config fails to read dont scrap entire device
            return status;
        }
        
        dev->configs[dev->num_configs++] = config;
    }

    return USB_SUCCESS;
}

/**
 * @brief Choose a configuration for the device
 * @param dev The device to select for
 * @note This is a naive approach. Need to extend driver framework
 */
static usb_status_t usb_chooseConfiguration(usb_device_t *dev) {
    if (dev->num_configs == 0) return USB_SUCCESS;
    usb_configuration_t *config = dev->configs[0];

    usb_status_t status = usb_setConfiguration(dev, config);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to select configuration 0: %s\n", usb_strerror(status));
        return status;
    }

    if (config->desc.iConfiguration) {
        char buf[USB_MAX_STRING_LENGTH];
        if (usb_getString(dev, config->desc.iConfiguration, buf) == USB_SUCCESS) {
            LOG(DEBUG, "Selected configuration 0: %s\n", buf);
        } else {
            LOG(DEBUG, "Selected configuration 0 (failed to get string)\n");
        }
    } else {
        LOG(DEBUG, "Selected configuration 0 on device successfully\n");
    }

    return USB_SUCCESS;
}

/**
 * @brief Initialize a device
 * @param dev The device to initialize
 */
usb_status_t usb_initializeDevice(usb_device_t *dev) {
    usb_bus_t *bus = dev->bus;

    // Read the device descriptor's first 8 bytes to determine actual mps
    usb_status_t status = usb_getDescriptor(dev, USB_DESC_DEVICE, 0, &dev->desc, 8);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error reading device descriptor: %s\n", usb_strerror(status));
        return status;
    }

    LOG(DEBUG, "bLength = %d bDescriptorType = 0x%x bMaxPacketSize0 = %d\n", dev->desc.bLength, dev->desc.bDescriptorType, dev->desc.bMaxPacketSize0);
    
    // Request the bus to configure endpoint 0 now
    if (USB_IS_SUPERSPEED(dev->speed)) {
        dev->control_ep.mps = 1u << dev->desc.bMaxPacketSize0;
    } else {
        dev->control_ep.mps = dev->desc.bMaxPacketSize0;
    }


    if (bus->ops->configure_control) {
        status = bus->ops->configure_control(bus, dev);
        if (USB_ERROR(status)) {
            LOG(INFO, "Failed to configure control endpoint: %s\n", usb_strerror(status));
            return status;
        }
    }

    // Address device
    if (bus->ops->address_device) {
        status = bus->ops->address_device(bus, dev);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to address device: %s\n", usb_strerror(status));
            return status;
        }
    } else {
        mutex_acquire(&bus->lock);
        int bit = bitmap_find_first(bus->address_map, USB_MAX_ADDRESS);
        if (bit == -1) {
            assert(0 && "out of USB device addresses, this is probably a bug");
        }

        bitmap_set(bus->address_map, bit);
        mutex_release(&bus->lock);

        // Issue a set address request to the device
        usb_device_request_t req = {
            .bmRequestType = USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEV,
            .bRequest = USB_REQ_SET_ADDR,
            .wIndex = 0,
            .wValue = bit+1,
            .wLength = 0
        };

        status = usb_request(dev, &req, NULL);
        if (USB_ERROR(status)) {
            // !!! leaking the address bit
            LOG(ERR, "Failed to issue SET_ADDRESS for address=%2x: %s\n", bit+1, usb_strerror(status));
            return status;
        }

        dev->address = bit+1;
    }

    // This device is now addressed
    dev->state = USB_DEVICE_ADDRESSED;

    // Read the full device descriptor
    assert(dev->desc.bLength >= sizeof(usb_device_desc_t));
    status = usb_getDescriptor(dev, USB_DESC_DEVICE, 0, &dev->desc, sizeof(usb_device_desc_t));
    if (USB_ERROR(status)) {
        LOG(ERR, "Error reading device descriptor (phase 2): %s\n", usb_strerror(status));
        return status;
    }

    LOG(INFO, "Detected a USB device with vendor ID %04x product ID %04x\n", dev->desc.idVendor, dev->desc.idProduct);

    // TODO
    dev->langid = USB_LANGID_ENGLISH;

    char buffer[USB_MAX_STRING_LENGTH];

    if (dev->desc.iProduct) {
        status = usb_getString(dev, dev->desc.iProduct, buffer);
        if (USB_ERROR(status)) {
            LOG(ERR, "Error reading product string descriptor: %s\n", usb_strerror(status));
        } else {
            LOG(INFO, "Device: %s\n", buffer);
        }
    }

    if (dev->desc.iManufacturer) {
        status = usb_getString(dev, dev->desc.iManufacturer, buffer);
        if (USB_ERROR(status)) {
            LOG(ERR, "Error reading manufacturer string descriptor: %s\n", usb_strerror(status));
        } else {
            LOG(INFO, "Manufacturer: %s\n", buffer);
        }
    }

    if (dev->desc.iSerialNumber) {
        status = usb_getString(dev, dev->desc.iSerialNumber, buffer);
        if (USB_ERROR(status)) {
            LOG(ERR, "Error reading serial number string descriptor: %s\n", usb_strerror(status));
        } else {
            LOG(INFO, "Serial number: %s\n", buffer);
        }
    }

    // Start probing configurations
    status = usb_probeDeviceConfigurations(dev);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error probing for configurations.\n");
        return status;
    }

    // Select the best configuration
    status = usb_chooseConfiguration(dev);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error selecting configuration\n");
        return status;
    }

    // Attach a driver to the device
    usb_attachDriver(dev);

    LOG(INFO, "Device initialization finished\n");
    return USB_SUCCESS;
}

