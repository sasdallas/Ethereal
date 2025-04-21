/**
 * @file hexahedron/include/kernel/arch/x86_64/mem.h
 * @brief x86_64-specific memory systems
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_MEM_H
#define KERNEL_ARCH_X86_64_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** TYPES ****/

typedef union page {
    // You can manually manipulate these flags
    struct {
        uint64_t present:1;         // Present in memory
        uint64_t rw:1;              // R/W
        uint64_t usermode:1;        // Usermode
        uint64_t writethrough:1;    // Writethrough
        uint64_t cache_disable:1;   // Uncacheable
        uint64_t accessed:1;        // Accessed
        uint64_t available1:1;      // Free bits!
        uint64_t size:1;            // Page size - 4MiB or 4KiB
                                    // IMPORTANT: This is also the PAT bit for PTEs
        uint64_t global:1;          // Global
        uint64_t available2:3;      // Free bits!
        uint64_t address:28;        // The page data
        uint64_t reserved:12;       // These should be set to 0
        uint64_t cow:1;             // Copy on write, part of available and impl-specific
        uint64_t available3:10;     // Free bits!
        uint64_t nx:1;              // No execute bit
    } bits;
    
    // Or use the raw flags
    uint64_t data;
} page_t;

/**** DEFINITIONS ****/

#define PAGE_SIZE       0x1000      // 4 KiB
#define PAGE_SIZE_LARGE 0x200000    // 2 MiB

// Page shifting
#define MEM_PAGE_SHIFT  12


// IMPORTANT: THIS IS THE HEXAHEDRON MEMORY MAP CONFIGURED FOR I386
// 0x0000000000000000 - 0x0000000000200000: Kernel code - this can be expanded a decent amount.
// 0x00000000A0000000 - 0x00000000F0000000: DMA region (in low memory)
// 0x0000600000000000 - 0x0000700000000000: Usermode stack. Only a small amount of this is mapped to start with
// 0x0000800000000000 - 0x0000800000400000: Framebuffer memory (todo: this can probably be relocated).  
// 0xFFFFFF0000000000 - 0xFFFFFF0000010000: Heap memory 
// 0xFFFFFF8000000000 - 0xFFFFFF9000000000: High base region for identity mapping
// 0xFFFFFFF000000000 - 0xFFFFFFF100000000: MMIO region
// 0xFFFFFFFF00000000 - 0xFFFFFFFF80000000: Driver memory space

#define MEM_DMA_REGION              (uintptr_t)0x00000000A0000000
#define MEM_USERMODE_STACK_REGION   (uintptr_t)0x0000060000000000 
#define MEM_USERMODE_DEVICE_REGION  (uintptr_t)0x0000400000000000
#define MEM_FRAMEBUFFER_REGION      (uintptr_t)0x0000080000000000
#define MEM_HEAP_REGION             (uintptr_t)0xFFFFFF0000000000
#define MEM_PHYSMEM_MAP_REGION      (uintptr_t)0xFFFFFF8000000000 // !!!: PHYSMEM_MAP is close to kernel heap
#define MEM_MMIO_REGION             (uintptr_t)0xFFFFFFF000000000
#define MEM_DRIVER_REGION           (uintptr_t)0xFFFFFFFF00000000

#define MEM_MMIO_REGION_SIZE        (uintptr_t)0x0000000100000000
#define MEM_USERMODE_STACK_SIZE     (uintptr_t)0x0000010000000000 
#define MEM_DMA_REGION_SIZE         (uintptr_t)0x0000000050000000
#define MEM_PHYSMEM_MAP_SIZE        (uintptr_t)0x0000001000000000
#define MEM_DRIVER_REGION_SIZE      (uintptr_t)0x0000000080000000

/**** MACROS ****/

#define MEM_ALIGN_PAGE(addr) ((addr + PAGE_SIZE) & ~0xFFF) // Align an address to the nearest page
#define MEM_ALIGN_PAGE_DESTRUCTIVE(addr) (addr & ~0xFFF) // Align an address to the nearest page, discarding any bits

#define MEM_PML4_INDEX(x) ((x >> (MEM_PAGE_SHIFT + 27)) & 0x1FF)
#define MEM_PDPT_INDEX(x) ((x >> (MEM_PAGE_SHIFT + 18)) & 0x1FF)
#define MEM_PAGEDIR_INDEX(x) ((x >> (MEM_PAGE_SHIFT + 9)) & 0x1FF)
#define MEM_PAGETBL_INDEX(x) ((x >> MEM_PAGE_SHIFT) & 0x1FF)

#define MEM_SET_FRAME(page, frame) (page->bits.address = ((uintptr_t)frame >> MEM_PAGE_SHIFT))      // Set the frame of a page. Used because of our weird union thing.
#define MEM_GET_FRAME(page) (page->bits.address << MEM_PAGE_SHIFT)                                  // Get the frame of a page. Used because of our weird union thing.

#define MEM_IS_CANONICAL(addr) (((addr & 0xFFFF000000000000) == 0xFFFF000000000000) || !(addr & 0xFFFF000000000000))    // Ugly macro to verify if an address is canonical

/**** FUNCTIONS ****/

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will identity map the kernel into memory and setup page tables.
 * For x86_64, it also sets up the PMM allocator.
 * 
 * @param mem_size The size of memory (aka highest possible address)
 * @param kernel_addr The first free page after the kernel
 */
void mem_init(uintptr_t mem_size, uintptr_t kernel_addr);

#endif