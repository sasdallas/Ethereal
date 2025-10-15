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
    { .name = "Shebang", .load = binfmt_shebang, .match_count = 2, .bytes = { '#', '!' } },
};

int binfmt_last_entry = 2;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "LOADER:BINFMT", __VA_ARGS__)

/**
 * @brief Shebang
 */
static int binfmt_shebang(char *path, fs_node_t *file, int argc, char **argv, char **envp) {
    char buf[256];
    if (fs_read(file, 0, 256, (uint8_t*)buf) <= 0) return -EIO;
    fs_close(file);

    char *sb_start = buf + 2;
    while (*sb_start == ' ') sb_start++;
    if (!*sb_start) return -ENOEXEC;

    char *nl = strchr(sb_start, '\n');
    if (!nl) return -ENOEXEC;
    *nl = 0;

    char *arg = strchr(sb_start, ' ');
    if (arg) *(arg++) = 0;

    fs_node_t *interp = kopen(sb_start, 0);
    if (!interp) return -ENOENT;

    int new_argc = argc + (arg ? 2 : 1);
    char **nargv = kmalloc(sizeof(char*) * (new_argc + 1));
    nargv[0] = strdup(sb_start);
    nargv[1] = arg ? strdup(arg) : strdup(path);
    nargv[2] = arg ? strdup(path) : NULL;

    for (int i = 1, o = arg ? 3 : 2; i < argc; i++, o++) nargv[o] = argv[i];
    nargv[new_argc] = NULL;

    return binfmt_exec(nargv[0], interp, new_argc, nargv, envp);
}

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
            if (memcmp(entry.bytes, bytes, entry.match_count)) continue;

            // Start execution
            LOG(INFO, "Executing file as \"%s\"\n", entry.name);
            return entry.load(path, file, argc, argv, envp);
        }
    }

    return -ENOEXEC;
}