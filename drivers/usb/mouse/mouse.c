/**
 * @file drivers/usb/mouse/mouse.c
 * @brief USB mouse driver
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
#include <kernel/loader/driver.h>
#include <kernel/fs/periphfs.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>

/* HID mouse state */
typedef struct mouse_state {
    bool have_abs;

    union {
        struct {
            int rel_x;
            int rel_y;
        } rel;

        struct {
            int abs_x;
            int abs_y;
            int min_x;
            int max_x;
            int min_y;
            int max_y;
        } abs;
    };

    uint8_t buttons;
    int8_t scroll;
} mouse_state_t;

/* HID driver matches */
hid_match_t mouse_matches[] = {
    { .usage_id = 0x02, .usage_page = 0x01 },
};

/* HID driver */
static bool mouse_probe(hid_collection_t *collection);
static void mouse_attach(hid_collection_t *collection);
static void mouse_remove(hid_collection_t *collection);
static void mouse_event(hid_collection_t *collection, hid_event_t *event);
static void mouse_end(hid_collection_t *collection);

hid_driver_t mouse_driver = {
    .name = "HID Mouse",
    .matches = mouse_matches,
    .num_matches = sizeof(mouse_matches) / sizeof(mouse_matches[0]),
    .ops = {
        .probe = mouse_probe,
        .attach = mouse_attach,
        .remove = mouse_remove,
        .event = mouse_event,
        .end = mouse_end
    }
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USB:MOUSE", __VA_ARGS__)

/**
 * @brief Mouse probe
 */
static bool mouse_probe(hid_collection_t *collection) {
    return true;
}

/**
 * @brief Mouse attach
 */
static void mouse_attach(hid_collection_t *collection) {
    mouse_state_t *state = kzalloc(sizeof(mouse_state_t));
    collection->priv = state;
}

/**
 * @brief Mouse remove
 */
static void mouse_remove(hid_collection_t *collection) {
    kfree(collection->priv);
}

/**
 * @brief Mouse event
 */
static void mouse_event(hid_collection_t *collection, hid_event_t *event) {
    mouse_state_t *state = collection->priv;

    if (event->flags & HID_EVENT_FLAG_VARIABLE) {
        if (event->flags & HID_EVENT_FLAG_RELATIVE) {
            // relative mouse
            if (event->usage_page == 0x1) {
                if (event->usage == 0x30) {
                    state->rel.rel_x = (int)event->value;
                } else if (event->usage == 0x31) {
                    state->rel.rel_y = -((int)event->value);
                } else if (event->usage == 0x38) {
                    state->scroll = (int8_t)event->value;
                } else {
                    LOG(WARN, "Unsupported relative usage ID: 0x%04x\n", event->usage);
                }
            } else {
                LOG(WARN, "Unsupported relative usage page: 0x%04x\n", event->usage_page);
            }
        } else {
            // abs mouse or buttons
            
            if (event->usage_page == 0x01) {
                if (event->usage == 0x30) {
                    state->have_abs = true;
                    state->abs.abs_x = event->value;
                    state->abs.min_x = event->field->physical_min;
                    state->abs.max_x = event->field->physical_max;

                    if (!state->abs.min_x && !state->abs.max_x) {
                        // hack i dont know if this is right, have nothing to test
                        state->abs.min_x = event->field->logical_min;
                        state->abs.max_x = event->field->logical_max;
                    }
                } else if (event->usage == 0x31) {
                    state->have_abs = true;
                    state->abs.abs_y = event->value;
                    state->abs.min_y = event->field->physical_min;
                    state->abs.max_y = event->field->physical_max;

                    if (!state->abs.min_y && !state->abs.max_y) {
                        // hack i dont know if this is right, have nothing to test
                        state->abs.min_y = event->field->logical_min;
                        state->abs.max_y = event->field->logical_max;
                    }
                } else {
                    LOG(WARN, "Unsupported absolute usage ID: 0x%04x\n", event->usage);
                }
            } else if (event->usage_page == 0x09) {
                // buttons
                if (event->value) state->buttons |= (1 << (event->usage-1));
                else state->buttons &= ~(1 << (event->usage-1));
            } else {
                LOG(WARN, "Unsupported absolute usage page: 0x%04x\n", event->usage_page);
            }
        }
    } else {
        LOG(WARN, "Unsupported mouse event flags=0x%x usage_page=0x%04x usage=0x%08x val=0x%x\n", event->flags, event->usage_page, event->usage, event->value);
    }
}

/**
 * @brief End method
 */
static void mouse_end(hid_collection_t *collection) {
    mouse_state_t *state = collection->priv;
    uint32_t buttons = 0;
    if (state->buttons & (1 << 0)) buttons |= MOUSE_BUTTON_LEFT;
    if (state->buttons & (1 << 1)) buttons |= MOUSE_BUTTON_RIGHT;
    if (state->buttons & (1 << 2)) buttons |= MOUSE_BUTTON_MIDDLE;

    uint32_t scroll = MOUSE_SCROLL_NONE;
    if (state->scroll > 0) scroll = MOUSE_SCROLL_UP;
    if (state->scroll < 0) scroll = MOUSE_SCROLL_DOWN;

    if (state->have_abs) {
        state->have_abs = false;
    
        mouse_event_t e = {
            .event_type = EVENT_MOUSE_ABSOLUTE,
            .abs = {
                .min_x = state->abs.min_x,
                .max_x = state->abs.max_x,
                .min_y = state->abs.min_y,
                .max_y = state->abs.max_y,
                .x = state->abs.abs_x,
                .y = state->abs.abs_y,
            },
            .buttons = buttons,
            .scroll = scroll
        };

        periphfs_sendMouseEvent(&e);
    } else {
        periphfs_sendMouseEventRelative(buttons, state->rel.rel_x, state->rel.rel_y, scroll);
    }
}

/**
 * @brief Mouse init method
 */
static int mouse_init(int argc, char *argv[]) {
    hid_register(&mouse_driver);
    return DRIVER_STATUS_SUCCESS;
}

/**
 * @brief Mouse deinit method
 */
static int mouse_deinit() {
    return DRIVER_STATUS_SUCCESS;
}

/* Driver metadata */
struct driver_metadata driver_metadata = {
    .name = "USB Mouse Driver",
    .author = "Samuel Stuart",
    .init = mouse_init,
    .deinit = mouse_deinit
};
