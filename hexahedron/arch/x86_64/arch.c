/**
 * @file hexahedron/arch/x86_64/arch.c
 * @brief Architecture startup header for x86_64
 * 
 * This file handles the beginning initialization of everything specific to this architecture.
 * For x86_64, it sets up things like interrupts, TSSes, SMP cores, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


// Polyhedron
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Architecture-specific
#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/mem.h>
#include <kernel/arch/x86_64/interrupt.h>

// General
#include <kernel/kernel.h>
#include <kernel/config.h>
#include <kernel/multiboot.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/generic_mboot.h>
#include <kernel/processor_data.h>

// Memory
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/pmm.h>

// Misc
#include <kernel/misc/spinlock.h>
#include <kernel/misc/args.h>
#include <kernel/misc/ksym.h>

// Graphics
#include <kernel/gfx/gfx.h>

// Filesystem
#include <kernel/fs/vfs.h>
#include <kernel/fs/tarfs.h>
#include <kernel/fs/ramdev.h>

// Loader
#include <kernel/loader/driver.h>

/* Parameters */
generic_parameters_t *parameters = NULL;

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 */
void arch_say_hello(int is_debug) {

    if (!is_debug) {
        printf("Hexahedron %d.%d.%d-%s-%s (codename \"%s\")\n", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower, 
                    __kernel_architecture,
                    __kernel_build_configuration,
                    __kernel_version_codename);

        printf("%i system processors - %u KB of RAM\n", smp_getCPUCount(), pmm_getMaximumBlocks() * PMM_BLOCK_SIZE / 1024);
        printf("Booting with command line: %s\n", parameters->kernel_cmdline);

        // Draw logo
        gfx_drawLogo(RGB(255, 255, 255));
        return;
    }

    // Print out a hello message
    dprintf(NOHEADER, "%s\n", __kernel_ascii_art_formatted);
    dprintf(NOHEADER, "Hexahedron %d.%d.%d-%s-%s (codename \"%s\")\n", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower, 
                    __kernel_architecture,
                    __kernel_build_configuration,
                    __kernel_version_codename);
    
    dprintf(NOHEADER, "\tCompiled by %s on %s %s\n\n", __kernel_compiler, __kernel_build_date, __kernel_build_time);
}



/**
 * @brief Perform a stack trace using ksym
 * @pa ram depth How far back to stacktrace
 * @param registers Optional registers
 */
void arch_panic_traceback(int depth, registers_t *regs) {
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nSTACK TRACE:\n");

extern uintptr_t __kernel_start, __kernel_end;
    stack_frame_t *stk = (stack_frame_t*)(regs ? (void*)regs->rbp : __builtin_frame_address(0));
    uintptr_t ip = (regs ? regs->rip : (uintptr_t)&arch_panic_traceback);

    for (int frame = 0; stk && frame < depth; frame++) {
        dprintf(NOHEADER, COLOR_CODE_RED " 0x%016llX ", ip);

        // Check to see if fault was in a driver
        if (ip >= MEM_DRIVER_REGION && ip <= MEM_DRIVER_REGION + MEM_DRIVER_REGION_SIZE) {
            // Fault in a driver - try to get it
            loaded_driver_t *data = driver_findByAddress(ip);
            if (data) {
                // We could get it
                dprintf(NOHEADER, COLOR_CODE_RED    " (in driver '%s', loaded at %016llX)\n", data->metadata->name, data->load_address);
            } else {
                // We could not
                dprintf(NOHEADER, COLOR_CODE_RED    " (in an unknown driver)\n");
            }
        } else if (ip <= MEM_USERSPACE_REGION_END && ip >= 0x1000) {
            dprintf(NOHEADER, COLOR_CODE_RED    " (in userspace)\n");
        } else if (ip <= (uintptr_t)&__kernel_end && ip >= (uintptr_t)&__kernel_start) {
            // In the kernel, check the name
            char *name;
            uintptr_t addr = ksym_find_best_symbol(ip, (char**)&name);
            if (addr) {
                dprintf(NOHEADER, COLOR_CODE_RED    " (%s+0x%llX)\n", name, ip - addr);
            } else {
                dprintf(NOHEADER, COLOR_CODE_RED    " (symbols unavailable)\n");
            }
        } else {
            // Corrupt frame?
            dprintf(NOHEADER, COLOR_CODE_RED    " (unknown address)\n");
        }
        
        // Next frame
        ip = stk->ip;
        stk = stk->nextframe;

        // Validate
        if (!mem_validate((void*)stk, PTR_USER)) {
            dprintf(NOHEADER,   COLOR_CODE_RED      "Backtrace stopped at bad stack frame %p\n", stk);
            break;
        }
    }
}

/**
 * @brief Prepare the architecture to enter a fatal state. This means cleaning up registers,
 * moving things around, whatever you need to do
 */
void arch_panic_prepare() {
    dprintf(ERR, "Fatal panic state detected - please wait, cleaning up...\n");
    smp_disableCores();
}

