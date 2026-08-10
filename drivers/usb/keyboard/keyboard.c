/**
 * @file drivers/usb/keyboard/keyboard.c
 * @brief USB keyboard driver
 * 
 * @todo auto-repeat
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>
#include <kernel/loader/driver.h>
#include <kernel/fs/periphfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>

/* Keyboard state */
typedef struct hid_keyboard_state {
    bool mod[8];
} hid_keyboard_state_t;

/* HID to PS/2 scancode translation table */
/* https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf */
uint16_t hid_to_ps2_scancode[] = {
    0x0, 0x0, 0x0, 0x0, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
    0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f,
    0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 
    0x0b, 0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, 0x1b, 0x2b, 0x2b, 0x27, 
    0x28, 0x29, 0x33, 0x34, 0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
    0x42, 0x43, 0x44, 0x57, 0x58, 0xE037, 0x46, 0xE046, 0xE052, 0xE047, 0xE049,
    0xE053, 0xE04F, 0xE051, 0xE04D, 0xE04B, 0xE050, 0xE048
};

uint16_t hid_modifier_to_ps2_scancode[] = {
    0x1d, 0x2a, 0x38, 0xe05b, 
    0xe01d, 0x59, 0xe038, 0xe05c
};

#define HID_TO_PS2_SCANCODE_COUNT (sizeof(hid_to_ps2_scancode) / sizeof(hid_to_ps2_scancode[0]))
#define HID_SEND(event_type, sc)    if ((sc) > UINT8_MAX) periphfs_sendKeyboardEvent(event_type, 0xE0); \
                                    periphfs_sendKeyboardEvent(event_type, (sc) & 0xFF);

/* HID driver matches */
hid_match_t keyboard_matches[] = {
    { .usage_id = 0x06, .usage_page = 0x01 }   
};

/* HID driver */
static bool keyboard_probe(hid_collection_t *col);
static void keyboard_attach(hid_collection_t *col);
static void keyboard_remove(hid_collection_t *col);
static void keyboard_event(hid_collection_t *col, hid_event_t *event);

hid_driver_t keyboard_driver = {
    .name = "HID Keyboard",
    .matches = keyboard_matches,
    .num_matches = sizeof(keyboard_matches) / sizeof(keyboard_matches[0]),
    .ops = {
        .probe = keyboard_probe,
        .attach = keyboard_attach,
        .remove = keyboard_remove,
        .event = keyboard_event
    },
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USB:KEYBOARD", __VA_ARGS__)


/**
 * @brief Keyboard probe method
 */
static bool keyboard_probe(hid_collection_t *col) {
    return true;
}

/**
 * @brief Attach method
 */
static void keyboard_attach(hid_collection_t *col) {
    hid_keyboard_state_t *state = kzalloc(sizeof(hid_keyboard_state_t));
    col->priv = state;
}

/**
 * @brief Remove method
 */
static void keyboard_remove(hid_collection_t *col) {
    kfree(col->priv);
}

/**
 * @brief Event method
 */
static void keyboard_event(hid_collection_t *col, hid_event_t *event) {
    if (event->usage_page != 0x07) {
        LOG(WARN, "Unsupported usage page: %04x\n", event->usage_page);
        return;
    }

    hid_keyboard_state_t *state = col->priv;

    if (event->flags == 0) {
        if (event->flags & HID_EVENT_FLAG_RELATIVE) {
            return;
        }

        uint8_t key = (uint8_t)event->usage;
        if (key >= HID_TO_PS2_SCANCODE_COUNT) {
            // probably unsupported scancode
            return;
        }
        
        uint16_t sc = hid_to_ps2_scancode[key];

        if (event->value) {
            HID_SEND(EVENT_KEY_PRESS, sc);
        } else {
            HID_SEND(EVENT_KEY_RELEASE, sc | 0x80);
        }
    } else if (event->flags == HID_EVENT_FLAG_VARIABLE) {
        if (event->usage >= 0xE0 && event->usage <= 0xE7) {
            uint8_t index = event->usage - 0xE0;
            bool on = (event->value != 0);

            if (on == false && state->mod[index] == true) {
                HID_SEND(EVENT_KEY_RELEASE, hid_modifier_to_ps2_scancode[index] | 0x80);
            } else if (on == true && state->mod[index] == false) {
                HID_SEND(EVENT_KEY_PRESS, hid_modifier_to_ps2_scancode[index]);
            }

            state->mod[index] = on;
        } else {
            uint8_t key = (uint8_t)event->usage;
            if (key >= HID_TO_PS2_SCANCODE_COUNT) {
                // probably unsupported scancode
                return;
            }
            
            uint16_t sc = hid_to_ps2_scancode[key];

            if (event->value) {
                HID_SEND(EVENT_KEY_PRESS, sc);
            } else {
                HID_SEND(EVENT_KEY_RELEASE, sc | 0x80);
            }
        }
    } else {
        LOG(WARN, "Received unsupported keyboard event flags=0x%08x usage_page=%04x usage=%08x val=0x%x\n", event->flags, event->usage_page, event->usage, event->value);
    }
}

/**
 * @brief Initialize driver
 */
static int keyboard_init() {
    hid_register(&keyboard_driver);
    return DRIVER_STATUS_SUCCESS;
}

/**
 * @brief Deinitialize driver
 */
static int keyboard_deinit() {
    return DRIVER_STATUS_SUCCESS;
}

struct driver_metadata driver_metadata = {
    .name = "USB HID Keyboard Driver",
    .author = "Samuel Stuart",
    .init = keyboard_init,
    .deinit = keyboard_deinit
};
