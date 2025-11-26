/**
 * @file hexahedron/include/kernel/mm/vmm.h
 * @brief Memory system functions
 * 
 * This file is an interface for the memory mapper
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/mm/vmm.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/mem.h> // Arch-specific definitions, like directory, entries, etc)
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/mem.h>
#elif defined(__ARCH_AARCH64__)
#include <kernel/arch/aarch64/mem.h>
#else
#error "Unsupported architecture for memory management"
#endif

#include <kernel/mem/regions.h>


/**** DEFINITIONS ****/

// General flags
#define MEM_DEFAULT             0x00    // Default settings to everything. For mem_allocatePage: usermode, writable, and present

// Flags to mem_getPage
#define MEM_CREATE              0x01    // Create the page. Commonly used with mem_getPage during mappings

// Flags to mem_allocatePage
#define MEM_PAGE_KERNEL             0x02    // The page is kernel-mode only
#define MEM_PAGE_READONLY           0x04    // The page is read-only
#define MEM_PAGE_WRITETHROUGH       0x08    // The page is marked as writethrough
#define MEM_PAGE_NOT_CACHEABLE      0x10    // The page is not cacheable
#define MEM_PAGE_NOT_PRESENT        0x20    // The page is not present in memory
#define MEM_PAGE_NOALLOC            0x40    // Do not allocate the page and instead use what was given
#define MEM_PAGE_FREE               0x80    // Free the page. Sets it to zero if specified in mem_allocatePage
#define MEM_PAGE_NO_EXECUTE         0x100   // (x86_64 only) Set the page as non-executable.
#define MEM_PAGE_WRITE_COMBINE      0x200   // Sets up the page as write-combining if the architecture supports it

// Flags to mem_allocate
#define MEM_ALLOC_CONTIGUOUS        0x01    // Allocate contiguous blocks of memory, rather than fragmenting PMM blocks
#define MEM_ALLOC_FRAGILE           0x02    // Fragile, meaning that a check is first run to determine if any pages are already marked as present
#define MEM_ALLOC_HEAP              0x04    // Allocate from the heap
#define MEM_ALLOC_CRITICAL          0x08    // This allocation is critical, on failure terminate kernel

// Flags to mem_validate
#define PTR_USER                    0x01    // Usermode or kernel pointer (STRICT: Only usermode pointer)
#define PTR_READONLY                0x02    // Read-only or read-write pointer (STRICT: Only read-only)
#define PTR_STRICT                  0x04    // Strict pointer validation

/**** FUNCTIONS ****/



#endif