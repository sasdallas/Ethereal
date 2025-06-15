/**
 * @file hexahedron/loader/elf32_loader.c
 * @brief 32-bit ELF loader
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#if defined(__ARCH_I386__) || defined(__INTELLISENSE__)

#include <kernel/loader/elf_loader.h>
#include <kernel/loader/elf.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/ksym.h>
#include <kernel/debug.h>
#include <kernel/processor_data.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "ELFLDR", __VA_ARGS__)

/* Macros to assist in getting section header/section/phdr */
#define ELF_SHDR(ehdr) ((Elf32_Shdr*)((uintptr_t)ehdr + ehdr->e_shoff))
#define ELF_SECTION(ehdr, idx) ((Elf32_Shdr*)&ELF_SHDR(ehdr)[idx])
#define ELF_PHDR(ehdr, idx) ((Elf32_Phdr*)((uintptr_t)ehdr + ehdr->e_phoff + ehdr->e_phentsize * idx))

/**
 * @brief Check if an ELF file is supported
 * @param ehdr The EHDR to check
 * @returns 1 or 0 depending on validity
 */
int elf_checkSupported(Elf32_Ehdr *ehdr) {
    // Check the EI_MAG fields
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || 
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
            LOG(ERR, "elf_checkSupported(): Invalid ELF header");
            return 0;
    }

    // Check the EI_CLASS fields
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        LOG(ERR, "elf_checkSupported(): Unsupported ELF file class\n");
        return 0;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        LOG(ERR, "elf_checkSupported(): Unimplemented data order (ELFDATA2LSB expected)\n");
        return 0;
    }

    if (ehdr->e_machine != EM_386) {
        LOG(ERR, "elf_checkSupported(): Unimplemented machine type: %i\n", ehdr->e_machine);
        return 0;
    }

    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        LOG(ERR, "elf_checkSupported(): Bad ELF file version: %i\n", ehdr->e_ident[EI_VERSION]);
        return 0;
    }

    if (ehdr->e_type != ET_REL && ehdr->e_type != ET_EXEC) {
        LOG(ERR, "elf_checkSupported(): Unsupported ELF file type: %i\n", ehdr->e_type);
        return 0;
    }

    return 1;
}

/**
 * @brief Lookup the name of a section (DEBUG)
 * @param ehdr The EHDR of the file
 * @param idx The index of the section
 */
