/**
 * @file userspace/dynlinker/sym.c
 * @brief Linker replacement symbols
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "dynlinker.h"
#include <stdio.h>

static char **__get_environ_ld(void);
static char **__get_argv_ld(void);
extern char **__argv;
extern char **environ;

const elf_symbol_t __linker_symbols[] = {
    { .name = "__get_environ", .addr = __get_environ_ld },
    { .name = "__get_argv", .addr = __get_argv_ld },
};

static char **__get_environ_ld(void) {
    return environ;
}

static char **__get_argv_ld(void) {
    return &__argv[2];
}