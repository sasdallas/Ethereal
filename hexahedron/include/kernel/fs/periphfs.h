/**
 * @file hexahedron/include/kernel/fs/periphfs.h
 * @brief Peripheral filesystem (/device/keyboard and /device/mouse)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_PERIPHFS_H
#define KERNEL_FS_PERIPHFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/spinlock.h>


/**** DEFINITIONS ****/

#define SCANCODE_F1                     0x01
#define SCANCODE_F2                     0x02
#define SCANCODE_F3                     0x03
#define SCANCODE_F4                     0x04
#define SCANCODE_F5                     0x05
#define SCANCODE_F6                     0x06
#define SCANCODE_F7                     0x07
#define SCANCODE_F8                     0x08
#define SCANCODE_F9                     0x09
#define SCANCODE_F10                    0x10
#define SCANCODE_F11                    0x11
#define SCANCODE_F12                    0x12

#define SCANCODE_LEFT_SHIFT             0x13
#define SCANCODE_RIGHT_SHIFT            0x14

#define EVENT_KEY_RELEASE               0x00
#define EVENT_KEY_PRESS                 0x01
#define EVENT_KEY_MODIFIER_RELEASE      0x02
#define EVENT_KEY_MODIFIER_PRESS        0x03

#define KBD_QUEUE_EVENTS                4096

/**** TYPES ****/

/**
 * @brief Keyboard event
 */
typedef struct key_event {
    int event_type;         // Type of event
    uint8_t scancode;       // Scancode
} key_event_t;

/**
 * @brief Keyboard queue buffer
 */
typedef struct key_buffer_t {
    spinlock_t lock;
    key_event_t event[KBD_QUEUE_EVENTS];
    volatile int head;
    volatile int tail;
} key_buffer_t;

/**** MACROS ****/

#define KEY_CONTENT_AVAILABLE(buffer) ((buffer)->head != (buffer)->tail)


/**** FUNCTIONS ****/

/**
 * @brief Initialize the peripheral filesystem interface
 */
void periphfs_init();

/**
 * @brief Write a new event to the keyboard interface
 * @param event_type The type of event to write
 * @param scancode The scancode relating to the event
 * @returns 0 on success
 */
int periphfs_sendKeyboardEvent(int event_type, uint8_t scancode);

#endif