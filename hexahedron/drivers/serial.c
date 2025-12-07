/**
 * @file hexahedron/drivers/serial.c
 * @brief Generic serial driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/config.h>
#include <kernel/drivers/serial.h>
#include <kernel/mm/alloc.h>
#include <kernel/fs/pty.h>
#include <kernel/debug.h>

#ifdef __ARCH_X86_64__
#include <kernel/drivers/x86/serial.h>
#else
#error "Please create a serial driver for your architecture"
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

/* Early write character method */
int (*serial_write_character_early)(char ch) = NULL;

/* Port data */
static serial_port_t *ports[MAX_COM_PORTS] = { 0 };
static serial_port_t *main_port = NULL;

static pty_t *serial_ptys[MAX_COM_PORTS] = { 0 };

/**
 * @brief Set port
 * @param port The port to set. Depending on the value of COM port it will be added.
 * @param is_main_port Whether this port should be classified as the main port
 * @warning This will overwrite any driver/port already configured
 */
void serial_setPort(serial_port_t *port, int is_main_port) {
    if (!port || port->com_port > MAX_COM_PORTS || port->com_port == 0) return;

    ports[port->com_port - 1] = port;

    if (is_main_port) main_port = port;
}

/**
 * @brief Returns the port configured
 * @param port The port configured.
 * @returns Either the port or NULL. Whatever's in the list.
 */
serial_port_t *serial_getPort(int port) {
    if (port < 1 || port > MAX_COM_PORTS) return NULL;
    return ports[port-1];
} 

/**
 * @brief Put character method - puts characters to main_port or early write method.
 * @param user Can be put as a serial_port object to write to that, or can be NULL.
 */
int serial_print(void *user, char ch) {
    // If user was specified and not NULL, then we are probably trying to print to a specific port
    if (user) {
        if (ch == '\n') ((serial_port_t*)user)->write((serial_port_t*)user, '\r');
        return ((serial_port_t*)user)->write((serial_port_t*)user, ch);
    }

    // Else, do we have a main port?
    if (!main_port) {
        // No, use early write method.
        if (ch == '\n') serial_write_character_early('\r');
        return serial_write_character_early(ch);
    } else {
        // Yes, use that.
        if (ch == '\n') main_port->write(main_port, '\r');
        return main_port->write(main_port, ch);
    }
}

/**
 * @brief Set the serial early write method
 */
void serial_setEarlyWriteMethod(int (*write_method)(char ch)) {
    if (write_method) {
        serial_write_character_early = write_method;
    }
}

/**
 * @brief Serial printing method - writes to main_port
 */
int serial_printf(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int out = xvasprintf(serial_print, NULL, format, ap);
    va_end(ap);
    return out;
}

/**
 * @brief Serial printing method - writes to a specific port
 * @param port The port to write to
 */
int serial_portPrintf(serial_port_t *port, char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int out = xvasprintf(serial_print, (void*)port, format, ap);
    va_end(ap);
    return out;
}

/**
 * @brief Serial input handler
 */
void serial_handleInput(serial_port_t *port, char ch) {
    if (serial_ptys[port->com_port-1]) {
        pty_input(serial_ptys[port->com_port-1], ch);
    }
}

/**
 * @brief Serial write out
 */
static int serial_writeOut(pty_t *pty, char ch) {
    serial_port_t *p = (serial_port_t*)pty->_impl;
    p->write(p, ch);
    return 1;
}

/**
 * @brief Initialize serial port VFS hooks
 */
void serial_mount() {
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        if (ports[i]) {
            serial_ptys[i] = pty_create(NULL, NULL, i);
            serial_ptys[i]->write_out = serial_writeOut;
            serial_ptys[i]->_impl = ports[i];
            char name[128] = { 0 };
            snprintf(name, 128, "/device/ttyS%d", i);
            vfs_mount(serial_ptys[i]->slave, name);
        }
    }
}