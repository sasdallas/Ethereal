/**
 * @file hexahedron/include/kernel/drivers/serial.h
 * @brief Generic serial driver header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
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
 * @param is_main_port Whether this port should be classified as the main port
 * @warning This will overwrite any driver/port already configured
 */
void serial_setPort(serial_port_t *port, int is_main_port);

/**
 * @brief Returns the port configured
 * @param port The port configured.
 * @returns Either the port or NULL. Whatever's in the list.
 */
serial_port_t *serial_getPort(int port);

/**
 * @brief Set the serial early write method
 */
void serial_setEarlyWriteMethod(int (*write_method)(char ch));

/**
 * @brief Put character method - puts characters to main_port or early write method.
 * @param user Can be put as a serial_port object to write to that, or can be NULL.
 */
int serial_print(void *user, char ch);

/**
 * @brief Serial printing method - writes to main_port
 */
int serial_printf(char *format, ...);

/**
 * @brief Serial printing method - writes to a specific port
 * @param port The port to write to
 */
int serial_portPrintf(serial_port_t *port, char *format, ...);

/**
 * @brief Serial reading method
 * @param buffer The string to output to
 * @param port The port to read from
 * @param size How many characters to read.
 * @param timeout How long to wait for each character. If zeroed, it will wait forever
 * @returns The amount of characters read
 */
int serial_readBuffer(char *buffer, serial_port_t *port, size_t size, size_t timeout);

/**
 * @brief Serial reading method - reads from a specific port
 * @param port The port to read from
 * @param size How many characters to read
 * @param timeout How long to wait for each character. If zeroed, it will wait forever
 * @returns A pointer to the allocated buffer
 */
char *serial_readPort(serial_port_t *port, size_t size, size_t timeout);

/**
 * @brief Serial reading method - reads from main_port
 * @param size How many characters to read.
 * @param timeout How long to wait for each character. If zeroed, it will wait forever
 * @returns The amount of characters read
 */
char *serial_read(size_t size, size_t timeout);

#endif