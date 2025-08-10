/**
 * @file hexahedron/drivers/usb/dev.c
 * @brief USB device handler
 * 
 * Handles initialization and requests between controllers and devices
 * 
 * @warning A bit of messy code lingers here. A full API for drivers will likely not be exposed in this file
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/dev.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/fs/kernelfs.h>
#include <string.h>
#include <kernel/misc/util.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:DEV", __VA_ARGS__)

/* Last device ID */
static volatile uint32_t usb_last_device_id = 0;

/**
 * @brief Create a new USB device structure for initialization
 * @param controller The controller
 * @param port The device port
 * @param speed The device speed
 * 
 * @param shutdown The HC device shutdown method
 * @param control The HC control request method
 * @param interrupt The HC interrupt request method
 */
USBDevice_t *usb_createDevice(USBController_t *controller, uint32_t port, int speed, hc_shutdown_t shutdown, hc_control_t control, hc_interrupt_t interrupt) {
    USBDevice_t *dev = kmalloc(sizeof(USBDevice_t));
    memset(dev, 0, sizeof(USBDevice_t));

    dev->c = controller;
    dev->shutdown = shutdown;
    dev->control = control;
    dev->interrupt = interrupt;
    dev->port = port;
    dev->speed = speed;

    // By default, during initialization the USB device expects to receive an address of 0
    dev->address = 0;

    dev->config_list = list_create("usb config list");

    return dev;
}

/**
 * @brief Destroy a USB device
 * @param controller The controller
 * @param dev The device to destroy
 * 
 * @warning Does not shut the device down, just frees it from memory
 */
void usb_destroyDevice(USBController_t *controller, USBDevice_t *dev) {
    if (!controller || !dev) return;

    if (dev->langs) kfree((void*)dev->langs);

    // TODO: This part needs a cleanup
    if (dev->config_list) {
        foreach(conf_node, dev->config_list) {
            USBConfiguration_t *config = (USBConfiguration_t*)conf_node->value;
            if (!config) continue;

            // If the interface list wasn't created, don't free it
            if (!config->interface_list) {
                kfree(config);
                kfree(conf_node);
                continue;
            }

            foreach(intf_node, config->interface_list) {
                USBInterface_t *interface = (USBInterface_t*)intf_node->value;
                if (!interface) {
                    kfree(intf_node);
                    continue;
                }

                if (!interface->endpoint_list) {
                    if (interface->driver) {
                        if (interface->driver->name) kfree(interface->driver->name);
                        kfree(interface->driver);
                    }
                    kfree(interface);
                    kfree(intf_node);
                    continue;
                }

                foreach(endp_node, interface->endpoint_list) {
                    USBEndpoint_t *endp = (USBEndpoint_t*)endp_node->value;
                    if (!endp) {
                        kfree(endp_node);
                        continue;
                    }

                    kfree(endp);
                    kfree(endp_node);
                }

                if (interface->driver) {
                    if (interface->driver->name) kfree(interface->driver->name);
                    kfree(interface->driver);
                }

                kfree(interface->endpoint_list);
                kfree(interface);
                kfree(intf_node);
            }

            kfree(config->interface_list);
            kfree(config);
            kfree(conf_node);
        }

        kfree(dev->config_list);
    }

    if (dev->address) {
        // !!!: Need to find a way to free and reclaim addresses
    }

    // Delete and free the device
    if (controller->devices) {
        node_t *dev_node = list_find(controller->devices, dev);
        if (dev_node) list_delete(controller->devices, dev_node);
    }

    kfree(dev);
}

/**
 * @brief USB device request method
 * 
 * @param device The device
 * @param type The request type (should have a direction, type, and recipient - bmRequestType)
 * @param request The request to send (USB_REQ_... - bRequest)
 * @param value Optional parameter to the request (wValue)
 * @param index Optional index for the request (this field differs depending on the recipient - wIndex)
 * @param length The length of the data (wLength)
 * @param data The data
 * 
 * @returns The request status, in terms of @c USB_TRANSFER_xxx
 */
