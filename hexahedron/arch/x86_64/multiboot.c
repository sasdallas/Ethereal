/**
 * @file hexahedron/arch/x86_64/multiboot.c
 * @brief Multiboot functions
 * 
 * @warning This code is messy af. If you want to understand what it's doing, please just RTFM.
 *          It will make your life so much easier.
 *          https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 *          https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 * 
 * @warning x86_64 has a specific quirk. See bottom of file
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
#include <kernel/mm/pmm.h>
#include <kernel/mm/vmm.h>
#include <kernel/arch/arch.h>
#include <kernel/arch/x86_64/smp.h>


extern uintptr_t arch_allocate_structure(size_t bytes);
extern uintptr_t arch_relocate_structure(uintptr_t structure_ptr, size_t size);

#define MBRELOC(x) ((uintptr_t)arch_mmu_remap_physical((uintptr_t)(x), 0, REMAP_TEMPORARY))

static int arch_e820_to_pmm[] = {
    [MULTIBOOT_MEMORY_AVAILABLE]        = PHYS_MEMORY_AVAILABLE,
    [MULTIBOOT_MEMORY_RESERVED]         = PHYS_MEMORY_RESERVED,
    [MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = PHYS_MEMORY_ACPI_RECLAIMABLE,
    [MULTIBOOT_MEMORY_NVS]              = PHYS_MEMORY_ACPI_NVS,
    [MULTIBOOT_MEMORY_BADRAM]           = PHYS_MEMORY_BADRAM,
};

extern uintptr_t __kernel_end_phys, __kernel_start_phys;

struct multiboot_rsdp {
    char signature[8];      // "RSD PTR ", not null terminated
    uint8_t checksum;       // Checksum validation
    char oemid[6];          // OEM string
    uint8_t revision;       // Revision of the RSDP. If not 0, cast this to an XSDP.
    uint32_t rsdt_address;  // RSDT address
    uint32_t length;        // Length
    uint64_t xsdt_address;  // XSDT address
    uint8_t checksum_ext;   // Checksum extended
    uint8_t reserved[3];    // Reserved
};

/**
 * @brief Find a tag
 * @param header The header of the structure to start from. Can be a tag. If you're providing bootinfo make sure to adjust += 8
 * @param type The type of tag to find
 * @todo Check against total_size for invalid MB2 tags?
 */
