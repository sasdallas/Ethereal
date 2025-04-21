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
#include <kernel/generic_mboot.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/debug.h>
#include <kernel/config.h>
#include <kernel/panic.h>
#include <kernel/misc/pool.h>
#include <structs/list.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#endif

/**** VARIABLES ****/

/* Log method */
#define LOG(status, format, ...) dprintf_module(status, "DEBUGGER", format, ## __VA_ARGS__)
#define UNIMPLEMENTED(func) kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "debugger", "*** %s\n", func)

/* Debugger interface port */
serial_port_t *debugger_port = NULL;

/* Debugger lock */ 
spinlock_t *debug_lock = NULL;

/* Breakpoint state */
int debugger_inBreakpointState = 0;

/* Breakpoints */
extern list_t *breakpoints;

/**** FUNCTIONS ****/

/**
 * @brief Handshake with debugger
 * @returns Non-zero on successful handshake
 */
int debugger_handshake() {
    // Create a packet containing kernel information and system information
    json_value *data = json_object_new(10);
    
    // Create version info (major, minor, lower)
    json_value *version_info = json_object_new(4);
    json_object_push(version_info, "major", json_integer_new(__kernel_version_major));
    json_object_push(version_info, "minor", json_integer_new(__kernel_version_minor));
    json_object_push(version_info, "lower", json_integer_new(__kernel_version_lower));
    json_object_push(version_info, "codename", json_string_new(__kernel_version_codename));
    json_object_push(data, "version", version_info);

    // Create build information (date/time/debug/architecture/etc.)
    json_value *build_info = json_object_new(6);
    json_object_push(build_info, "date", json_string_new(__kernel_build_date));
    json_object_push(build_info, "time", json_string_new(__kernel_build_time));
    json_object_push(build_info, "conf", json_string_new(__kernel_build_configuration));
    json_object_push(build_info, "arch", json_string_new(__kernel_architecture));
    json_object_push(build_info, "compiler", json_string_new(__kernel_compiler));
    json_object_push(data, "build", build_info);


    // Create heap and allocator information (location/usage/name)

extern uintptr_t mem_kernelHeap;
extern uintptr_t mem_identityMapCacheSize;
extern pool_t *mem_mapPool;

    json_value *heap_info = json_object_new(7);
    char *temp_string = kmalloc(512);

    snprintf(temp_string, 16, "%x", mem_kernelHeap);
    json_object_push(heap_info, "heap_location", json_string_new(temp_string));
    json_object_push(heap_info, "im_pool_usage", json_integer_new(mem_mapPool ? mem_mapPool->allocated : 0));
    json_object_push(heap_info, "im_cache_size", json_integer_new(mem_identityMapCacheSize));

    json_object_push(heap_info, "total_pmm_blocks", json_integer_new(pmm_getMaximumBlocks()));
    json_object_push(heap_info, "free_pmm_blocks", json_integer_new(pmm_getFreeBlocks()));
    json_object_push(heap_info, "pmm_block_size", json_integer_new(PMM_BLOCK_SIZE));
    
    allocator_info_t *allocInfo = alloc_getInfo();
    snprintf(temp_string, 128, "%s %i.%i", allocInfo->name, allocInfo->version_major, allocInfo->version_minor);
    json_object_push(heap_info, "alloc_name", json_string_new(temp_string));

    json_object_push(data, "heap", heap_info);

    // Create system information
    generic_parameters_t *parameters = arch_get_generic_parameters();
    json_value *sys_info = json_object_new(4);
    json_object_push(sys_info, "memory_size", json_integer_new(parameters->mem_size));
    json_object_push(sys_info, "bootloader", json_string_new(parameters->bootloader_name));
    json_object_push(sys_info, "cmdline", json_string_new(parameters->kernel_cmdline));

    // Create memory map information (subset of sysinfo)
    // !!! json_array_new(0) appears to have issues or something (breaks allocator), so this is disabled.
    // TODO: This seems to have been fixed. We should be able to uncomment this code.
    // json_value *mmap_info = json_array_new(0);

    // generic_mmap_desc_t *desc = parameters->mmap_start;
    // while (desc) {
    //     json_value *desc_value = json_object_new(4);
    //     snprintf(temp_string, 16, "%016llX", desc->address);
    //     json_object_push(desc_value, "address", json_string_new(temp_string));
    //     snprintf(temp_string, 16, "%016llX", desc->length);
    //     json_object_push(desc_value, "length", json_string_new(temp_string));
    //     json_object_push(desc_value, "type", json_integer_new(desc->type));
    //     json_array_push(mmap_info, desc_value);

    //     desc = desc->next;
    // }
    // json_object_push(sys_info, "mmap", mmap_info);

    json_object_push(data, "sysinfo", sys_info);

    // Finally, collect a bit of image information
extern uint32_t __kernel_start, __text_start, __rodata_start, __data_start, __bss_start, __kernel_end;
    json_value *image_info = json_object_new(6);

    snprintf(temp_string, 16, "%08x", &__kernel_start);
    json_object_push(image_info, "kernel_start", json_string_new(temp_string));    
    snprintf(temp_string, 16, "%08x", &__text_start);
    json_object_push(image_info, "text", json_string_new(temp_string));    
    snprintf(temp_string, 16, "%08x", &__rodata_start);
    json_object_push(image_info, "rodata", json_string_new(temp_string));    
    snprintf(temp_string, 16, "%08x", &__data_start);
    json_object_push(image_info, "data", json_string_new(temp_string));    
    snprintf(temp_string, 16, "%08x", &__bss_start);
    json_object_push(image_info, "bss", json_string_new(temp_string));    
    snprintf(temp_string, 16, "%08x", &__kernel_end);
    json_object_push(image_info, "kernel_end", json_string_new(temp_string));    

    json_object_push(data, "image", image_info);

    // Send it off!
    int packet = debugger_sendPacket(PACKET_TYPE_HELLO, data);
    if (packet != 0) goto _cleanup;

    // Response!
    debug_packet_t *response = debugger_receivePacket(__debugger_wait_time);
    if (!response) goto _cleanup;

    json_builder_free(data);
    kfree(temp_string);
    return 1;

_cleanup:
    json_builder_free(data);
    kfree(temp_string);

    return 0;
}

