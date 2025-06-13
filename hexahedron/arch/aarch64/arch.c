/**
 * @file hexahedron/arch/aarch64/arch.c
 * @brief aarch64 main
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

// Polyhedron
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Architecture-specific
#include <kernel/arch/aarch64/arch.h>
#include <kernel/arch/aarch64/hal.h>
#include <kernel/arch/aarch64/mem.h>
#include <kernel/arch/aarch64/interrupt.h>

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

        printf("1 system processors - %u KB of RAM\n", parameters->mem_size);

        // NOTE: This should only be called once, so why not just modify some parameters?
        // parameters->cpu_count = smp_getCPUCount();

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
 * @brief Log putchar method
 */
int arch_putchar_early(void *user, char ch) {
    if (ch == '\n') arch_putchar_early(user, '\r');
    *((volatile uint8_t*)0xffffff8009000000) = ch;
    return 0;
}

/**
 * @brief Main architecture function
 * @param dtb The location of the DTB in the kernel
 * @param phys_base The physical base of the kernel
 */
void arch_main(uintptr_t dtb, uintptr_t phys_base) {
    debug_setOutput(arch_putchar_early);
    arch_say_hello(1);
    for (;;);
}

/**
 * @brief Prepare for a panic
 * @param bugcode The bugcode provided
 */
void arch_panic_prepare(uint32_t bugcode) {

}

/**
 * @brief Finalize a panic
 */
void arch_panic_finalize() {
	while (1) {
		asm volatile ("wfi");
	}
}