struct multiboot_tag *multiboot2_find_tag(void *header, uint32_t type) {
    uint8_t *start = (uint8_t*)header;
    struct multiboot_tag *tag = (struct multiboot_tag*)start;
    while (tag->type != 0) {
        // Start going through the tags
        if (tag->type == type) return tag;
        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    return NULL;
}

/** 
 * @brief Parse a Multiboot 2 header and packs into a @c generic_parameters structure
 * @param bootinfo A pointer to the multiboot informatino
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot2(multiboot_t *bootinfo) {
    multiboot2_t *mb2 = (multiboot2_t*)MBRELOC(bootinfo);

    // First, get some bytes for a generic_parameters structure
    generic_parameters_t *parameters = (generic_parameters_t*)arch_allocate_structure(sizeof(generic_parameters_t));
    memset(parameters, 0, sizeof(generic_parameters_t));

    // Let's go!
    int mmap_found = 0;
    int old_rsdp_found = 0;

    struct multiboot_tag *tag = (struct multiboot_tag*)MBRELOC(mb2->tags);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            mmap_found = 1;

            // Setup parameters for generic
            parameters->mmap_start = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
            memset(parameters->mmap_start, 0, sizeof(generic_mmap_desc_t));
            generic_mmap_desc_t *last_mmap_descriptor = parameters->mmap_start;
            generic_mmap_desc_t *descriptor = parameters->mmap_start;

            // Start iterating!
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap*)tag;
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
                // dprintf(DEBUG, "Memory descriptor 0x%x - 0x%016llX len 0x%016llX type 0x%x last descriptor 0x%x\n", descriptor, descriptor->address, descriptor->length, descriptor->type, last_mmap_descriptor);

                // If we're not the first update the last memory map descriptor
                if (mmap != mmap_tag->entries) {
                    last_mmap_descriptor->next = descriptor;
                    last_mmap_descriptor = descriptor;
                }

                // Reallocate & repeat
                descriptor = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
                memset(descriptor, 0, sizeof(generic_mmap_desc_t));
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
            struct multiboot_tag_basic_meminfo *meminfo_tag = (struct multiboot_tag_basic_meminfo*)tag;
            parameters->mem_size = meminfo_tag->mem_lower + meminfo_tag->mem_upper;
        } else if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            generic_module_desc_t *module = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
            memset(module, 0, sizeof(generic_module_desc_t));
            struct multiboot_tag_module *mod_tag = (struct multiboot_tag_module*)tag;

            if (!parameters->module_start) {
                module = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
                parameters->module_start = module;
            } else {
                generic_module_desc_t *target = parameters->module_start;
                while (target->next) target = target->next;
                target->next = module;
            }

            module->mod_start = arch_mmu_remap_physical(mod_tag->mod_start, 0xDEADBEEF, REMAP_TEMPORARY);
            module->mod_end = arch_mmu_remap_physical(mod_tag->mod_end, 0xDEADBEEF, REMAP_TEMPORARY);
            module->cmdline = (char*)arch_relocate_structure(MBRELOC(mod_tag->cmdline), strlen((char*)MBRELOC(mod_tag->cmdline)));
            
            // Null-terminate cmdline
            module->cmdline[strlen((char*)MBRELOC(mod_tag->cmdline)) - 1] = 0;

            module->next = NULL;
        } else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD) {
            dprintf(DEBUG, "Found Multiboot2 old RSDP tag\n");

            struct multiboot_tag_old_acpi *acpi = (struct multiboot_tag_old_acpi *)tag;
            if (!hal_getRSDP()) {
                uintptr_t rsdp = arch_relocate_structure((uintptr_t)acpi->rsdp, 20);
                hal_setRSDP(arch_mmu_physical(NULL, rsdp));
            }   

            old_rsdp_found = 1;
        } else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
            struct multiboot_tag_new_acpi *acpi = (struct multiboot_tag_new_acpi*)tag;

            dprintf(DEBUG, "Found Multiboot2 new RSDP tag\n");
            if (!hal_getRSDP() || old_rsdp_found) {
                struct multiboot_rsdp *acpi_rsdp = (struct multiboot_rsdp*)acpi->rsdp;

                uintptr_t rsdp = arch_relocate_structure((uintptr_t)acpi_rsdp, (acpi_rsdp->length > sizeof(struct multiboot_rsdp)) ? sizeof(struct multiboot_rsdp) : acpi_rsdp->length);
                hal_setRSDP(arch_mmu_physical(NULL, rsdp));
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME) {
            struct multiboot_tag_string *bootldr = (struct multiboot_tag_string *)tag; 
            parameters->bootloader_name = (char*)arch_relocate_structure((uintptr_t)bootldr->string, strlen(bootldr->string));
            parameters->bootloader_name[strlen(bootldr->string)] = 0;
        } else if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE) {
            struct multiboot_tag_string *cmdline = (struct multiboot_tag_string *)tag; 
            if (strlen(cmdline->string)) {
            parameters->kernel_cmdline = (char*)arch_relocate_structure((uintptr_t)cmdline->string, strlen(cmdline->string));
            parameters->kernel_cmdline[strlen(cmdline->string)] = 0;
            } else {
                parameters->kernel_cmdline = (char*)arch_allocate_structure(1);
                parameters->kernel_cmdline[0] = 0;
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer *fb_tag = (struct multiboot_tag_framebuffer*)tag;
            parameters->framebuffer = (generic_fb_desc_t*)arch_allocate_structure(sizeof(generic_fb_desc_t));
            parameters->framebuffer->framebuffer_addr = fb_tag->common.framebuffer_addr;
            parameters->framebuffer->framebuffer_width = fb_tag->common.framebuffer_width;
            parameters->framebuffer->framebuffer_height = fb_tag->common.framebuffer_height;
            parameters->framebuffer->framebuffer_bpp = fb_tag->common.framebuffer_bpp;
            parameters->framebuffer->framebuffer_pitch = fb_tag->common.framebuffer_pitch;
        }
        
        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }


    if (!mmap_found) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a memory map to startup properly. A memory map was not found in the Multiboot structure.\n");
        __builtin_unreachable();
    }

    return parameters;
}







/**
 * @brief Parse a Multiboot 1 header and packs into a @c generic_parameters structure
 * @param bootinfo The Multiboot information
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot1(multiboot_t *bootinfo) {
    bootinfo = (multiboot_t*)MBRELOC(bootinfo);

    // First, get some bytes for a generic_parameters structure
    generic_parameters_t *parameters = (generic_parameters_t*)arch_allocate_structure(sizeof(generic_parameters_t));
    
    // Get the easy stuff out of the way - copy some strings.
    size_t cmdline_len = strlen((char*)MBRELOC(bootinfo->cmdline));
    if (cmdline_len > 0) {
        parameters->kernel_cmdline = (char*)arch_relocate_structure(MBRELOC(bootinfo->cmdline), cmdline_len + 1);
        parameters->kernel_cmdline[strlen(parameters->kernel_cmdline)] = 0;
    } else {
        parameters->kernel_cmdline = (char*)NULL;
    }

    size_t btldr_name_len = strlen((char*)MBRELOC(bootinfo->boot_loader_name));
    if (btldr_name_len > 0) {
        parameters->bootloader_name = (char*)arch_relocate_structure(MBRELOC(bootinfo->boot_loader_name), btldr_name_len + 1);
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
    multiboot1_mod_t *module = (multiboot1_mod_t*)MBRELOC(bootinfo->mods_addr);
    parameters->module_start = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
    parameters->module_start->cmdline = (char*)arch_relocate_structure(MBRELOC(module->cmdline), strlen((char*)MBRELOC(module->cmdline)));
    
    dprintf(DEBUG, "Relocating module %p - %p (%d)\n", module->mod_start, module->mod_end, (uintptr_t)module->mod_end - (uintptr_t)module->mod_start);
    parameters->module_start->mod_start = (uintptr_t)arch_mmu_remap_physical(module->mod_start, (uintptr_t)module->mod_end - (uintptr_t)module->mod_start, REMAP_TEMPORARY); 
    parameters->module_start->mod_end = parameters->module_start->mod_start + (module->mod_end - module->mod_start);

    // Are we done yet?
    if (bootinfo->mods_count == 1) goto _done_modules;

    // Iterate
    generic_module_desc_t *last_mod_descriptor = parameters->module_start;
    for (uint32_t i = 1; i < bootinfo->mods_count; i++) {
        module++;

        generic_module_desc_t *mod_descriptor = (generic_module_desc_t*)arch_allocate_structure(sizeof(generic_module_desc_t));
        mod_descriptor->cmdline = (char*)arch_relocate_structure(MBRELOC(module->cmdline), strlen((char*)MBRELOC(module->cmdline)));
        
        // Relocate the module's contents
        mod_descriptor->mod_start = arch_relocate_structure((uintptr_t)module->mod_start, (uintptr_t)module->mod_end - (uintptr_t)module->mod_start);
        mod_descriptor->mod_end = mod_descriptor->mod_start + (module->mod_end - module->mod_start);

        // Null-terminate cmdline
        mod_descriptor->cmdline[strlen((char*)MBRELOC(mod_descriptor->cmdline)) - 1] = 0;

        last_mod_descriptor->next = mod_descriptor;
        last_mod_descriptor = mod_descriptor;
    }

_done_modules:

    // Done with the modules, handle the memory map now.
    if (!(bootinfo->flags & 0x040)) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a memory map to startup properly. A memory map was not found in the Multiboot structure.\n");
        __builtin_unreachable();
    }

    // mem_upper and mem_lower can be inaccurate
    // parameters->mem_size = bootinfo->mem_upper +s bootinfo->mem_lower;

    parameters->mmap_start = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));

    multiboot1_mmap_entry_t *mmap;
    generic_mmap_desc_t *last_mmap_descriptor = parameters->mmap_start;
    generic_mmap_desc_t *descriptor = parameters->mmap_start;
    

    uintptr_t memory_size = 0x0;

    for (mmap = (multiboot1_mmap_entry_t*)MBRELOC(bootinfo->mmap_addr);
                (uintptr_t)mmap < MBRELOC(bootinfo->mmap_addr) + bootinfo->mmap_length;
                mmap = (multiboot1_mmap_entry_t*)MBRELOC(((uintptr_t)mmap + mmap->size + sizeof(mmap->size)))) {
                

        descriptor->address = mmap->addr;
        descriptor->length = mmap->len;
        
        // Translate mmap type to generic type 
        switch (mmap->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                descriptor->type = GENERIC_MEMORY_AVAILABLE;
                memory_size = descriptor->address + descriptor->length;
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
        if ((uintptr_t)mmap != MBRELOC(bootinfo->mmap_addr)) {
            last_mmap_descriptor->next = descriptor;
            last_mmap_descriptor = descriptor;
        }

        // Reallocate & repeat
        descriptor = (generic_mmap_desc_t*)arch_allocate_structure(sizeof(generic_mmap_desc_t));
    }

    // Set memory size
    parameters->mem_size = memory_size;

    last_mmap_descriptor->next = NULL;

      
    return parameters;    
}


/**** x86_64 specific ****/

static multiboot_t *stored_bootinfo = NULL;
static int is_mb2 = 0;

/**
 * @brief Mark/unmark valid spots in memory
 * @param highest_address The highest kernel address
 * @param mem_size The memory size.
 */
void arch_mark_memory(uintptr_t highest_address, uintptr_t mem_size) { }



/**
 * @brief x86_64-specific parser function for Multiboot1
 * @param bootinfo The boot information
 * @param mem_size Output pointer to mem_size
 * @param first_free_page First free page output
 * 
 * This is here because paging is already enabled in x86_64, meaning
 * we have to initialize the allocator. It's very hacky, but it does end up working.
 * (else it will overwrite its own page tables and crash or something, i didn't do much debugging)
 */
void arch_parse_multiboot1_early(multiboot_t *bootinfo, uintptr_t *mem_size, uintptr_t *first_free_page) {
    // Store structure for later use
    stored_bootinfo = bootinfo;
    is_mb2 = 0;
    uintptr_t kernel_addr = (uintptr_t)&__kernel_end_phys;
    uintptr_t msize = (uintptr_t)&__kernel_end_phys;

    // Check if memory map was provided 
    if (!(bootinfo->flags & 0x040)) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "arch", "*** The kernel requires a memory map to startup properly. A memory map was not found in the Multiboot structure.\n");
        __builtin_unreachable();
    }
    
    multiboot1_mmap_entry_t *mmap = (void*)(uintptr_t)bootinfo->mmap_addr;

    // Handle the memory map in relation to highest kernel address
    if ((uintptr_t)mmap + bootinfo->mmap_length > kernel_addr) {
        kernel_addr = (uintptr_t)mmap + bootinfo->mmap_length;
    }

    while ((uintptr_t)mmap < bootinfo->mmap_addr + bootinfo->mmap_length) {
        if (mmap->type == 1 && mmap->len && mmap->addr + mmap->len - 1 > msize) {
            msize = mmap->addr + mmap->len - 1;
        }

        mmap = (multiboot1_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(uint32_t));
    }

    if (bootinfo->mods_count) {
        multiboot1_mod_t *mods = (multiboot1_mod_t*)(uintptr_t)bootinfo->mods_addr;
        for (uint32_t i = 0; i < bootinfo->mods_count; i++) {
            if ((uintptr_t)mods[i].mod_end > kernel_addr) {
                dprintf(DEBUG, "Module found that is greater than kernel address (%p)\n", mods[i].mod_end);
                kernel_addr = mods[i].mod_end;
            }
        }
    }

    // Round maximum kernel address up a page
    kernel_addr = (kernel_addr + 0x1000) & ~0xFFF;

    // Set pointers
    *first_free_page = kernel_addr;
    *mem_size = msize; 
}

