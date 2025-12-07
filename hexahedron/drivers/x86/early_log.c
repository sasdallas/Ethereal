/**
 * @file hexahedron/drivers/x86/early_log.c
 * @brief Early log device
 * 
 * Pre-serial initialization
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/x86/early_log.h>

typedef enum {
    SERIAL_COM1,
    SERIAL_COM2,
    SERIAL_COM3,
    PORT_E9,
} ELOG_DEVICE;

/* SELECTED DEVICE */
const int __early_log_device = SERIAL_COM1;

/* Log port */
static uint16_t __early_log_port = 0x0;


/**
 * @brief Early log write method
 */
static int earlylog_write(void *user, char ch) {
    outportb(__early_log_port, ch);
    return 0;
}

/**
 * @brief Initialize early log
 */
void earlylog_init() {
    switch (__early_log_device) {
        case SERIAL_COM1:
            __early_log_port = 0x3f8;
            outportb(0x3f8+3, 0x03);
            break;
        case SERIAL_COM2:
            __early_log_port = 0x2f8;
            outportb(0x2f8+3, 0x03);
            break;
        case SERIAL_COM3:
            __early_log_port = 0x3e8;
            outportb(0x3e8+3, 0x03);
            break;
        case PORT_E9:
            __early_log_port = 0xe9;
            break; // 0xE9 needs no init
    }

    debug_setOutput(earlylog_write);
}