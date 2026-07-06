/**
 * @file hexahedron/loader/elf64_loaderv2.c
 * @brief ELF64 loader v2
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/loader/elfv2.h>
#include <kernel/loader/elf.h>
#include <kernel/mm/vmm.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>
#include <string.h>
#include <assert.h>


/* Macros to assist in getting section header/section */
#define ELF_SHDR(ehdr) ((Elf64_Shdr*)((uintptr_t)ehdr + ehdr->e_shoff))
#define ELF_SECTION(ehdr, idx) ((Elf64_Shdr*)&ELF_SHDR(ehdr)[idx])
#define ELF_PHDR(ehdr, idx) ((Elf64_Phdr*)((uintptr_t)ehdr + ehdr->e_phoff + ehdr->e_phentsize * idx))

/* Log method */
#define LOG(status, ...) dprintf_module(status, "LOADER:ELF2", __VA_ARGS__)

#ifdef ELF_DEBUG
#define DEBUG(...) LOG(DEBUG, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

/**
 * @brief Open an ELF image
 * @param file The image to open
 * @param image The image output
 * @returns Status code
 */
int elf_openImage(vfs_file_t *file, elf_image_t *img) {
    img->file = file;
    
    // Read the file into memory
    img->file_size = inode_size(file->inode);
    img->file_base = kmalloc(img->file_size);
    
    ssize_t r = vfs_read(file, 0, img->file_size, (char*)img->file_base);
    if (r < (ssize_t)img->file_size) {
        LOG(WARN, "Failed to read in ELF image (error code %d)\n", r);
        return r;
    }

    // Check this is a valid ELF image
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)img->file_base;
    
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || 
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
            LOG(ERR, "Invalid ELF header magic");
            return -ENOEXEC;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        LOG(ERR, "Unsupported ELF file class\n");
        return -ENOEXEC;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        LOG(ERR, "Unimplemented data order (ELFDATA2LSB expected)\n");
        return -ENOEXEC;
    }

    if (ehdr->e_machine != EM_X86_64) {
        LOG(ERR, "Unimplemented machine type: %i\n", ehdr->e_machine);
        return -ENOEXEC;
    }

    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        LOG(ERR, "Bad ELF file version: %i\n", ehdr->e_ident[EI_VERSION]);
        return -ENOEXEC;
    }

    if (ehdr->e_type != ET_REL && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        LOG(ERR, "Unsupported ELF file type: %i\n", ehdr->e_type);
        return -ENOEXEC;
    }

    // Setup some other fields of the image
    img->entrypoint = 0x0;
    img->img_base = 0x0;
    img->load_bias = 0x0;
    img->interp_path = NULL;

    // If this is an EXEC/DYN image look for a interpreter path
    if (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) {
        for (unsigned i = 0; i < ehdr->e_phnum; i++) {
            Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);
            if (phdr->p_type == PT_INTERP) {
                // TODO: string checks
                img->interp_path = kmalloc(phdr->p_filesz);
                memcpy(img->interp_path, (char*)img->file_base + phdr->p_offset, phdr->p_filesz);
                if (phdr->p_filesz == 0) img->interp_path[0] = '\0';
                else img->interp_path[phdr->p_filesz - 1] = '\0';
                break;
            }
        }
    }

    return 0;
}

/**
 * @brief Check the image to make sure it is valid
 * @param image The image to check
 * @param type The type of image to look for
 * @returns 1 on a valid image
 */
int elf_checkImage(elf_image_t *image, int type) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)image->file_base;
    if (type == ELF_EXECUTABLE) {
        return ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN;
    } else {
        return ehdr->e_type == ET_REL;
    }
}

/**
 * @brief Load a PT_LOAD PHDR
 */
