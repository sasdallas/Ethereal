/**
 * @file hexahedron/drivers/usb/hid/keyboard.c
 * @brief Generic HID keyboard driver
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
#include <kernel/drivers/usb/hid/keyboard.h>
#include <kernel/drivers/usb/hid/hid.h>
#include <kernel/fs/periphfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <string.h>

/* HID to PS/2 scancode translation table */
/* https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf */
uint16_t hid_to_ps2_scancode[] = {
    0x0, 0x0, 0x0, 0x0, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
    0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f,
    0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 
    0x0b, 0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, 0x1b, 0x2b, 0x2b, 0x27, 
    0x28, 0x29, 0x33, 0x34, 0x35, 0x3a 
};

uint16_t hid_modifier_to_ps2_scancode[] = {
    0x1d, 0x2a, 0x38, 0xe05b, 
    0xe01d, 0x59, 0xe038, 0xe05c
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID:KEYBOARD", __VA_ARGS__)

/**
 * @brief Initialize driver
 * @param collection The collection to init on
 */
USB_STATUS usb_keyboardInitDriver(USBHidCollection_t *collection) {
    USBHidKeyboardState_t *state = kzalloc(sizeof(USBHidKeyboardState_t));
    collection->d = state;
    return USB_SUCCESS;
}

/**
 * @brief Begin report
 * @param collection The collection to begin the report on
 */
USB_STATUS usb_keyboardBeginReport(USBHidCollection_t *collection) {
    LOG(DEBUG, "Begin report\n");
    return USB_SUCCESS;
}

/**
 * @brief End report
 * @param collection The collection to finish the report on
 */
USB_STATUS usb_keyboardFinishReport(USBHidCollection_t *collection) {
    USBHidKeyboardState_t *kbd = (USBHidKeyboardState_t*)collection->d;
    
    for (size_t i = 0; i < 8; i++) {
        if (kbd->last_keyboard_state[i] > sizeof(hid_to_ps2_scancode)) {
            LOG(WARN, "Unrecognized/unsupported scancode %02x\n", kbd->last_keyboard_state[i]);
            continue;
        }

        if (kbd->current_keyboard_state[i] > sizeof(hid_to_ps2_scancode)) {
            LOG(WARN, "Unrecognized/unsupported scancode %02x\n", kbd->current_keyboard_state[i]);
            continue;
        }

        if (kbd->last_keyboard_state[i] != kbd->current_keyboard_state[i]) {
            if (!memchr(kbd->last_keyboard_state, kbd->current_keyboard_state[i], 8)) {
                uint16_t sc = hid_to_ps2_scancode[kbd->current_keyboard_state[i]];
                if (sc > UINT8_MAX) {
                    periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, 0xE0); // PS/2 extended
                }

                periphfs_sendKeyboardEvent(EVENT_KEY_PRESS, hid_to_ps2_scancode[kbd->current_keyboard_state[i]] & 0xFF);
            }

            if (!memchr(kbd->current_keyboard_state, kbd->last_keyboard_state[i], 8)) {
                uint16_t sc = hid_to_ps2_scancode[kbd->last_keyboard_state[i]];
                if (sc > UINT8_MAX) {
                    periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, 0xE0); // PS/2 extended
                }

                periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, hid_to_ps2_scancode[kbd->last_keyboard_state[i]] & 0xFF);
            }
        }
    }

    // Modifiers
    for (size_t i = 0; i < 8; i++) {
        if (kbd->last_modifiers & (1 << i) && !(kbd->modifiers & (1 << i))) {
            uint16_t sc  = hid_modifier_to_ps2_scancode[i];
            if (sc > UINT8_MAX) {
                periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, 0xE0); // PS/2 extended
            }

            periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, hid_modifier_to_ps2_scancode[i]);
        } else if (kbd->modifiers & (1 << i) && !(kbd->last_modifiers & (1 << i))) {
            uint16_t sc  = hid_modifier_to_ps2_scancode[i];
            if (sc > UINT8_MAX) {
                periphfs_sendKeyboardEvent(EVENT_KEY_RELEASE, 0xE0); // PS/2 extended
            }

            periphfs_sendKeyboardEvent(EVENT_KEY_PRESS, hid_modifier_to_ps2_scancode[i]);
        }
    }

    // Reset modifiers
    memcpy(kbd->last_keyboard_state, kbd->current_keyboard_state, 8);
    memset(kbd->current_keyboard_state, 0, 8);
    kbd->last_modifiers = kbd->modifiers;
    kbd->modifiers = 0x0;
    kbd->idx = 0;

    HEXDUMP(kbd->current_keyboard_state, 8);

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
USB_STATUS usb_keyboardProcessAbsolute(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, uint32_t usage_id, int64_t value) {
    if (usage_page != 0x7) { LOG(ERR, "Unsupported HID usage page: %04x\n", usage_page); return USB_FAILURE; }
    if (usage_id < 0xe0 || usage_id > 0xe7) {
        LOG(WARN, "Unexpected absolute data with usage ID: 0x%x\n");
        return USB_FAILURE;
    }

    // Any absolute data is modifiers. Store them
    USBHidKeyboardState_t *kbd = (USBHidKeyboardState_t*)collection->d;

    if (value) kbd->modifiers |= (1 << (usage_id-0xe0));
    else kbd->modifiers &= ~(1 << (usage_id-0xe0));

    // LOG(DEBUG, "Process absolute data: %x %x %x\n", usage_page, usage_id, value);
    return USB_SUCCESS;
}

/**
 * @brief Process array data
 * @param collection The collection to process the array on
 * @param item The report item being processed
 * @param usage_page The usage page of the array
 * @param array The data array
 */
USB_STATUS usb_keyboardProcessArray(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, int64_t array) {
    if (usage_page != 0x7) { LOG(ERR, "Unsupported HID usage page: %04x\n", usage_page); return USB_FAILURE; }

    // These are all data
    USBHidKeyboardState_t *kbd = (USBHidKeyboardState_t*)collection->d;

    kbd->current_keyboard_state[kbd->idx++] = array;
    return USB_SUCCESS;
}

/**
 * @brief Initialize generic USB HID keyboard driver
 */
void usb_keyboardInit() {
    USBHidDeviceDriver_t *driver = kzalloc(sizeof(USBHidDeviceDriver_t));
    driver->name = strdup("Generic HID keyboard driver");
    driver->init = usb_keyboardInitDriver;
    driver->begin = usb_keyboardBeginReport;
    driver->finish = usb_keyboardFinishReport;
    driver->array = usb_keyboardProcessArray;
    driver->absolute = usb_keyboardProcessAbsolute;
    driver->usage_page = 0x01;
    driver->usage_id = 0x06;
    hid_registerDriver(driver);
}