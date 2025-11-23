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
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <stdarg.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:VMM", __VA_ARGS__)

MUTEX_DEFINE_LOCAL(vmm_kcontext_mutex);

static vmm_space_t __vmm_kernel_space = {
    .range = NULL,
    .start = MMU_KERNELSPACE_START,
    .end = MMU_KERNELSPACE_END,
    .mut = &vmm_kcontext_mutex,
};

static vmm_context_t __vmm_kernel_context = {
    .dir = NULL,
    .space = &__vmm_kernel_space,
};

/* Kernel context + space */
vmm_context_t *vmm_kernel_context = &__vmm_kernel_context;
vmm_space_t *vmm_kernel_space = &__vmm_kernel_space;

/* Context cache */
slab_cache_t *vmm_context_cache = NULL;

/**
 * @brief Get the appropriate VMM space for the address
 * @param addr The address to get the VMM space for
 */
vmm_space_t *vmm_getSpaceForAddress(void *addr) {
    if ((uintptr_t)addr >= __vmm_kernel_space.start) {
        return &__vmm_kernel_space;
    }

    vmm_space_t *cur = current_cpu->current_context->space;
    if (!((uintptr_t)addr >= cur->start && (uintptr_t) addr < cur->end)) {
        LOG(WARN, "The current space %p - %p does not contain %p (attempted: %p)\n", cur->start, cur->end, addr, __builtin_return_address(0));
        return NULL;
    }


    return cur;
}

/**
 * @brief VMM slab object initializer
 */
int vmm_initializeContext(slab_cache_t *cache, void *object) {
    vmm_context_t *ctx = (vmm_context_t*)object;
    vmm_space_t *sp = (vmm_space_t*)((uintptr_t)object + sizeof(vmm_context_t));
    ctx->space = sp;

    // Create page directory
    ctx->dir = arch_mmu_new_dir();
    arch_mmu_copy_kernel(ctx->dir);

    // Setup VMM space
    sp->start = MMU_USERSPACE_START;
    sp->end = MMU_USERSPACE_END;
    sp->mut = mutex_create("mut");
    sp->range = NULL;

    return 0;
}

/**
 * @brief Create a new VMM context
 */
vmm_context_t *vmm_createContext() {
    if (!vmm_context_cache) vmm_context_cache = slab_createCache("vmm context cache", sizeof(vmm_context_t) + sizeof(vmm_space_t), 0, vmm_initializeContext, NULL);
    return slab_allocate(vmm_context_cache);
}

/**
 * @brief Map VMM memory
 * @param addr The address to use
 * @param size The size to map in
 * @param vm_flags VM flags for the mapping
 * @param prot MMU protection flags 
 * 
 * The remaining arguments depend on @c vm_flags
 * For VM_FLAG_FILE, pass the filesystem node to map into memory.
 * 
 * @returns The address mapped or NULL on failure.
 */
void *vmm_map(void *addr, size_t size, vmm_flags_t vm_flags, mmu_flags_t prot, ...) {
    // Locate the free range
    addr = (void*)PAGE_ALIGN_DOWN((uintptr_t)addr);
    if (size & 0xFFF) size = PAGE_ALIGN_UP(size); // !!!: hope I dont forget about this 

    if (addr == NULL && !(vm_flags & VM_FLAG_FIXED)) {
        addr = (void*)MMU_KERNELSPACE_START;
    }

    void *r = NULL;

    // Find the appropriate space for the address
    vmm_space_t *sp = vmm_getSpaceForAddress(addr);
    assert(sp);

    // Mutex
    mutex_acquire(sp->mut);

    // Find a free region
    uintptr_t start = vmm_findFree(sp, (uintptr_t)addr, size);
    if (!start) {
        // Very specific case...
        if (!(addr == NULL && vm_flags & VM_FLAG_FIXED)) {
            goto _finish;
        }
    }

    if (start != (uintptr_t)addr && vm_flags & VM_FLAG_FIXED) {
        LOG(WARN, "Couldn't match address hint and VM_FLAG_FIXED - find_free returned %p but needed %p\n", start, addr);
        goto _finish; // Did not match address hint
    }

    // Create the new range
    vmm_memory_range_t *range = vmm_createRange(start, start+size, vm_flags, prot);

    // Insert into range
    vmm_insertRange(sp, range);

    // We can back these pages if we're in the kernel's context.
    // Otherwise, allocations will be auto-backed by VMM faults.
    if (vm_flags & VM_FLAG_ALLOC && sp == &__vmm_kernel_space) {
        // Back the pages now
        LOG(DEBUG, "Backing %p - %p\n", range->start, range->end);
        for (uintptr_t i = range->start; i < range->end; i += PAGE_SIZE) {
            uintptr_t p = pmm_allocatePage(ZONE_DEFAULT);
            arch_mmu_map(NULL, i, p, prot);
        }

        arch_mmu_invalidate_range(range->start, range->end);
    }

    r = (void*)range->start;

_finish:
    mutex_release(sp->mut);
    return r;
}

