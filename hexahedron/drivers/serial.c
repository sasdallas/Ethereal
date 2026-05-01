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
#include <kernel/debug.h>
#include <kernel/fs/tty.h>

#ifdef __ARCH_X86_64__
#include <kernel/drivers/x86/serial.h>
#else
#error "Please create a serial driver for your architecture"
#endif

#include <kernel/init.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

/* Early write character method */
int (*serial_write_character_early)(char ch) = NULL;

/* Port data */
static serial_port_t *ports[MAX_COM_PORTS] = { 0 };

tty_t *serial_ttys[MAX_COM_PORTS] = { 0 };

/**
 * @brief Set port
 * @param port The port to set. Depending on the value of COM port it will be added.
 * @warning This will overwrite any driver/port already configured
 */
void serial_setPort(serial_port_t *port) {
    if (!port || port->com_port > MAX_COM_PORTS || port->com_port == 0) return;
    ports[port->com_port - 1] = port;
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
static int serial_print(void *user, char ch) {
    // If user was specified and not NULL, then we are probably trying to print to a specific port
    if (ch == '\n') ((serial_port_t*)user)->write((serial_port_t*)user, '\r');
    return ((serial_port_t*)user)->write((serial_port_t*)user, ch);
}

/**
 * @brief Serial printing method - writes to a specific port
 * @param port The port to write to
 */
int serial_printf(serial_port_t *port, char *format, ...) {
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
extern void tty_handle(tty_t *tty, char ch) ;
    tty_handle(serial_ttys[port->com_port-1], ch);
}

static int serial_writeOut(tty_t *tty, char *buffer, size_t size) {
    serial_port_t *port = (serial_port_t*)tty->priv;

    for (unsigned i = 0; i < size; i++) {
        port->write(port, buffer[i]);
    }

    return 0;
}

/**
 * @brief Initialize serial port VFS hooks
 */
static int serial_init_tty() {
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        serial_port_t *p = serial_initializePort(i+1, 9600); // !!!: stupid

        if (p) {
            serial_setPort(p);

            char name[128];
            snprintf(name, 128, "ttyS%d", i);

            serial_ttys[i] = tty_create(name);
            serial_ttys[i]->write = serial_writeOut;
            serial_ttys[i]->priv = ports[i]; // TODO: tty fill_tios
            serial_reinitializePort(p, &serial_ttys[i]->tios);
        }
    }

    return 0;
}

FS_INIT_ROUTINE(serial, INIT_FLAG_DEFAULT, serial_init_tty, tty);

/**
 * @brief Initialize serial ports
 */
int serial_init() {
    serial_port_t *com1 = serial_initializePort(1, 9600);
    if (com1) serial_setPort(com1);
    serial_port_t *com2 = serial_initializePort(2, 9600);
    if (com2) serial_setPort(com2);
    serial_port_t *com3 = serial_initializePort(3, 9600);
    if (com3) serial_setPort(com3);
    serial_port_t *com4 = serial_initializePort(4, 9600);
    if (com4) serial_setPort(com4);

    return 0;
}

KERN_EARLY_INIT_ROUTINE(serial, INIT_FLAG_DEFAULT, serial_init);
