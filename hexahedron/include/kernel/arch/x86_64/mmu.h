/**
 * @file hexahedron/include/kernel/arch/x86_64/mmu.h
 * @brief x86_64 MMU
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_MMU_H
#define KERNEL_ARCH_X86_64_MMU_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096 
#endif

#define MMU_SHIFT 12

// IMPORTANT: THIS IS THE HEXAHEDRON MEMORY MAP CONFIGURED FOR X86_64
// 0x0000000000000000 - 0x0000700000000000: Userspace region
// 0x0000600000000000 - 0x0000700000000000: Usermode stack. Only a small amount of this is mapped to start with
// 0xFFFFF00000000000 - 0xFFFFF00000000000: Kernel code in memory
// 0xFFFFFF8000000000 - 0xFFFFFF9000000000: High base region for identity mapping

#define MMU_KERNELSPACE_START   (uintptr_t)0xFFFFF00000000000 
#define MMU_KERNELSPACE_END     (uintptr_t)0xFFFFFFFFFFFFF000

#define MMU_USERSPACE_START     (uintptr_t)0x0000000000000000
#define MMU_USERSPACE_END       (uintptr_t)0x0000800000000000

#define MMU_KERNEL_REGION       (uintptr_t)0xFFFFF00000000000
#define MMU_HHDM_REGION         (uintptr_t)0xFFFFFF8000000000
#define MMU_HHDM_SIZE           (uintptr_t)0x0000001000000000

/**** MACROS ****/
#define PAGE_ALIGN_UP(x) (((x) + (PAGE_SIZE-1)) & ~0xFFF)
#define PAGE_ALIGN_DOWN(x) (((x) & ~0xFFF))
#define MMU_PML4_INDEX(x) (((x) >> (MMU_SHIFT + 27)) & 0x1FF)
#define MMU_PDPT_INDEX(x) (((x) >> (MMU_SHIFT + 18)) & 0x1FF)
#define MMU_PAGEDIR_INDEX(x) (((x) >> (MMU_SHIFT + 9)) & 0x1FF)
#define MMU_PAGETBL_INDEX(x) (((x) >> MMU_SHIFT) & 0x1FF)

#define MMU_CANONICAL_MASK (~0ULL << (48- 1))

#define MMU_IS_CANONICAL(addr) \
    ((((uint64_t)(addr) & MMU_CANONICAL_MASK) == 0ULL) || \
     (((uint64_t)(addr) & MMU_CANONICAL_MASK) == MMU_CANONICAL_MASK))

#define BARRIER() asm volatile ("" ::: "memory")

/**** TYPES ****/
typedef uintptr_t mmu_dir_t;

typedef uint64_t mmu_page_t;

/**** FUNCTIONS ****/

/**
 * @brief MMU AP initialize
 */
void arch_mmu_ap();

#endif