/**
 * @file hexahedron/mm/vmm.c
 * @brief VMM init code
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:VMMINIT", __VA_ARGS__)

/**
 * @brief Initialize the VMM
 * @param region The list of PMM regions to use
 */
void vmm_init(pmm_region_t *region) {
    arch_mmu_init();
    pmm_init(region);
    arch_mmu_finish(region);


    
}