/**
 * @file hexahedron/debug/debugger.c
 * @brief Provides the main interface of the Hexahedron debugger.
 * 
 * The debugger and the kernel communicate via JSON objects.
 * On startup, the kernel will wait for a hello packet from the debugger, then start
 * communication from there.
 * 
 * Packets are constructed like so:
 * - Newline
 * - Initial @c PACKET_START byte
 * - Length of the packets (int)
 * - JSON string
 * - Final @c PACKET_END byte
 * - Newline
 * 
 * The JSON itself isn't very important (you can provide your own JSON
 * fields) - the main important thing is the pointer to the packet's structure.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#include <kernel/debugger.h>

/**
 * @brief Initialize the debugger. This will wait for a hello packet if configured.
 * @param port The serial port to use
 * @returns 1 on a debugger connecting, 0 on a debugger not connecting, and anything below zero is a bad input.
 */
int debugger_initialize(serial_port_t *port) {
    // The debugger was removed in Hexahedron 2.0.0
    return 0;
}
