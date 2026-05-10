/**
 * @file hexahedron/include/kernel/loader/elfv2.h
 * @brief ELF loader
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */


#ifndef KERNEL_LOADER_ELFV2_H
#define KERNEL_LOADER_ELFV2_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs_new.h>

/**** DEFINITIONS ****/
#define ELF_EXECUTABLE      0       // Looks for valid ET_EXEC/ET_DYN
#define ELF_RELOCATABLE     1       // Looks for a valid ET_REL

#define ELF_AUXV_ENTRIES    11

/**** TYPES ****/

typedef struct elf_image {
    vfs_file_t *file;
    
    void *file_base;
    size_t file_size;

    uintptr_t entrypoint;
    uintptr_t img_base;
    uintptr_t load_bias;
    size_t image_size;
    
    char *interp_path;
} elf_image_t;

typedef struct elf_auxv_entry {
    uintptr_t type;
    uintptr_t value;
} elf_auxv_entry_t;

typedef struct elf_auxv {
    elf_auxv_entry_t entries[ELF_AUXV_ENTRIES];
    size_t entry_count;
} elf_auxv_t;

/**** FUNCTIONS ****/

/**
 * @brief Open an ELF image
 * @param file The image to open
 * @param image The image output
 * @returns Status code
 * 
 * The file is left open and won't be closed, so make sure you do that.
 */
int elf_openImage(vfs_file_t *file, elf_image_t *img);

/**
 * @brief Load an ELF image
 * @param image The image to load
 * @returns Status code
 */
int elf_loadImage(elf_image_t *img);

/**
 * @brief Check the image to make sure it is valid
 * @param image The image to check
 * @param type The type of image to look for
 * @returns 1 on a valid image
 */
int elf_checkImage(elf_image_t *image, int type);

/**
 * @brief Build an auxv
 * @param image The image to build the auxiliary vector for
 * @param auxv The auxv to build
 * @returns 0 on success
 */
int elf_buildAuxv(elf_image_t *image, elf_auxv_t *auxv);

/**
 * @brief Destroy an ELF image
 */
int elf_destroyImage(elf_image_t *image);

#endif
