/**
 * @file hexahedron/arch/x86_64/mmu.c
 * @brief MMU logic
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
#include <kernel/arch/x86_64/mmu.h>
#include <kernel/processor_data.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <string.h>

/* The only things we need are a kernel + HHDM structures */
/* This is the initial map we use */
static mmu_page_t __mmu_kernel_pml[512] __attribute__((aligned(PAGE_SIZE))) = {0};
static mmu_page_t __mmu_hhdm_pdpt[512] __attribute__((aligned(PAGE_SIZE))) = {0};
static mmu_page_t __mmu_hhdm_pd[128][512] __attribute__((aligned(PAGE_SIZE))) = { 0 };


#define TO_HHDM(x) ((uintptr_t)(x) | MMU_HHDM_REGION)
#define FROM_HHDM(x) ((uintptr_t)(x) & ~MMU_HHDM_REGION)

#define KERNEL_PHYS(x) (uintptr_t)((uintptr_t)(x) - 0xFFFFF00000000000)

/**
 * @brief Initialize the base components of the MMU system
 */
void arch_mmu_init() {
    current_cpu->current_dir = (mmu_dir_t*)__mmu_kernel_pml;

    // First build the HHDM structures
    for (int i = 0; i < 128; i++) {
        __mmu_hhdm_pdpt[i].bits.address = (KERNEL_PHYS(&__mmu_hhdm_pd[i]) >> MMU_SHIFT);
        __mmu_hhdm_pdpt[i].bits.present = 1;
        __mmu_hhdm_pdpt[i].bits.rw = 1;
        for (int j = 0; j < 512; j++) {
            __mmu_hhdm_pd[i][j].bits.present = 1;
            __mmu_hhdm_pd[i][j].bits.size = 1;
            __mmu_hhdm_pd[i][j].bits.rw = 1;
            __mmu_hhdm_pd[i][j].bits.address = ((i << 30) + (j << 21)) >> MMU_SHIFT;
        }
    }

    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.address = KERNEL_PHYS(__mmu_hhdm_pdpt) >> MMU_SHIFT;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.rw = 1;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.present = 1;

    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.address = KERNEL_PHYS(__mmu_hhdm_pdpt) >> MMU_SHIFT;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.rw = 1;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.present = 1;

    arch_mmu_load((mmu_dir_t*)KERNEL_PHYS(__mmu_kernel_pml));
}

/**
 * @brief Finish initializing the MMU after PMM init
 */
void arch_mmu_finish(pmm_region_t *region) {

    // Calculate kernel size
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


    // Calculate kernel spanning
    size_t kernel_pages = (kend - kstart) / PAGE_SIZE;
    size_t kernel_pts = (kernel_pages >= 512) ? (kernel_pages / 512) + ((kernel_pages%512) ? 1 : 0) : 1;
    kernel_pts++;

    // Lazy
    if ((kernel_pts / 512) / 512 > 1) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Hexahedron is too big - requires %i PDPTs when 1 is given\n", (kernel_pts / 512) / 512);
        __builtin_unreachable();
    }

    // Allocate the PDPT for Hexahedron
    mmu_page_t *kernel_pdpt = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
    assert(kernel_pdpt);
    memset(kernel_pdpt, 0, PAGE_SIZE);

    for (unsigned i = 0; i < (kernel_pts+512) / 512; i++) {
        mmu_page_t *pd = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
        memset(pd, 0, PAGE_SIZE);
        kernel_pdpt[i].bits.present = 1;
        kernel_pdpt[i].bits.rw = 1;
        kernel_pdpt[i].bits.address  = FROM_HHDM(pd) >> MMU_SHIFT;
        
        for (unsigned j = 0; j < kernel_pts; j++) {
            mmu_page_t *pt = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
            memset(pt, 0, PAGE_SIZE);
            pd[j].bits.present = 1;
            pd[j].bits.rw = 1;
            pd[j].bits.address = FROM_HHDM(pt) >> MMU_SHIFT;
            
            for (unsigned k = 0; k < 512; k++) {
                pt[k].bits.present = 1;
                pt[k].bits.rw = 1;
                pt[k].bits.address = ((PAGE_SIZE*512) * j + PAGE_SIZE * k) >> MEM_PAGE_SHIFT;
            }
        }
    }

extern uintptr_t __kernel_start;
    __mmu_kernel_pml[MMU_PML4_INDEX(((uintptr_t)&__kernel_start))].bits.address = FROM_HHDM(kernel_pdpt) >> MMU_SHIFT;

    
    // Flush TLB
    arch_mmu_load((mmu_dir_t*)__mmu_kernel_pml);
}

/**
 * @brief Remap a physical address into a virtual address (HHDM-like)
 * @param addr The physical address to map
 * @param size The size of the physical address to map
 * @param flags Remap flags, for your usage in internal tracking
 * @returns Remapped address
 */
uintptr_t arch_mmu_map_physical(uintptr_t addr, size_t size, int flags) {
    return addr | MMU_HHDM_REGION;
}

/**
 * @brief Unmap a physical address from the HHDM
 * @param addr The virtual address to unmap
 * @param size The size of the virtual address to unmap
 */
void arch_mmu_unmap_physical(uintptr_t addr, size_t size) {
    return; // Nothing to do here.
}

/**
 * @brief Map a physical address to a virtual address
 * @param dir The directory to map in (or NULL for current)
 * @param virt The virtual address to map
 * @param phys The physical address to map to
 * @param flags The flags for the mapping
 */
void arch_mmu_map(mmu_dir_t *dir, uintptr_t virt, uintptr_t phys, mmu_flags_t flags) { STUB(); }

/**
 * @brief Unmap a virtual address (mark it as non-present)
 * @param dir The directory to unmap in (or NULL for current)
 * @param virt The virtual address to unmap
 */
void arch_mmu_unmap(mmu_dir_t *dir, uintptr_t virt) { STUB(); }

/**
 * @brief Invalidate a page range
 * @param start Start of page range
 * @param end End of page range
 */
void arch_mmu_invalidate_range(uintptr_t start, uintptr_t end) { STUB(); }

/**
 * @brief Retrieve page flags
 * @param dir The directory to get the flags in
 * @param virt The virtual address to retrieve
 * @returns Flags, if the page is not present it just returns 0x0 so @c MMU_FLAG_PRESENT isn't set
 */
mmu_flags_t arch_mmu_read_flags(mmu_dir_t *dir, uintptr_t addr) { STUB(); }

/**
 * @brief Load new directory
 * @param dir The directory to switch to (PHYSICAL)
 */
void arch_mmu_load(mmu_dir_t *dir) {
    if (arch_mmu_dir() == dir) return;

    // !!!: Move directory from HHDM
    if ((uintptr_t)dir > MMU_HHDM_REGION) dir = (mmu_dir_t*)((uintptr_t)dir & ~MMU_HHDM_REGION);
    asm volatile ("movq %0, %%cr3" :: "r"((uintptr_t)dir & ~0xFFF));

}

/**
 * @brief Create a new page table directory
 */
mmu_dir_t *arch_mmu_new_dir() { STUB(); }

/**
 * @brief Get the current directory
 */
mmu_dir_t *arch_mmu_dir() {
    return current_cpu->current_dir;
}

/**
 * @brief Free the page directory
 * @param dir The directory to destroy
 */
void arch_mmu_destroy(mmu_dir_t *dir) { STUB(); }