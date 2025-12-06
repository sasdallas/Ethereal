/**
 * @file hexahedron/drivers/usb/usb.c
 * @brief Main USB interface
 * 
 * This USB interface handles basic controller stuff such as registering controllers
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/hid/hid.h>
#include <kernel/drivers/clock.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <kernel/fs/kernelfs.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB", __VA_ARGS__)

/* List of USB controllers */
list_t *usb_controller_list = NULL;

/* USB kernelfs node */
kernelfs_dir_t *usb_kernelfs = NULL;

/* Last controller ID */
static volatile uint32_t usb_last_controller_id = 0;

/**
 * @brief Initialize the USB system (no controller drivers)
 * 
 * Controller drivers are loaded from the initial ramdisk
 */
void usb_init() {
    // Create the controller list
    usb_controller_list = list_create("usb controllers");

    // Create the driver list
    usb_driver_list = list_create("usb drivers");

    // Initialize builtin drivers
    hid_init();

    LOG(INFO, "USB system online\n");
}

/**
 * @brief Create a USB controller
 * 
 * @param hc The host controller
 * @returns A @c USBController_t structure
 */
USBController_t *usb_createController(void *hc) {
    USBController_t *controller = kmalloc(sizeof(USBController_t));
    controller->hc = hc;
    controller->devices = list_create("usb devices");
    controller->last_address = 1;   // Always start at 1 - default address is 0x0
    controller->id = usb_last_controller_id++;

    return controller;
}

/**
 * @brief Register a new USB controller
 * 
 * @param controller The controller to register
 */
void usb_registerController(USBController_t *controller) {
    if (!controller) return;

    list_append(usb_controller_list, controller);
}

/**
 * @brief Mount USB KernelFS node 
 */
void usb_mount() {
    usb_kernelfs = kernelfs_createDirectory(NULL, "usb", 1);
}
