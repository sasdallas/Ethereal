/**
 * @file hexahedron/include/kernel/drivers/serial.h
 * @brief Generic serial driver header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

/**** INCLUDES ****/

#include <stdint.h>
#include <stddef.h>

/**** TYPES ****/

struct _serial_port;

// Write method
typedef int (*serial_port_write_t)(struct _serial_port *port, char ch);

// Read method (if timeout is 0, wait forever)
typedef char (*serial_port_read_t)(struct _serial_port *port, size_t timeout);

typedef struct _serial_port {
    int com_port;               // COM port
    uint32_t baud_rate;         // Baud rate
    
    uint32_t io_address;        // I/O address (for use by driver)

    serial_port_read_t read;    // Read method 
    serial_port_write_t write;  // Write method
} serial_port_t;


/**** DEFINITIONS ****/

#define MAX_COM_PORTS 5

/**** FUNCTIONS ****/

/**
 * @brief Set port
 * @param port The port to set. Depending on the value of COM port it will be added.
 * @warning This will overwrite any driver/port already configured
 */
void serial_setPort(serial_port_t *port);

/**
 * @brief Returns the port configured
 * @param port The port configured.
 * @returns Either the port or NULL. Whatever's in the list.
 */
serial_port_t *serial_getPort(int port);

/**
 * @brief Serial printing method - writes to a specific port
 * @param port The port to write to
 */
int serial_printf(serial_port_t *port, char *format, ...);

/**
 * @brief Serial input handler
 */
void serial_handleInput(serial_port_t *port, char ch);

#endif