/**
 * @file hexahedron/loader/elf64_loader.c
 * @brief 64-bit ELF loader
 * 
 * @todo This needs a better implementation of getting and parsing symbols (relocation-wise)
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#if defined(__ARCH_X86_64__) || defined(__INTELLISENSE__) || defined(__ARCH_AARCH64__)

#include <kernel/loader/elf_loader.h>
#include <kernel/loader/elf.h>
#include <kernel/misc/ksym.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <kernel/processor_data.h>

#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "ELFLDR", __VA_ARGS__)

/* Macros to assist in getting section header/section */
#define ELF_SHDR(ehdr) ((Elf64_Shdr*)((uintptr_t)ehdr + ehdr->e_shoff))
#define ELF_SECTION(ehdr, idx) ((Elf64_Shdr*)&ELF_SHDR(ehdr)[idx])
#define ELF_PHDR(ehdr, idx) ((Elf64_Phdr*)((uintptr_t)ehdr + ehdr->e_phoff + ehdr->e_phentsize * idx))

/**
 * @brief Check if an ELF file is supported
 * @param ehdr The EHDR to check
 * @returns 1 or 0 depending on validity
 */
static int elf_checkSupported(Elf64_Ehdr *ehdr) {
    // Check the EI_MAG fields
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || 
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
            LOG(ERR, "elf_checkSupported(): Invalid ELF header");
            return 0;
    }

    // Check the EI_CLASS fields
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        LOG(ERR, "elf_checkSupported(): Unsupported ELF file class\n");
        return 0;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        LOG(ERR, "elf_checkSupported(): Unimplemented data order (ELFDATA2LSB expected)\n");
        return 0;
    }

    if (ehdr->e_machine != EM_X86_64) {
        LOG(ERR, "elf_checkSupported(): Unimplemented machine type: %i\n", ehdr->e_machine);
        return 0;
    }

    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        LOG(ERR, "elf_checkSupported(): Bad ELF file version: %i\n", ehdr->e_ident[EI_VERSION]);
        return 0;
    }

    if (ehdr->e_type != ET_REL && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        LOG(ERR, "elf_checkSupported(): Unsupported ELF file type: %i\n", ehdr->e_type);
        return 0;
    }

    return 1;
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
static uintptr_t elf_getSymbolAddress(Elf64_Ehdr *ehdr, int table, uintptr_t idx, int flags) {
    // First make sure parameters are correct
    if (table == SHN_UNDEF || idx == SHN_UNDEF || flags > ELF_DRIVER) return ELF_FAIL;

    // Get the symbol table and calculate its entries
    Elf64_Shdr *symtab = ELF_SECTION(ehdr, table);
    uintptr_t entry_count = symtab->sh_size / symtab->sh_entsize;
    
    if (idx >= entry_count) {
        LOG(ERR, "elf_getSymbolAddress(): Symbol index out of range (%d:%u)\n", table, idx);
        return ELF_FAIL;
    }

    // Now get the symbol
    uintptr_t symaddr = (uintptr_t)ehdr + symtab->sh_offset;
    Elf64_Sym *symbol = &((Elf64_Sym*)symaddr)[idx];

    // Let's check what we need to do with the symbol
    switch (symbol->st_shndx) {
        case SHN_UNDEF:
            // This means that we need to lookup the value (ext. symbol).
            Elf64_Shdr *strtab = ELF_SECTION(ehdr, symtab->sh_link);
            char *name = (char*)ehdr + strtab->sh_offset + symbol->st_name;

            if (flags != ELF_KERNEL && flags != ELF_DRIVER) {
                LOG(ERR, "elf_getSymbolAddress(): Unimplemented usermode lookup for symbol '%s'\n", name);
                return ELF_FAIL;
            }

            uintptr_t addr = ksym_resolve(name);
            if (addr == (uintptr_t)NULL) {
                // Not found. Is it a weak symbo?
                if (ELF64_ST_BIND(symbol->st_info) & STB_WEAK) {
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
        
        default:
            // Internally defined symbol
            Elf64_Shdr *target = ELF_SECTION(ehdr, symbol->st_shndx);
            return (uintptr_t)ehdr + symbol->st_value + target->sh_offset;
    }
}

/**
 * @brief Lookup the name of a section (DEBUG)
 * @param ehdr The EHDR of the file
 * @param idx The index of the section
 */
static char *elf_lookupSectionName(Elf64_Ehdr *ehdr, int idx) {
    // Get the string table
    if (ehdr->e_shstrndx == SHN_UNDEF) return NULL;
    char *strtab = (char*)ehdr + ELF_SECTION(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + idx;
}

/**
 * @brief Relocate a specific symbol
 * @param ehdr The EHDR of the file
 * @param rel The symbol to relocate
 * @param reltab The section header of the symbol
 * @param flags The flags passed to @c elf_load
 * @returns Symbol value or @c ELF_FAIL
 * 
 * @see https://wiki.osdev.org/ELF_Tutorial
 */
static uintptr_t elf_relocateSymbol(Elf64_Ehdr *ehdr, Elf64_Rel *rel, Elf64_Shdr *reltab, int flags) {
    // Get the target reference from the relocation offset
    Elf64_Shdr *shdr = ELF_SECTION(ehdr, reltab->sh_info); // TODO: Check SHF_INFO_LINK?
    uintptr_t addr = (uintptr_t)ehdr + shdr->sh_offset;
    uintptr_t *reference = (uintptr_t*)(addr + rel->r_offset); 

    // Resolve the symbol if needed
    uintptr_t symval = 0x0;
    if (ELF64_R_SYM(rel->r_info) != SHN_UNDEF) {
        // We need to get the symbol value for this
        symval = elf_getSymbolAddress(ehdr, reltab->sh_link, ELF64_R_SYM(rel->r_info), flags);
        if (symval == ELF_FAIL) return -1; // Didn't work..
    }

    // Relocate based on the type
    switch (ELF64_R_TYPE(rel->r_info)) {    
    #ifdef __ARCH_X86_64__
        case R_X86_64_NONE:
            // No relocation
            break;

        case R_X86_64_64:
            // Symbol + Offset
            *((uint64_t*)reference) = RELOCATE_X86_64_3264(symval, *reference);
            break;
        
        case R_X86_64_32:
            // Symbol + Offset
            *((uint32_t*)reference) = RELOCATE_X86_64_3264(symval, *reference);
            break;

        case R_X86_64_PLT32:
            LOG(ERR, "Cannot parse PLT32! Link with -nostdlib and compile with -fno-pie!\n"); // Spent about an entire day debugging this. I hate PLT32.
            return ELF_FAIL;

        case R_X86_64_PC32:
            // Symbol + Offset - Section Offset
            *((uint32_t*)reference) = RELOCATE_X86_64_PC32(symval, *reference, (uintptr_t)reference);
            break;
    #elif defined(__ARCH_AARCH64__)
        /* TODO */
    #else
        #error "Please define your ELF relocation types"
    #endif

        default:
            LOG(ERR, "Unsupported relocaton type: %i\n", rel->r_info);
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
static uintptr_t elf_relocateSymbolAddend(Elf64_Ehdr *ehdr, Elf64_Rela *rel, Elf64_Shdr *reltab, int flags) {
    // Calculate offset
    Elf64_Shdr *target_section = ELF_SECTION(ehdr, reltab->sh_info);
    uintptr_t addr = (uintptr_t)ehdr + target_section->sh_offset;
    uint64_t *reference = (uint64_t*)(addr + rel->r_offset); 

    
    // Resolve the symbol if needed
    uintptr_t symval = 0x0;
    if (ELF64_R_SYM(rel->r_info) != SHN_UNDEF) {
        // We need to get the symbol value for this
        symval = elf_getSymbolAddress(ehdr, reltab->sh_link, ELF64_R_SYM(rel->r_info), flags);
        if (symval == ELF_FAIL) return -1; // Didn't work..
    }

    // Relocate based on the type
    switch (ELF64_R_TYPE(rel->r_info)) {    
#ifdef __ARCH_X86_64__
        case R_X86_64_NONE:
            // No relocation
            break;

        case R_X86_64_64:
            // Symbol + Offset
            uint64_t r64 = RELOCATE_X86_64_3264(symval, (int)rel->r_addend);
            memcpy(reference, &r64, sizeof(uint64_t));
            break;
        
        case R_X86_64_32:
            // Symbol + Offset
            uint32_t r32 = (uint32_t)RELOCATE_X86_64_3264(symval, (int)rel->r_addend);
            memcpy(reference, &r32, sizeof(uint32_t));
            break;

        case R_X86_64_PLT32:
            LOG(ERR, "Cannot parse PLT32! Link with -nostdlib and compile with -fno-pie!\n"); // Spent about an entire day debugging this. I hate PLT32.
            return ELF_FAIL;

        case R_X86_64_PC32:
            // Symbol + Offset - Section Offset
            uint32_t pc32 = RELOCATE_X86_64_PC32(symval, (int)rel->r_addend, (uintptr_t)reference);
            memcpy(reference, &pc32, sizeof(uint32_t));
            break;

#elif defined(__ARCH_AARCH64__)
        /* TODO */
#else
        #error "Please define your ELF relocation types"
#endif

        default:
            LOG(ERR, "Unsupported relocaton type: %i\n", rel->r_info);
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
int elf_loadRelocatable(Elf64_Ehdr *ehdr, int flags) {
    if (!ehdr || flags > ELF_DRIVER) return -1; // stupid users


    Elf64_Shdr *shdr = ELF_SHDR(ehdr);
    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *section = &shdr[i];

        if ((section->sh_flags & SHF_ALLOC) && section->sh_size && section->sh_type == SHT_NOBITS) {
            // Allocate the section memory
            void *addr = vmm_map(NULL, section->sh_size, VM_FLAG_ALLOC, MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
            
            // Clear the memory
            memset(addr, 0, section->sh_size);
            
            // Assign the address and offset
            section->sh_addr = (Elf64_Addr)addr;
            section->sh_offset = (uintptr_t)addr - (uintptr_t)ehdr;
            // LOG(DEBUG, "Allocated memory for section %i: %s (%ld)\n", i, elf_lookupSectionName(ehdr, section->sh_name), section->sh_size);
        } else {
            // Rebase sh_addr using offset
            section->sh_addr = (uintptr_t)ehdr + section->sh_offset;
        }
    }

    // Relocate & parse entries
    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *section = &shdr[i];

        if (section->sh_type == SHT_REL) {
            // We need to do relocation, process entries.
            for (unsigned int idx = 0; idx < section->sh_size / section->sh_entsize; idx++) {
                // Get the relocation table
                Elf64_Rel *rel = &((Elf64_Rel*)((uintptr_t)ehdr + section->sh_offset))[idx];

                // Relocate the symbol
                uintptr_t result = elf_relocateSymbol(ehdr, rel, section, flags);
                if (result == ELF_FAIL) return -1;
            }
        } else if (section->sh_type == SHT_RELA) {
            // We need to do relocation, process entries
            for (unsigned int idx = 0; idx < section->sh_size / section->sh_entsize; idx++) {
                Elf64_Rela *rela = &((Elf64_Rela*)((uintptr_t)ehdr + section->sh_offset))[idx];

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
int elf_loadExecutable(Elf64_Ehdr *ehdr) {
    if (!ehdr) return ELF_FAIL;

    // All we have to do is load PHDRs
    for (int i = 0; i < ehdr->e_phnum; i++) {
        // Get the PHDR
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        switch (phdr->p_type) {
            case PT_NULL:
                // No work to be done - PT_NULL
                break;
            
            case PT_LOAD:
                // We have to load and map it into memory
                // !!!: Presume that if we're being called, the page directory in use is the one assigned to the executable
                LOG(DEBUG, "PHDR #%d PT_LOAD: OFFSET 0x%x VADDR %p PADDR %p FILESIZE %d MEMSIZE %d\n", i, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);
                
                assert(vmm_map((void*)phdr->p_vaddr, phdr->p_memsz, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_USER) == (void*)PAGE_ALIGN_DOWN(phdr->p_vaddr));

                memcpy((void*)phdr->p_vaddr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);

                // Zero remainder
                if (phdr->p_memsz > phdr->p_filesz) {
                    memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
                }

                break;

            case PT_TLS:
                // Basically the same as a PT_LOAD, but don't zero?
                LOG(DEBUG, "PHDR #%d PT_TLS: OFFSET 0x%x VADDR %p PADDR %p FILESIZE %d MEMSIZE %d\n", i, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);
                // assert(vmm_map((void*)phdr->p_vaddr, phdr->p_memsz, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_USER) == (void*)PAGE_ALIGN_DOWN(phdr->p_vaddr));
    
                memcpy((void*)phdr->p_vaddr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);

                break;

            case PT_NOTE:
                // Ignore, this doesn't matter.
                LOG(DEBUG, "PHDR %d PT_NOTE: Ignored\n", i);
                break;
                
            case PT_GNU_PROPERTY:
                LOG(DEBUG, "PHDR %d PT_GNU_PROPERTY: Ignored\n", i);
                break;
            
            default:
                LOG(WARN, "Failed to load PHDR #%d - unimplemented type 0x%x\n", i, phdr->p_type);
                break; // Keep going
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
    

    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)ehdr_address;

    // Start checking symbpols
    for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = ELF_SECTION(ehdr, i);

        if (shdr->sh_type != SHT_SYMTAB) continue; // We only want symbol tables.

        // Get the string taable
        Elf64_Shdr *strtab = ELF_SECTION(ehdr, shdr->sh_link);
        if (!strtab) {
            LOG(ERR, "String table not found\n");
            return (uintptr_t)NULL;
        }

        // Get the symbol table now
        Elf64_Sym *symtable = (Elf64_Sym*)shdr->sh_addr;

        for (unsigned int sym = 0; sym < shdr->sh_size / shdr->sh_entsize; sym++) {
            // Get the name of the symbol
            Elf64_Sym *symbol = &symtable[sym];
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
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)ehdr_address;

    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) return 0x0;

    return ehdr->e_entry;
}


/**
 * @brief Load an ELF file fully
 * @param fbuf The buffer to load the file from
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_startExecution or @c elf_findSymbol - or NULL if there was an error. 
 */
uintptr_t elf_loadBuffer(uint8_t *fbuf, int flags) {
    // Cast ehdr to fbuf - routines can now access sections by adding to ehdr
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)fbuf;

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
        case ET_DYN:
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
    Elf64_Ehdr ehdrtmp;
    if (fs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdrtmp) != sizeof(Elf64_Ehdr)) {
        LOG(ERR, "Failed to read ELF file\n");
        return 0;
    }

    // Check to make sure the ELF file is supported
    if (!elf_checkSupported(&ehdrtmp)) {
        return 0;
    }

    // Does it match the class?
    if (type == ELF_EXEC && ehdrtmp.e_type != ET_EXEC && ehdrtmp.e_type != ET_DYN) return 0;
    if (type == ELF_RELOC && ehdrtmp.e_type != ET_REL) return 0;

    if (type == ELF_DYNAMIC) {
        // We have extra work to do, checking for PT_DYNAMIC PHDRs
        if (ehdrtmp.e_type != ET_EXEC) return 0;

        // !!!: Load PHDR sections into memory
        for (int i = 0; i < ehdrtmp.e_phnum; i++) {
            Elf64_Phdr phdr;
            if (fs_read(file, ehdrtmp.e_phoff + (ehdrtmp.e_phentsize * i), sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
                LOG(ERR, "Error reading PHDR %d into memory\n", ehdrtmp.e_phnum);
                return 0;
            }

            if (phdr.p_type == PT_DYNAMIC) return 1;
        }

        return 0;
    }

    return 1;
}

/**
 * @brief Load an ELF file into memory
 * @param file The file to load into memory
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_startExecution or @c elf_findSymbol - or NULL if there was an error. 
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
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf_address; 
    if (!elf_checkSupported(ehdr)) return ELF_FAIL;

    // Check EHDR type
    if (ehdr->e_type == ET_REL) {
        // Cleanup by finding any sections allocated with SHF_ALLOC and destroy them
        Elf64_Shdr *shdr = ELF_SHDR(ehdr);
        for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
            Elf64_Shdr *section = &shdr[i];

            if ((section->sh_flags & SHF_ALLOC) && section->sh_size  && section->sh_type == SHT_NOBITS) {
                kfree((void*)section->sh_addr);
            }
        }
    } else if (ehdr->e_type == ET_EXEC) {
        for (int i = 0; i < ehdr->e_phnum; i++) {
            // Get the PHDR
            Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

            switch (phdr->p_type) {
                case PT_NULL:
                    // No work to be done - PT_NULL
                    break;
                
                case PT_LOAD:
                    // We have to unload and unmap it from memory
                    // !!!: Presume that if we're being called, the page directory in use is the one assigned to the executable
                    for (uintptr_t i = 0; i < PAGE_ALIGN_UP(phdr->p_memsz); i += PAGE_SIZE) {
                        uintptr_t blk = arch_mmu_physical(NULL, i + phdr->p_vaddr);
                        arch_mmu_unmap(NULL, i + phdr->p_vaddr);
                        if (blk) pmm_freePage(blk);
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
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf_address;

    // Check to make sure the ELF file is supported
    if (!elf_checkSupported(ehdr)) {
        return 0x0;
    }

    // Now we should handle based on the type
    if (ehdr->e_type == ET_REL) {
        // Relocatable
        LOG(ERR, "Heap locations for relocatable files are not implemented\n");
        return 0x0;
    } else if (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) {
        // Executable
        uintptr_t heap_base = 0x0;
        
        // Iterate through PHDRs
        for (uintptr_t i = 0; i < ehdr->e_phnum; i++) {
            Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);
            if (phdr->p_vaddr + phdr->p_memsz > heap_base) heap_base = phdr->p_vaddr + phdr->p_memsz;
        }

        // Align
        heap_base = PAGE_ALIGN_UP(heap_base);
        return heap_base;
    } else {
        // ???
        LOG(ERR, "Unknown ELF file type: %d\n", ehdr->e_type);
        return 0x0;
    }

    return 0x0;
}

/**
 * @brief Populate a process image object
 * @param elf_address Address given by @c elf_load
 */
void elf_createImage(uintptr_t elf_address) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf_address;

    // Setup entrypoint
    current_cpu->current_process->image.entry = elf_getEntrypoint(elf_address);

    // Find TLS if it exists
    current_cpu->current_process->image.tls = 0x0;
    current_cpu->current_process->image.tls_size = 0;

    for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        if (phdr->p_type == PT_TLS) {
            current_cpu->current_process->image.tls = phdr->p_vaddr;
            current_cpu->current_process->image.tls_size = phdr->p_memsz;
        }
    }
}

/**
 * @brief Detect and find the interpreter for a dynamically linked file
 * @param file The file to get the interpreter for
 * @returns Interpreter string on success or NULL on failure
 * @note Free this yourself
 */
char *elf_getInterpreter(fs_node_t *file) {
    // Read the EHDR field into a temporary holder
    Elf64_Ehdr ehdrtmp;
    if (fs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdrtmp) != sizeof(Elf64_Ehdr)) {
        LOG(ERR, "Failed to read ELF file\n");
        return NULL;
    }


    for (int i = 0; i < ehdrtmp.e_phnum; i++) {
        Elf64_Phdr phdr;
        if (fs_read(file, ehdrtmp.e_phoff + (ehdrtmp.e_phentsize * i), sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            LOG(ERR, "Error reading PHDR %d into memory\n", ehdrtmp.e_phnum);
            return NULL;
        }

        if (phdr.p_type == PT_INTERP) {
            // Found the PT_DYNAMIC, read it in
            char *buffer = kmalloc(phdr.p_filesz); // rip stack

            if (fs_read(file, phdr.p_offset, phdr.p_filesz, (uint8_t*)buffer) != (ssize_t)phdr.p_filesz) {
                LOG(ERR, "Error reading PT_DYNAMIC section into memory\n");
                kfree(buffer);
                return NULL;
            }

            return buffer;
        }
    }

    LOG(ERR, "ELF file is a dynamically loaded object file but does not contain a PT_INTERP\n");
    return NULL; // No PT_DYNAMIC section found
} 


/**
 * @brief Load dynamic ELF file
 * @param file The file to load dynamically
 * @param info Output @c elf_dynamic_info_t
 * @returns 0 on success
 */
int elf_loadDynamicELF(fs_node_t *file, elf_dynamic_info_t *info) {
    // TODO: We could merge this with elf_load.. improve flags?

    uint8_t *buf = kmalloc(file->length);
    if (fs_read(file, 0, file->length, buf) != (ssize_t)file->length) {
        LOG(ERR, "Error reading ELF file\n");
        return 1;
    }

    // Determine PHDR bounds first
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)buf;
    uintptr_t lowest = UINTPTR_MAX;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        // PT_LOAD is the only one that matters
        if (phdr->p_type == PT_LOAD && phdr->p_vaddr < lowest) lowest = phdr->p_vaddr;
    }

    // Begin actual PHDR loading
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        switch (phdr->p_type) {
            case PT_LOAD:
                assert(vmm_map((void*)phdr->p_vaddr, phdr->p_memsz, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_USER) == (void*)PAGE_ALIGN_DOWN(phdr->p_vaddr));

                memcpy((void*)phdr->p_vaddr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);

                // Zero remainder
                if (phdr->p_memsz > phdr->p_filesz) {
                    memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
                }

                break;

            case PT_PHDR:
                // Important that we save this
                info->at_phdr = phdr->p_vaddr;
                break;
            
            default:
                LOG(WARN, "Unrecognized PHDR type: 0x%x\n", phdr->p_type);
                break;
        }
    }

    // Setup remaining info
    info->at_entry = ehdr->e_entry;
    info->at_phent = ehdr->e_phentsize;
    info->at_phnum = ehdr->e_phnum;

    kfree(buf);
    return 0;
}


#endif