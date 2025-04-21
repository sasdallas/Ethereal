/**
 * @file hexahedron/include/kernel/generic_mboot.h
 * @brief Provides definitions for a generic Multiboot-like structure
 * 
 * This structure is used when passing to the general kernel, but only some are actually
 * used (marked as REQUIRED).
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_GENERIC_MBOOT_H
#define KERNEL_GENERIC_MBOOT_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

// Generic module descriptor
typedef struct generic_module_descriptor {
    uintptr_t mod_start;                        // Starting address of the module
    uintptr_t mod_end;                          // Ending address of the module
    char *cmdline;                              // Command-line options passed to the module
    struct generic_module_descriptor *next;     // Next module
} generic_module_desc_t;

// Generic framebuffer descriptor for LFB video - mainly for records when setting up
typedef struct generic_fb_desc_t {
    uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t  framebuffer_bpp;
} generic_fb_desc_t;

// Generic memory map descriptor
typedef struct generic_mmap_desc {
    uint64_t address;                   // Address of the memory
    uint64_t length;                    // Length of the region
    uint32_t type;                      // Type
    struct generic_mmap_desc *next;     // Next memory descriptor (NULL = finished)
} generic_mmap_desc_t;

// Generic parameters passed to the kernel and used in other arch stuff. This is useful when loading.
typedef struct generic_parameters {
    /* Kernel load options */
    char *kernel_cmdline;                   // REQUIRED - Kernel command line
    char *bootloader_name;                  // Bootloader name

    /* Modules */
    generic_module_desc_t *module_start;    // REQUIRED - List of modules

    /* Framebuffer */
    generic_fb_desc_t *framebuffer;         // Framebuffer descriptor

    /* Memory */
    generic_mmap_desc_t *mmap_start;        // Starting point of the memory map
    uint64_t mem_size;                      // Memory size in KB

    /* SMP */
    uint32_t cpu_count;                     // System processor count
} generic_parameters_t;


/**** DEFINITIONS ****/

#define GENERIC_MEMORY_AVAILABLE        0
#define GENERIC_MEMORY_RESERVED         1
#define GENERIC_MEMORY_ACPI_RECLAIM     2
#define GENERIC_MEMORY_ACPI_NVS         3
#define GENERIC_MEMORY_BADRAM           4

#endif