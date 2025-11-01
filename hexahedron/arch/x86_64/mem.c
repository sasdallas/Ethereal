/**
 * @file hexahedron/arch/x86_64/mem.c
 * @brief Memory management functions for x86_64
 * 
 * @warning A lot of functions in this file do not conform to the "standard" of unmapping
 *          physical addresses after you have finished. This is fine for now, but may have issues
 *          later.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/mem.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/registers.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/regions.h>
#include <kernel/mem/pmm.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/misc/spinlock.h>
#include <kernel/task/process.h>
#include <kernel/misc/util.h>

#include <stdint.h>
#include <string.h>

// Heap/MMIO/driver space
uintptr_t mem_kernelHeap                = 0xAAAAAAAAAAAAAAAA;   // Kernel heap
uintptr_t mem_driverRegion              = MEM_DRIVER_REGION;    // Driver space
uintptr_t mem_dmaRegion                 = MEM_DMA_REGION;       // DMA region
uintptr_t mem_mmioRegion                = MEM_MMIO_REGION;      // MMIO region

// Spinlocks
static spinlock_t heap_lock = { 0 };
static spinlock_t driver_lock = { 0 };
static spinlock_t dma_lock = { 0 };
static spinlock_t mmio_lock = { 0 };

// Stub variables (for debugger)
uintptr_t mem_mapPool                   = 0xAAAAAAAAAAAAAAAA;
uintptr_t mem_identityMapCacheSize      = 0xAAAAAAAAAAAAAAAA;   

// Whether to use 5-level paging (TODO)
static int mem_use5LevelPaging = 0;

// TODO: i386 doesn't use this method of having preallocated page structures... maybe we should use it there?

// Base page layout - loader uses this
page_t mem_kernelPML[3][512] __attribute__((aligned(PAGE_SIZE))) = {0};

// Low base PDPT/PD/PT (identity mapping space for kernel/other stuff)
page_t mem_identityBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0}; 
page_t mem_identityBasePD[128][512] __attribute__((aligned(PAGE_SIZE))) = {0};

// High base PDPT/PD/PT (identity mapping space for anything)
page_t mem_highBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_highBasePD[512] __attribute__((aligned(PAGE_SIZE))) = {0}; // If we're already using 2MiB paging, why bother with PTs? 
page_t mem_highBasePTs[512*12] __attribute__((aligned(PAGE_SIZE))) = {0};

// Heap PDPT/PD/PT
page_t mem_heapBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_heapBasePD[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_heapBasePT[512*3] __attribute__((aligned(PAGE_SIZE))) = {0};

/* Helper macro */
#define KERNEL_PHYS(x) (uintptr_t)((uintptr_t)(x) - 0xFFFFF00000000000)

// Log method
#define LOG(status, ...) dprintf_module(status, "ARCH:MEM", __VA_ARGS__);

/**
 * @brief Get the current directory (just current CPU)
 */
page_t *mem_getCurrentDirectory() {
    assert(0);
    return NULL;
}

/**
 * @brief Get the kernel page directory/root-level PML
 * @note RETURNS A VIRTUAL ADDRESS
 */
page_t *mem_getKernelDirectory() {
    return (page_t*)&mem_kernelPML[0];
}

/**
 * @brief Get the current position of the kernel heap
 * @returns The current position of the kernel heap
 */
uintptr_t mem_getKernelHeap() {
    return mem_kernelHeap;
}

/**
 * @brief Invalidate a page in the TLB
 * @param addr The address of the page 
 * @warning This function is only to be used when removing P-V mappings. Just free the page if it's identity.
 */
inline void mem_invalidatePage(uintptr_t addr) {
    STUB();
}

/**
 * @brief Switch the memory management directory
 * @param pagedir The virtual address of the page directory to switch to, or NULL for the kernel region
 * 
 * @warning Pass something mapped by mem_clone() or something in the identity-mapped PMM region.
 *          Anything greater than IDENTITY_MAP_MAXSIZE will be truncated in the PDBR.
 * 
 * @returns -EINVAL on invalid, 0 on success.
 */
