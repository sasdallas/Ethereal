/**
 * @file hexahedron/include/drivers/x86/serial.h
 * @brief x86 serial driver header
 * 
 * Implements a standard for the x86 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_X86_SERIAL_H
#define DRIVERS_X86_SERIAL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/serial.h>
#include <sys/termios.h>

/**** DEFINITIONS ****/

// General
#define SERIAL_CLOCK_RATE       115200

// IO addresses
#define SERIAL_COM1_PORT        0x3F8
#define SERIAL_COM2_PORT        0x2F8
#define SERIAL_COM3_PORT        0x3E8
#define SERIAL_COM4_PORT        0x2E8

// Registers (in terms of IO offsets) (0/1 - DLAB setting)
#define SERIAL_RECEIVE_BUFFER           0   // (0) Receive buffer
#define SERIAL_TRANSMIT_BUFFER          0   // (0) Transmit buffer
#define SERIAL_INTENABLE                1   // (0) Interrupt enable register
#define SERIAL_BAUDRATE_LSB             0   // (1) LSB of baud rate
#define SERIAL_BAUDRATE_MSB             1   // (1) MSB of baud rate
#define SERIAL_IDENTIFICATION           2   // (N) Interrupt identification
#define SERIAL_FIFO_CONTROL             2   // (N) FIFO control register
#define SERIAL_LINE_CONTROL             3   // (N) Line control register
#define SERIAL_MODEM_CONTROL            4   // (N) Modem control register
#define SERIAL_LINE_STATUS              5   // (N) Line status register
#define SERIAL_MODEM_STATUS             6   // (N) Modem status register
#define SERIAL_SCRATCH                  7   // (N) Scratch register

// Bitmasks. Only define what we currently use for now.
// TODO: Define the rest.
#define SERIAL_LINECTRL_DLAB            0x80    // DLAB bit

#define SERIAL_MODEMCTRL_DTR            0x01    // Data transmit ready
#define SERIAL_MODEMCTRL_RTS            0x02    // Ready to send     
#define SERIAL_MODEMCTRL_OUT2           0x08    // Hardware pin OUT2 for IRQs
#define SERIAL_MODEMCTRL_LOOPBACK       0x10    // Loopback diagnostic mode

#define SERIAL_LINESTATUS_DATA          0x1

#define SERIAL_8_DATA                   0x03    // Specify 8 data bits
#define SERIAL_1_STOP                   0x00    // One stop bit
#define SERIAL_NO_PARITY                0x00    // No parity bits

#define SERIAL_FIFO_ENABLE              0x01 | 0x02 | 0x04 // Sets the first 3 bits to enable FIFO and clear receive/transmit


/**** FUNCTIONS ****/

/**
 * @brief Initialize the serial system with the debug port.
 * 
 * Sets baud rate to whatever's configured in config, enables FIFO,
 * sets up an interface, you know the drill.
 */
int serial_initialize();

/**
 * @brief Changes the serial port baud rate
 * 
 * @param device The device to perform (NOTE: Providing NULL should only be done by serial_initialize, it will set baud rate of debug port.)
 * @param baudrate The baud rate to set
 * @returns -EINVAL on bad baud rate, 0 on success.
 */
int serial_setBaudRate(serial_port_t *device, uint16_t baudrate);

/**
 * @brief Initialize a specific serial port
 * @param com_port The port to initialize. Can be from 1-4 for COM1-4. It is not recommended to go past COM2.
 * @param baudrate The baudrate to use
 * @returns A serial port structure or NULL if bad.
 */
serial_port_t *serial_initializePort(int com_port, uint16_t baudrate);

/**
 * @brief Create serial port data
 * @param com_port The port to create the data from
 * @param baudrate The baudrate to use
 * @returns Port structure or NULL
 */
serial_port_t *serial_createPortData(int com_port, uint16_t baudrate);

/**
 * @brief Reinitialize port
 * @param port The port
 * @param tios The termios
 */
void serial_reinitializePort(serial_port_t *port, struct termios *tios);

#endif