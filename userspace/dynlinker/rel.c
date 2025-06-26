/**
 * @file userspace/dynlinker/rel.c
 * @brief Dynamic relocation system
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

hashmap_t *__linker_symbol_table = NULL;


#define ELF_SHDR(ehdr) ((Elf64_Shdr*)((uintptr_t)ehdr + ehdr->e_shoff))
#define ELF_SECTION(ehdr, idx) ((Elf64_Shdr*)&ELF_SHDR(ehdr)[idx])

/**
 * @brief Perform DT_HASH
 * @param name The name
 */
uint32_t elf_hash(char *n) {
    const uint8_t* name = (const uint8_t*)n;
    uint32_t h = 0, g;
    for (; *name; name++) {
        h = (h << 4) + *name;
        if ((g = h & 0xf0000000)) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

/**
 * @brief Try to find a symbol in a dynamic library
 * @param lib The dynamic library to search
 * @param name The symbol to look up
 * @returns ELF symbol or NULL if it could not be funod
 */
Elf64_Sym *elf_lookupFromLibrary(elf_obj_t *obj, char *name) {
    // First, make sure the ELF has a hash
    if (!obj->dyntab.hash) {
        // Huh, we could probably work around this
        fprintf(stderr, "ld.so: Library \"%s\" is missing HASH table\n", obj->filename);
        return NULL;
    }

    // Hash name
    uint32_t hash = elf_hash(name);

    // Get variables
    uint32_t *ht = (uint32_t*)obj->dyntab.hash;
    uint32_t nbucket = ht[0];
    uint32_t *bucket = &ht[2];
    uint32_t *chain = &bucket[nbucket];

    // Get string table and symbol table
    char *strtab = (char*)(obj->dyntab.strtab);
    Elf64_Sym *symtab = (Elf64_Sym*)(obj->dyntab.symtab);

    for (uint32_t i = bucket[hash % nbucket]; i; i = chain[i]) {
        if (!strcmp(name, strtab + symtab[i].st_name)) {
            return &symtab[i];
        }
    }

    return NULL;
}

/**
 * @brief Try to find a symbol in an object
 * @param obj The ELF object to look for
 * @param sym The symbol to search for
 * @param rel The relocation
 * @param name Name of the symbol
 * @returns Symbol address or (uintptr_t)-1
 */
uintptr_t elf_lookup(elf_obj_t *obj, Elf64_Sym *sym, Elf64_Rela *rel, char *name) {
    if (name) {
        // First see if we have it in __linker_symbols
        for (unsigned int i = 0; i < LINKER_SYMBOL_COUNT; i++) {
            if (!strcmp(name, __linker_symbols[i].name)) {
                // Yup, we do
                uintptr_t symbol_value = (uintptr_t)__linker_symbols[i].addr;
                LD_DEBUG("[%s] Linker symbol \"%s\" resolved to %p\n", obj->filename, name, symbol_value);
                return symbol_value;
            }
        }
    }

    // Check what we need to do with the symbol
    switch (sym->st_shndx) {
        case SHN_UNDEF:
            LD_DEBUG("[%s] SHN_UNDEF %s\n", obj->filename, name);
            Elf64_Sym *found_sym = NULL;
            uintptr_t symbol_value = 0x0;

            // Check each library
            foreach(lib, __linker_libraries) {
                found_sym = elf_lookupFromLibrary((elf_obj_t*)lib->value, name);
                if (found_sym) {
                    symbol_value = (uintptr_t)((elf_obj_t*)lib->value)->base + found_sym->st_value;
                    break;
                }
            }

            if (!found_sym) {
                LD_DEBUG("[%s] WARNING: Symbol \"%s\" was not found\n", obj->filename, name);
                return (uintptr_t)-1;
            }

            LD_DEBUG("[%s] Symbol \"%s\" located at %p\n", obj->filename, name, symbol_value);
            return symbol_value;

        case SHN_ABS:
            return sym->st_value;

        default:
            return (uintptr_t)obj->base + sym->st_value;
    }
}

/**
 * @brief Determine whether a symbol address is needed
 * @param type Type of relocation
 * This is needed because ELF64_R_SYM(rel->r_info) != SHN_UNDEF isnt available
 */
static int elf_needSymbol(unsigned int type) {
    switch (type) {
        case R_X86_64_64:
		case R_X86_64_PC32:
		case R_X86_64_COPY:
		case R_X86_64_GLOB_DAT:
		case R_X86_64_JUMP_SLOT:
			return 1;
        default:
            return 0;
    } 
}

/**
 * @brief Relocate a specific symbol with addend
 * @param obj The ELF object
 * @param ehdr The EHDR of the file
 * @param rel The symbol to relocate
 * @param reltab The section header of the symbol
 * @returns 0 on success
 */
static int elf_relocateSymbolAddend(elf_obj_t *obj, Elf64_Ehdr *ehdr, Elf64_Rela *rel, Elf64_Shdr *reltab) {
    // Get symbol and name from dynamic symbol table
    uint32_t symidx = ELF64_R_SYM(rel->r_info);
    Elf64_Sym *sym = &(((Elf64_Sym*)(obj->dyntab.symtab))[symidx]);
    char *name = (char *)(obj->dyntab.strtab + sym->st_name);

    // Need to set symbol value?
    uintptr_t symbol_value = 0;
    if (elf_needSymbol(ELF64_R_TYPE(rel->r_info))) {
        symbol_value = elf_lookup(obj, sym, rel, name);
        // if (name) LD_DEBUG("[%s %016llX] Resolved symbol %s to %p\n", obj->filename, obj->base, name, symbol_value);
        if (symbol_value == (uintptr_t)-1) return 0;
    } else {
        // LD_DEBUG("[%s] Symbol is alrefady present: %s\n", obj->filename, name);
        symbol_value = sym->st_value;
    }

    // Perform relocations
    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_64:
            // S + A
            // LD_DEBUG("[%s] R_X86_64_64 %016llX\t%s + %d\n", obj->filename, symbol_value, name, rel->r_addend);
            *((uintptr_t*)(rel->r_offset + obj->base)) = symbol_value + rel->r_addend;
            break;

        case R_X86_64_COPY:
            // Recalculate the symbol value from other libraries
            symbol_value = 0x0;
            foreach(lib, __linker_libraries) {
                if (lib->value != obj) {
                    Elf64_Sym *sym = elf_lookupFromLibrary((elf_obj_t*)lib->value, name);
                    if (sym) {
                        symbol_value = (uintptr_t)sym->st_value + (uintptr_t)((elf_obj_t*)lib->value)->base;
                        break;
                    }
                }
            }

            if (!symbol_value) {
                fprintf(stderr, "[%s] R_X86_64_COPY: No copy found for symbol %s!\n", obj->filename, name);
                return 0;
            }

            LD_DEBUG("[%s] R_X86_64_COPY %s %p %d\n", obj->filename, name, symbol_value, sym->st_size);
            memcpy((void*)(rel->r_offset + obj->base), (void*)symbol_value, sym->st_size);
            break;

        case R_X86_64_GLOB_DAT:
            // TODO
            LD_DEBUG("[%s] ERROR: R_X86_64_GLOB_DAT not implemented yet!\n", obj->filename);
            return 1;
        
        case R_X86_64_JUMP_SLOT:
            // S
            LD_DEBUG("[%s] R_X86_64_JUMP_SLOT %016llX\t%s + %d\n", obj->filename, symbol_value, name, rel->r_addend);
            *((uintptr_t*)(rel->r_offset + obj->base)) = symbol_value;
            break;
        
        case R_X86_64_RELATIVE:
            // B + A
            // LD_DEBUG("[%s] R_X86_64_RELATIVE %016llX + %d\n", obj->filename, obj->base, rel->r_addend);
            *((uintptr_t*)(rel->r_offset + obj->base)) = (uintptr_t)(obj->base + rel->r_addend);
            break;

        default:
            fprintf(stderr, "[%s] Unknown relocation: %d\n", obj->filename, ELF64_R_TYPE(rel->r_info));
            break;
    }

    return 0;
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
 * @brief Apply ELF relocations
 * @param obj The object to relocate
 * @returns 0 on success
 */
int elf_relocate(elf_obj_t *obj) {
    // Handle processing the dynamic symbol table for our special
    // if (obj->dyntab.symtab) {
    //     Elf64_Sym *sym = obj->dyntab.symtab;

    //     for (unsigned i = 0; i < obj->dyntab.symtab_sz; i++) {
    //         char *symname = (char*)(obj->dyntab.strtab + sym->st_name);
    //         LD_DEBUG("Dynamic symbol %s\n", symname); 
            
    //         sym++; // Next symbol
    //     }
    // }

    // Go through section headers
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)obj->buffer;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *section = ELF_SECTION(ehdr, i);

        // LD_DEBUG("[%s] Load section %s\n", obj->filename, elf_lookupSectionName(ehdr, section->sh_name));

        if (section->sh_type == SHT_REL) {
            return 1;
        } else if (section->sh_type == SHT_RELA) {
            for (unsigned int idx = 0; idx < section->sh_size / section->sh_entsize; idx++) {
                Elf64_Rela *rela = &((Elf64_Rela*)((uintptr_t)ehdr + section->sh_offset))[idx];

                // Relocate the symbol
                elf_relocateSymbolAddend(obj, ehdr, rela, section);
            }
        } else if (section->sh_flags & SHF_ALLOC && section->sh_type == SHT_NOBITS && section->sh_size) {
            void *addr = malloc(section->sh_size);
            memset(addr, 0, section->sh_size);
            section->sh_addr = (Elf64_Addr)addr;
            section->sh_offset = (uintptr_t)addr - (uintptr_t)ehdr;
        }
    }

    return 0;
}