int mem_switchDirectory(page_t *pagedir) {
    STUB();
}

/**
 * @brief Create a new, completely blank virtual address space
 * @returns A pointer to the VAS
 */
page_t *mem_createVAS() {
    page_t *vas = (page_t*)mem_remapPhys(pmm_allocateBlock(), PMM_BLOCK_SIZE);
    memset((void*)vas, 0, PMM_BLOCK_SIZE);
    return vas;
}


/**
 * @brief Destroys and frees the memory of a VAS
 * @param vas The VAS to destroy
 * 
 * @warning IMPORTANT: DO NOT FREE ANY PAGES. Just free the associated PML/PDPT/PD/PT.  
 * 
 * @warning Make sure the VAS being freed isn't the current one selected
 */
void mem_destroyVAS(page_t *vas) {
    STUB();
}

/**
 * @brief Copy a usermode page. 
 * @param src_page The source page
 * @param dest_page The destination page
 */
static void mem_copyUserPage(page_t *src_page, page_t *dest_page) {
    STUB();
}


/**
 * @brief Clone a page directory.
 * 
 * This is a full PROPER page directory clone.
 * This function does it properly and clones the page directory, its tables, and their respective entries fully.
 * YOU SHOULD NOT DO COW ON THIS. The virtual address system (VAS) will handle CoW for you. 
 * 
 * @param dir The source page directory. Keep as NULL to clone the current page directory.
 * @returns The page directory on success
 */
page_t *mem_clone(page_t *dir) {
    STUB();
}

/**
 * @brief Map a physical address to a virtual address
 * 
 * @param dir The directory to map them in (leave blank for current)
 * @param phys The physical address
 * @param virt The virtual address
 * @param flags Additional flags to use (e.g. MEM_KERNEL)
 */
void mem_mapAddress(page_t *dir, uintptr_t phys, uintptr_t virt, int flags) {
    STUB();
}


/**
 * @brief Returns the page entry requested
 * @param dir The directory to search. Specify NULL for current directory
 * @param address The virtual address of the page (will be aligned for you if not aligned)
 * @param flags The flags of the page to look for
 * 
 * @warning Specifying MEM_CREATE will only create needed structures, it will NOT allocate the page!
 *          Please use a function such as mem_allocatePage to do that.
 */
page_t *mem_getPage(page_t *dir, uintptr_t address, uintptr_t flags) {
    STUB();
}


/**
 * @brief Allocate a page using the physical memory manager
 * 
 * @param page The page object to use. Can be obtained with mem_getPage
 * @param flags The flags to follow when setting up the page
 * 
 * @note You can also use this function to set bits of a specific page - just specify @c MEM_NOALLOC in @p flags.
 * @warning The function will automatically allocate a PMM block if NOALLOC isn't specified
 */
void mem_allocatePage(page_t *page, uintptr_t flags) {
    STUB();
}

/**
 * @brief Free a page
 * 
 * @param page The page to free
 */
void mem_freePage(page_t *page) {
    STUB();
}


/**
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 * 
 * @note You need to call @c mem_unmapPhys when you are finished with the address.
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) {
    STUB();
}

/**
 * @brief Unmap a PMM address in the identity mapped region
 * @param frame_address The address of the frame to unmap, as returned by @c mem_remapPhys
 * @param size The size of the frame to unmap
 */
void mem_unmapPhys(uintptr_t frame_address, uintptr_t size) {
    // No caching system is in place, no unmapping.
}


/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr) {
    STUB();
}

/**
 * @brief Page fault handler
 * @param exception_index 14
 * @param regs Registers
 * @param extended Extended registers
 */