static char *elf_lookupSectionName(Elf32_Ehdr *ehdr, int idx) {
    // Get the string table
    if (ehdr->e_shstrndx == SHN_UNDEF) return NULL;
    char *strtab = (char*)ehdr + ELF_SECTION(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + idx;
}

/**
 * @brief Get the absolute address of a symbol
 * @param ehdr The EHDR of the ELF file
 * @param table The table of the symbol
 * @param idx The index of the symbol
 * @param flags The flags of loading the ELF file, by default assume ELF_USER
 * @returns Absolute address of a symbol, 0x0 (does not indicate failure), or ELF_FAIL if failed.
 * 
 * @note This will also bind symbols if they are required
 * 
 * @see https://wiki.osdev.org/ELF_Tutorial for reference
 */
uintptr_t elf_getSymbolAddress(Elf32_Ehdr *ehdr, int table, uintptr_t idx, int flags) {
    // First make sure parameters are correct
    if (table == SHN_UNDEF || idx == SHN_UNDEF || flags > ELF_DRIVER) return ELF_FAIL;

    // Get the symbol table and calculate its entries
    Elf32_Shdr *symtab = ELF_SECTION(ehdr, table);
    uintptr_t entry_count = symtab->sh_size / symtab->sh_entsize;

    if (idx >= entry_count) {
        LOG(ERR, "elf_getSymbolAddress(): Symbol index out of range (%d:%u)\n", table, idx);
        return ELF_FAIL;
    }

    // Now get the symbol
    uintptr_t symaddr = (uintptr_t)ehdr + symtab->sh_offset;
    Elf32_Sym *symbol = &((Elf32_Sym*)symaddr)[idx];

    // Let's check what we need to do with the symbol
    switch (symbol->st_shndx) {
        case SHN_UNDEF: ;
            // This means that we need to lookup the value (ext. symbol).
            Elf32_Shdr *strtab = ELF_SECTION(ehdr, symtab->sh_link);
            char *name = (char*)ehdr + strtab->sh_offset + symbol->st_name;

            if (flags != ELF_KERNEL && flags != ELF_DRIVER) {
                LOG(ERR, "elf_getSymbolAddress(): Unimplemented usermode lookup for symbol '%s'\n", name);
                return ELF_FAIL;
            }

            uintptr_t addr = ksym_resolve(name);
            if (addr == (uintptr_t)NULL) {
                // Not found. Is it a weak symbo?
                if (ELF32_ST_BIND(symbol->st_info) & STB_WEAK) {
                    LOG(DEBUG, "elf_getSymbolAddress(): Weak symbol '%s' not found - initialized as 0\n", name);
                    return 0;
                } else {
                    LOG(ERR, "elf_getSymbolAddress(): External symbol '%s' not found in kernel.\n", name);
                    return ELF_FAIL;
                }
            } else {
                return addr;
            }

        case SHN_ABS:
            // Absolute symbol
            return symbol->st_value;
        
        default: ;
            // Internally defined symbol
            Elf32_Shdr *target = ELF_SECTION(ehdr, symbol->st_shndx);
            return (uintptr_t)ehdr + symbol->st_value + target->sh_offset;
    }
}

/**
 * @brief Relocate a specific symbol
 * @param ehdr The EHDR of the file
 * @param rel The relocatable object
 * @param reltab The SHDR of the relocatable object
 * @param flags Flags passed to @c elf_load
 * @returns The symbol value or ELF_RELOC_ERROR
 * 
 * @see https://wiki.osdev.org/ELF_Tutorial
 */
static uintptr_t elf_relocateSymbol(Elf32_Ehdr *ehdr, Elf32_Rel *rel, Elf32_Shdr *reltab, int flags) {
    // Calculate the reference address
    Elf32_Shdr *target = ELF_SECTION(ehdr, reltab->sh_info);
    uintptr_t addr = (uintptr_t)ehdr + target->sh_offset;
    uintptr_t *ref = (uintptr_t*)(addr + rel->r_offset);

    // Get the symbol value
    uintptr_t symval = 0; // Default to 0
    if (ELF32_R_SYM(rel->r_info) != SHN_UNDEF) {
        symval = elf_getSymbolAddress(ehdr, reltab->sh_link, ELF32_R_SYM(rel->r_info), flags);
        if (symval == ELF_FAIL) return ELF_FAIL;
    }

    // Now we can start relocation based off type
    switch (ELF32_R_TYPE(rel->r_info)) {
        #ifdef __ARCH_I386__
        
        case R_386_NONE:
            // No relocation
            break;
        
        case R_386_32: 
            // Symbol + Offset
            *ref = RELOCATE_386_32(symval, *ref);
            break;
        
        case R_386_PC32:
            // Symbol + Offset - Section Offset
            *ref = RELOCATE_386_PC32(symval, *ref, (uintptr_t)ref);
            break;
            
        #else
        #error "Please add your architecture"
        #endif

        default:
            LOG(ERR, "Relocation unsupported: %d\n", ELF32_R_TYPE(rel->r_info));
            return ELF_FAIL;
    }
    
    return symval;
}

/**
 * @brief Relocate a specific symbol with addend
 * @param ehdr The EHDR of the file
 * @param rel The symbol to relocate
 * @param reltab The section header of the symbol
 * @param flags The flags passed to @c elf_load
 * @returns Symbol value or @c ELF_FAIL
 */
static uintptr_t elf_relocateSymbolAddend(Elf32_Ehdr *ehdr, Elf32_Rela *rel, Elf32_Shdr *reltab, int flags) {
    // Calculate offset
    Elf32_Shdr *target_section = ELF_SECTION(ehdr, reltab->sh_info);
    uintptr_t *reference = (uintptr_t*)rel->r_offset + target_section->sh_addr;

    // Resolve the symbol if needed
    uintptr_t symval = 0x0;
    if (ELF32_R_SYM(rel->r_info) != SHN_UNDEF) {
        // We need to get the symbol value for this
        symval = elf_getSymbolAddress(ehdr, reltab->sh_link, ELF32_R_SYM(rel->r_info), flags);
        if (symval == ELF_FAIL) return -1; // Didn't work..
    }

    // Now we can start relocation based off type
    switch (ELF32_R_TYPE(rel->r_info)) {
#ifdef __ARCH_I386__
        
        case R_386_NONE:
            // No relocation
            break;
        
        case R_386_32:
            // Symbol + Offset
            *reference = RELOCATE_386_32(symval, rel->r_addend);
            break;
        
        case R_386_PC32:
            // Symbol + Offset - Section Offset
            *reference = RELOCATE_386_PC32(symval, rel->r_addend, (uintptr_t)reference);
            break;
            
#else
        #error "Please add your architecture"
#endif

        default:
            LOG(ERR, "Relocation unsupported: %d\n", ELF32_R_TYPE(rel->r_info));
            return ELF_FAIL;
    }

    return symval;
}


/**
 * @brief Load a relocatable file
 * 
 * Performs relocation of all symbols and loads any needed sections.
 *
 * @param ehdr The EHDR of the relocatable file
 * @param flags The flags passed to @c elf_load
 * @returns 0 on success, anything else is failure
 */
int elf_loadRelocatable(Elf32_Ehdr *ehdr, int flags) {
    if (!ehdr || flags > ELF_DRIVER) return -1; // stupid users

    // Handle loading initial sections into memory

    Elf32_Shdr *shdr = ELF_SHDR(ehdr);

    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *section = &shdr[i];

        if ((section->sh_flags & SHF_ALLOC) && section->sh_size && section->sh_type == SHT_NOBITS) {
            // Allocate the section memory.
            void* addr = kmalloc(section->sh_size);
            
            // Clear the memory
            memset(addr, 0, section->sh_size);

            // Assign the address and offset
            section->sh_addr = (Elf32_Addr)addr;
            section->sh_offset = (uintptr_t)addr - (uintptr_t)ehdr;
            // LOG(DEBUG, "Allocated memory for section %i: %s (%ld)\n", i, elf_lookupSectionName(ehdr, section->sh_name), section->sh_size);
        } else {
            // Rebase sh_addr using offset
            section->sh_addr = (uintptr_t)ehdr + section->sh_offset;
        }
    }
    
    // Now start relocating
    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *section = &shdr[i];

        if (section->sh_type == SHT_REL) {
            // This is a relocatable section, handle that.
            for (unsigned int entry = 0; entry < section->sh_size / section->sh_entsize ; entry++) {
                Elf32_Rel *rel = &((Elf32_Rel*)((uintptr_t)ehdr + section->sh_offset))[entry];

                // Relocate the symbol
                uintptr_t result = elf_relocateSymbol(ehdr, rel, section, flags);
                if (result == ELF_FAIL) return -1;
            }
        } else if (section->sh_type == SHT_RELA) {
            // This is a relocatable section with addend, handle that.

            for (unsigned int entry = 0; entry < section->sh_size / section->sh_entsize; entry++) {
                Elf32_Rela *rela = &((Elf32_Rela*)((uintptr_t)ehdr + section->sh_offset))[entry];

                // Relocate the symbol
                uintptr_t result = elf_relocateSymbolAddend(ehdr, rela, section, flags);
                if (result == ELF_FAIL) return -1;
            }
        }
    }




    return 0;
}

