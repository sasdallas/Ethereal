/**
 * @file drivers/misc/ps2/ps2.h
 * @brief PS/2 driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef PS2_H
#define PS2_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define PS2_DATA        0x60    // Data port
#define PS2_STATUS      0x64    // Status port
#define PS2_COMMAND     0x64    // Command port

#define PS2_STATUS_OUTPUT_FULL      0x01    // Output buffer full
#define PS2_STATUS_INPUT_FULL       0x02    // Input buffer full
#define PS2_STATUS_SYSTEM_FLAG      0x04    // System flag
#define PS2_STATUS_COMMAND_DATA     0x08    // Command/data flag
#define PS2_STATUS_TIMEOUT          0x40    // Timeout error
#define PS2_STATUS_PARITY           0x80    // Parity error

#define PS2_COMMAND_READ_CCB            0x20    // Read byte 0 from internal RAM
#define PS2_COMMAND_WRITE_CCB           0x60    // Write CCB
#define PS2_COMMAND_DISABLE_PORT2       0xA7    // Disables second PS/2 port
#define PS2_COMMAND_ENABLE_PORT2        0xA8    // Enables second PS/2 port
#define PS2_COMMAND_TEST_PORT2          0xA9    // Test second PS/2 port
#define PS2_COMMAND_TEST_CONTROLLER     0xAA    // Test controller
#define PS2_COMMAND_TEST_PORT1          0xAB    // Test first PS/2 port
#define PS2_COMMAND_DISABLE_PORT1       0xAD    // Disables first PS/2 port
#define PS2_COMMAND_ENABLE_PORT1        0xAE    // Enables first PS/2 port
#define PS2_COMMAND_READ_CONOUT         0xD0    // Read controller output port
#define PS2_COMMAND_WRITE_CONOUT        0xD1    // Write controller output port
#define PS2_COMMAND_WRITE_PORT2         0xD4    // Command to write to PORT2

// Device commands
#define PS2_DEVCMD_IDENTIFY             0xF2
#define PS2_DEVCMD_ENABLE_SCANNING      0xF4
#define PS2_DEVCMD_DISABLE_SCANNING     0xF5
#define PS2_DEVCMD_RESET                0xFF

// CCB
#define PS2_CCB_PORT1INT                0x01    // First PS/2 port interrupt
#define PS2_CCB_PORT2INT                0x02    // Second PS/2 port interrupt
#define PS2_CCB_SYSTEM_FLAG             0x04    // System flag
#define PS2_CCB_PORT1CLK                0x10    // First PS/2 port clock
#define PS2_CCB_PORT2CLK                0x20    // Second PS/2 port clock
#define PS2_CCB_PORTTRANSLATION         0x40    // Port translation

// Controller Output Port
#define PS2_CONOUT_SYSTEM_RESET         0x01    // System reset bit
#define PS2_CONOUT_A20_GATE             0x02    // A20 gate
#define PS2_CONOUT_PORT2_CLK            0x04    // Second PS/2 port clock
#define PS2_CONOUT_PORT2_DATA           0x08    // Second PS/2 port data
#define PS2_CONOUT_PORT1_FULL           0x10    // First PS/2 port buffer full
#define PS2_CONOUT_PORT2_FULL           0x20    // Second PS/2 port buffer full
#define PS2_CONOUT_PORT1_CLK            0x40    // First PS/2 port clock
#define PS2_CONOUT_PORT1_DATA           0x80    // First PS/2 port data

// Test results
#define PS2_PORT_TEST_PASS              0x00
#define PS2_CONTROLLER_TEST_PASS        0x55
#define PS2_SELF_TEST_PASS              0xAA
#define PS2_SELF_TEST_FAIL              0xFC

// IRQs
#define PS2_KEYBOARD_IRQ                1
#define PS2_MOUSE_IRQ                   12

// Keyboard shenanigans
#define PS2_KEYBOARD_SET_SCANCODE       0xF0    // Get/set scancode
#define PS2_KEYBOARD_SCANCODE           2       // Scancode to use

// Mouse shenanigans
#define PS2_MOUSE_GET_DEVICE_ID             0xF2    // Get device ID
#define PS2_MOUSE_SET_DEFAULTS              0xF6    // Set device defaults
#define PS2_MOUSE_ENABLE_DATA_REPORTING     0xF4    // Enable data reporting
#define PS2_MOUSE_SET_SAMPLE_RATE           0xF3    // Set sampe rate
#define PS2_MOUSE_RESET                     0xFF    // Reset the mouse

// Mouse data byte
#define PS2_MOUSE_DATA_LEFTBTN              0x01
#define PS2_MOUSE_DATA_RIGHTBTN             0x02
#define PS2_MOUSE_DATA_MIDDLEBTN            0x04
#define PS2_MOUSE_DATA_SIGN_X               0x10
#define PS2_MOUSE_DATA_SIGN_Y               0x20
#define PS2_MOUSE_DATA_X_OVERFLOW           0x40
#define PS2_MOUSE_DATA_Y_OVERFLOW           0x80

/* Responses */
#define PS2_ACK                             0xFA
#define PS2_RESEND                          0xFE


/**** FUNCTIONS ****/

/**
 * @brief Read a PS/2 byte
 * @returns LESS than 0 on error
 */
int ps2_readByte();

/**
 * @brief Initialize the PS/2 keyboard
 */
void kbd_init(uint8_t port);

/**
 * @brief Initialize the PS/2 mouse
 */
void mouse_init(uint8_t port);

/**
 * @brief Wait for input buffer to be empty
 * @returns 1 on timeout
 */
int ps2_waitForInputClear();

/**
 * @brief Wait for output buffer to be full
 * @returns 1 on timeout
 */
int ps2_waitForOutput();

/**
 * @brief Send PS/2 command (single-byte)
 * @param command The command byte to send
 */
int ps2_sendCommand(uint8_t command);

/**
 * @brief Send PS/2 command (return value)
 * @param command The command byte to send
 * @returns PS2_DATA value or -1 on failure
 */
int ps2_sendCommandResponse(uint8_t command);

/**
 * @brief Send a multi-byte command
 * @param command The command byte to send
 * @param data The data byte to send
 */
void ps2_sendCommandParameter(uint8_t command, uint8_t data);

/**
 * @brief Send a command to a device
 */
void ps2_sendDevice(uint8_t port, uint8_t data);

/**
 * @brief Send a command to a device and get an ACK response
 * Will wait for a PS2 ACK response, resend up to 10 times.
 * @returns 0 on success
 */
int ps2_sendDeviceACK(uint8_t port, uint8_t data);


#endif