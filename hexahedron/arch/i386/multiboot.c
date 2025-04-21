/**
 * @file hexahedron/arch/i386/multiboot.c
 * @brief Multiboot functions
 * 
 * @warning This code is messy af. If you want to understand what it's doing, please just RTFM.
 *          It will make your life so much easier.
 *          https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 *          https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdint.h>
#include <string.h>

#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/generic_mboot.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/mem/pmm.h>

extern uintptr_t arch_allocate_structure(size_t bytes);
extern uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size);


/**
 * @brief Find a tag
 * @param header The header of the structure to start from. Can be a tag. If you're providing bootinfo make sure to adjust += 8
 * @param type The type of tag to find
 * @todo Check against total_size for invalid MB2 tags?
 */
struct multiboot_tag *multiboot2_find_tag(void *header, uint32_t type) {
    uint8_t *start = (uint8_t*)header;
    
    while ((uintptr_t)start & 7) start++; // Skip over padding

    struct multiboot_tag *tag = (struct multiboot_tag*)start;
    while (tag->type != 0) {
        // Start going through the tags
        if (tag->type == type) return tag;
        start += tag->size;
        while ((uintptr_t)start & 7) start++;
        tag = (struct multiboot_tag*)start;
    }

    return NULL;
}


/** 
 * @brief Parse a Multiboot 2 header and packs into a @c generic_parameters structure
 * @param bootinfo A pointer to the multiboot informatino
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot2(multiboot_t *bootinfo) {
    void *updated_bootinfo = (void*)((uint8_t*)bootinfo + 8); // TODO: ugly af

    // First, get some bytes for a generic_parameters structure
    generic_parameters_t *parameters = (generic_parameters_t*)arch_allocate_structure(sizeof(generic_parameters_t));
    
    // Find the memory map, we'll parse it first.
    struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_MMAP);
    if (mmap_tag == NULL) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a memory map to startup properly. A memory map was not found in the Multiboot structure.\n");
        __builtin_unreachable();
    }

    // Setup parameters for generic
    parameters->mmap_start = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
    generic_mmap_desc_t *last_mmap_descriptor = parameters->mmap_start;
    generic_mmap_desc_t *descriptor = parameters->mmap_start;

    // Start iterating!
    multiboot_memory_map_t *mmap;
    for (mmap = mmap_tag->entries; 
            (uint8_t*)mmap < (uint8_t*)mmap_tag + mmap_tag->size;
            mmap = (multiboot_memory_map_t*)((unsigned long)mmap + mmap_tag->entry_size)) {

        descriptor->address = mmap->addr;
        descriptor->length = mmap->len;
        switch (mmap->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                descriptor->type = GENERIC_MEMORY_AVAILABLE;
                break;
            
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                descriptor->type = GENERIC_MEMORY_ACPI_RECLAIM;
                break;
            
            case MULTIBOOT_MEMORY_NVS:
                descriptor->type = GENERIC_MEMORY_ACPI_NVS;
                break;
            
            case MULTIBOOT_MEMORY_BADRAM:
                descriptor->type = GENERIC_MEMORY_BADRAM;
                break;

            case MULTIBOOT_MEMORY_RESERVED:
            default:
                descriptor->type = GENERIC_MEMORY_RESERVED;
                break;
        }

        // Debugging output
        dprintf(DEBUG, "Memory descriptor 0x%x - 0x%016llX len 0x%016llX type 0x%x last descriptor 0x%x\n", descriptor, descriptor->address, descriptor->length, descriptor->type, last_mmap_descriptor);

        // If we're not the first update the last memory map descriptor
        if (mmap != mmap_tag->entries) {
            last_mmap_descriptor->next = descriptor;
            last_mmap_descriptor = descriptor;
        }

        // Reallocate & repeat
        descriptor = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
    }

    // Next, get the basic meminfo structure
    struct multiboot_tag_basic_meminfo *meminfo_tag = (struct multiboot_tag_basic_meminfo*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_BASIC_MEMINFO);
    if (meminfo_tag == NULL) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a Multiboot2 tag that was not provided (BASIC_MEMINFO)\n");
    }

    parameters->mem_size = meminfo_tag->mem_lower + meminfo_tag->mem_upper;

    // Parse modules
    struct multiboot_tag_module *mod_tag = (struct multiboot_tag_module*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_MODULE);
    
    if (mod_tag == NULL) goto _done_modules; // Let generic deal with this...

    parameters->module_start = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
    generic_module_desc_t *module = parameters->module_start;
    generic_module_desc_t *last_module = module;
    while (mod_tag) {
        // Relocate cmdline
        module->cmdline = (char*)arch_relocate_structure((uintptr_t)mod_tag->cmdline, strlen((char*)mod_tag->cmdline));
        module->cmdline[strlen(module->cmdline) - 1] = 0; // TODO: need to do this?
        
        // Relocate the module's contents
        module->mod_start = arch_relocate_structure((uintptr_t)mod_tag->mod_start, (uintptr_t)(mod_tag->mod_end - mod_tag->mod_start));
        module->mod_end = module->mod_start + (mod_tag->mod_end - mod_tag->mod_start);

        struct multiboot_tag_module *next_tag = (struct multiboot_tag_module*)multiboot2_find_tag((void*)mod_tag, MULTIBOOT_TAG_TYPE_MODULE);
        if (next_tag == NULL) break;
        mod_tag = next_tag;

        // Allocate the next one
        last_module = module;
        module = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
        last_module->next = module;
    }

_done_modules: ;

    // Let's get the framebuffer
    struct multiboot_tag_framebuffer *fb_tag = (struct multiboot_tag_framebuffer*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
    if (fb_tag) {
        parameters->framebuffer = (generic_fb_desc_t*)arch_allocate_structure(sizeof(generic_fb_desc_t));
        parameters->framebuffer->framebuffer_addr = fb_tag->common.framebuffer_addr;
        parameters->framebuffer->framebuffer_width = fb_tag->common.framebuffer_width;
        parameters->framebuffer->framebuffer_height = fb_tag->common.framebuffer_height;
        parameters->framebuffer->framebuffer_bpp = fb_tag->common.framebuffer_bpp;
        parameters->framebuffer->framebuffer_pitch = fb_tag->common.framebuffer_pitch;
    }

    // Get command line and bootloader name
    struct multiboot_tag_string *cmdline = (struct multiboot_tag_string*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_CMDLINE);
    struct multiboot_tag_string *bootldr = (struct multiboot_tag_string*)multiboot2_find_tag(updated_bootinfo, MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME);

    // TODO: Unsafe strcpy
    if (cmdline && strlen((char*)cmdline->string) > 0) {
        parameters->kernel_cmdline = (char*)arch_relocate_structure((uintptr_t)cmdline->string, strlen((char*)cmdline->string));
        parameters->kernel_cmdline[strlen(parameters->kernel_cmdline) - 1] = 0;
    } else {
        parameters->kernel_cmdline = (char*)NULL;
    }
    

    if (bootldr && strlen((char*)bootldr->string) > 0) {
        parameters->bootloader_name = (char*)arch_relocate_structure((uintptr_t)bootldr->string, strlen((char*)bootldr->string));
        parameters->bootloader_name[strlen(parameters->bootloader_name) - 1] = 0;
    } else {
        parameters->bootloader_name = (char*)NULL;
    }


    // Finished!
    return parameters;
}







/**
 * @brief Parse a Multiboot 1 header and packs into a @c generic_parameters structure
 * @param bootinfo The Multiboot information
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot1(multiboot_t *bootinfo) {
    // First, get some bytes for a generic_parameters structure
    generic_parameters_t *parameters = (generic_parameters_t*)arch_allocate_structure(sizeof(generic_parameters_t));
    
    // Get the easy stuff out of the way - copy some strings.
    if (strlen((char*)bootinfo->cmdline) > 0) {
        parameters->kernel_cmdline = (char*)arch_relocate_structure(bootinfo->cmdline, strlen((char*)bootinfo->cmdline) + 1);
        parameters->kernel_cmdline[strlen(parameters->kernel_cmdline)] = 0;
    } else {
        parameters->kernel_cmdline = (char*)NULL;
    }

    if (strlen((char*)bootinfo->boot_loader_name) > 0) {
        parameters->bootloader_name = (char*)arch_relocate_structure(bootinfo->boot_loader_name, strlen((char*)bootinfo->boot_loader_name) + 1);
        parameters->bootloader_name[strlen(parameters->bootloader_name)] = 0;
    } else {
        parameters->bootloader_name = (char*)NULL;
    }

    // Make a framebuffer tag
    parameters->framebuffer = (generic_fb_desc_t*)arch_allocate_structure(sizeof(generic_fb_desc_t));
    parameters->framebuffer->framebuffer_addr = bootinfo->framebuffer_addr;
    parameters->framebuffer->framebuffer_width = bootinfo->framebuffer_width;
    parameters->framebuffer->framebuffer_height = bootinfo->framebuffer_height;
    parameters->framebuffer->framebuffer_bpp = bootinfo->framebuffer_bpp;
    parameters->framebuffer->framebuffer_pitch = bootinfo->framebuffer_pitch;

    // If we don't have any modules, womp womp
    if (bootinfo->mods_count == 0) goto _done_modules;

    // Construct the initial module
    multiboot1_mod_t *module = (multiboot1_mod_t*)bootinfo->mods_addr;
    parameters->module_start = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
    parameters->module_start->cmdline = (char*)arch_relocate_structure((uintptr_t)module->cmdline, strlen((char*)module->cmdline));

    uintptr_t relocated = arch_relocate_structure((uintptr_t)module->mod_start, (uintptr_t)module->mod_end - (uintptr_t)module->mod_start);
    parameters->module_start->mod_start = relocated;
    parameters->module_start->mod_end = parameters->module_start->mod_start + (module->mod_end - module->mod_start);

    if (bootinfo->mods_count == 1) goto _done_modules;

    // Iterate
    generic_module_desc_t *last_mod_descriptor = parameters->module_start;
    for (uint32_t i = 1; i < bootinfo->mods_count; i++) {
        module++;

        generic_module_desc_t *mod_descriptor = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
        mod_descriptor->cmdline = (char*)arch_relocate_structure((uintptr_t)module->cmdline, strlen((char*)module->cmdline));
        
        // Relocate the module's contents
        mod_descriptor->mod_start = arch_relocate_structure((uintptr_t)module->mod_start, (uintptr_t)module->mod_end - (uintptr_t)module->mod_start);
        mod_descriptor->mod_end = mod_descriptor->mod_start + (module->mod_end - module->mod_start);

        // Null-terminate cmdline
        mod_descriptor->cmdline[strlen(mod_descriptor->cmdline) - 1] = 0;

        last_mod_descriptor->next = mod_descriptor;
        last_mod_descriptor = mod_descriptor;
    }

_done_modules:

    // Done with the modules, handle the memory map now.
    if (bootinfo->mmap_length == 0) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a memory map to startup properly. A memory map was not found in the Multiboot structure.\n");
        __builtin_unreachable();
    }

    parameters->mem_size = bootinfo->mem_upper + bootinfo->mem_lower;

    parameters->mmap_start = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));

    multiboot1_mmap_entry_t *mmap;
    generic_mmap_desc_t *last_mmap_descriptor = parameters->mmap_start;
    generic_mmap_desc_t *descriptor = parameters->mmap_start;
    

    for (mmap = (multiboot1_mmap_entry_t*)bootinfo->mmap_addr;
                (uint32_t)mmap < bootinfo->mmap_addr + bootinfo->mmap_length;
                mmap = (multiboot1_mmap_entry_t*) ((uint32_t)mmap + mmap->size + sizeof(mmap->size))) {
                

        descriptor->address = mmap->addr;
        descriptor->length = mmap->len;
        
        // Translate mmap type to generic type 
        switch (mmap->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                descriptor->type = GENERIC_MEMORY_AVAILABLE;
                break;
            
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                descriptor->type = GENERIC_MEMORY_ACPI_RECLAIM;
                break;
            
            case MULTIBOOT_MEMORY_NVS:
                descriptor->type = GENERIC_MEMORY_ACPI_NVS;
                break;
            
            case MULTIBOOT_MEMORY_BADRAM:
                descriptor->type = GENERIC_MEMORY_BADRAM;
                break;

            case MULTIBOOT_MEMORY_RESERVED:
            default:
                descriptor->type = GENERIC_MEMORY_RESERVED;
                break;
            
        }

        // Debugging output
        // dprintf(DEBUG, "Memory descriptor 0x%x - 0x%016llX len 0x%016llX type 0x%x last descriptor 0x%x\n", descriptor, descriptor->address, descriptor->length, descriptor->type, last_mmap_descriptor);

        // If we're not the first update the last memory map descriptor
        if ((uintptr_t)mmap != bootinfo->mmap_addr) {
            last_mmap_descriptor->next = descriptor;
            last_mmap_descriptor = descriptor;
        }

        // Reallocate & repeat
        descriptor = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
    }

    last_mmap_descriptor->next = NULL;

      
    return parameters;    
}

/**
 * @brief Mark/unmark valid spots in memory
 * @todo Work in tandem with mem.h to allow for a maximum amount of blocks to be used
 */
