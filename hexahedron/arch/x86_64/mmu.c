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
#include <kernel/arch/x86_64/smp.h>
#include <kernel/processor_data.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <string.h>

/* The only things we need are a kernel + HHDM structures */
/* This is the initial map we use */
mmu_page_t __mmu_kernel_pml[512] __attribute__((aligned(PAGE_SIZE))) = {0};
static mmu_page_t __mmu_hhdm_pdpt[512] __attribute__((aligned(PAGE_SIZE))) = {0};
static mmu_page_t __mmu_hhdm_pd[128][512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
mmu_page_t __mmu_initial_page_region[3][512] __attribute__((aligned(PAGE_SIZE))) = { 0 };

#define TO_HHDM(x) ((uintptr_t)(x) | MMU_HHDM_REGION)
#define FROM_HHDM(x) ((uintptr_t)(x) & ~MMU_HHDM_REGION)

#define KERNEL_PHYS(x) (uintptr_t)((uintptr_t)(x) - 0xFFFFF00000000000)

/**
 * @brief MMU page fault handler
 */
int arch_mmu_pf(uintptr_t useless, registers_t *regs, extended_registers_t *regs_extended) {
    // Build fault flags
    int loc = (regs->cs == 0x08) ?  VMM_FAULT_FROM_KERNEL : VMM_FAULT_FROM_USER;

    int flags = 
        (regs->err_code & 0x1) ? VMM_FAULT_PRESENT : VMM_FAULT_NONPRESENT |
        (regs->err_code & 0x2) ? VMM_FAULT_WRITE : VMM_FAULT_READ |
        (regs->err_code & 0x10) ? VMM_FAULT_EXECUTE : 0;
    
    vmm_fault_information_t info = {
        .from = loc,
        .exception_type = flags,
        .address = (uintptr_t)regs_extended->cr2
    };

    if (vmm_fault(&info) == VMM_FAULT_RESOLVED) {
        return 0;
    }

    dprintf(ERR, "Could not resolve #PF exception (%p) from IP %04x:%016llX SP %016llX\n", regs_extended->cr2, regs->cs, regs->rip, regs->rsp);
    
    if (info.from == VMM_FAULT_FROM_USER) {
        signal_send(current_cpu->current_process, SIGSEGV);
        return 0;
    }

    // Prepare
    kernel_panic_prepare(CPU_EXCEPTION_UNHANDLED);

    // Print it out
    dprintf(NOHEADER, COLOR_CODE_RED "*** Page fault at address " COLOR_CODE_RED_BOLD "0x%016llX\n" COLOR_CODE_RED, regs_extended->cr2);

    // Determine fault cause
    int present = (regs->err_code & (1 << 0));
    int write = (regs->err_code & (1 << 1));
    int rsvd = (regs->err_code & (1 << 3));
    int instruction = (regs->err_code & (1 << 4));

    dprintf(NOHEADER, "*** Memory access type: %s%s%s%s\n\n", 
                            (present) ? "PRESENT_IN_MEMORY " : "NOT_PRESENT_IN_MEMORY ",
                            (write) ? "WRITE " : "READ ",
                            (rsvd) ? "RSVD_BIT_SET " : "",
                            (instruction) ? "INSTRUCTION_FETCH " : "");

    dprintf(NOHEADER, "\033[1;31mFAULT REGISTERS:\n\033[0;31m");

    dprintf(NOHEADER, "RAX %016llX RBX %016llX RCX %016llX RDX %016llX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    dprintf(NOHEADER, "RDI %016llX RSI %016llX RBP %016llX RSP %016llX\n", regs->rdi, regs->rsi, regs->rbp, regs->rsp);
    dprintf(NOHEADER, "R8  %016llX R9  %016llX R10 %016llX R11 %016llX\n", regs->r8, regs->r9, regs->r10, regs->r11);
    dprintf(NOHEADER, "R12 %016llX R13 %016llX R14 %016llX R15 %016llX\n", regs->r12, regs->r13, regs->r14, regs->r15);
    dprintf(NOHEADER, "ERR %016llX RIP %016llX RFL %016llX\n\n", regs->err_code, regs->rip, regs->rflags);

    dprintf(NOHEADER, "CS %04X DS %04X SS %04X\n\n", regs->cs, regs->ds, regs->ss);
    dprintf(NOHEADER, "CR0 %08X CR2 %016llX CR3 %016llX CR4 %08X\n", regs_extended->cr0, regs_extended->cr2, regs_extended->cr3, regs_extended->cr4);
    dprintf(NOHEADER, "GDTR %016llX %04X\n", regs_extended->gdtr.base, regs_extended->gdtr.limit);
    dprintf(NOHEADER, "IDTR %016llX %04X\n", regs_extended->idtr.base, regs_extended->idtr.limit);

    // !!!: not conforming (should call kernel_panic_finalize) but whatever
    // We want to do our own traceback.
extern void arch_panic_traceback(int depth, registers_t *regs);
    arch_panic_traceback(10, regs);

    // Show core processes
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nCPU DATA:\n" COLOR_CODE_RED);

    for (int i = 0; i < MAX_CPUS; i++) {
        if (processor_data[i].cpu_id || !i) {
            // We have valid data here
            if (processor_data[i].current_thread != NULL) {
                dprintf(NOHEADER, COLOR_CODE_RED "CPU%d: Current thread %p (process '%s') - page directory %p\n", i, processor_data[i].current_thread, processor_data[i].current_process->name, processor_data[i].current_context->dir);
            } else {
                dprintf(NOHEADER, COLOR_CODE_RED "CPU%d: No thread available. Page directory %p\n", processor_data[i].current_context->dir);
            }
        }
    }

    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nVMM DUMP:\n");
    vmm_dumpContext(current_cpu->current_context);

    // Display message
    dprintf(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**
 * @brief Initialize the base components of the MMU system
 */
void arch_mmu_init() {
    // First build the HHDM structures
    for (int i = 0; i < 128; i++) {
        __mmu_hhdm_pdpt[i].bits.address = (KERNEL_PHYS(&__mmu_hhdm_pd[i]) >> MMU_SHIFT);
        __mmu_hhdm_pdpt[i].bits.present = 1;
        __mmu_hhdm_pdpt[i].bits.rw = 1;
        for (int j = 0; j < 512; j++) {
            __mmu_hhdm_pd[i][j].bits.present = 1;
            __mmu_hhdm_pd[i][j].bits.size = 1;
            __mmu_hhdm_pd[i][j].bits.rw = 1;
            __mmu_hhdm_pd[i][j].bits.address = ((((uint64_t)i) << 30) + (((uint64_t)j) << 21)) >> MMU_SHIFT;
        }
    }

    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.address = KERNEL_PHYS(__mmu_hhdm_pdpt) >> MMU_SHIFT;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.rw = 1;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_HHDM_REGION)].bits.present = 1;

    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.address = KERNEL_PHYS(__mmu_hhdm_pdpt) >> MMU_SHIFT;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.rw = 1;
    __mmu_kernel_pml[MMU_PML4_INDEX(MMU_KERNEL_REGION)].bits.present = 1;

    // Load directory
    arch_mmu_load((mmu_dir_t*)KERNEL_PHYS(__mmu_kernel_pml));
    
    // PAT
    asm volatile(
    	"movl $0x277, %%ecx\n"
    	"rdmsr\n"
    	"movw $0x0401, %%dx\n"
    	"wrmsr\n"
    	::: "eax", "ecx", "edx", "memory"
    );

    // Enable WP
    asm volatile (
        "movq %%cr0, %%rax\n"
        "orq $0x10000, %%rax\n"
        "movq %%rax, %%cr0" ::: "rax");
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
                pt[k].bits.address = ((PAGE_SIZE*512) * j + PAGE_SIZE * k) >> MMU_SHIFT;
            }
        }
    }

