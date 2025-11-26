/**
 * @file hexahedron/drivers/usb/hid/mouse.c
 * @brief Generic USB HID mouse driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/hid/mouse.h>
#include <kernel/drivers/usb/hid/hid.h>
#include <kernel/fs/periphfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID:MOUSE", __VA_ARGS__)

/**
 * @brief Initialize driver
 * @param collection The collection to init on
 */
USB_STATUS usb_mouseInitDriver(USBHidCollection_t *collection) {
    LOG(DEBUG, "Mouse driver start\n");
    USBHidMouseState_t *state = kzalloc(sizeof(USBHidMouseState_t));
    collection->d = state;
    return USB_SUCCESS;
}

/**
 * @brief Deinitialize driver
 * @param collection The collection to deinit
 */
USB_STATUS usb_mouseDeinitDriver(USBHidCollection_t *collection) {
    kfree(collection->d);
    return USB_SUCCESS;
}

/**
 * @brief End report
 * @param collection The collection to finish the report on
 */
USB_STATUS usb_mouseFinishReport(USBHidCollection_t *collection) {
    USBHidMouseState_t *mouse = (USBHidMouseState_t*)collection->d;

    uint32_t buttons = 0;
    if (mouse->buttons & (1 << 0)) buttons |= MOUSE_BUTTON_LEFT;
    if (mouse->buttons & (1 << 1)) buttons |= MOUSE_BUTTON_RIGHT;
    if (mouse->buttons & (1 << 2)) buttons |= MOUSE_BUTTON_MIDDLE;

    // TODO: Scrolling support
    periphfs_sendMouseEvent(EVENT_MOUSE_UPDATE, buttons, mouse->rel_x, mouse->rel_y, MOUSE_SCROLL_NONE);

    return USB_SUCCESS;
}

/**
 * @brief Process absolute data
 * @param collection The collection to process the variable on
 * @param item The report item being processed
 * @param usage_page The usage page of the variable
 * @param usage_id The usage ID
 * @param value The value
 */
USB_STATUS usb_mouseProcessAbsolute(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, uint32_t usage_id, int64_t value) {
    // TODO: absmouse support
    USBHidMouseState_t *mouse = (USBHidMouseState_t*)collection->d;
    
    switch (usage_page) {
        case 0x09:
            if (value) mouse->buttons |= (1 << (usage_id-1));
            else mouse->buttons &= ~(1 << (usage_id-1));
            break;

        default:
            LOG(WARN, "Unsupported mouse usage page: %04x\n", usage_page);
            return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Process relative data
 * @param collection The collection to process the variable on
 * @param item The report item being processed
 * @param usage_page The usage page of the variable
 * @param usage_id The usage ID
 * @param value The value
 */
USB_STATUS usb_mouseProcessRelative(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, uint32_t usage_id, int64_t value) {
    USBHidMouseState_t *mouse = (USBHidMouseState_t*)collection->d;

    if (usage_page != 0x01) {
        LOG(WARN, "Unsupported mouse usage page: %04x\n", usage_page);
        return USB_FAILURE;
    }

    if (usage_id == 0x30) {
        mouse->rel_x = value;
    } else if (usage_id == 0x31) {
        mouse->rel_y = -value;
    } else if (usage_id == 0x38) {
        // TODO: Wheel
    } else {
        LOG(WARN, "Unsupported mouse usage ID: %04x\n", usage_id);
    }

    return USB_SUCCESS;
}

/**
 * @brief Initialize generic HID mouse driver
 */
void usb_mouseInit() {
    USBHidDeviceDriver_t *driver = kzalloc(sizeof(USBHidDeviceDriver_t));
    driver->name = strdup("Generic HID mouse driver");
    driver->init = usb_mouseInitDriver;
    driver->finish = usb_mouseFinishReport;
    driver->relative = usb_mouseProcessRelative;
    driver->absolute = usb_mouseProcessAbsolute;
    driver->usage_page = 0x01;
    driver->usage_id = 0x02;
    hid_registerDriver(driver);
}