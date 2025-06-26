/**
 * @file userspace/dynlinker/phdr.c
 * @brief Handles PHDR parsing in the dynamic linker
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "dynlinker.h"
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>

#define ELF_PHDR(ehdr, idx) ((Elf64_Phdr*)((uintptr_t)ehdr + ehdr->e_phoff + ehdr->e_phentsize * idx))

/**
 * @brief Load the PHDRs of an ELF file to a designated load address
 * @param obj The object to load
 * @returns 0 on success
 */
int elf_load(elf_obj_t *obj) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)obj->buffer;

    // Calculate the total size of the object
    // !!!: Ugh..
    uintptr_t base = (uintptr_t)-1;
    uintptr_t end = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        if (phdr->p_type == PT_LOAD || phdr->p_type == PT_TLS) {
            if (phdr->p_vaddr < base) base = phdr->p_vaddr;
            if (phdr->p_vaddr + phdr->p_memsz > end) end = phdr->p_vaddr + phdr->p_memsz;
        }
    }

    if (base == (uintptr_t)-1) {
        fprintf(stderr, "[%s] No valid PT_LOAD PHDRs were found.\n", obj->filename);
        return 1;
    }

    // Now that we have base/end, calculate size
    obj->size = (end - base);
    obj->base = (void*)base;

    // Make some memory for it
    obj->base = mmap((ehdr->e_type == ET_DYN) ? NULL : obj->base, obj->size, PROT_EXEC | PROT_READ | PROT_WRITE, (ehdr->e_type != ET_DYN ? MAP_FIXED : 0) | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (obj->base == MAP_FAILED) {
        fprintf(stderr, "%s: mmap: %s\n", obj->filename, strerror(errno));
        return 1;
    }

    if (ehdr->e_type != ET_DYN) obj->base = 0x0;

    LD_DEBUG("[%s] Base %p Size %d\n", obj->filename, obj->base, obj->size);

    // Start loading
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        switch (phdr->p_type) {
            case PT_LOAD:
                LD_DEBUG("[%s] PT_LOAD Off %p VirtAddr %p PhysAddr %p FileSize %d MemSize %d\n", obj->filename, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);
                
                void *addr = (void*)(obj->base + phdr->p_vaddr);
                memcpy(addr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);

                // Zero remainder
                if (phdr->p_memsz > phdr->p_filesz) {
                    memset((void*)addr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
                }

                break;
            
            case PT_INTERP:
                LD_DEBUG("[%s] PT_INTERP %s\n", obj->filename, (char*)((uintptr_t)ehdr + phdr->p_offset));
                break;
            
            case PT_PHDR:
                LD_DEBUG("[%s] PT_PHDR\n", obj->filename);
                break;
            
            case PT_DYNAMIC:
                LD_DEBUG("[%s] PT_DYNAMIC %p\n", obj->filename, phdr->p_vaddr);
                obj->dynamic = (void*)((uintptr_t)obj->base + phdr->p_vaddr);
                break;

            default:
                LD_DEBUG("[%s] Unknown PHDR %d\n", obj->filename, phdr->p_type);
                break;
        }
    }

    return 0;
}

/**
 * @brief Parse dynamic table to get all dependencies
 * @param obj The object to parse the table of
 * @returns 0 on success
 */
int elf_dynamic(elf_obj_t *obj) {
    if (obj->dynamic) {
        // Get the dynamic table
        Elf64_Dyn *dyn = (Elf64_Dyn*)obj->dynamic;

        // First find the string table
        while (dyn->d_tag != DT_NULL) {
            if (dyn->d_tag == DT_STRTAB) {
                obj->dyntab.strtab = (void*)((uintptr_t)obj->base + dyn->d_un.d_ptr);
                break;
            }

            dyn++;
        }

        // Reset and actually look
        dyn = (Elf64_Dyn*)obj->dynamic;
        while (dyn->d_tag != DT_NULL) {
            switch (dyn->d_tag) {
                case DT_NEEDED:
                    LD_DEBUG("[%s] (NEEDED ) %s\n", obj->filename, obj->dyntab.strtab + dyn->d_un.d_val);
                    list_append(obj->dependencies, strdup(obj->dyntab.strtab + dyn->d_un.d_val));
                    break;
                case DT_SYMTAB:
                    LD_DEBUG("[%s] (SYMTAB ) %p\n", obj->filename, obj->base + dyn->d_un.d_ptr);
                    obj->dyntab.symtab = obj->base + dyn->d_un.d_ptr;
                    break;
                case DT_HASH:
                    LD_DEBUG("[%s] (HASH   ) %p\n", obj->filename, obj->base + dyn->d_un.d_ptr);
                    obj->dyntab.hash = obj->base + dyn->d_un.d_ptr;
                    obj->dyntab.symtab_sz = ((Elf64_Word*)obj->dyntab.hash)[1];
                    break;
                case DT_INIT:
                    LD_DEBUG("[%s] (INIT   ) %p\n", obj->filename, obj->base + dyn->d_un.d_ptr);
                    obj->dyntab.init = obj->base + dyn->d_un.d_ptr;
                    break;
                case DT_INIT_ARRAY:
                    LD_DEBUG("[%s] (INITAR ) %p\n", obj->filename, obj->base + dyn->d_un.d_ptr);
                    obj->dyntab.init_array = obj->base + dyn->d_un.d_ptr;
                    break;
                case DT_INIT_ARRAYSZ:
                    LD_DEBUG("[%s] (INITARS) %d\n", obj->filename, dyn->d_un.d_val);
                    obj->dyntab.init_arraysz = dyn->d_un.d_val / sizeof(uintptr_t);
                    break;
            }

            dyn++;
        }
    }

    return 0;
}