extern uintptr_t __kernel_start;
    __mmu_kernel_pml[MMU_PML4_INDEX(((uintptr_t)&__kernel_start))].bits.address = FROM_HHDM(kernel_pdpt) >> MMU_SHIFT;

    
    // Finally, fill the rest of the kernel PML with blank PDPTs to allow cloning easier
    for (unsigned i = 256; i < 512; i++) {
        if (!__mmu_kernel_pml[i].bits.present) {
            uintptr_t fr = TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
            memset((void*)fr, 0, PAGE_SIZE);
            
            __mmu_kernel_pml[i].bits.present = 1;
            __mmu_kernel_pml[i].bits.rw = 1;
            __mmu_kernel_pml[i].bits.usermode = 0;
            __mmu_kernel_pml[i].bits.address = FROM_HHDM(fr) >> MMU_SHIFT;
        }
    }

    // Flush TLB
    arch_mmu_load((mmu_dir_t*)KERNEL_PHYS(__mmu_kernel_pml));
    current_cpu->current_context->dir = (mmu_dir_t*)(__mmu_kernel_pml);

    hal_registerExceptionHandler(14, arch_mmu_pf);
}

/**
 * @brief Remap a physical address into a virtual address (HHDM-like)
 * @param addr The physical address to map
 * @param size The size of the physical address to map
 * @param flags Remap flags, for your usage in internal tracking
 * @returns Remapped address
 */