/**
 * @brief Load an executable
 * @param ehdr The EHDR of the executable
 * @returns 0 on success
 */
int elf_loadExecutable(Elf32_Ehdr *ehdr) {
    if (!ehdr) return ELF_FAIL;

    // All we have to do is load PHDRs
    for (int i = 0; i < ehdr->e_phnum; i++) {
        // Get the PHDR
        Elf32_Phdr *phdr = ELF_PHDR(ehdr, i);

        switch (phdr->p_type) {
            case PT_NULL:
                // No work to be done - PT_NULL
                break;
            
            case PT_LOAD:
                // We have to load and map it into memory
                // !!!: Presume that if we're being called, the page directory in use is the one assigned to the executable
                LOG(DEBUG, "PHDR #%d - OFFSET 0x%x VADDR %p PADDR %p FILESIZE %d MEMSIZE %d\n", i, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);
                
                for (uintptr_t i = 0; i < phdr->p_memsz + phdr->p_filesz; i += PAGE_SIZE) {
                    page_t *pg = mem_getPage(NULL, i + phdr->p_vaddr, MEM_CREATE);
                    if (pg) {
                        mem_allocatePage(pg, MEM_DEFAULT);
                    }
                }

                // !!!: HACK
                if (current_cpu->current_process) {
                    vas_reserve(current_cpu->current_process->vas, phdr->p_vaddr, MEM_ALIGN_PAGE(phdr->p_memsz), VAS_ALLOC_EXECUTABLE);
                }

                memcpy((void*)phdr->p_vaddr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);

                // Zero remainder
                if (phdr->p_memsz > phdr->p_filesz) {
                    memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
                }
                break;
            default:
                LOG(ERR, "Failed to load PHDR #%d - unimplemented type 0x%x\n", i, phdr->p_type);
                return ELF_FAIL;
        }
    }


    return 0;
}