/**
 * @brief x86_64-specific parser function for Multiboot2
 * @param bootinfo The boot information
 * @param mem_size Output pointer to mem_size
 * @param first_free_page First free page output
 * 
 * This is here because paging is already enabled in x86_64, meaning
 * we have to initialize the allocator. It's very hacky, but it does end up working.
 * (else it will overwrite its own page tables and crash or something, i didn't do much debugging)
 */
void arch_parse_multiboot2_early(multiboot_t *bootinfo1, uintptr_t *mem_size, uintptr_t *first_free_page) {
    multiboot2_t *bootinfo = (multiboot2_t*)bootinfo1;
    stored_bootinfo = bootinfo1;
    is_mb2 = 1;

    uintptr_t memory_size = 0;
    uintptr_t kend = (uintptr_t)&__kernel_end_phys;

    struct multiboot_tag *tag = (struct multiboot_tag*)(bootinfo->tags);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mm = (struct multiboot_tag_mmap*)tag;
            struct multiboot_mmap_entry *ent = mm->entries;

            while ((uintptr_t)ent < (uintptr_t)mm + mm->size) {
                if (ent->type == 1 && ent->len && ent->addr + ent->len - 1 > memory_size) {
                    memory_size = ent->addr + ent->len - 1;
                }

                ent = (struct multiboot_mmap_entry*)((uintptr_t)ent + mm->entry_size);
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module*)tag;
            if (mod->mod_end > kend) kend = mod->mod_end;
        }


        uintptr_t tag_ptr = (uintptr_t)tag + ((tag->size + 7) & ~7);
        if (tag_ptr > kend) kend = tag_ptr;

        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    kend = PAGE_ALIGN_UP(kend);

    *first_free_page = kend;
    *mem_size = memory_size;
}