/**
 * @brief Permanent loop waiting for packets until a continue one is received
 */
void debugger_packetLoop() {
    while (1) {
        debug_packet_t *packet = debugger_receivePacket(0);

        json_value *type = debugger_getPacketField(packet, "type");
        if (!type) {
            LOG(WARN, "Invalid packet received (no type field/bad data)\n");
            goto _next_packet;
        }


        json_value *data  = debugger_getPacketField(packet, "data");
        if (!data) {
            LOG(ERR, "Invalid packet received (no data field/bad data)\n");
            goto _next_packet;
        }

        json_value *resp_data; // gcc will whine if this is declared mult. times

        switch (type->u.integer) {
            case PACKET_TYPE_CONTINUE:
                LOG(INFO, "Continue packet received - exiting breakpoint state\n");
                json_builder_free(packet);
                return;

            case PACKET_TYPE_READMEM: ;
                json_value *addr_val = debugger_getPacketField(data, "addr");
                json_value *length_val = debugger_getPacketField(data, "length");

                if (!addr_val || !length_val) {
                    LOG(ERR, "Invalid packet received (addr/length field not found in data)\n");
                    goto _next_packet;
                }

                char *addr_str = addr_val->u.string.ptr;
                uintptr_t addr = strtoull(addr_str, NULL, 16);

                int length = length_val->u.integer;
                LOG(DEBUG, "READMEM 0x%x %i\n", addr, length);

                // Check the memory to make sure its okay
                json_value *arr = json_array_new(MEM_ALIGN_PAGE(length));
                for (uintptr_t cur = addr; cur < MEM_ALIGN_PAGE(addr + length); cur += PAGE_SIZE) {
                    if (mem_getPage(NULL, cur, 0)->bits.present == 0) {
                        json_value *error = json_object_new(1);
                        char err_str[32];
                        snprintf(err_str, 32, "%x not present", addr);
                        json_object_push(error, "error", json_string_new("Page not present"));
                        debugger_sendPacket(PACKET_TYPE_READMEM, error);
                        json_builder_free(arr);
                        break;
                    }
                }

                for (uint8_t *cur = (uint8_t*)addr; cur < (uint8_t*)(addr + (uintptr_t)length); cur++) {
                    json_array_push(arr, json_integer_new(*cur));
                }

                resp_data = json_object_new(1);
                json_object_push(resp_data, "buffer", arr);
                debugger_sendPacket(PACKET_TYPE_READMEM, resp_data);
                json_builder_free(resp_data);
                break;
            
            case PACKET_TYPE_WRITEMEM: ;
                UNIMPLEMENTED("PACKET_TYPE_WRITEMEM");
                __builtin_unreachable();
            
            case PACKET_TYPE_BP_UPDATE: ;
                json_value *address = debugger_getPacketField(data, "address");
                json_value *operation = debugger_getPacketField(data, "operation");

                if (!address || !operation || address->type != json_string || operation->type != json_integer) {
                    LOG(ERR, "Invalid packet received (addr/operation field not found in data)\n");
                    goto _next_packet;
                }


                int success;
                if (operation->u.integer == 1) {
                    // Add breakpoint
                    LOG(DEBUG, "Adding breakpoint to %s\n", address->u.string.ptr);
                    success = debugger_setBreakpoint(strtoull(address->u.string.ptr, NULL, 16));
                } else {
                    // Remove breakpoint
                    LOG(DEBUG, "Removing breakpoint from %s\n", address->u.string.ptr);
                    success = debugger_removeBreakpoint(strtoull(address->u.string.ptr, NULL, 16));
                }

                resp_data = json_object_new(1);
                json_object_push(resp_data, "return_value", json_integer_new(success));
                debugger_sendPacket(PACKET_TYPE_BP_UPDATE, resp_data);
                json_builder_free(resp_data);
                break;
        }

    _next_packet:
        // Free the packet
        json_builder_free(packet);
    }
}

