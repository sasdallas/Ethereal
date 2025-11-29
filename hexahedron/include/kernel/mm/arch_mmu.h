/**
 * @file hexahedron/include/kernel/mem/arch_mmu.h
 * @brief Architecture-specific VMM/MMU interfaces
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MEM_ARCH_MMU_H
#define KERNEL_MEM_ARCH_MMU_H

/**** INCLUDES ****/
#include <kernel/arch/arch.h>
#include <kernel/mm/pmm.h>
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

/* Flags for your benefit */
#define REMAP_PERMANENT 0x0
#define REMAP_TEMPORARY 0x1

/* MMU flags */
#define MMU_FLAG_NONPRESENT             0x0     // Not present in memory
#define MMU_FLAG_PRESENT                0x1     // Present in memory
#define MMU_FLAG_RO                     0x0     // Read-only memory
#define MMU_FLAG_RW                     0x2     // Read-write memory
#define MMU_FLAG_KERNEL                 0x0     // Kernel only memory
#define MMU_FLAG_USER                   0x4     // Userspace memory
#define MMU_FLAG_NOEXEC                 0x8     // Non-executable memory
#define MMU_FLAG_GLOBAL                 0x10    // Global memory
#define MMU_FLAG_WB                     0x00    // Writeback
#define MMU_FLAG_WC                     0x20    // Write combine
#define MMU_FLAG_WT                     0x40    // Write through
#define MMU_FLAG_UC                     0x80    // Not cacheable

#define MMU_USERMODE_STACK_REGION   (uintptr_t)0x0000060000000000 
#define MMU_USERMODE_STACK_SIZE     (uintptr_t)0x0000010000000000 

/**** TYPES ****/

typedef uint64_t mmu_flags_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the base components of the MMU system
 */
void arch_mmu_init();

/**
 * @brief Finish initializing the MMU after PMM init
 */
void arch_mmu_finish(pmm_region_t *region);

/**
 * @brief Remap a physical address into a virtual address (HHDM-like)
 * @param addr The physical address to map
 * @param size The size of the physical address to map
 * @param flags Remap flags, for your usage in internal tracking
 * @returns Remapped address
 */
uintptr_t arch_mmu_remap_physical(uintptr_t addr, size_t size, int flags);

/**
 * @brief Unmap a physical address from the HHDM
 * @param addr The virtual address to unmap
 * @param size The size of the virtual address to unmap
 */
void arch_mmu_unmap_physical(uintptr_t addr, size_t size);

/**
 * @brief Map a physical address to a virtual address
 * @param dir The directory to map in (or NULL for current)
 * @param virt The virtual address to map
 * @param phys The physical address to map to
 * @param flags The flags for the mapping
 */
void arch_mmu_map(mmu_dir_t *dir, uintptr_t virt, uintptr_t phys, mmu_flags_t flags);

/**
 * @brief Unmap a virtual address (mark it as non-present)
 * @param dir The directory to unmap in (or NULL for current)
 * @param virt The virtual address to unmap
 */
void arch_mmu_unmap(mmu_dir_t *dir, uintptr_t virt);

/**
 * @brief Invalidate a page range
 * @param start Start of page range
 * @param end End of page range
 */
void arch_mmu_invalidate_range(uintptr_t start, uintptr_t end);

/**
 * @brief Retrieve page flags
 * @param dir The directory to get the flags in
 * @param virt The virtual address to retrieve
 * @returns Flags, if the page is not present it just returns 0x0 so @c MMU_FLAG_PRESENT isn't set
 */
mmu_flags_t arch_mmu_read_flags(mmu_dir_t *dir, uintptr_t addr);

/**
 * @brief Get physical address of page
 * @param dir The directory to get the address in
 * @param virt The virtual address to retrieve
 * @returns The physical address or 0x0 if the page is not mapped
 */
uintptr_t arch_mmu_physical(mmu_dir_t *dir, uintptr_t addr);

/**
 * @brief Load new directory
 * @param dir The directory to switch to
 */
void arch_mmu_load(mmu_dir_t *dir);

/**
 * @brief Create a new page table directory
 */
mmu_dir_t *arch_mmu_new_dir();

/**
 * @brief Get the current directory
 */
mmu_dir_t *arch_mmu_dir();

/**
 * @brief Free the page directory
 * @param dir The directory to destroy
 */
void arch_mmu_destroy(mmu_dir_t *dir);

/**
 * @brief Copy kernel mappings into a new directory
 * @param dir The directory to copy into
 */
void arch_mmu_copy_kernel(mmu_dir_t *dir);

/**
 * @brief Set flags
 * @param dir The directory to set flags in
 * @param i The virtual address
 * @param flags The flags to set
 * @returns 0 on success, 1 on failure
 */
int arch_mmu_setflags(mmu_dir_t *dir, uintptr_t i, mmu_flags_t flags);
 
#endif