static void arch_insert_region(pmm_region_t *region, pmm_region_t *regions, int *i) {
    // Effectively rebuilds the region map...
    pmm_region_t tmp[64];
    int cnt = 0;
    int added = 0;
    int orig_count = *i;

    for (int idx = 0; idx < orig_count; idx++) {
        pmm_region_t *r = &regions[idx];

        if (r->end <= region->start) {
            assert(cnt < 64 && "Memory region overflow");
            tmp[cnt++] = *r;
            continue;
        }

        if (r->start >= region->end) {
            if (!added) {
                assert(cnt < 64 && "Memory region overflow");
                tmp[cnt].start = region->start;
                tmp[cnt].end = region->end;
                tmp[cnt].type = region->type;
                tmp[cnt].next = NULL;
                cnt++;
                added = 1;
            }

            assert(cnt < 64 && "Memory region overflow");
            tmp[cnt++] = *r;
            continue;
        }

        if (r->start < region->start) {
            assert(cnt < 64 && "Memory region overflow");
            tmp[cnt].start = r->start;
            tmp[cnt].end = region->start;
            tmp[cnt].type = r->type;
            tmp[cnt].next = NULL;
            cnt++;
        }

        if (!added) {
            assert(cnt < 64 && "Memory region overflow");
            tmp[cnt].start = region->start;
            tmp[cnt].end = region->end;
            tmp[cnt].type = region->type;
            tmp[cnt].next = NULL;
            cnt++;
            added = 1;
        }

        if (r->end > region->end) {
            assert(cnt < 64 && "Memory region overflow");
            tmp[cnt].start = region->end;
            tmp[cnt].end = r->end;
            tmp[cnt].type = r->type;
            tmp[cnt].next = NULL;
            cnt++;

            for (int j = idx + 1; j < orig_count; j++) {
                assert(cnt < 64 && "Memory region overflow");
                tmp[cnt++] = regions[j];
            }
            break;
        }

    }

    // append region
    if (!added) {
        assert(cnt < 64 && "Memory region overflow");
        tmp[cnt].start = region->start;
        tmp[cnt].end = region->end;
        tmp[cnt].type = region->type;
        tmp[cnt].next = NULL;
        cnt++;
    }

    // restore original array
    for (int k = 0; k < cnt; k++) {
        regions[k] = tmp[k];
        regions[k].next = (k + 1 < cnt) ? &regions[k + 1] : NULL;
    }

    *i = cnt;
}

