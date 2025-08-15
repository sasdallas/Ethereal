/**
 * @file hexahedron/debug/packet.c
 * @brief Handles debugger packets
 * 
 * @see debugger.c for a packet explanation
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/debugger.h>
#include <kernel/debug.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

/* Debug port */
extern serial_port_t *debugger_port;

/* Debug spinlock */
extern spinlock_t *debug_lock;

/* Log method */
#define LOG(status, message, ...) dprintf_module(status, "DEBUGGER:PKT", message, ## __VA_ARGS__)

/**** GCC ****/
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized" // debugger_parseJSON takes in a pointer and allocates it 

/**** FUNCTIONS ****/

/**
 * @brief Parse JSON returned
 * @param json_str The JSON string to parse
 * @param debug_packet The packet to output in (you don't need to allocate)
 * @returns Error string on failure. Use this string if @c debug_packet is NULL.
 */
static char *debugger_parseJSON(char *json_str, debug_packet_t **debug_packet) {
    if (!json_str || !debug_packet) return "lmao";

    json_settings settings = {};
    settings.value_extra = json_builder_extra; // We use the json-builder interop

    char *error = kmalloc(128);
    debug_packet_t *packet = json_parse_ex(&settings, json_str, strlen(json_str), error);
    *debug_packet = packet;


    if (debug_packet) { kfree(error); return NULL; }

    return error;
}

/**
 * @brief Send a packet, internally
 * @param packet The packet to send
 */
static void debugger_sendPacketInternal(debug_packet_t *packet) {
    if (!packet) return;
    spinlock_acquire(debug_lock);

    char *buffer = kmalloc(json_measure(packet));
    json_serialize(buffer, packet);

    // Print the packet
    serial_print((void*)debugger_port, '\n');
    serial_print((void*)debugger_port, PACKET_START);
    serial_portPrintf(debugger_port, "%i", strlen(buffer));
    serial_portPrintf(debugger_port, "%s", buffer);
    serial_print((void*)debugger_port, PACKET_END);
    serial_print((void*)debugger_port, '\n');

    spinlock_release(debug_lock);
}

/**
 * @brief Receive a packet, internally
 * @param timeout   The timeout to wait for each section. This may be exceeded - see @c debugger_receivePacket
 */
static debug_packet_t *debugger_receivePacketInternal(size_t timeout_ms) {
    // Lock it!
    spinlock_acquire(debug_lock);

    // Wait for a PACKET_START byte or timeout.
    uint8_t start_byte = 0x00;
    uint64_t timeout = (now() * 1000) + timeout_ms;
    while ((timeout_ms == 0) ? 1 : timeout > (now() * 1000)) {
        if (start_byte == PACKET_START) break;
        start_byte = debugger_port->read(debugger_port, timeout_ms);
    }

    // Got anything?
    if (start_byte != PACKET_START) goto _cleanup; 

    // We found a start byte!
    
    // Try to read the length. The maximum length for packets is defined in MAXIMUM_PACKET_LENGTH,
    // and it is terms of length of the number.
    // !!! The debugger MUST send this length in 4-digits, to make my life easier.
    char length_str[MAXIMUM_PACKET_LENGTH];
    int i = 0;
    while (i < MAXIMUM_PACKET_LENGTH) {
        length_str[i] = debugger_port->read(debugger_port, timeout_ms);
        if (length_str[i] == 0) goto _cleanup;; // The timeout was triggered
        i++;
    }

    // Convert to integer
    int length = strtol(length_str, NULL, 10);
    if (length <= 0) goto _cleanup; // Invalid length

    // This length should be the length of the entire JSON string.
    char *json_string = kmalloc(length);
    if (serial_readBuffer(json_string, debugger_port, length, timeout_ms) < length) {
        // Something must've gone wrong.
        kfree(json_string);
        goto _cleanup;
    }

    // Null-terminate
    json_string[length] = 0;

    // Parse!
    debug_packet_t *packet = NULL; 
    char *error = debugger_parseJSON(json_string, &packet);

    if (packet == NULL) {
        // Something went wrong
        LOG(ERR, "Parsing packet failed. Error: %s\n", error);
        LOG(ERR, "\tJSON string: %s\n", json_string);
        kfree(json_string);
        goto _cleanup;
    }

    kfree(json_string);

    // Return the packet
    spinlock_release(debug_lock);
    return packet;

_cleanup:
    spinlock_release(debug_lock);
    return NULL;
}


/**** EXPOSED FUNCTIONS ****/

/**
 * @brief Create a new packet that you can add to
 * @param type The type of the packet
 * @returns A packet object or NULL on failure
 */
debug_packet_t *debugger_createPacket(uint32_t type) {
    // Create the packet structure
    debug_packet_t *packet = json_object_new(3); // 3 objects for type, time, and data
    if (!packet) {
        LOG(WARN, "json_object_new returned NULL\n");
        return NULL;
    }

    // Get time first
    // time_t rawtime;
    // time(&rawtime);
    // struct tm *tm = localtime(&rawtime);

    // Now we should pack some information
    json_object_push(packet, "type", json_integer_new(type)); // "type": type
    // json_object_push(packet, "time", json_string_new(asctime(tm))); // "time": time

    return packet;
}


/**
 * @brief Send a packet to the debugger if connected
 * @param type The type of packet to send
 * @param object Any JSON object. This will be tied to a key labelled "data".
 * @returns 0 on success, non-zero is failure.
 */
int debugger_sendPacket(uint32_t type, json_value *object) {
    if (!debugger_port) return -EBADF;
    if (!object) return -EINVAL;

    // Create the packet structure
    debug_packet_t *packet = debugger_createPacket(type);

    // Pack the object
    json_object_push(packet, "data", object);

    // Send the packet
    debugger_sendPacketInternal(packet);

    LOG(DEBUG, "PACKET_SEND 0x%x 0x%x\n", type, object);
    return 0;
}

/**
 * @brief Waits to receive a packet
 * @param timeout   The time to wait before giving up - 0 means infinitely waiting.
 *                  
 * @warning Specifying @c timeout doesn't mean that the packet will be returned within timeout.
 *          This timeout is the maximum amount of time to wait before giving up on a byte.
 *          The function itself IS NOT under effect of the timeout, but if one byte takes too long then it triggers.
 *          
 * @returns NULL on nothing found, else a packet
 */
debug_packet_t *debugger_receivePacket(size_t timeout) {
    if (!debugger_port) return NULL;

    debug_packet_t *packet = debugger_receivePacketInternal(timeout);
    if (packet) LOG(DEBUG, "PACKET_RECV 0x%x\n", packet);
    return packet;
}

/**
 * @brief Get a field in a packet
 * @param packet The packet to check
 * @param field The field to look for
 * @returns The value of the field or NULL
 */
json_value *debugger_getPacketField(debug_packet_t *packet, char *field) {
    if (packet->type != json_object) return NULL;

    for (unsigned int i = 0; i < packet->u.object.length; i++) {
        if (!strncmp(packet->u.object.values[i].name, field, strlen(packet->u.object.values[i].name))) {
            // These don't have to end in a \0 but if their lengths match then its good
            if (strlen(packet->u.object.values[i].name) != strlen(field)) continue;
            return packet->u.object.values[i].value;
        }
    }

    return NULL;
}