static int elf_mapSegment(elf_image_t *img, Elf64_Phdr *phdr) {
    // Zero out the memory region that will be loaded
    DEBUG("Map segment p_vaddr=%p p_offset=%p p_filesz=0x%x p_memsz=0x%x\n", phdr->p_vaddr, phdr->p_offset, phdr->p_filesz, phdr->p_memsz);
    uintptr_t seg_start = img->load_bias + phdr->p_vaddr;
    uintptr_t seg_end = seg_start + phdr->p_memsz;

    // Copy the file data into memory
    memcpy((void*)seg_start, (char*)img->file_base + phdr->p_offset, phdr->p_filesz);

    // Zero out the BSS section
    if (phdr->p_memsz > phdr->p_filesz) {
        memset((void*)(seg_start + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
    }

    return 0;
}

/**
 * @brief Load an executable image
 */
static int elf_loadImageExecutable(elf_image_t *img) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)img->file_base;

    // Determine the full image's size in memory
    // TODO maybe add some protection against annoying segmentation
    uintptr_t min_vaddr = MMU_USERSPACE_END;
    uintptr_t max_vaddr = 0x0;
    for (unsigned i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_vaddr < min_vaddr) {
                min_vaddr = phdr->p_vaddr;
            }

            if (phdr->p_vaddr+phdr->p_memsz > max_vaddr) {
                max_vaddr = phdr->p_vaddr+phdr->p_memsz;
            }
        }
    }

    DEBUG("Calculated ELF file span: %p - %p\n", min_vaddr, max_vaddr);

    size_t image_size = max_vaddr - min_vaddr;
    image_size += min_vaddr & 0xFFF; // !!!: hack that should be moved to vmm_map, ensures that we can fully map in this region
    img->image_size = image_size;

    min_vaddr = PAGE_ALIGN_DOWN(min_vaddr);
    max_vaddr = PAGE_ALIGN_UP(max_vaddr);


    void *base;
    if (ehdr->e_type == ET_EXEC) {
        // ET_EXEC must have a zero load bias
        base = vmm_map((void*)min_vaddr, image_size, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_USER | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
        if (base && base != (void*)min_vaddr) {
            LOG(ERR, "Failed to match min_vaddr (could only get %p)\n", base);
            vmm_unmap(base, image_size);
            return -ENOEXEC;
        }
    } else {
        // ET_DYN can be really anywhere
        base = vmm_map(min_vaddr ? (void*)min_vaddr : (void*)0x1000, image_size, VM_FLAG_ALLOC, MMU_FLAG_USER | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    }

    if (!base) {
        LOG(ERR, "Failed to load ELF executable (vmm_map)\n");
        return -ENOMEM;
    }

    DEBUG("Loading ELF file to base: %p\n", base);
    img->load_bias = (uintptr_t)base - min_vaddr;
    img->img_base = (uintptr_t)base;
    img->entrypoint = img->load_bias + ehdr->e_entry;

    // PHDR round 2 time
    for (unsigned i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = ELF_PHDR(ehdr, i);

        if (phdr->p_type == PT_LOAD) {
            int r = elf_mapSegment(img, phdr);
            if (r != 0) return r;
        }
    }
    
    return 0;
}

/**
 * @brief Load a relocatable image
 */
static int elf_loadImageRelocatable(elf_image_t *img) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)img->file_base;
    assert(0 && "UNIMPL");
}

/**
 * @brief Load an ELF image
 * @param image The image to load
 * @returns Status code
 */
int elf_loadImage(elf_image_t *img) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)img->file_base;

    if (ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC) {
        return elf_loadImageExecutable(img);
    } else if (ehdr->e_type == ET_REL) {
        return elf_loadImageRelocatable(img);
    } else {
        assert(0);
    }

    return 0;
}

/**
 * @brief Build an auxv
 * @param image The image to build the auxiliary vector for
 * @param auxv The auxv to build
 * @returns 0 on success
 */
int elf_buildAuxv(elf_image_t *image, elf_auxv_t *auxv) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)image->file_base;

    auxv->entry_count = 0;

#define AUXV_PUSH(_type,_val)   auxv->entries[auxv->entry_count].type = (_type); \
                                auxv->entries[auxv->entry_count].value = (_val); \
                                auxv->entry_count++;

    AUXV_PUSH(AT_PHDR, image->img_base + ehdr->e_phoff);
    AUXV_PUSH(AT_PHENT, ehdr->e_phentsize);
    AUXV_PUSH(AT_PHNUM, ehdr->e_phnum);
    AUXV_PUSH(AT_ENTRY, image->entrypoint);
    AUXV_PUSH(AT_BASE, image->load_bias);
    AUXV_PUSH(AT_PAGESZ, PAGE_SIZE);
    AUXV_PUSH(AT_UID, current_cpu->current_process->uid);
    AUXV_PUSH(AT_GID, current_cpu->current_process->gid);
    AUXV_PUSH(AT_EUID, current_cpu->current_process->euid);
    AUXV_PUSH(AT_EGID, current_cpu->current_process->egid);
    AUXV_PUSH(AT_NULL, 0);
#undef AUXV_PUSH

    return 0;
}

/**
 * @brief Destroy an ELF image
 */
int elf_destroyImage(elf_image_t *image) {
    kfree(image->file_base);
    if (image->interp_path) kfree(image->interp_path);
    return 0;
}