uintptr_t arch_mmu_remap_physical(uintptr_t addr, size_t size, int flags) {
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
 * @brief Get a page (internal)
 */
static mmu_page_t *arch_mmu_get_page(mmu_dir_t *dir, uintptr_t virt, bool allow_nonpresent) {
    if (!dir) dir = arch_mmu_dir();
    mmu_page_t *d = (mmu_page_t*)dir;

    // PDPT
    mmu_page_t *pdpt = NULL;
    
    if (d[MMU_PML4_INDEX(virt)].bits.present == 0) {
        if (!allow_nonpresent) return NULL;

        // Create a new entry
        pdpt = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
        memset(pdpt, 0, PAGE_SIZE);

        d[MMU_PML4_INDEX(virt)].bits.address = FROM_HHDM(pdpt) >> MMU_SHIFT;
        d[MMU_PML4_INDEX(virt)].bits.usermode = 1;
        d[MMU_PML4_INDEX(virt)].bits.rw = 1;
        d[MMU_PML4_INDEX(virt)].bits.present = 1;
    } else {
        pdpt = (mmu_page_t*)TO_HHDM(d[MMU_PML4_INDEX(virt)].bits.address << MMU_SHIFT);
    }

    // PD
    mmu_page_t *pd = NULL;

    if (pdpt[MMU_PDPT_INDEX(virt)].bits.present == 0) {
        if (!allow_nonpresent) return NULL;

        // Create a new entry
        pd = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
        memset(pd, 0, PAGE_SIZE);

        pdpt[MMU_PDPT_INDEX(virt)].bits.address = FROM_HHDM(pd) >> MMU_SHIFT;
        pdpt[MMU_PDPT_INDEX(virt)].bits.usermode = 1;
        pdpt[MMU_PDPT_INDEX(virt)].bits.rw = 1;
        pdpt[MMU_PDPT_INDEX(virt)].bits.present = 1;
    } else {
        pd = (mmu_page_t*)TO_HHDM(pdpt[MMU_PDPT_INDEX(virt)].bits.address << MMU_SHIFT);
    }

    // PT
    mmu_page_t *pt = NULL;

    if (pd[MMU_PAGEDIR_INDEX(virt)].bits.present == 0) {
        if (!allow_nonpresent) return NULL;

        pt = (mmu_page_t*)TO_HHDM(pmm_allocatePage(ZONE_DEFAULT));
        memset(pt, 0, PAGE_SIZE);

        pd[MMU_PAGEDIR_INDEX(virt)].bits.present = 1;
        pd[MMU_PAGEDIR_INDEX(virt)].bits.rw = 1;
        pd[MMU_PAGEDIR_INDEX(virt)].bits.usermode = 1;
        pd[MMU_PAGEDIR_INDEX(virt)].bits.address = FROM_HHDM(pt) >> MMU_SHIFT;
    } else {
        pt = (mmu_page_t*)TO_HHDM(pd[MMU_PAGEDIR_INDEX(virt)].bits.address << MMU_SHIFT);
    }

    return &pt[MMU_PAGETBL_INDEX(virt)];
}

/**
 * @brief Map a physical address to a virtual address
 * @param dir The directory to map in (or NULL for current)
 * @param virt The virtual address to map
 * @param phys The physical address to map to
 * @param flags The flags for the mapping
 */
void arch_mmu_map(mmu_dir_t *dir, uintptr_t virt, uintptr_t phys, mmu_flags_t flags) {
    assert(MMU_IS_CANONICAL(virt));
    virt = PAGE_ALIGN_DOWN(virt);
    

    // Configure the bits in the entry
    mmu_page_t *page = arch_mmu_get_page(dir, virt, true);
    page->bits.present = (flags & MMU_FLAG_PRESENT) ? 1 : 0;
    page->bits.rw = (flags & MMU_FLAG_WRITE) ? 1 : 0;
    page->bits.usermode = (flags & MMU_FLAG_USER) ? 1 : 0;
    page->bits.nx = (flags & MMU_FLAG_NOEXEC) ? 1 : 0;
    page->bits.global = (flags & MMU_FLAG_GLOBAL) ? 1 : 0;
    page->bits.size = (flags & MMU_FLAG_WC) ? 1 : 0;
    page->bits.writethrough = (flags & MMU_FLAG_WT) ? 1 : 0;
    page->bits.cache_disable = (flags & MMU_FLAG_UC) ? 1 : 0;
    page->bits.address = phys >> MMU_SHIFT;
}

/**
 * @brief Unmap a virtual address (mark it as non-present)
 * @param dir The directory to unmap in (or NULL for current)
 * @param virt The virtual address to unmap
 */
void arch_mmu_unmap(mmu_dir_t *dir, uintptr_t virt) {
    assert(MMU_IS_CANONICAL(virt));

    mmu_page_t *pg = arch_mmu_get_page(dir, virt, true);
    pg->data = 0;
}

/**
 * @brief Get physical address of page
 * @param dir The directory to get the address in
 * @param virt The virtual address to retrieve
 * @returns The physical address or 0x0 if the page is not mapped
 */
uintptr_t arch_mmu_physical(mmu_dir_t *dir, uintptr_t addr) {
    if (addr >= MMU_HHDM_REGION && addr < MMU_HHDM_REGION + MMU_HHDM_SIZE) return FROM_HHDM(addr);

    uintptr_t off = addr & 0xFFF;
    mmu_page_t *page = arch_mmu_get_page(dir, addr, false);
    if (!page) return 0x0;

    return (((uint64_t)(page->bits.address) << MMU_SHIFT)) + off;
}

/**
 * @brief Invalidate a page range
 * @param start Start of page range
 * @param end End of page range
 */
void arch_mmu_invalidate_range(uintptr_t start, uintptr_t end) {
    if (end-start > PAGE_SIZE*16) {
        // reload cr3 instead, its faster
        asm volatile ("movq %%cr3, %%rax\n"
                      "movq %%rax, %%cr3" ::: "rax", "memory");
    } else {
        for (uintptr_t i = start; i < end; i += PAGE_SIZE) {
            asm volatile ("invlpg (%0)" :: "r"(i) : "memory");
        }
    }

    // TLB shootdown other CPUs, only if conditions are met.
    if (start >= MMU_KERNELSPACE_START || (end < MMU_USERSPACE_END && current_cpu->current_process && current_cpu->current_process->thread_list && current_cpu->current_process->thread_list->length)) {
        smp_tlbShootdown(start, end-start);
    }
}

/**
 * @brief Retrieve page flags
 * @param dir The directory to get the flags in
 * @param virt The virtual address to retrieve
 * @returns Flags, if the page is not present it just returns 0x0 so @c MMU_FLAG_PRESENT isn't set
 */
mmu_flags_t arch_mmu_read_flags(mmu_dir_t *dir, uintptr_t addr) {
    mmu_page_t *pg = arch_mmu_get_page(dir, addr, false);
    if (!pg) return 0;

    uint32_t flags = 0x0;

#define MMU_FLAG_CASE(bit, flag1, flag2) flags |= (pg->bits.bit) ? flag1 : flag2
    MMU_FLAG_CASE(present, MMU_FLAG_PRESENT, 0);
    MMU_FLAG_CASE(rw, MMU_FLAG_WRITE, 0);
    MMU_FLAG_CASE(usermode, MMU_FLAG_USER, 0);
    MMU_FLAG_CASE(nx, MMU_FLAG_NOEXEC, 0);
    MMU_FLAG_CASE(global, MMU_FLAG_GLOBAL, 0);
#undef MMU_FLAG_CASE

    int index = 
        (pg->bits.size << 2) |
        (pg->bits.cache_disable << 1) |
        (pg->bits.writethrough);


    // !!!: hardcoded PAT
    static int pat_indexes[] = {
        MMU_FLAG_WB,
        MMU_FLAG_WT,
        MMU_FLAG_UC,
        MMU_FLAG_UC,
        MMU_FLAG_WC,
        MMU_FLAG_WT,
        MMU_FLAG_UC,
        MMU_FLAG_UC
    };

    flags |= pat_indexes[index];
    return flags;
}

/**
 * @brief Load new directory
 * @param dir The directory to switch to (PHYSICAL)
 */
void arch_mmu_load(mmu_dir_t *dir) {
    if ((void*)FROM_HHDM(arch_mmu_dir()) == dir) return;

    // !!!: Move directory from HHDM
    dir = (mmu_dir_t*)FROM_HHDM(dir);
    asm volatile ("movq %0, %%cr3" :: "r"((uintptr_t)dir & ~0xFFF));
}

/**
 * @brief Create a new page table directory
 */
mmu_dir_t *arch_mmu_new_dir() { 
    uintptr_t p = pmm_allocatePage(ZONE_DEFAULT);
    memset((void*)TO_HHDM(p), 0, PAGE_SIZE);
    return (mmu_dir_t*)TO_HHDM(p);
}

/**
 * @brief Get the current directory
 */
inline mmu_dir_t *arch_mmu_dir() {
    return current_cpu->current_context->dir;
}

/**
 * @brief Free the page directory
 * @param dir The directory to destroy
 */
void arch_mmu_destroy(mmu_dir_t *dir) {
    return; // !!!: will be fixed next revision, memory leak btw
    
    // We should free any associated PMM blocks below kernelspace
    for (int i = 0; i < 256; i++) {
        mmu_page_t *pmle = &((mmu_page_t*)dir)[i];
        if (pmle->bits.address == 0 || pmle->bits.present == 0) continue;
        mmu_page_t *pdpt  = (mmu_page_t*)TO_HHDM(pmle->bits.address << 12);

        for (int j = 0; j < 512; j++) {
            mmu_page_t *pdpte = &pdpt[j];
            if (pdpte->bits.address == 0 || pdpte->bits.present == 0) continue;
            dprintf(DEBUG, "freeing pdpte: %p\n", pdpte->bits.address << 12);
            pmm_freePage(pdpte->bits.address << 12);
            pdpte->data = 0;
        }

        dprintf(DEBUG, "freeing pmle: %p\n", pmle->bits.address << 12);

        pmm_freePage(pmle->bits.address << 12);
        pmle->data = 0;
    }

    pmm_freePage(FROM_HHDM(dir));
}


/**
 * @brief Copy kernel mappings into a new directory
 * @param dir The directory to copy into
 */
void arch_mmu_copy_kernel(mmu_dir_t *dir) {
    memcpy(&dir[256], &__mmu_kernel_pml[256], sizeof(mmu_page_t) * 256);
}

/**
 * @brief Set flags
 * @param dir The directory to set flags in
 * @param i The virtual address
 * @param flags The flags to set
 * @returns 0 on success, 1 on failure
 */
int arch_mmu_setflags(mmu_dir_t *dir, uintptr_t i, mmu_flags_t flags) {
    mmu_page_t *page = arch_mmu_get_page(dir, i, false);
    if (!page) return 1;

    page->bits.present = (flags & MMU_FLAG_PRESENT) ? 1 : 0;
    page->bits.rw = (flags & MMU_FLAG_WRITE) ? 1 : 0;
    page->bits.usermode = (flags & MMU_FLAG_USER) ? 1 : 0;
    page->bits.nx = (flags & MMU_FLAG_NOEXEC) ? 1 : 0;
    page->bits.global = (flags & MMU_FLAG_GLOBAL) ? 1 : 0;
    page->bits.size = (flags & MMU_FLAG_WC) ? 1 : 0;
    page->bits.writethrough = (flags & MMU_FLAG_WT) ? 1 : 0;
    page->bits.cache_disable = (flags & MMU_FLAG_UC) ? 1 : 0;

    return 0;
}