int usb_requestDevice(USBDevice_t *device, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data) {
    if (!device) return -1;

    // Create a new device request
    USBDeviceRequest_t *req = kmalloc(sizeof(USBDeviceRequest_t));
    req->bmRequestType = type;
    req->bRequest = request;
    req->wIndex = index;
    req->wValue = value;
    req->wLength = length;

    // Create a new transfer
    USBTransfer_t *transfer = kmalloc(sizeof(USBTransfer_t));
    transfer->req = req;
    transfer->endpoint = 0;
    transfer->status = USB_TRANSFER_IN_PROGRESS;
    transfer->length = length;
    transfer->data = data;
    
    // Now send the device control request
    if (device->control) device->control(device->c, device, transfer);

    // Cleanup and return whether the transfer was successful
    int status = transfer->status;
    kfree(transfer);
    kfree(req);
    return status;
}

/**
 * @brief Read a string from the USB device
 * @param dev The device to read the string from
 * @param idx The index of the string to read
 * @param lang The language code
 * @returns ASCII string (converted from the normal unicode) or NULL if we failed to get the descriptor
 */
static char *usb_getStringIndex(USBDevice_t *device, int idx, uint16_t lang) {
    if (idx == 0) {
        // String index #0 is reserved for languages - this usually means that a driver attempted to get a nonexistant string ID
        LOG(WARN, "Tried to access string ID #0 - nonfatal\n");
        return NULL;
    }

    // Request the descriptor
    uint8_t bLength;

    if (usb_requestDevice(device, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
                USB_REQ_GET_DESC, (USB_DESC_STRING << 8) | idx, lang, 1, &bLength) != USB_TRANSFER_SUCCESS)
    {
        LOG(WARN, "Failed to get string index %i for device\n", idx);
        return NULL;
    }


    // Now read the full descriptor
    USBStringDescriptor_t *desc = kmalloc(bLength);

    if (usb_requestDevice(device, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
                USB_REQ_GET_DESC, (USB_DESC_STRING << 8) | idx, lang, bLength, desc) != USB_TRANSFER_SUCCESS)
    {
        LOG(WARN, "Failed to get string index %i for device\n", idx);
        kfree(desc);
        return NULL;
    }

    // Convert it to ASCII
    char *string_output = kmalloc(((bLength - 2) / 2) + 1); // Subtract 2 for the 2 extra descriptor bytes and divide by 2 for short -> byte conversion, with one extra for null termination
    memset(string_output, 0, ((bLength - 2) / 2) + 1);

    for (int i = 0; i < (bLength - 2); i += 2) {
        string_output[i/2] = desc->bString[i];
    }

    kfree(desc);

    return string_output;
}

/**
 * @brief Read model of device
 */
static int usb_readModelFS(kernelfs_entry_t *ent, void *d) {
    USBDevice_t *dev = (USBDevice_t*)d;
    char *product_str = usb_getStringIndex(dev, dev->device_desc.iProduct, dev->chosen_language);
    
    if (product_str) {
        kernelfs_writeData(ent, "%s\n", product_str);
    }

    return 0;
}

/**
 * @brief Create the KernelFS node for a device
 * @param dev The device to create the KernelFS node for
 */
static int usb_createKernelFS(USBDevice_t *dev) {
    char name[64] = { 0 };
    snprintf(name, 64, "%d-%d", dev->c->id, dev->dev_id);
    dev->dir = kernelfs_createDirectory(usb_kernelfs, name, 1);
    kernelfs_createEntry(dev->dir, "model", usb_readModelFS, (void*)dev);
    return 0;
}


/**
 * @brief Request a configuration
 * @param dev The device
 * @param idx The index of the configuration
 * 
 * 
 * @note This will also allocate and request all interfaces and their endpoints
 */ 
