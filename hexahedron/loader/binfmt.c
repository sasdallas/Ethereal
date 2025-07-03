/**
 * @file hexahedron/loader/binfmt.c
 * @brief binfmt execution 
 * 
 * Contains a variable table of @c BINFMT_MAX entries, where each entry will have some identifying bytes
 * 
 * @todo Maybe use an identification function as well?
 * @todo Maybe rework @c process_execute to call this?
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/loader/binfmt.h>
#include <kernel/loader/elf_loader.h>
#include <kernel/loader/elf.h>
#include <kernel/debug.h>
#include <kernel/task/process.h>
#include <string.h>
#include <kernel/mem/alloc.h>
#include <errno.h>

static int binfmt_shebang(char *path, fs_node_t *file, int argc, char **argv, char **envp);


/* binfmt table */
binfmt_entry_t binfmt_table[BINFMT_MAX] = {
    { .name = "ELF Executable", .load = process_execute, .match_count = 4, .bytes = { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 } },
    { .name = "Shebang", .load = binfmt_shebang, .match_count = 2, .bytes = { '#', '!' }},
    { 0 },
};

int binfmt_last_entry = 2;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "LOADER:BINFMT", __VA_ARGS__)

/**
 * @brief Register a new entry in the binfmt table
 * @param entry The binary entry to register
 */
int binfmt_register(binfmt_entry_t entry) {
    if (binfmt_last_entry >= BINFMT_MAX) return 1;
    binfmt_table[binfmt_last_entry] = entry;
    binfmt_last_entry++;
    return 0;
}

/**
 * @brief Shebang executable loader
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 */
static int binfmt_shebang(char *path, fs_node_t *file, int argc, char **argv, char **envp) {
    char buf[256];
    if (fs_read(file, 0, 256, (uint8_t*)buf) <= 0) return -EIO;
    fs_close(file);

    // We know that the file begins with a shebang, but how shebang is it?
    char *sb_start = &buf[2];

    // Handle any leading spaces
    while (*sb_start == ' ' && *sb_start) sb_start++;
    if (!(*sb_start)) return -ENOEXEC;


    // First take out the nl
    char *nl = strchr(sb_start, '\n');
    if (!nl) return -ENOEXEC;
    *nl = 0;

    // Is there a space?
    char *arg = NULL;
    char *sp = strchr(sb_start, ' ');
    if (sp) {
        // There is! Let's get it!
        *sp = 0;
        sp++;
        arg = sp; // We accept an interpreter argument
    }

    // Try to open the interpreter
    fs_node_t *interp = kopen(sb_start, 0);
    if (!interp) return -ENOENT;

    // Construct the arguments like so:
    // Interpreter (sb_start) + Optional argument + Script

    char **nargv = kmalloc(sizeof(char*) * (argc + (arg ? 4 : 3)));
    nargv[0] = strdup(sb_start); // !!!: MEMORY LEAK. Perhaps process_execute could clean this..?

    // Construct argument list
    if (arg) {
        nargv[1] = strdup(arg);
        nargv[2] = strdup(path);
        nargv[3] = NULL;
    } else {
        nargv[1] = strdup(path);
        nargv[2] = NULL;
    }

    // Now create the remaining arguments
    int o = (arg ?  3 : 2);

    for (int i = 1; i < argc; i++, o++) {
        nargv[o] = argv[i];
    }

    nargv[o] = NULL;

    // Go!
    return binfmt_exec(nargv[0], interp, argc + (arg ? 2 : 1), nargv, envp);
}

/**
 * @brief Start execution of a process or return an error code
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 */
int binfmt_exec(char *path, fs_node_t *file, int argc, char **argv, char **envp) {
    // Read bytes of the file
    char bytes[BINFMT_BYTE_MAX];
    if (fs_read(file, 0, BINFMT_BYTE_MAX, (uint8_t*)bytes) != BINFMT_BYTE_MAX) return -EIO;

    // Start comparing
    for (int i = 0; i < binfmt_last_entry; i++) {
        binfmt_entry_t entry = binfmt_table[i];
        if (entry.match_count) {
            int match = 1;
            for (size_t j = 0; j < entry.match_count; j++) {
                if (entry.bytes[j] != bytes[j]) {
                    match = 0;
                    break;
                }
            }

            if (!match) continue;

            // Start execution
            LOG(INFO, "Executing file as \"%s\"\n", entry.name);
            return entry.load(path, file, argc, argv, envp);
        }
    }

    return -ENOEXEC;
}