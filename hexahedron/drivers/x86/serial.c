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
#include <kernel/init.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include <kernel/config.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>

// Main configured serial port (TODO: Move this to a structure)
uint16_t serial_defaultPort = SERIAL_COM1_PORT;
uint16_t serial_defaultBaud = 9600;

// Threads
thread_t *serial_thread_ac;
thread_t *serial_thread_bd;

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
 * @brief Serial thread
 */
void serial_thread(void *context) {
    uint16_t port = (uint16_t)(uintptr_t)context;
    serial_port_t *a_port = serial_getPort((port == 0x3f8) ? 1 : 2);
    serial_port_t *c_port = serial_getPort((port == 0x3f8) ? 3 : 4);

    for (;;) {
        sleep_prepare();
        sleep_enter();

        int data = 1;
        while (data) {
            data = 0;
            uint8_t a = inportb(port+SERIAL_LINE_STATUS);
            if (a != 0xff && (a & SERIAL_LINESTATUS_DATA)) {
                serial_handleInput(a_port, inportb(port));
                data = 1;
            }

            uint8_t c = inportb((port-0x10) + SERIAL_LINE_STATUS);
            if (c != 0xff && (c & SERIAL_LINESTATUS_DATA)) {
                serial_handleInput(c_port, inportb(port - 0x10));
                data = 1;
            }
        }
    }
}

/**
 * @brief Serial IRQ (A/C)
 */
int serial_irq_ac(void *context) {
    sleep_wakeup(serial_thread_ac); // DPC when :(
    return 0;
}

/**
 * @brief Serial IRQ (B/D)
 */
int serial_irq_bd(void *context) {
    sleep_wakeup(serial_thread_bd); // DPC when :(
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

    // // Sleep for just an eency weency bit (Bochs)
    // for (int i = 0; i < 10000; i++);

    // // Now try to test the serial port. Configure the chip in loopback mode
    // outportb(ser_port->io_address + SERIAL_MODEM_CONTROL,
    //             SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2 | SERIAL_MODEMCTRL_LOOPBACK);

    // // Now send a byte and check if we get it back.
    // outportb(ser_port->io_address + SERIAL_TRANSMIT_BUFFER, 0xAE);
    // if (inportb(ser_port->io_address + SERIAL_TRANSMIT_BUFFER) != 0xAE) {
    //     dprintf(WARN, "COM%i is faulty or nonexistent\n", com_port);
    //     return NULL; // The chip must be faulty, or it doesn't exist
    // } 

    // // Not faulty! Reset in normal mode, aka RTS/DTR/OUT2
    // outportb(ser_port->io_address + SERIAL_MODEM_CONTROL,
    //             SERIAL_MODEMCTRL_DTR | SERIAL_MODEMCTRL_RTS | SERIAL_MODEMCTRL_OUT2);

    // Enable interrupts on the port (enable received data available interrupt)
    outportb(ser_port->io_address + SERIAL_INTENABLE, 0x01);



    // Clear RBR
    // inportb(ser_port->io_address + SERIAL_RECEIVE_BUFFER);

    dprintf(INFO, "Successfully initialized COM%i\n", com_port);
    return ser_port;
}

/**
 * @brief Serial thread spawner
 */
static int serial_spawn() {
    if (serial_getPort(1) || serial_getPort(3)) {
        process_t *proc = process_createKernel("serial_thread_ac", PROCESS_KERNEL, PRIORITY_MED, serial_thread, (void*)(uintptr_t)0x3f8);
        serial_thread_ac = proc->main_thread;
        scheduler_insertThread(serial_thread_ac);
        hal_registerInterruptHandler(4, serial_irq_ac, NULL);
    }
    if (serial_getPort(2) || serial_getPort(4)) {
        process_t *proc = process_createKernel("serial_thread_bd", PROCESS_KERNEL, PRIORITY_MED, serial_thread, (void*)(uintptr_t)0x2f8);
        serial_thread_bd = proc->main_thread;
        scheduler_insertThread(serial_thread_bd);
        hal_registerInterruptHandler(3, serial_irq_bd, NULL);
    }

    return 0;
}

SCHED_INIT_ROUTINE(serial_thread, INIT_FLAG_DEFAULT, serial_spawn);