USBConfiguration_t *usb_getConfigurationFromIndex(USBDevice_t *dev, int index) {
    // Read the descriptor's bare minimum in
    USBConfigurationDescriptor_t config_temp;
    if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
            USB_REQ_GET_DESC, (USB_DESC_CONF << 8) | index, 0, sizeof(USBConfigurationDescriptor_t), &config_temp) != USB_TRANSFER_SUCCESS)
    {
        LOG(ERR, "Device initialization failed - could not get configuration for index %i\n", index);
        return NULL;
    }

    // Create the configuration
    USBConfiguration_t *config = kmalloc(sizeof(USBConfiguration_t));
    config->index = index;
    config->interface_list = list_create("usb interface list");

    // Now we can read the full descriptor
    USBConfigurationDescriptor_t *config_full = kmalloc(config_temp.wTotalLength);
    if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
            USB_REQ_GET_DESC, (USB_DESC_CONF << 8) | index, 0, config_temp.wTotalLength, config_full) != USB_TRANSFER_SUCCESS)
    {
        LOG(ERR, "Device initialization failed - could not get configuration for index %i\n", index);
        kfree(config);
        kfree(config_full);
        return NULL;
    }

    // Copy part of the descriptor into the USBConfiguration
    memcpy((void*)&config->desc, (void*)config_full, sizeof(USBConfigurationDescriptor_t));

    // Now start iterating through each descriptor
    uint8_t *buffer = ((uint8_t*)config_full + config_full->bLength);
    uint8_t *buffer_end = ((uint8_t*)config_full + config_full->wTotalLength);


    while (buffer < buffer_end) {
        // The first two bytes of every descriptor are always bLength and bDescriptorType.
        // Byte #2 will correspond to the type - it can be either INTF or ENDP  
        // TODO: Clean this up
        if (buffer[1] == USB_DESC_INTF) {
            USBInterface_t *interface = kmalloc(sizeof(USBInterface_t));
            memset(interface, 0, sizeof(USBInterface_t));

            // Setup values
            memcpy((void*)&interface->desc, buffer, sizeof(USBInterfaceDescriptor_t));
            interface->endpoint_list = list_create("usb endpoint list");
            interface->additional_desc_list = list_create("usb additional descriptor list");
            interface->dev = dev;
            list_append(config->interface_list, (void*)interface);

            LOG(INFO, "This interface has %i available endpoints, with class 0x%x subclass 0x%x\n", interface->desc.bNumEndpoints+1, interface->desc.bInterfaceClass, interface->desc.bInterfaceSubClass);

            // Push buffer ahead
            buffer += interface->desc.bLength;
        } else if (buffer[1] == USB_DESC_ENDP) {
            if (config->interface_list->tail) {
                USBInterface_t *intf = (USBInterface_t*)(config->interface_list->tail->value); 
                USBEndpointDescriptor_t *endpoint_desc = (USBEndpointDescriptor_t*)buffer;
                USBEndpoint_t *endp = kmalloc(sizeof(USBEndpoint_t));
                memcpy((void*)&endp->desc, (void*)endpoint_desc, sizeof(USBEndpointDescriptor_t));
                list_append(intf->endpoint_list, (void*)endp);
            }

            buffer += buffer[0];
        } else {
            if (config->interface_list->tail) {
                USBInterface_t *intf = (USBInterface_t*)(config->interface_list->tail->value); 
                
                void *intf_buffer = kmalloc(buffer[0]);
                memcpy(intf_buffer, buffer, buffer[0]);

                list_append(intf->additional_desc_list, intf_buffer);
                HEXDUMP(buffer, buffer[0]);
            } else {
                LOG(WARN, "Unrecognized descriptor type while parsing configuration: 0x%x - assuming class specific descriptor\n", buffer[1]);
            }


            buffer += buffer[0];
        }
    } 


    return config;
}

/**
 * @brief Initialize a USB device and assign to the USB controller's list of devices
 * @param dev The device to initialize
 * 
 * @todo This is a bit hacky
 * 
 * @note If this fails, please call @c usb_destroyDevice to cleanup
 * @returns USB_FAILURE on failure and USB_SUCCESS on success
 */