/**
 * @brief Returns whether we are in a breakpoint state
 */
int debugger_isInBreakpointState() {
    return debugger_inBreakpointState;
}


#if defined(__ARCH_I386__)
/**
 * @brief Interrupt 3 breakpoint handler
 */
int debugger_breakpointHandler(uint32_t exception_number, registers_t *regs, extended_registers_t *extended) {
    // First thing - check if breakpoint needs to be resolved.

    json_value *breakpoint_data = json_object_new(2);

    // Encode the registers structure as bytes
    json_value *registers_encoded = json_array_new(sizeof(regs));
    uint8_t *ptr = (uint8_t*)regs;
    for (size_t i = 0; i < sizeof(registers_t); i++) {
        json_array_push(registers_encoded, json_integer_new((int)ptr[i]));
    }

    json_object_push(breakpoint_data, "registers", registers_encoded);

    // Now do the same for the extended registers structure
    json_value *ext_registers_encoded = json_array_new(sizeof(extended));
    ptr = (uint8_t*)extended;
    for (size_t i = 0; i < sizeof(extended_registers_t); i++) {
        json_array_push(ext_registers_encoded, json_integer_new((int)ptr[i]));
    }

    json_object_push(breakpoint_data, "extended_registers", ext_registers_encoded);

    // Send the packet!
    LOG(DEBUG, "Entering breakpoint state (INT3 triggered)\n");
    debugger_sendPacket(PACKET_TYPE_BREAKPOINT, breakpoint_data);

    // Enter a permanent loop waiting for more packets
    debugger_inBreakpointState = 1;
    debugger_packetLoop();
    debugger_inBreakpointState = 0;

    return 0;
}
#endif


/**
 * @brief Initialize the debugger. This will wait for a hello packet if configured.
 * @param port The serial port to use
 * @returns 1 on a debugger connecting, 0 on a debugger not connecting, and anything below zero is a bad input.
 */
int debugger_initialize(serial_port_t *port) {
    if (!port || !port->read || !port->write) return -EINVAL;
    debugger_port = port;
    debug_lock = spinlock_create("debugger_lock");
    breakpoints = list_create("breakpoints");

    LOG(INFO, "Trying to initialize the debugger...\n");

    

    int handshake = debugger_handshake();
    if (!handshake) goto _no_debug;

#if defined(__ARCH_I386__)
    // Register an exception handler for INT3
    hal_registerExceptionHandler(0x03, debugger_breakpointHandler);
#endif

    // Enter breakpoint state and wait for packets
    BREAKPOINT(); 

    return 1;

_no_debug:
    debugger_port = NULL; // Null these out to prevent any stupidity
    return 0;
}

/**
 * @brief Returns whether a debugger is connected
 */
int debugger_isConnected() {
    return debugger_port == NULL ? 0 : 1;
}