void arch_mark_memory(generic_parameters_t *parameters, uintptr_t highest_address, uintptr_t mem_size) {
    generic_mmap_desc_t *mmap = parameters->mmap_start;
    while (mmap) {
        // Working with 64-bits in a 32-bit environment is scary...
        if (mmap->address > UINT32_MAX) {
            dprintf(WARN, "Bad memory descriptor encountered - %016llX length %016llX (32-bit - 64-bit overflow)\n", mmap->address, mmap->length);
            mmap = mmap->next;
            continue;
        }

        if (mmap->type == GENERIC_MEMORY_AVAILABLE) {
            dprintf(DEBUG, "Marked memory descriptor %016llX - %016llX (%i KB) as available memory\n", mmap->address, mmap->address + mmap->length, mmap->length / 1024);
            pmm_initializeRegion((uintptr_t)mmap->address, (uintptr_t)mmap->length);
        } else {
            // Make sure mmap->addr isn't out of memory - most emulators like to have reserved
            // areas outside of their actual memory space, which the PMM really does not like.

            // As well as that, QEMU bugs out and forgets about certain DMA regions (see below), so
            // don't uninitialize anything below 0x10000 
            if (mmap->address > 0x100000 && mmap->address + mmap->length < mem_size) {
                dprintf(DEBUG, "Marked memory descriptor %016llX - %016llX (%i KB) as unavailable memory\n", mmap->address, mmap->address + mmap->length, mmap->length / 1024); 
                pmm_deinitializeRegion((uintptr_t)mmap->address, (uintptr_t)mmap->length);
            }
        }

        mmap = mmap->next;
    }

    // While working on previous versions of Ethereal Operating System, I accidentally brute-forced this.
    // QEMU doesn't properly unmark DMA regions, apparently - according to libvfio-user issue $493
    // https://github.com/nutanix/libvfio-user/issues/463
    
    // These DMA regions occur within the range of 0xC0000 - 0xF0000, but we'll unmap
    // the rest of the memory too. x86 real mode's memory map dictates that the first 1MB
    // or so is reserved from like 0x0-0xFFFFF for BIOS structures.
    // TODO: It may be possible to reinitialize this memory later
    dprintf(DEBUG, "Marked memory descriptor %016llX - %016llX (%i KB) as reserved memory (QEMU bug)\n", (uint64_t)0x0, (uint64_t)0x100000, ((uint64_t)0x100000 - (uint64_t)0x0) / 1024);
    pmm_deinitializeRegion(0x00000, 0x100000);

    // Unmark kernel region
    extern uint32_t __text_start;
    uintptr_t kernel_start = (uint32_t)&__text_start;
    dprintf(DEBUG, "Marked memory descriptor %016X - %016X (%i KB) as kernel memory\n", kernel_start, highest_address, (highest_address - kernel_start) / 1024);
    pmm_deinitializeRegion(kernel_start, highest_address - kernel_start);    

    dprintf(DEBUG, "Marked valid memory - PMM has %i free blocks / %i max blocks\n", pmm_getFreeBlocks(), pmm_getMaximumBlocks());
}