USB_STATUS usb_initializeDevice(USBDevice_t *dev) {
    LOG(DEBUG, "Initializing USB device on port 0x%x...\n", dev->port);

    // Get first few bytes of the device descriptor
    // TODO: Bochs requests that this have a size equal the mps of a device - implement this
    if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
                    USB_REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0, 8, &dev->device_desc) != USB_TRANSFER_SUCCESS) 
    {
        // The request did not succeed
        LOG(ERR, "USB_REQ_GET_DESC did not succeed\n");
        return USB_FAILURE;
    }

    LOG(DEBUG, "bMaxPacketSize0: %d\n", dev->device_desc.bMaxPacketSize0);
        
    // Set the maximum packet size
    dev->mps = dev->device_desc.bMaxPacketSize0;

    if (dev->evaluate) {
        // Evaluate the endpoint context (and update the internal Input Context mps)
        if (dev->evaluate(dev->c, dev) != USB_SUCCESS) {
            LOG(ERR, "Device initialization failed - Evaluate command did not succeed\n");
            return 1;
        }
    }

    if (!dev->setaddr && !dev->evaluate) {
        // Get an address for it 
        uint32_t address = dev->c->last_address;
        dev->c->last_address++;

        // Request it to set that address
        if (usb_requestDevice(dev, USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEV,
                        USB_REQ_SET_ADDR, address, 0, 0, NULL) != USB_TRANSFER_SUCCESS)
        {
            // The request did not succeed
            LOG(ERR, "Device initialization failed - USB_REQ_SET_ADDR 0x%x did not succeed\n", address);
            return USB_FAILURE;
        }

        // Allow the device a 20ms recovery time
        clock_sleep(20);

        dev->address = address;
    }

    // Now we can read the whole descriptor
    if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
                USB_REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0, sizeof(USBDeviceDescriptor_t), &dev->device_desc) != USB_TRANSFER_SUCCESS) 
    {
        // The request did not succeed
        LOG(ERR, "Device initialization failed - failed to read full descriptor\n");
        return USB_FAILURE;
    }


    LOG(DEBUG, "USB Device: Version %d.%d, VID 0x%04x, PID 0x%04x PROTOCOL 0x%04x\n", dev->device_desc.bcdUSB >> 8, (dev->device_desc.bcdUSB >> 4) & 0xF, dev->device_desc.idVendor, dev->device_desc.idProduct, dev->device_desc.bDeviceProtocol);

    if (!dev->device_desc.bNumConfigurations) {
        LOG(ERR, "Device initialization failed - corrupted device descriptor (bNumConfigurations = 0)\n");
        return USB_FAILURE;
    }

    // Add it to the device list of the controller
    list_append(dev->c->devices, dev);

    // // Read the language IDs supported by this device
    // uint8_t lang_length;

    // // We need bLength so only read 1 byte
    // if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV, 
    //             USB_REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0, 1, &lang_length) != USB_TRANSFER_SUCCESS) 
    // {
    //     LOG(ERR, "Device initialization failed - could not get language codes\n");
    //     return USB_FAILURE;
    // }

    // // Now we can read the full descriptor
    // dev->langs = kmalloc(lang_length);

    // if (usb_requestDevice(dev, USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEV,
    //             USB_REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0, lang_length, dev->langs) != USB_TRANSFER_SUCCESS)
    // {
    //     LOG(ERR, "Device initialization failed - could not get all language codes\n");
    //     return USB_FAILURE;
    // }

    // for (int i = 0; i < (dev->langs->bLength - 2) / 2; i++) {
    //     LOG(DEBUG, "Supports language code: 0x%02x\n", dev->langs->wLangID[i]);
    //     if (dev->langs->wLangID[i] & USB_LANGID_ENGLISH) {
    //         // We don't support Unicode yet
    //         dev->chosen_language = dev->langs->wLangID[i];
    //     }
    // }

    dev->chosen_language = 0x409;

    // Done! We've got the device language codes. Now we've unlocked usb_getStringIndex
    char *product_str = usb_getStringIndex(dev, dev->device_desc.iProduct, dev->chosen_language);

    if (!product_str && dev->device_desc.iProduct) {
        // We aren't accessing string index 0 and transfer failed
        LOG(ERR, "Device initialization failed - could not read product string when it is present\n");
        return USB_FAILURE;
    }

    char *vendor_str = usb_getStringIndex(dev, dev->device_desc.iManufacturer, dev->chosen_language);  
    
    if (!vendor_str && dev->device_desc.iManufacturer) {
        // We aren't accessing string index 0 and transfer failed
        LOG(ERR, "Device initialization failed - could not read manufacturer string when it is present\n");
        return USB_FAILURE;
    }
    
    char *serial_number = usb_getStringIndex(dev, dev->device_desc.iSerialNumber, dev->chosen_language);
    
    if (!serial_number && dev->device_desc.iSerialNumber) {
        // We aren't accessing string index 0 and transfer failed
        LOG(ERR, "Device initialization failed - could not read SN string when it is present\n");
        return USB_FAILURE;
    }

    // Now we need to finish configuring the device - this involves picking a configuration, interface, and endpoint.
    // We can get the configuration by using bNumConfigurations from the device descriptor
    for (uint32_t conf = 0; conf < dev->device_desc.bNumConfigurations; conf++) {
        // Get the configuration
        USBConfiguration_t *config = usb_getConfigurationFromIndex(dev, conf);
        if (!config) break;

        // Get string of the configuration
        char *conf_str = usb_getStringIndex(dev, config->desc.iConfiguration, dev->chosen_language);
        LOG(INFO, "Configuration '%s' available (%i)\n", conf_str, conf);
        if (conf_str) {
            kfree(conf_str);
        }

        list_append(dev->config_list, (void*)config); // Add to the list
    }

    // TODO: We're just picking the first configuration we can find!
    dev->config = (USBConfiguration_t*)dev->config_list->head->value;
    if (!dev->config) {
        LOG(ERR, "No configurations?? KERNEL BUG!\n");
        return USB_FAILURE;
    }

    char *conf_str = usb_getStringIndex(dev, dev->config->desc.iConfiguration, dev->chosen_language);
    LOG(INFO, "Selected configuration '%s'\n", conf_str);
    if (conf_str) kfree(conf_str); 

    // Now send the device the request to set its configuration
    if (usb_requestDevice(dev, USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEV,
                USB_REQ_SET_CONF, dev->config->index, 0, 0, NULL) != USB_TRANSFER_SUCCESS)
    {
        LOG(ERR, "USB initialization failed - could not set configuration\n");
        return USB_FAILURE;
    }

    // Create KernelFS
    dev->dev_id = usb_last_device_id++;
    usb_createKernelFS(dev);

    // Try to find a driver
    usb_initializeDeviceDriver(dev);

    // All done!
    LOG(INFO, "Initialized USB device '%s' from '%s' (SN %s)\n", product_str, vendor_str, serial_number);
    if (product_str) kfree(product_str);
    if (vendor_str) kfree(vendor_str);
    if (serial_number) kfree(serial_number);

    return USB_SUCCESS;
}

/**
 * @brief Deinitialize a USB device
 * @param dev The device to deinitialize
 * 
 * @note This WILL NOT free the memory of the device. Call @c usb_destroyDevice after this.
 */
USB_STATUS usb_deinitializeDevice(USBDevice_t *dev) { 
    if (!dev) return USB_FAILURE;

    // We need to find all interfaces with a registered device driver
    foreach(conf_node, dev->config_list) {
        USBConfiguration_t *conf = (USBConfiguration_t*)conf_node->value;
        if (!conf) continue;

        foreach(intf_node, conf->interface_list) {
            USBInterface_t *intf = (USBInterface_t*)intf_node->value;
            if (!intf || !intf->driver || !intf->driver->dev_deinit) continue;

            if (intf->driver->dev_deinit(intf) != USB_SUCCESS) {
                LOG(WARN, "Driver '%s' failed to deinitialize\n", intf->driver->name);
            }
        }
    }

    if (dev->shutdown) {
        dev->shutdown(dev->c, dev);
    }

    return USB_SUCCESS;
}