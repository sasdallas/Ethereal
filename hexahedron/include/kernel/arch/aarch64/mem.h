/**
 * @file hexahedron/include/kernel/arch/aarch64/mem.h
 * @brief aarch64-specific memory systems
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_AARCH64_MEM_H
#define KERNEL_ARCH_AARCH64_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** TYPES ****/

typedef union page {
    // You can manually manipulate these flags
    struct {
        uint64_t present:1;         // Present in memory
        uint64_t table:1;           // Is a table
        uint64_t indx:3;            // MAIR index
        uint64_t ns:1;              // Security
        uint64_t ap:1;              // Access permission
        uint64_t sh:1;              // Shareable
        uint64_t af:1;              // Access flag
        uint64_t address:36;        // Address/frame
        uint64_t reserved:4;        // Reserved
        uint64_t contig:1;          // Contiguous
        uint64_t pxn:1;             // Priviliged eXecute Never
        uint64_t uxn:1;             // Unprivileged eXecute Never
        uint64_t cow:1;             // Copy-on-write
        uint64_t available:3;       // Available!
        uint64_t reserved2:5;       // Reserved
    } bits;

    // Or use the raw flags
    uint64_t data;
} page_t;

/**** DEFINITIONS ****/

#define PAGE_SIZE       0x1000      // 4 KiB

// Page shifting
#define MEM_PAGE_SHIFT  12


// IMPORTANT: THIS IS THE HEXAHEDRON MEMORY MAP CONFIGURED FOR AARCH64
// 0x0000000000000000 - 0x0000700000000000: Userspace region
// 0x0000600000000000 - 0x0000700000000000: Usermode stack. Only a small amount of this is mapped to start with
// 0x0000700000000000 - 0x0000800000000000: DMA region
// 0x0000800000000000 - 0x0000800000400000: Framebuffer memory (NO LONGER IN USE).
// 0xFFFFF00000000000 - 0xFFFFF00000000000: Kernel code in memory
// 0xFFFFFF0000000000 - 0xFFFFFF0000010000: Heap memory 
// 0xFFFFFF8000000000 - 0xFFFFFF9000000000: High base region for identity mapping
// 0xFFFFFFF000000000 - 0xFFFFFFF100000000: MMIO region
// 0xFFFFFFFF00000000 - 0xFFFFFFFF80000000: Driver memory space

#define MEM_USERSPACE_REGION_START  (uintptr_t)0x0000000000000000
#define MEM_USERSPACE_REGION_END    (uintptr_t)0x0000070000000000

#define MEM_USERMODE_STACK_REGION   (uintptr_t)0x0000060000000000 
#define MEM_DMA_REGION              (uintptr_t)0x0000070000000000
#define MEM_USERMODE_DEVICE_REGION  (uintptr_t)0x0000400000000000
#define MEM_FRAMEBUFFER_REGION      (uintptr_t)0x0000080000000000
#define MEM_HEAP_REGION             (uintptr_t)0xFFFFFF0000000000
#define MEM_PHYSMEM_MAP_REGION      (uintptr_t)0xFFFFFF8000000000 // !!!: PHYSMEM_MAP is close to kernel heap
#define MEM_MMIO_REGION             (uintptr_t)0xFFFFFFF000000000
#define MEM_DRIVER_REGION           (uintptr_t)0xFFFFFFFF00000000

#define MEM_MMIO_REGION_SIZE        (uintptr_t)0x0000000100000000
#define MEM_USERMODE_STACK_SIZE     (uintptr_t)0x0000010000000000 
#define MEM_DMA_REGION_SIZE         (uintptr_t)0x0000000100000000
#define MEM_PHYSMEM_MAP_SIZE        (uintptr_t)0x0000001000000000
#define MEM_DRIVER_REGION_SIZE      (uintptr_t)0x0000000080000000

/**** MACROS ****/

#define MEM_ALIGN_PAGE(addr) ((addr + PAGE_SIZE) & ~0xFFF) // Align an address to the nearest page
#define MEM_ALIGN_PAGE_DESTRUCTIVE(addr) (addr & ~0xFFF) // Align an address to the nearest page, discarding any bits

#define MEM_SET_FRAME(page, frame) (page->bits.address = ((uintptr_t)frame >> MEM_PAGE_SHIFT))      // Set the frame of a page. Used because of our weird union thing.
#define MEM_GET_FRAME(page) (page->bits.address << MEM_PAGE_SHIFT)                                  // Get the frame of a page. Used because of our weird union thing.

#define MEM_IS_CANONICAL(addr) (((addr & 0xFFFF000000000000) == 0xFFFF000000000000) || !(addr & 0xFFFF000000000000))    // Ugly macro to verify if an address is canonical

/* AP flags */
// https://developer.arm.com/documentation/102376/0100/Permissions-attributes
#define MEM_AP_USER_FLAG    0x1
#define MEM_AP_RO_FLAG      0x2

/* ACCESSIBLE MACROS */
#define PAGE_IS_PRESENT(pg) (pg && pg->bits.present)
#define PAGE_IS_WRITABLE(pg) (pg && !(pg->bits.ap & MEM_AP_RO_FLAG))
#define PAGE_IS_USERMODE(pg) (pg && pg->bits.ap & MEM_AP_USER_FLAG)
#define PAGE_IS_COW(pg) (pg && pg->bits.cow)
#define PAGE_IS_DIRTY(pg) (1)

#define PAGE_PRESENT(pg) (pg->bits.present)
#define PAGE_COW(pg) (pg->bits.cow)
#define PAGE_FRAME_RAW(pg) (pg->bits.address)

/**** FUNCTIONS ****/

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will identity map the kernel into memory and setup page tables.
 * For x86_64, it also sets up the PMM allocator.
 * 
 * @param mem_size The size of memory (aka highest possible address)
 * @param first_free_page The first free page after the kernel
 */
void mem_init(uintptr_t mem_size, uintptr_t first_free_page);

#endif