int mem_pageFault(uintptr_t exception_index, registers_t *regs, extended_registers_t *regs_extended) {
    // Check if this was a usermode page fault
    if (regs->cs != 0x08) {
        // Was this an exception because we didn't map their heap?
        if (regs_extended->cr2 >= current_cpu->current_process->heap_base && regs_extended->cr2 < current_cpu->current_process->heap) {
            // Yes, it was, handle appropriately by mapping this page
            mem_allocatePage(mem_getPage(NULL, regs_extended->cr2, MEM_CREATE), MEM_DEFAULT);
            return 0;
        }

        if (regs_extended->cr2 > MEM_USERMODE_STACK_REGION && regs_extended->cr2 < MEM_USERMODE_STACK_REGION + PAGE_SIZE*2) {
            mem_allocatePage(mem_getPage(NULL, regs_extended->cr2, MEM_CREATE), MEM_DEFAULT);
            return 0;
        }

        // Check for VAS fault
        // Default hint is 0x10000
        if (vas_fault(current_cpu->current_process->vas, regs_extended->cr2, 0x10000)) {
            return 0;
        }

        // TODO: This code can probably bug out - to be extensively tested
        printf(COLOR_CODE_RED "Process \"%s\" (TID: %d, PID: %d) encountered a page fault at address %p and will be shutdown\n" COLOR_CODE_RESET, current_cpu->current_process->name, current_cpu->current_thread->tid, current_cpu->current_process->pid, regs_extended->cr2);
        
        // Dump debug information
        LOG(ERR, "Process \"%s\" (PID: %d) encountered page fault at %p with no valid resolution (error code: 0x%x). Shutdown\n", current_cpu->current_process->name, current_cpu->current_process->pid, regs_extended->cr2, regs->err_code);
        LOG(ERR, "The fault occurred @ IP %04x:%016llX SP %016llX\n", regs->cs, regs->rip, regs->rsp);
        vas_dump(current_cpu->current_process->vas);
        
        // Perform traceback
        LOG(ERR, "STACK BACKTRACE:\n");
        LOG(ERR, "Starting @ IP: %016llX\n", regs->rip);
        stack_frame_t *stk = (stack_frame_t*)regs->rbp;
        while (stk) {
            if (!mem_validate(stk, PTR_USER)) {
                LOG(ERR, "Corrupted stack frame 0x%016llX detected\n", stk);
                break;
            }

            LOG(ERR, "FRAME 0x%016llX: 0x%016llX\n", stk, stk->ip);
        
            stk = stk->nextframe;
        }


        process_exit(current_cpu->current_process, 1);
        return 0;
    }

    if (current_cpu->current_process && current_cpu->current_process->vas) {
        // Check for fault
        if (vas_fault(current_cpu->current_process->vas, regs_extended->cr2, 0x2000)) {
            return 0;
        }
    }

    uintptr_t page_fault_addr = 0x0;
    asm volatile ("movq %%cr2, %0" : "=a"(page_fault_addr));

    LOG(ERR, "#PF (%016llX): IP %04x:%016llX SP %016llX\n", page_fault_addr, regs->cs, regs->rip, regs->rsp);

    // Page fault, get the address
    kernel_panic_prepare(CPU_EXCEPTION_UNHANDLED);
        
    // Print it out
    LOG(NOHEADER, COLOR_CODE_RED "*** Page fault at address " COLOR_CODE_RED_BOLD "0x%016llX\n" COLOR_CODE_RED, page_fault_addr);

    // Determine fault cause
    int present = (regs->err_code & (1 << 0));
    int write = (regs->err_code & (1 << 1));
    int rsvd = (regs->err_code & (1 << 3));
    int instruction = (regs->err_code & (1 << 4));

    LOG(NOHEADER, "*** Memory access type: %s%s%s%s\n\n", 
                            (present) ? "PRESENT_IN_MEMORY " : "NOT_PRESENT_IN_MEMORY ",
                            (write) ? "WRITE " : "READ ",
                            (rsvd) ? "RSVD_BIT_SET " : "",
                            (instruction) ? "INSTRUCTION_FETCH " : "");

    LOG(NOHEADER, "\033[1;31mFAULT REGISTERS:\n\033[0;31m");

    LOG(NOHEADER, "RAX %016llX RBX %016llX RCX %016llX RDX %016llX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    LOG(NOHEADER, "RDI %016llX RSI %016llX RBP %016llX RSP %016llX\n", regs->rdi, regs->rsi, regs->rbp, regs->rsp);
    LOG(NOHEADER, "R8  %016llX R9  %016llX R10 %016llX R11 %016llX\n", regs->r8, regs->r9, regs->r10, regs->r11);
    LOG(NOHEADER, "R12 %016llX R13 %016llX R14 %016llX R15 %016llX\n", regs->r12, regs->r13, regs->r14, regs->r15);
    LOG(NOHEADER, "ERR %016llX RIP %016llX RFL %016llX\n\n", regs->err_code, regs->rip, regs->rflags);

    LOG(NOHEADER, "CS %04X DS %04X SS %04X\n\n", regs->cs, regs->ds, regs->ss);
    LOG(NOHEADER, "CR0 %08X CR2 %016llX CR3 %016llX CR4 %08X\n", regs_extended->cr0, regs_extended->cr2, regs_extended->cr3, regs_extended->cr4);
    LOG(NOHEADER, "GDTR %016llX %04X\n", regs_extended->gdtr.base, regs_extended->gdtr.limit);
    LOG(NOHEADER, "IDTR %016llX %04X\n", regs_extended->idtr.base, regs_extended->idtr.limit);

    // !!!: not conforming (should call kernel_panic_finalize) but whatever
    // We want to do our own traceback.
extern void arch_panic_traceback(int depth, registers_t *regs);
    arch_panic_traceback(10, regs);

    // Show core processes
    LOG(NOHEADER, COLOR_CODE_RED_BOLD "\nCPU DATA:\n" COLOR_CODE_RED);

    for (int i = 0; i < MAX_CPUS; i++) {
        if (processor_data[i].cpu_id || !i) {
            // We have valid data here
            if (processor_data[i].current_thread != NULL) {
                LOG(NOHEADER, COLOR_CODE_RED "CPU%d: Current thread %p (process '%s') - page directory %p\n", i, processor_data[i].current_thread, processor_data[i].current_process->name, processor_data[i].current_context->dir);
            } else {
                LOG(NOHEADER, COLOR_CODE_RED "CPU%d: No thread available. Page directory %p\n", processor_data[i].current_context->dir);
            }
        }
    }

    // Display message
    LOG(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will identity map the kernel into memory and setup page tables.
 * For x86_64, it also sets up the PMM allocator.
 * 
 * @warning MEM_HEAP_REGION is hardcoded into this - we can probably use mem_mapAddress??
 * @warning This is some pretty messy code, I'm sorry :(
 * 
 * @param mem_size The size of memory (aka highest possible address)
 * @param first_free_page The first free page after the kernel
 */
void mem_init(uintptr_t mem_size, uintptr_t first_free_page) {
    STUB();
}

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b) {
    STUB();
}

/**
 * @brief Allocate a region of memory
 * @param start The starting virtual address (OPTIONAL IF YOU SPECIFY MEM_ALLOC_HEAP)
 * @param size How much memory to allocate (will be aligned)
 * @param flags Flags to use for @c mem_allocate (e.g. MEM_ALLOC_CONTIGUOUS)
 * @param page_flags Flags to use for @c mem_allocatePage (e.g. MEM_PAGE_KERNEL)
 * @returns Pointer to the new region of memory or 0x0 on failure
 * 
 * @note This is a newer addition to the memory subsystem. It may seem like it doesn't fit in.
 */
uintptr_t mem_allocate(uintptr_t start, size_t size, uintptr_t flags, uintptr_t page_flags) {
    STUB();
}

/**
 * @brief Free a region of memory
 * @param start The starting virtual address (must be specified)
 * @param size How much memory was allocated (will be aligned)
 * @param flags Flags to use for @c mem_free (e.g. MEM_ALLOC_HEAP)
 * @note Most flags do not affect @c mem_free
 */
void mem_free(uintptr_t start, size_t size, uintptr_t flags) {
    STUB()
}

/**
 * @brief Validate a specific pointer in memory
 * @param ptr The pointer you wish to validate
 * @param flags The flags the pointer must meet - by default, kernel mode and R/W (see PTR_...)
 * @returns 1 on a valid pointer
 */
int mem_validate(void *ptr, unsigned int flags) {
    STUB();
}