/**
 * @file hexahedron/debug/breakpoint.c
 * @brief Breakpoint handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/debugger.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <structs/list.h>
#include <string.h>
#include <errno.h>

/* Breakpoint list */
list_t *breakpoints = NULL;

/* Breakpoint instruction */
#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
#define BREAKPOINT_INSTRUCTION 0xCC // shorthand for INT3
#endif

/**
 * @brief Set a breakpoint at a specified address
 * @param address The address to set the breakpoint at
 * @returns 0 on success, -EEXIST if it exists, or -EINVAL if the address is invalid
 */
int debugger_setBreakpoint(uintptr_t address) {
    foreach(i, breakpoints) {
        if (((breakpoint_t*)i->value)->address == address) {
            return -EEXIST;   
        } 
    }

    if (mem_getPage(NULL, address, 0)->bits.present != 1) {
        // The page that this address is in is not present
        return -EINVAL;
    }

    breakpoint_t *bp = kmalloc(sizeof(breakpoint_t));
    bp->address = address;
    bp->original_instruction = *((uint8_t*)address);
    *((uint8_t*)address) = BREAKPOINT_INSTRUCTION; 

    list_append(breakpoints, bp);

    return 0;
}

/**
 * @brief Removes a breakpoint at a specified address and resets the instruction
 * @param address The address to remove the breakpoint from
 * @returns 0 on success or -EEXIST if it does not exist
 */
int debugger_removeBreakpoint(uintptr_t address) {
    foreach(i, breakpoints) {
        if (((breakpoint_t*)i->value)->address == address) {
            // Found it!
            *((uint8_t*)((breakpoint_t*)i->value)->address) = ((breakpoint_t*)i->value)->original_instruction;
            list_delete(breakpoints, i);
            kfree(i->value);
            kfree(i);
        }
    }

    return -EEXIST;
}

