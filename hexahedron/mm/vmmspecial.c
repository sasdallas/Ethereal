/**
 * @file hexahedron/mm/vmmspecial.c
 * @brief Handles "special" regions like DMA/MMIO
 * 
 * Not actually anything special about them. These are just hooks to VMM
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/vmm.h>
#include <kernel/panic.h>

/**
 * @brief Map MMIO memory
 * @param physical The physical address of the MMIO memory
 * @param size The size of MMIO to map
 */
uintptr_t mmio_map(uintptr_t physical, size_t size) {
    if (!size) return 0;

    uintptr_t virt = (uintptr_t)vmm_map(NULL, size, VM_FLAG_DEFAULT, MMU_FLAG_PRESENT | MMU_FLAG_UC | MMU_FLAG_WRITE);
    
    if (virt == 0x0) {
        kernel_panic_extended(OUT_OF_MEMORY, "vmm", "*** Could not allocate %d bytes for MMIO\n", size);
    }

    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        arch_mmu_map(NULL, virt+i, physical+i, MMU_FLAG_UC | MMU_FLAG_PRESENT | MMU_FLAG_WRITE);
    }

    return virt;
}

/**
 * @brief Unmap MMIO memory
 * @param virt Virtual address of MMIO memory
 * @param size The size of MMIO to unmap
 */
void mmio_unmap(uintptr_t virt, size_t size) {
    return vmm_unmap((void*)virt, size);
}

/**
 * @brief Map DMA memory
 * @param size The size of DMA memory to allocate
 * 
 * Backs with contiguous pages in memory
 */
uintptr_t dma_map(size_t size) {
    if (size % PAGE_SIZE) size = PAGE_ALIGN_UP(size);

    uintptr_t virt = (uintptr_t)vmm_map(NULL, size, VM_FLAG_DEFAULT, MMU_FLAG_UC | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    uintptr_t phys = pmm_allocatePages(size / 4096, ZONE_DEFAULT);
    for (uintptr_t i = 0; i < size; i++) {
        arch_mmu_map(NULL, virt+i, phys+i, MMU_FLAG_UC | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    }

    return virt;
}

/**
 * @brief Unmap DMA memory
 * @param virt The virtual address of DMA to umap
 * @param size The size of DMA to unmap
 */
void dma_unmap(uintptr_t virt, size_t size) {
    return vmm_unmap((void*)virt, size);
}
