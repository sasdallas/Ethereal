
/**
 * @file hexahedron/arch/i386/arch.c
 * @brief Architecture startup header for I386
 * 
 * This file handles the beginning initialization of everything specific to this architecture.
 * For I386, it sets up things like interrupts, TSSes, SMP cores, etc.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

// Polyhedron
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// General
#include <kernel/kernel.h>
#include <kernel/config.h>
#include <kernel/multiboot.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/pmm.h>
#include <kernel/generic_mboot.h>
#include <kernel/misc/spinlock.h>
#include <kernel/processor_data.h>
#include <kernel/gfx/gfx.h>
#include <kernel/misc/args.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/ksym.h>
#include <kernel/loader/driver.h>

// Architecture-specific
#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/arch.h>
#include <kernel/arch/i386/smp.h>

// Generic drivers
#include <kernel/drivers/serial.h>
#include <kernel/drivers/video.h>

// Structures
#include <structs/json-builder.h>
#include <structs/json.h>


// Parameters
generic_parameters_t *parameters;

// External kernel variables
extern uintptr_t __bss_end;

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 */
void arch_say_hello(int is_debug) {

    if (!is_debug) {
        gfx_drawLogo(COLOR_WHITE);

        printf("Hexahedron %d.%d.%d-%s-%s (codename \"%s\")\n", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower, 
                    __kernel_architecture,
                    __kernel_build_configuration,
                    __kernel_version_codename);

        printf("%i system processors - %i KB of RAM\n", smp_getCPUCount(), parameters->mem_size);

        // NOTE: This should only be called once, so why not just modify some parameters?
        parameters->cpu_count = smp_getCPUCount();


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
 * @param depth How far back to stacktrace
 * @param registers Optional registers
 */
void arch_panic_traceback(int depth, registers_t *regs) {
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nSTACK TRACE:\n");

    stack_frame_t *stk = (stack_frame_t*)(regs ? (void*)regs->ebp : __builtin_frame_address(0));
    uintptr_t ip = (regs ? regs->eip : (uintptr_t)&arch_panic_traceback);

    for (int frame = 0; stk && frame < depth; frame++) {
        // Check to see if fault was in a driver
        if (ip > MEM_DRIVER_REGION && ip < MEM_DRIVER_REGION + MEM_DRIVER_REGION_SIZE) {
            // Fault in a driver - try to get it
            loaded_driver_t *data = driver_findByAddress(ip);
            if (data) {
                // We could get it
                dprintf(NOHEADER, COLOR_CODE_RED    "0x%08X (in driver '%s', loaded at %08X)\n", ip, data->metadata->name, data->load_address);
            } else {
                // We could not
                dprintf(NOHEADER, COLOR_CODE_RED    "0x%08X (in unknown driver)\n", ip);
            }

            goto _next_frame;
        }

        // Okay, make sure it was in the kernel
        if (ip > (uintptr_t)&__bss_end) {
            // Corrupt frame?
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%08X (corrupt frame - outside of kernelspace)\n", ip);
            goto _next_frame;
        }

        // In the kernel, check the name
        char *name;
        uintptr_t addr = ksym_find_best_symbol(ip, (char**)&name);
        if (addr) {
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%08X (%s+0x%x)\n", ip, name, ip - addr);
        } else {
            dprintf(NOHEADER, COLOR_CODE_RED    "0x%08X (symbols unavailable)\n", ip);
        }

    _next_frame:
        ip = stk->ip;
        stk = stk->nextframe;
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
    arch_panic_traceback(10, NULL);

    // Display message
    dprintf(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}


static uintptr_t highest_kernel_address = ((uintptr_t)&__bss_end);  // This is ONLY used until memory management is initialized.
                                                                    // mm will take over this

/**
 * @brief Zeroes and allocates bytes for a structure at the end of the kernel
 * @param bytes Amount of bytes to allocate
 * @returns The address to which the structure can be placed at 
 */
uintptr_t arch_allocate_structure(size_t bytes) {
    uintptr_t ptr = highest_kernel_address;
    highest_kernel_address += bytes;

    memset((void*)ptr, 0, bytes);
    return ptr;
}

/**
 * @brief Copy & relocate a structure to the end of the kernel.
 * @param structure_ptr A pointer to the structure
 * @param size The size of the structure
 * @returns The address to which it was relocated.
 */
uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size) {
    if (structure_ptr > (uintptr_t)&__bss_end && structure_ptr < highest_kernel_address) {
        dprintf(WARN, "arch_relocate_structure found that structure at %p likely overwritten already.\n", structure_ptr);
    }

    void *ptr_real = (void*)structure_ptr;

    if (structure_ptr > (uintptr_t)&__bss_end && highest_kernel_address + size > structure_ptr) {
        // To prevent any issues we need to copy the structure away from kmem first.
        // We'll do the allocation ourselves here
        // !!!: This is buggy and bad.
        memcpy((void*)structure_ptr + size * 2, (void*)structure_ptr, size);
        ptr_real = (void*)structure_ptr + size * 2;
    }

    uintptr_t location = arch_allocate_structure(size);
    memcpy((void*)location, ptr_real, size);
    return location;
}



/**
 * @brief Main architecture function
 * @returns Does not return
 */
__attribute__((noreturn)) void arch_main(multiboot_t *bootinfo, uint32_t multiboot_magic, void *esp) {
    // Initialize the HAL in stage #1. This sets up interrupts & more.
    hal_init(HAL_STAGE_1);

    // Align the kernel address
    highest_kernel_address += PAGE_SIZE;
    highest_kernel_address &= ~0xFFF;

    // Let's get some multiboot information.
    parameters = NULL;

    if (multiboot_magic == MULTIBOOT_MAGIC) {
        dprintf(INFO, "Found a Multiboot1 structure\n");
        parameters = arch_parse_multiboot1(bootinfo);
    } else if (multiboot_magic == MULTIBOOT2_MAGIC) {
        dprintf(INFO, "Found a Multiboot2 structure\n");
        parameters = arch_parse_multiboot2(bootinfo);
    } else {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** Unknown multiboot structure when checking kernel.\n");
    }

    dprintf(INFO, "Loaded by '%s' with command line '%s'\n", parameters->bootloader_name, parameters->kernel_cmdline);
    dprintf(INFO, "Available physical memory to machine: %i KB\n", parameters->mem_size);

    // Start the PMM system
    uintptr_t *pmm_frames = (uintptr_t*)arch_allocate_structure((parameters->mem_size * 1024) / PMM_BLOCK_SIZE);
    pmm_init(parameters->mem_size * 1024, pmm_frames);

    // Mark memory as valid/invalid. 
    arch_mark_memory(parameters, highest_kernel_address, parameters->mem_size * 1024);

    extern uint32_t __text_start;
    uintptr_t kernel_end = (uintptr_t)&__bss_end;
    uintptr_t kernel_start = (uintptr_t)&__text_start;
    dprintf(INFO, "Kernel is using %i KB in memory - extra datastructures are using %i KB\n", (kernel_end - kernel_start) / 1024, (highest_kernel_address - kernel_end) / 1024);

    // The memory subsystem is SEPARATE from the HAL.
    // This is intentional because it requires that we place certain objects such as multiboot information,
    // ACPI tables, PMM bitmaps, etc. all below the required factor.    
    mem_init(highest_kernel_address);
    
    allocator_info_t *info = alloc_getInfo();
    dprintf(INFO, "Allocator information: %s version %i.%i (valloc %s)\n", info->name, info->version_major, info->version_minor,
                                            info->support_valloc ? "supported" : "not supported");

    // Initialize arguments system
    kargs_init(parameters->kernel_cmdline);

    // We're clear to perform the second part of HAL startup
    hal_init(HAL_STAGE_2); 

    // All done. Jump to kernel main.
    kmain();

    // We shouldn't have returned - something isn't quite right.
    for (;;);
}