/**
 * @brief Find a specific symbol by name and get its value
 * @param ehdr_address The address of the EHDR (as elf64/elf32 could be in use)
 * @param name The name of the symbol
 * @returns A pointer to the symbol or NULL
 * 
 * @warning Make sure you've initialized the file!
 */
uintptr_t elf_findSymbol(uintptr_t ehdr_address, char *name) {
    if (!ehdr_address || !name) return (uintptr_t)NULL;
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)ehdr_address;

    // Start checking symbpols
    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *shdr = ELF_SECTION(ehdr, i);

        if (shdr->sh_type != SHT_SYMTAB) continue; // We only want symbol tables.

        // Get the string taable
        Elf32_Shdr *strtab = ELF_SECTION(ehdr, shdr->sh_link);
        if (!strtab) return (uintptr_t)NULL;

        // Get the symbol table now
        Elf32_Sym *symtable = (Elf32_Sym*)shdr->sh_addr;

        for (unsigned int sym = 0; sym < shdr->sh_size / shdr->sh_entsize; sym++) {
            // Get the name of the symbol
            Elf32_Sym *symbol = &symtable[sym];
            char *symname = (char*)ehdr + strtab->sh_offset + symbol->st_name;

            if (!strcmp(name, symname)) {
                return elf_getSymbolAddress(ehdr, i, sym, ELF_KERNEL);
            }
        }
    }

    return (uintptr_t)NULL;
}

/**
 * @brief Get the entrypoint of an executable file
 * @param ehdr_address The address of the EHDR (as elf64/elf32 could be in use)
 * @returns A pointer to the start or NULL
 */
uintptr_t elf_getEntrypoint(uintptr_t ehdr_address) {
    if (!ehdr_address) return (uintptr_t)NULL;
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)ehdr_address;

    if (ehdr->e_type != ET_EXEC) return 0x0;

    return ehdr->e_entry;
}


/**
 * @brief Load an ELF file fully
 * @param fbuf The buffer to load the file from
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_getEntrypoint or @c elf_findSymbol - or NULL if there was an error. 
 */
uintptr_t elf_loadBuffer(uint8_t *fbuf, int flags) {
    // Cast ehdr to fbuf - routines can now access sections by adding to ehdr
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)fbuf;

    // Check to make sure the ELF file is supported
    if (!elf_checkSupported(ehdr)) {
        return 0x0;
    }

    // Which are we loading?
    switch (ehdr->e_type) {
        case ET_REL:
            // Relocatable file
            if (elf_loadRelocatable(ehdr, flags)) {
                LOG(ERR, "Failed to load relocatable ELF file.\n");
                goto _error;
            }

            break;
        
        case ET_EXEC:
            // Executable file
            if (elf_loadExecutable(ehdr)) {
                LOG(ERR, "Failed to load executable ELF file.\n");
                goto _error;
            }

            break;
        
        default:
            // ???
            goto _error;
    }

    // Return a pointer to the EHDR
    return (uintptr_t)ehdr;

_error:
    return 0x0; 
}

/**
 * @brief Check a file to make sure it is a valid ELF file
 * @param file The file to check
 * @param type The type of file to look for
 * @returns 1 on valid ELF file, 0 on invalid
 */
int elf_check(fs_node_t *file, int type) {
    if (!file) return 0;

    // Read the EHDR field into a temporary holder
    Elf32_Ehdr ehdrtmp;
    if (fs_read(file, 0, sizeof(Elf32_Ehdr), (uint8_t*)&ehdrtmp) != sizeof(Elf32_Ehdr)) {
        LOG(ERR, "Failed to read ELF file\n");
        return 0;
    }

    // Check to make sure the ELF file is supported
    if (!elf_checkSupported(&ehdrtmp)) {
        return 0;
    }

    // Does it match the class?
    if (type == ELF_EXEC && ehdrtmp.e_type != ET_EXEC) return 0;
    if (type == ELF_RELOC && ehdrtmp.e_type != ET_REL) return 0;
    return 1;
}

