/**
 * @file hexahedron/mm/vmmfault.c
 * @brief VMM fault resolution code
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

#define LOG(status, ...) dprintf_module(status, "MM:VMM:FAULT", __VA_ARGS__)
#pragma GCC diagnostic ignored "-Wtype-limits" // SHUT THE FUCK UP

/**
 * @brief Try to handle VMM fault
 * @param info Fault information
 * @returns VMM_FAULT_RESOLVED on success and VMM_FAULT_UNRESOLVED on failure
 */
int vmm_fault(vmm_fault_information_t *info) {
    // The reasons why we don't support lazy kernel allocations is mainly philosophical:
    // I believe that the kernel should keep itself optimized enough to use all of the pages
    // which it allocates, and that page faults will simply slow down the system's running performance.
    // Just philosophical. Might add a config option for it.
    if (info->address < MMU_USERSPACE_START || info->address >= MMU_USERSPACE_END) return VMM_FAULT_UNRESOLVED;

    info->address = PAGE_ALIGN_DOWN(info->address);

    // Okay, the address is within the userspace. Get the space..
    vmm_space_t *sp = vmm_getSpaceForAddress((void*)info->address);
    if (!sp) return VMM_FAULT_UNRESOLVED;
    mutex_acquire(sp->mut);

    // Get the range for it.
    vmm_memory_range_t *r = vmm_getRange(sp, info->address, 1);
    if (!r) {
        LOG(WARN, "No range contains %p - fault resolution FAILED\n", info->address);
        mutex_release(sp->mut);
        return VMM_FAULT_UNRESOLVED;
    }

    // Determine the action they were trying to accomplish
    int actions = 0;
    if (r->mmu_flags & MMU_FLAG_RW) actions |= VMM_FAULT_WRITE;
    if (!(r->mmu_flags & MMU_FLAG_NOEXEC)) actions |= VMM_FAULT_EXECUTE;

    if (info->exception_type & ~(actions)) {
        LOG(WARN, "Cannot perform access on range - fault resolution failed\n");
        mutex_release(sp->mut);
        return VMM_FAULT_UNRESOLVED;
    }

    // Map in the page
    uint32_t fl = arch_mmu_read_flags(NULL, info->address);
    if (!(fl & MMU_FLAG_PRESENT)) {
        // Non-present - check the range
        if (r->vmm_flags & VM_FLAG_FILE) {
            // Map a page in
            LOG(DEBUG, "Mapping a page in...\n");
            fs_mmap(r->node, (void*)info->address, PAGE_SIZE, info->address - r->start);
        } else {
            // Anonymous memory, map a page
            arch_mmu_map(NULL, info->address, pmm_allocatePage(ZONE_DEFAULT), r->mmu_flags);
            memset((void*)info->address, 0, PAGE_SIZE);
        }
    } else {
        assert(0 && "unimplemented");
    }

    mutex_release(sp->mut);
    return VMM_FAULT_RESOLVED;
}