/**
 * @brief Convert the Multiboot2 memory map to an array
 * @param bootinfo The boot information
 * @param array The array of PMM memory descriptors
 */
void arch_parse_multiboot2_mmap(multiboot_t *_bootinfo, pmm_region_t *regions) {
    multiboot2_t *bootinfo = (multiboot2_t*)_bootinfo;
    stored_bootinfo = _bootinfo;
    is_mb2 = 1;

    // Three stage parsing.
    // 1. Create the initial memory map consisting of pure E820 regions (and calculate kend on the side).
    // 2. Split the region containing the kernel
    // 3. Further enshittify the map by splitting regions containing the modules
    // The last two should work since the modules and kernel MUST be in available memory

    int i = 0;
    uintptr_t kstart = (uintptr_t)&__kernel_start_phys;
    uintptr_t kend = (uintptr_t)&__kernel_end_phys;

    // Stage 1 of parsing
    struct multiboot_tag *tag = (struct multiboot_tag*)(bootinfo->tags);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mm = (struct multiboot_tag_mmap*)tag;
            struct multiboot_mmap_entry *ent = mm->entries;

            while ((uintptr_t)ent < (uintptr_t)mm + mm->size) {
                dprintf(DEBUG, "Memory map entry type=%d %016llX-%016llX\n", ent->type, ent->addr, ent->addr + ent->len);
                
                regions[i].start = ent->addr;
                regions[i].end = ent->addr + ent->len;
                regions[i].type = arch_e820_to_pmm[ent->type];
                regions[i].next = NULL;
                
                if (i != 0) regions[i-1].next = &regions[i];

                i++;
                assert(i < 64 && "Memory region overflow");

                ent = (struct multiboot_mmap_entry*)((uintptr_t)ent + mm->entry_size);
            }
        }

        uintptr_t tag_ptr = (uintptr_t)tag + ((tag->size + 7) & ~7);
        if (tag_ptr > kend) kend = tag_ptr;


        uintptr_t tag_pos  = (uintptr_t)tag;
        uintptr_t tag_end  = tag_pos + ((tag->size + 7) & ~7);

        if (tag_pos < kstart) kstart = PAGE_ALIGN_DOWN(tag_pos);
        if (tag_end > kend) kend = PAGE_ALIGN_UP(tag_end);

        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    if ((uintptr_t)bootinfo < kstart) kstart = (uintptr_t)bootinfo;

    kend = PAGE_ALIGN_UP(kend);
    assert(i != 0 && "Multiboot memory map tag had no entries or was corrupted");

    // Stage 2 of parsing
    // Figure out the region containing the kernel
    pmm_region_t kregion = {
        .start = PAGE_ALIGN_DOWN(kstart),
        .end = PAGE_ALIGN_UP(kend),
        .type = PHYS_MEMORY_KERNEL,
        .next = NULL
    };

    arch_insert_region(&kregion, regions, &i);


    // Stage 3 of parsing
    // !!!: The shitty part
    tag = (struct multiboot_tag*)(bootinfo->tags);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            // Find a region containing this
            struct multiboot_tag_module *mod = (struct multiboot_tag_module*)tag;

            pmm_region_t r = {
                .start = PAGE_ALIGN_DOWN(mod->mod_start),
                .end = PAGE_ALIGN_UP(mod->mod_end),
                .type = PHYS_MEMORY_MODULE,
                .next = NULL,
            };
            

            arch_insert_region(&r, regions, &i);
        }

        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    pmm_region_t r = {
        .start = SMP_AP_BOOTSTRAP_PAGE,
        .end = SMP_AP_BOOTSTRAP_PAGE + PAGE_SIZE,
        .type = PHYS_MEMORY_KERNEL,
        .next = NULL
    };

    arch_insert_region(&r, regions, &i);
}