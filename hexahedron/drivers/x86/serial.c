/**
 * @file hexahedron/drivers/x86/serial.c
 * @brief x86 serial driver
 * 
 * @todo    serial_setBaudRate and this entire implementation is flawed. 
 *          With the introduction of the port structure, I can't find a good way to
 *          reliably make this work before allocation.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdint.h>
#include <errno.h>

#include <stdio.h>
#include <stdarg.h>

#include <kernel/drivers/x86/serial.h>
#include <kernel/drivers/serial.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include <kernel/config.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/drivers/clock.h>

// Main configured serial port (TODO: Move this to a structure)
uint16_t serial_defaultPort = SERIAL_COM1_PORT;
uint16_t serial_defaultBaud = 9600;


// Internal function to get the COM port address using configuration info.
static uint16_t serial_getCOMAddress(int com_port) {
    switch (com_port) {
        case 1:
            return SERIAL_COM1_PORT;
        case 2:
            return SERIAL_COM2_PORT;
        case 3:
            return SERIAL_COM3_PORT;
        case 4:
            return SERIAL_COM4_PORT;
        default:
            // TODO: Log somehow?
            return SERIAL_COM1_PORT;
    }
}


/**
 * @brief Changes the serial port baud rate
 * 
 * @param device The device to perform (NOTE: Providing NULL should only be done by serial_initialize, it will set baud rate of debug port.)
 * @param baudrate The baud rate to set
 * @returns -EINVAL on bad baud rate, 0 on success.
 */
int serial_setBaudRate(serial_port_t *device, uint16_t baudrate) {
    if (SERIAL_CLOCK_RATE % baudrate != 0) return -EINVAL;

    uint16_t port = (!device) ? serial_defaultPort : device->io_address;

    // Enable the DLAB bit. LSB/MSB registers are not accessible without it
    uint8_t lcr = inportb(port + SERIAL_LINE_CONTROL);
    outportb(port + SERIAL_LINE_CONTROL, lcr | SERIAL_LINECTRL_DLAB);

    // Calculate the divisors and set them
    uint32_t mode = SERIAL_CLOCK_RATE / baudrate;
    outportb(port + SERIAL_BAUDRATE_LSB, (uint8_t)(mode & 0xFF));
    outportb(port + SERIAL_BAUDRATE_MSB, (uint8_t)((mode >> 8) & 0xFF));

    // Reset the DLAB bit
    outportb(port + SERIAL_LINE_CONTROL, lcr);

    // Update baud rate
    if (!device) serial_defaultBaud = baudrate;
    else device->baud_rate = baudrate;

    return 0;
}

/**
 * @brief Write a character to serial output (early output)
 */
static int write_early(char ch) {
    // Wait until transmit is empty
    while ((inportb(serial_defaultPort + SERIAL_LINE_STATUS) & 0x20) == 0x0);

    // Write character
    outportb(serial_defaultPort + SERIAL_TRANSMIT_BUFFER, ch);

    return 0;
}

/**
 * @brief Write a character to a serial device
 */
static int write_method(serial_port_t *device, char ch) {
    // Wait until transmit is empty
    while ((inportb(device->io_address + SERIAL_LINE_STATUS) & 0x20) == 0x0);

    // Write character
    outportb(device->io_address + SERIAL_TRANSMIT_BUFFER, ch);

    return 0;
}

/**
 * @brief Retrieves a character from serial
 * @param timeout The time to wait in seconds 
 */
static char receive_method(serial_port_t *device, size_t timeout) {
    // Wait until receive has something or the timeout hits
    unsigned long long finish_time = (now() * 1000) + timeout;

    while ((timeout == 0) ? 1 : (finish_time > now() * 1000)) {
        if ((inportb(device->io_address + SERIAL_LINE_STATUS) & 0x01) != 0x0) {
            // Return the character
            return inportb(device->io_address + SERIAL_RECEIVE_BUFFER);
        }
    };

    return 0; 
}



/**
 * @brief Initialize the serial system with the debug port.
 * 
 * Sets baud rate to whatever's configured in config, enables FIFO,
 * sets up an interface, you know the drill.
 */
