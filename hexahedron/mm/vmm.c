/**
 * @file hexahedron/mm/vmm.c
 * @brief General VMM functions
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
#include <kernel/processor_data.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:VMM", __VA_ARGS__)

MUTEX_DEFINE_LOCAL(vmm_kcontext_mutex);
static vmm_context_t __vmm_kernel_context = {
    .dir = NULL,
    .mut = &vmm_kcontext_mutex,
    .range = NULL,
    .start = MMU_KERNELSPACE_START,
    .end = MMU_KERNELSPACE_END,
};

/* Kernel context */
vmm_context_t *vmm_kernel_context = &__vmm_kernel_context;

/**
 * @brief Map VMM memory
 * @param addr The address to use
 * @param size The size to map in
 * @param vm_flags VM flags for the mapping
 * @param prot MMU protection flags 
 * @returns The address mapped or NULL on failure.
 */
void *vmm_map(void *addr, size_t size, vmm_flags_t vm_flags, mmu_flags_t prot) {
    // Locate the free range
    addr = (void*)PAGE_ALIGN_DOWN((uintptr_t)addr);
    if (size & (PAGE_SIZE-1)) size = PAGE_ALIGN_UP(size); // !!!: hope I dont forget about this 

    void *r = NULL;

    // Mutex
    mutex_acquire(current_cpu->current_context->mut);

    // Find a free region
    uintptr_t start = vmm_findFree(current_cpu->current_context, (uintptr_t)addr, size);
    if (!start) goto _finish;

    if (start != (uintptr_t)addr && vm_flags & VMM_MUST_BE_EXACT) {
        LOG(WARN, "Couldn't match address hint and VMM_MUST_BE_EXACT - find_free returned %p but needed %p\n", start, addr);
        goto _finish; // Did not match address hint
    }


    // Create the new range
    vmm_memory_range_t *range = vmm_createRange(start, start+size, vm_flags, prot);

    // Insert into range
    vmm_insertRange(current_cpu->current_context, range);

    if (vm_flags & VMM_ALLOCATE && VMM_IS_KERNEL_CTX()) {
        // Back the pages now
        for (uintptr_t i = range->start; i < range->end; i += PAGE_SIZE) {
            arch_mmu_map(NULL, i, pmm_allocatePage(ZONE_DEFAULT), prot);
        }

        // TODO: Shootdown, maybe? Need to flush out context API
    }

    r = (void*)range->start;

_finish:
    mutex_release(current_cpu->current_context->mut);
    return r;
}

/**
 * @brief Switch VMM contexts
 * @param ctx The context to switch to
 */
void vmm_switch(vmm_context_t *ctx) {
    current_cpu->current_context = ctx;
    arch_mmu_load(ctx->dir);
}

/**
 * @brief Dump all allocations in a context
 * @param context The context to dump
 */
void vmm_dumpContext(vmm_context_t *ctx) {
    vmm_memory_range_t *r = ctx->range;
    vmm_memory_range_t *last = NULL;
    while (r) {
        assert(r->prev == last);
        LOG(INFO, "VMM memory region %p - %p (FLAGS 0x%x MMU_FLAGS 0x%x)\n", r->start, r->end, r->vmm_flags, r->mmu_flags);
        last = r;
        r = r->next;
    }
}