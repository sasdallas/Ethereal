/**
 * @file userspace/dynlinker/dynlinker.c
 * @brief Ethereal dynamic linker (/lib/ld.so equivalent)
 * 
 * @todo Implement run-time linker support (GOT+0x10)
 * @todo i386 support
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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>


/* Enable linker debug */
int __linker_debug = 0;

int __linker_class = 0; // 1 for 32-bit objects, 2 for 64-bit

#define LD_VERSION_MAJOR            1
#define LD_VERSION_MINOR            0
#define LD_VERSION_LOWER            0

/* Libraries */
list_t *__linker_libraries = NULL;

/**
 * @brief Usage
 */
void usage() {
    printf("Usage: ld.so [OPTION]... EXECUTABLE-FILE [ARGS-FOR-PROGRAM...]\n");
    printf("Ethereal dynamically-linked ELF program loader\n\n");
    printf(" -d, --debug            Enable debug mode\n");
    printf(" -h, --help             Display this help message\n");
    printf(" -v, --version          Print the version of ld.so\n");
    exit(1);
}

/**
 * @brief Version
 */
void version() {
    printf("ld.so (Ethereal libc) version %d.%d.%d\n", LD_VERSION_MAJOR, LD_VERSION_MINOR, LD_VERSION_LOWER);
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Get the filepath of a library
 * @param filename The filename of the library
 * @returns Duplicated filename
 */
char *elf_find(char *filename) {
    if (strchr(filename, '/')) return strdup(filename);

    // Use LD_LIBRARY_PATH
    char *path = getenv("LD_LIBRARY_PATH");
    if (!path) {
        path = "/lib:/usr/lib:/device/initrd/lib:/device/initrd/usr/lib";
    }

    // Tokenize
    path = strdup(path);

    char *save;
    char *pch = strtok_r(path, ":", &save);
    while (pch) {
        // Create the path
        char test[strlen(pch) + strlen(filename) + 2];
        snprintf(test, strlen(pch) + strlen(filename) + 2, "%s/%s", pch, filename);

        LD_DEBUG("Trying %s\n", test);

        struct stat st;
        if (stat((const char*)test, &st) != 0 || S_ISDIR(st.st_mode)) {
            // Next!
            pch = strtok_r(NULL, ":", &save);
            continue;
        }

        return strdup(test);
    }

    free(path);
    return NULL;
}

/**
 * @brief Get an ELF object from a file
 * @param filename The filename to load
 * @returns ELF object or NULL
 */
elf_obj_t *elf_get(char *filename) {
    // Try to get a good filename
    char *fn = elf_find(filename);
    if (!fn) {
        fprintf(stderr, "ld.so: %s: Not found\n", filename);
        return NULL;
    }

    // Load the ELF file in
    FILE *f = fopen(fn, "r");
    if (!f) {
        fprintf(stderr, "ld.so: %s: %s\n", fn, strerror(errno));
        return NULL;
    }

    // Allocate space to read the entire ELF object
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buffer = malloc(fsize);

    if (fread(buffer, fsize, 1, f) < 1) {
        fprintf(stderr, "ld.so: %s: read failed\n", fn);
        return NULL;
    }

    // Validate the executable is in fact an ELF executable
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)buffer;
    __linker_class = ehdr->e_ident[EI_CLASS];
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
            ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
            ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
            __linker_class <= 0 || __linker_class > 2) {
        fprintf(stderr, "ld.so: %s: Not a valid ELF executable\n", fn);
        return NULL;
    }
    
    // TEMPORARY
    if (__linker_class == ELFCLASS32) {
        fprintf(stderr, "ld.so: %s: 32-bit objects are not supported yet\n", fn);
        return NULL;
    }

    // Create ELF object
    elf_obj_t *obj = malloc(sizeof(elf_obj_t));
    memset(obj, 0, sizeof(elf_obj_t));
    obj->filename = fn;
    obj->f = f;
    obj->buffer = buffer;
    obj->dependencies = list_create("dependencies");
    return obj;
}