int serial_initialize() {
    // Create basic port data
    serial_defaultPort = serial_getCOMAddress(__debug_output_com_port);

    // Disable port interrupts
    outportb(serial_defaultPort + SERIAL_LINE_CONTROL, 0);
    
    // Set baud rate of debug port
    serial_setBaudRate(NULL, __debug_output_baud_rate);
    
    // Configure port bit parameters
    outportb(serial_defaultPort + SERIAL_LINE_CONTROL,
                SERIAL_8_DATA | SERIAL_1_STOP | SERIAL_NO_PARITY);

    // Enable FIFO & clear transmit/receive
    outportb(serial_defaultPort + SERIAL_FIFO_CONTROL, 0xC7);

    // Enable DTR, RTS, and OUT2
    outportb(serial_defaultPort + SERIAL_MODEM_CONTROL,
                SERIAL_MODEMCTRL_DTR | SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2);
        
    // Sleep for just an eency weency bit (Bochs)
    for (int i = 0; i < 10000; i++);

    // Now try to test the serial port. Configure the chip in loopback mode
    outportb(serial_defaultPort + SERIAL_MODEM_CONTROL, 
                SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2 | SERIAL_MODEMCTRL_LOOPBACK);

    // Now send a byte and check if we get it back.
    outportb(serial_defaultPort + SERIAL_TRANSMIT_BUFFER, 0xAE);
    if (inportb(serial_defaultPort + SERIAL_TRANSMIT_BUFFER) != 0xAE) {
        return -1; // The chip must be faulty :(
    } 

    // Not faulty! Reset in normal mode, aka RTS/DTR/OUT2
    outportb(serial_defaultPort + SERIAL_MODEM_CONTROL,
                SERIAL_MODEMCTRL_DTR | SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2);

    // Clear RBR
    inportb(serial_defaultPort + SERIAL_RECEIVE_BUFFER);

    // Set ourselves as the write method
    serial_setEarlyWriteMethod(write_early);

    return 0;
}

/**
 * @brief Create serial port data
 * @param com_port The port to create the data from
 * @param baudrate The baudrate to use
 * @returns Port structure or NULL
 */
serial_port_t *serial_createPortData(int com_port, uint16_t baudrate) {
    if (com_port < 1 || com_port > 4) return NULL;      // Bad COM port
    if (SERIAL_CLOCK_RATE % baudrate != 0) return NULL; // Bad baud rate

    serial_port_t *ser_port = kmalloc(sizeof(serial_port_t));
    ser_port->baud_rate = baudrate;
    ser_port->com_port = com_port;
    ser_port->read = receive_method;
    ser_port->write = write_method;
    ser_port->io_address = serial_getCOMAddress(com_port);
    return ser_port;
}


/**
 * @brief Initialize a specific serial port
 * @param com_port The port to initialize. Can be from 1-4 for COM1-4. It is not recommended to go past COM2.
 * @param baudrate The baudrate to use
 * @returns A serial port structure or NULL if bad.
 */
serial_port_t *serial_initializePort(int com_port, uint16_t baudrate) {
    // Create the port data
    serial_port_t *ser_port = serial_createPortData(com_port, baudrate);
    if (!ser_port) {
        dprintf(ERR, "Could not create port data\n");
        return NULL;
    }

    // Now let's get to the initialization
    // Disable all interrupts
    outportb(ser_port->io_address + SERIAL_INTENABLE, 0);

    // Set baud rate of the port
    if (serial_setBaudRate(ser_port, baudrate)) {
        // Something went wrong.. hope the debug port is initialized
        dprintf(ERR, "Failed to set baud rate of COM%i to %i\n", com_port, baudrate);
        kfree(ser_port);
        return NULL;
    }

    // Configure port bit parameters
    outportb(ser_port->io_address + SERIAL_LINE_CONTROL,
                SERIAL_8_DATA | SERIAL_1_STOP | SERIAL_NO_PARITY);

    // Enable FIFO & clear transmit and receive
    outportb(ser_port->io_address + SERIAL_FIFO_CONTROL, 0xC7);

    // Enable DTR, RTS, and OUT2
    outportb(ser_port->io_address + SERIAL_MODEM_CONTROL,
                SERIAL_MODEMCTRL_DTR | SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2);

    // Sleep for just an eency weency bit (Bochs)
    for (int i = 0; i < 10000; i++);

    // Now try to test the serial port. Configure the chip in loopback mode
    outportb(ser_port->io_address + SERIAL_MODEM_CONTROL,
                SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2 | SERIAL_MODEMCTRL_LOOPBACK);

    // Now send a byte and check if we get it back.
    outportb(ser_port->io_address + SERIAL_TRANSMIT_BUFFER, 0xAE);
    if (inportb(ser_port->io_address + SERIAL_TRANSMIT_BUFFER) != 0xAE) {
        dprintf(WARN, "COM%i is faulty or nonexistent\n", com_port);
        return NULL; // The chip must be faulty, or it doesn't exist
    } 

    // Not faulty! Reset in normal mode, aka RTS/DTR/OUT2
    outportb(ser_port->io_address + SERIAL_MODEM_CONTROL,
                SERIAL_MODEMCTRL_DTR | SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2);

    // Clear RBR
    inportb(ser_port->io_address + SERIAL_RECEIVE_BUFFER);

    dprintf(INFO, "Successfully initialized COM%i\n", com_port);
    serial_portPrintf(ser_port, "Hello, COM%i!\n", com_port);
    return ser_port;
}