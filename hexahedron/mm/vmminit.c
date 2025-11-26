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
#include <kernel/processor_data.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:VMMINIT", __VA_ARGS__)

extern uintptr_t __kernel_start;

/**
 * @brief Initialize the VMM
 * @param region The list of PMM regions to use
 */
void vmm_init(pmm_region_t *region) {
    current_cpu->current_context = vmm_kernel_context;

    // Initialize MMU + PMM (both stages)
    arch_mmu_init();
    pmm_init(region);
    arch_mmu_finish(region);
    LOG(INFO, "MMU and PMM initialized successfully\n");

    // Setup the kernel context
    vmm_kernel_context->dir = arch_mmu_dir();

    // Switch to kernel context
    vmm_switch(vmm_kernel_context);

    // Calculate kernel size
    // TODO: Maybe just add a pmm_getKernelSize or something...
    pmm_region_t *r = region;
    uintptr_t kstart = 0;
    uintptr_t kend = 0;
    while (r) {
        if (r->type == PHYS_MEMORY_KERNEL) {
            if (r->start < kstart) kstart = r->start;
            if (r->end > kend) kend = r->end;
        }

        r = r->next;
    }


    kstart = PAGE_ALIGN_DOWN(kstart);
    kend = PAGE_ALIGN_UP(kend);

    // Calculate the maximum
    // !!!: This is FUCKING stupid
    
#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)
    size_t kernel_pages = (kend - kstart) / PAGE_SIZE;
    size_t kernel_pts = (kernel_pages >= 512) ? (kernel_pages / 512) + ((kernel_pages%512) ? 1 : 0) : 1;
    kernel_pts++;
    size_t kernel_map_size = kernel_pts * (PAGE_SIZE * 512);
#else
    size_t kernel_map_size = PAGE_ALIGN_UP(kend - kstart);
#endif

    // Map the kernel in
    assert(vmm_map(&__kernel_start, kernel_map_size, VM_FLAG_FIXED, MMU_FLAG_PRESENT | MMU_FLAG_RW | MMU_FLAG_KERNEL));

    // Map HHDM in
#ifdef MMU_HHDM_REGION
    assert(vmm_map((void*)MMU_HHDM_REGION, MMU_HHDM_SIZE, VM_FLAG_FIXED, MMU_FLAG_PRESENT | MMU_FLAG_RW | MMU_FLAG_KERNEL));
#endif
    
    LOG(DEBUG, "Mapped all necessary regions successfully.\n");
    
    // Initialize slab allocator
    slab_init();

    // Initialize allocator
    alloc_init();

    vmm_dumpContext(current_cpu->current_context);
}

/**
 * @brief Post-SMP init
 */
void vmm_postSMP() {
    slab_postSMPInit();
    alloc_postSMPInit();
    LOG(INFO, "Post-SMP initialization completed\n");
}