/**
 * @brief Finish handling the panic, clean everything up and halt.
 * @note Does not return
 */
void arch_panic_finalize() {
    // Perform a traceback
    arch_panic_traceback(30, NULL);

    // Display message
    dprintf(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**** INTERNAL ARCHITECTURE FUNCTIONS ****/


extern uintptr_t __kernel_end_phys;
static uintptr_t first_free_page = ((uintptr_t)&__kernel_end_phys);         // This is ONLY used until memory management is initialized.
                                                                            // mm will take over this
static uintptr_t memory_size = 0x0;                                         // Same as above

/**
 * @brief Zeroes and allocates bytes for a structure at the end of the kernel
 * @param bytes Amount of bytes to allocate
 * @returns The address to which the structure can be placed at 
 */
uintptr_t arch_allocate_structure(size_t bytes) {
    dprintf(DEBUG, "CREATE STRUCTURE: %d bytes\n", bytes);
    
    if (bytes > PAGE_SIZE) return mem_sbrk(MEM_ALIGN_PAGE(bytes));
    return (uintptr_t)kmalloc(bytes);
}

/**
 * @brief Copy & relocate a structure to the end of the kernel.
 * @param structure_ptr A pointer to the structure
 * @param size The size of the structure
 * @returns The address to which it was relocated.
 */
uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size) {
    if (!size) return 0x0;
    uintptr_t location = arch_allocate_structure(size);
    memcpy((void*)location, (void*)mem_remapPhys(structure_ptr, size), size);
    return location;
}

/**
 * @brief Set the GSbase using MSRs
 */
void arch_set_gsbase(uintptr_t base) {
    cpu_setMSR(X86_64_MSR_GSBASE, base & 0xFFFFFFFF, (base >> 32) & 0xFFFFFFFF);
    cpu_setMSR(X86_64_MSR_KERNELGSBASE, base & 0xFFFFFFFF, (base >> 32) & 0xFFFFFFFF);
    asm volatile ("swapgs");
}

/**
 * @brief Set the SYSCALL handler
 */
void arch_initialize_syscall_handler() {
    // First we need to enable usage of SYSCALL/SYSRET
    uint32_t efer_lo, efer_hi;
    cpu_getMSR(X86_64_MSR_EFER, &efer_lo, &efer_hi);
    efer_lo |= 1;
    cpu_setMSR(X86_64_MSR_EFER, efer_lo, efer_hi);

    // Now we need to configure STAR (segment bases)
    cpu_setMSR(X86_64_MSR_STAR, 0x00, ((0x1B) << 16) | 0x08);

    // Set bases in LSTAR
    cpu_setMSR(X86_64_MSR_LSTAR, ((uintptr_t)&halSyscallEntrypoint & 0xFFFFFFFF), ((uintptr_t)&halSyscallEntrypoint >> 32));

    // Configure SFMASK to clear direction flag, IF, and TF
    cpu_setMSR(X86_64_MSR_SFMASK, 0x700, 0);
}

/**
 * @brief Main architecture function
 */
void arch_main(multiboot_t *bootinfo, uint32_t multiboot_magic, void *esp) {
    // !!!: Relocations may be required if I ever add back the relocatable tag
    // !!!: (which I should for compatibility)

    // Setup GSBase first
    arch_set_gsbase((uintptr_t)&processor_data[0]);

    // Initialize the hardware abstraction layer
    hal_init(HAL_STAGE_1);

    // Syscall handler
    arch_initialize_syscall_handler();

    // Align kernel address
    first_free_page += PAGE_SIZE;
    first_free_page &= ~0xFFF;

    // Parse Multiboot information
    if (multiboot_magic == MULTIBOOT_MAGIC) {
        dprintf(INFO, "Found a Multiboot1 structure\n");
        arch_parse_multiboot1_early(bootinfo, &memory_size, &first_free_page);
    } else if (multiboot_magic == MULTIBOOT2_MAGIC) {
        dprintf(INFO, "Found a Multiboot2 structure\n");
        arch_parse_multiboot2_early(bootinfo, &memory_size, &first_free_page);
    } else {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** Unknown multiboot structure when checking kernel.\n");
    }

    // Now, we can initialize memory systems.
    mem_init(memory_size, first_free_page);

    // Now we can ACTUALLY parse Multiboot information
    if (multiboot_magic == MULTIBOOT_MAGIC) {
        parameters = arch_parse_multiboot1(bootinfo);
    } else if (multiboot_magic == MULTIBOOT2_MAGIC) {
        parameters = arch_parse_multiboot2(bootinfo);
    }

    dprintf(INFO, "Loaded by '%s' with command line '%s'\n", parameters->bootloader_name, parameters->kernel_cmdline);
    dprintf(INFO, "Available physical memory to machine: %i KB\n", parameters->mem_size);

    // Initialize arguments system
    kargs_init(parameters->kernel_cmdline);

    // We're clear to perform the second part of HAL startup
    hal_init(HAL_STAGE_2); 

    // All done. Jump to kernel main.
    kmain();

    for (;;);
}