/**
 * @brief Load an ELF file into memory
 * @param file The file to load into memory
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_getEntrypoint or @c elf_findSymbol - or NULL if there was an error. 
 */
uintptr_t elf_load(fs_node_t *node, int flags) {
    if (!node) return 0x0;

    // Check the file
    if (elf_check(node, ELF_ANY) == 0) {
        return 0x0;
    }

    // Now we can read the full file into a buffer 
    uint8_t *fbuf = kmalloc(node->length);
    memset(fbuf, 0, node->length);
    if (fs_read(node, 0, node->length, fbuf) != (ssize_t)node->length) {
        LOG(ERR, "Failed to read ELF file\n");
        return 0x0;
    }

    return elf_loadBuffer(fbuf, flags);
}

/**
 * @brief Cleanup an ELF file after it has finished executing
 * @param elf_address The address given by @c elf_load or another loading function
 * @returns 0 on success, anything else is a failure
 * 
 * @note REMEMBER TO FREE ELF BUFFER WHEN FINISHED!
 */
int elf_cleanup(uintptr_t elf_address) {
    if (elf_address == 0x0) return ELF_FAIL;

    // Get EHDR
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)elf_address; 
    if (!elf_checkSupported(ehdr)) return ELF_FAIL;

    // Check EHDR type
    if (ehdr->e_type == ET_REL) {
        // Cleanup by finding any sections allocated with SHF_ALLOC and destroy them
        Elf32_Shdr *shdr = ELF_SHDR(ehdr);
        for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
            Elf32_Shdr *section = &shdr[i];

            if ((section->sh_flags & SHF_ALLOC) && section->sh_size) {
                kfree((void*)section->sh_addr);
            }
        }
    } else if (ehdr->e_type == ET_EXEC) {
        for (int i = 0; i < ehdr->e_phnum; i++) {
            // Get the PHDR
            Elf32_Phdr *phdr = ELF_PHDR(ehdr, i);

            switch (phdr->p_type) {
                case PT_NULL:
                    // No work to be done - PT_NULL
                    break;
                
                case PT_LOAD:
                    // We have to unload and unmap it from memory
                    // !!!: Presume that if we're being called, the page directory in use is the one assigned to the executable
                    
                    for (uintptr_t i = 0; i < MEM_ALIGN_PAGE(phdr->p_memsz); i += PAGE_SIZE) {
                        page_t *pg = mem_getPage(NULL, i + phdr->p_vaddr, MEM_CREATE);
                        if (pg) mem_freePage(pg);
                    }

                    break;
                default:
                    LOG(ERR, "Failed to cleanup PHDR #%d - unimplemented type 0x%x\n", i, phdr->p_type);
                    break;
            }
        }
    }

    return 0;
}

/**
 * @brief Get the end of an ELF binary (for heap locations)
 * @param elf_address Address given by @c elf_load
 * @returns Pointer to heap placement location (aligned)
 */
uintptr_t elf_getHeapLocation(uintptr_t elf_address) {
    if (!elf_address) return 0x0;
    
    // Cast ehdr to fbuf - routines can now access sections by adding to ehdr
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)elf_address;

    // Check to make sure the ELF file is supported
    if (!elf_checkSupported(ehdr)) {
        return 0x0;
    }

    // Now we should handle based on the type
    if (ehdr->e_type == ET_REL) {
        // Relocatable
        LOG(ERR, "Heap locations for relocatable files are not implemented\n");
        return 0x0;
    } else if (ehdr->e_type == ET_EXEC) {
        // Executable
        uintptr_t heap_base = 0x0;
        
        // Iterate through PHDRs
        for (uintptr_t i = 0; i < ehdr->e_phnum; i++) {
            Elf32_Phdr *phdr = ELF_PHDR(ehdr, i);

            if (phdr->p_vaddr + phdr->p_memsz > heap_base) heap_base = phdr->p_vaddr + phdr->p_memsz;
        }

        // Align
        heap_base = MEM_ALIGN_PAGE(heap_base);
        return heap_base;
    } else {
        // ???
        LOG(ERR, "Unknown ELF file type: %d\n", ehdr->e_type);
        return 0x0;
    }

    return 0x0;
}

#endif
