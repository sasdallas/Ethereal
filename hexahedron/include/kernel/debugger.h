/**
 * @file hexahedron/include/kernel/debugger.h
 * @brief Debugger header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_DEBUGGER_H
#define KERNEL_DEBUGGER_H

/**** INCLUDES ****/

#include <stdint.h>
#include <stddef.h>
#include <kernel/drivers/serial.h>

#include <structs/json-builder.h>
#include <structs/json.h>

/**** TYPES ****/

// Debug packets are just JSON objects
typedef json_value debug_packet_t;

/* Breakpoint structure */
typedef struct _breakpoint {
    uintptr_t   address;
    uint8_t     original_instruction;
} breakpoint_t;

/**** DEFINITIONS ****/

// General
#define MAXIMUM_PACKET_LENGTH   4       // WARNING: THIS IS IN PLACES. With 4, you can go up to 9999 (4 #s)

// Packet bytes
#define PACKET_START            0xAA
#define PACKET_END              0xBB

// Packet types
#define PACKET_TYPE_HELLO       0x01    // This is a hello packet
#define PACKET_TYPE_HELLO_RESP  0x02    // Debugger's response to our HELLO packet.
#define PACKET_TYPE_BREAKPOINT  0x03    // Kernel will register handlers on INT3 for this and report state back.
#define PACKET_TYPE_CONTINUE    0x04    // Continue execution
#define PACKET_TYPE_READMEM     0x05    // Read memory request
#define PACKET_TYPE_WRITEMEM    0x06    // Write memory request
#define PACKET_TYPE_PANIC       0x07    // Panic! Sent by kernel
#define PACKET_TYPE_BP_UPDATE   0x08    // Update breakpoint (add/remove)

/**** MACROS ****/

#define BREAKPOINT() if (debugger_isConnected()) asm volatile ("int $0x03")

/**** FUNCTIONS ****/

/**
 * @brief Initialize the debugger. This will wait for a hello packet if configured.
 * @param port The serial port to use
 * @returns 1 on a debugger connecting, 0 on a debugger not connecting, and anything below zero is a bad input.
 */
int debugger_initialize(serial_port_t *port);

/**
 * @brief Send a packet to the debugger if connected
 * @param type The type of packet to send
 * @param object Any JSON object. This will be tied to a key labelled "data".
 * @returns 0 on success, non-zero is failure.
 */
int debugger_sendPacket(uint32_t type, json_value *object);

/**
 * @brief Waits to receive a packet
 * @param timeout How long to wait before stopping
 * @returns NULL on nothing found, else a packet
 */
debug_packet_t *debugger_receivePacket(size_t timeout);

/**
 * @brief Create a new packet that you can add to
 * @param type The type of the packet
 * @returns A packet object or NULL on failure
 */
debug_packet_t *debugger_createPacket(uint32_t type);

/**
 * @brief Get a field in a packet
 * @param packet The packet to check
 * @param field The field to look for
 * @returns The value of the field or NULL
 */
json_value *debugger_getPacketField(debug_packet_t *packet, char *field);

/**
 * @brief Returns whether a debugger is connected
 */
int debugger_isConnected();

/**
 * @brief Returns whether we are in a breakpoint state
 */
int debugger_isInBreakpointState();

/**
 * @brief Set a breakpoint at a specified address
 * @param address The address to set the breakpoint at
 * @returns 0 on success, -EEXIST if it exists, or -EINVAL if the address is invavlid
 */
int debugger_setBreakpoint(uintptr_t address);

/**
 * @brief Removes a breakpoint at a specified address and resets the instruction
 * @param address The address to remove the breakpoint from
 * @returns 0 on success or -EEXIST if it does not exist
 */
int debugger_removeBreakpoint(uintptr_t address);

#endif