/**
 * @brief Switch VMM contexts
 * @param ctx The context to switch to
 */
void vmm_switch(vmm_context_t *ctx) {
    assert(ctx);
    if (ctx == current_cpu->current_context) return; // No switching actually needed
    current_cpu->current_context = ctx;
    arch_mmu_load(ctx->dir);
}

/**
 * @brief Unmap/free VMM memory
 * @param addr The address to unmap
 * @param size The size of the region to unmap
 */
void vmm_unmap(void *addr, size_t size) {
    if (size & 0xFFF) size = PAGE_ALIGN_UP(size); // !!!: hope I dont forget about this 

    // Locate the space this belongs in
    assert(((uintptr_t)addr & 0xFFF) == 0);
    vmm_space_t *sp = vmm_getSpaceForAddress(addr);
    assert(sp);

    // Mutex
    mutex_acquire(sp->mut);

    // Find the range
    vmm_memory_range_t *r = vmm_getRange(sp, (uintptr_t)addr, size);
    
    // I don't care enough to support these
    if (!((uintptr_t)addr == r->start && ((uintptr_t)addr + size) == r->end)) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "vmm", "*** Partial unmap attempted of %p - %p in region %p - %p by function %p. Not supported.\n", addr, addr+size, r->start, r->end, __builtin_return_address(0));
    }

    // Destroy the range
    vmm_destroyRange(sp, r);

    mutex_release(sp->mut);
}

/**f
 * @brief Dump all allocations in a context
 * @param context The context to dump
 */
void vmm_dumpContext(vmm_context_t *ctx) {
    vmm_memory_range_t *r = ctx->space->range;
    vmm_memory_range_t *last = NULL;
    while (r) {
        assert(r->prev == last);
        LOG(INFO, "VMM memory region %p - %p (FLAGS 0x%x MMU_FLAGS 0x%x)\n", r->start, r->end, r->vmm_flags, r->mmu_flags);
        last = r;
        r = r->next;
    }
}


/**
 * @brief Validate a range of memory
 * @param start Start of the range
 * @param size The size of the range to submit
 * @param flags See @c VMM_PTR
 * 
 * @warning Pointers are NOT allowed to cross from user-kernel space.
 */
int vmm_validate(uintptr_t start, size_t size, int flags) {
    if (!start) return 0;

    // !!!: Clean this up?
    start = PAGE_ALIGN_DOWN(start);
    if (size & 0xfff) size = PAGE_ALIGN_UP(size);

    vmm_space_t *sp = vmm_getSpaceForAddress((void*)start);
    if (!sp || !RANGE_IN_RANGE(start, start+size, sp->start, sp->end)) {
        return 0; // Not contained
    }

    mutex_acquire(sp->mut);

    for (uintptr_t i = start; i < start+size; i += PAGE_SIZE) {
        vmm_memory_range_t *r = vmm_getRange(sp, i, PAGE_SIZE);
        if (!r) {
            mutex_release(sp->mut);
            return 0;
        }

        // Ensure flags are okay
        if ((!(flags & VMM_PTR_USER) && r->mmu_flags & MMU_FLAG_USER) ||
            ((flags & VMM_PTR_USER && flags & VMM_PTR_STRICT) && !(r->mmu_flags & MMU_FLAG_USER))) {
            mutex_release(sp->mut);
            return 0;
        }        
    }

    // Pointer validation succeeded
    mutex_release(sp->mut);
    return 1;
}

/**
 * @brief Destroy a context
 * @param ctx The context to destroy
 */
void vmm_destroyContext(vmm_context_t *ctx) {
    // Kinda stupid
    vmm_context_t *old = current_cpu->current_context;

    mutex_acquire(ctx->space->mut);
    vmm_switch(ctx);

    vmm_memory_range_t *r = ctx->space->range;
    
    while (r) {
        vmm_memory_range_t *next = r->next;
        vmm_destroyRange(ctx->space, r);
        r = next;
    }

    vmm_switch(old);
    arch_mmu_destroy(ctx->dir);
    mutex_release(ctx->space->mut);

    slab_free(vmm_context_cache, ctx);
}