int main(int argc, char *argv[], char **envp) {
    struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v' },
        { .name = "debug", .flag = NULL, .has_arg = no_argument, .val = 'd'}
    };

    // Hack for LD_DEBUG
    if (getpid() == 0) {
        open("/device/stdin", O_RDONLY);
        open("/device/kconsole", O_RDWR);
        open("/device/kconsole", O_RDWR);
    }

    int index;
    int ch;
    while ((ch = getopt_long(argc, argv, "hvd", (const struct option*)&options, &index)) != -1) {
        if (!ch && options[index].flag == 0) {
            ch = options[index].val;
        }
        
        switch (ch) {
            case 'd':
                __linker_debug = 1;
                break;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    if (argc-optind < 1) {
        usage();
    }

    if (!__linker_debug) {
        char *debug_enable = getenv("LD_DEBUG");
        if (debug_enable && (!strcmp(debug_enable, "1") || !strcmp(debug_enable, "yes"))) {
            __linker_debug = 1;
        }
    }

    __linker_symbol_table = hashmap_create("ld symbol table", 10);

    elf_obj_t *obj = elf_get(argv[optind]);
    if (!obj) {
        fprintf(stderr, "ld.so: %s: Failed to load file\n", argv[optind]);
        return 1;
    }

    // Load object
    if (elf_load(obj) != 0) {
        fprintf(stderr, "ld.so: %s: Failed to load executable\n", argv[optind]);
        return 1;
    }

    // Load dynamic
    if (elf_dynamic(obj) != 0) {
        fprintf(stderr, "ld.so: %s: Failed to parse dynamic table\n", argv[optind]);
        return 1;
    }

    __linker_libraries = list_create("ld loaded libs");
    foreach(dep, obj->dependencies) {
        LD_DEBUG("[%s] Loading dependency %s\n", obj->filename, dep->value);

        elf_obj_t *lib = elf_get(dep->value);
        if (!lib) {
            fprintf(stderr, "ld.so: %s: Failed to load file\n", dep->value);
            return 1;
        }

        // Load object
        if (elf_load(lib) != 0) {
            fprintf(stderr, "ld.so: %s: Failed to load library\n", dep->value);
            return 1;
        }

        // Load dynamic
        if (elf_dynamic(lib) != 0) {
            fprintf(stderr, "ld.so: %s: Failed to parse dynamic table\n", dep->value);
            return 1;
        }

        // Add to library list
        list_append(__linker_libraries, lib);

        // Perform relocations
        if (elf_relocate(lib) != 0) {
            fprintf(stderr, "ld.so: %s: Failed to handle relocations\n", lib->filename);
            return 1;
        }
    }

    // Relocate main object
    if (elf_relocate(obj) != 0) {
        fprintf(stderr, "ld.so: %s: Failed to load relocations for file\n", obj->filename);
        return 1;
    }


    // // Relocate all libraries
    // foreach(libnode, __linker_libraries) {
    //     elf_obj_t *lib = (elf_obj_t*)libnode->value;

    //     // Perform relocations
    //     if (elf_relocate(lib) != 0) {
    //         fprintf(stderr, "ld.so: %s: Failed to handle relocations\n", lib->filename);
    //         return 1;
    //     }
    // }

    // All libraries have been loaded
    // Call constructors
    foreach(libnode, __linker_libraries) {
        elf_obj_t *lib = (elf_obj_t*)libnode->value;
        if (lib->dyntab.init) {
            // Call init function
            void (*init)(void) = (void*)lib->dyntab.init;
            LD_DEBUG("[%s] Executing init function %p\n", lib->filename, init);
            init();
        }

        if (lib->dyntab.init_array && lib->dyntab.init_arraysz) {
            for (unsigned i = 0; i < lib->dyntab.init_arraysz; i++) {
                void (*constructor)(void) = (void*)((uintptr_t*)lib->dyntab.init_array)[i];
                LD_DEBUG("[%s] Executing constructor %p\n", lib->filename, constructor);
                constructor();
            }
        }
    }

    if (obj->dyntab.init) {
        void (*init)(void) = (void*)obj->dyntab.init;
        LD_DEBUG("[%s] Executing init function %p\n", obj->filename, init);
        init();
    }

    if (obj->dyntab.init_array && obj->dyntab.init_arraysz) {
        for (unsigned i = 0; i < obj->dyntab.init_arraysz; i++) {
            void (*constructor)(void) = (void*)((uintptr_t*)obj->dyntab.init_array)[i];
            LD_DEBUG("[%s] Executing constructor %p\n", obj->filename, constructor);
            constructor();
        }
    }

    // Update + fix environ
    Elf64_Sym *envsym = elf_lookupFromLibrary(obj, "environ");
    if (envsym) {
        LD_DEBUG("[%s] Fixing environ\n", obj->filename);
        
        char ***env = (char***)(obj->base + envsym->st_value);
        LD_DEBUG("[%s] Environ symbol is located at %p, redirecting to our environ %p\n", obj->filename, env, envp);
        *env = envp;
    }

    // Now go!
    void (*entry)(int, char **, char **) = (void*)((Elf64_Ehdr*)obj->buffer)->e_entry;
    LD_DEBUG("Setup completed, executing app at %p\n", entry);
    
#ifdef __ARCH_X86_64__
    // Since _start from x86_64 libc pops from the stack, this is to be extra safe.
    // __libc_main handles this correctly by adjusting for __argv, __argc, __envp
    // TODO: NOT??
    asm volatile (
        "push $0\n"
        "push $0\n"
        "push $0\n"
        "push $0\n"
        "jmp * %0\n" :: "r"(entry));
#else
    // TODO: Probably need to also push on ARM
    entry(argc-2, argv+2, environ);
#endif


    return 0;
}