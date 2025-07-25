/**
 * @file libpolyhedron/libc/main.c
 * @brief libc startup code
 * 
 * @ref https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/libc/main.c for idea of using __get_argv and __get_environ
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ethereal/auxv.h>

/* Environment */
char **environ = NULL;
char **__envp = NULL;
int envc = 0;

/* argv */
char **__argv = NULL;
int __argc = 0;

/* auxv */
__auxv_t *__auxv = NULL;

// linker will override this, so we will know if we are statically linked
extern __attribute__((noinline)) char ** __get_argv() {
    return __argv;
}

// linker will override this, so we will know if we are statically linked
extern __attribute__((noinline)) char ** __get_environ() {
    return __envp;
}

// linker will override this, so we will know if we are statically linked
extern __attribute__((noinline)) __auxv_t * __get_auxv() {
    return __auxv;
}

// crtn/crti
extern void _init();
extern void _fini();

// TLS
extern void __tls_init();
extern void __elf_load_tls();

// TCB
extern void __tcb_init_dummy();

void __create_environ(char **envp) {
    // First calculate envc
    char **envpp = envp;
    while (*envpp++) {
        envc++;
    }

    envc++;

    // Now start copying
    environ = malloc(envc * sizeof(char*));
    for (int i = 0; i < envc; i++) {
        if (envp[i]) environ[i] = strdup(envp[i]);
        else environ[i] = NULL;
    }

    envc--;
}

void __create_auxv(__auxv_t *auxv) {
    __auxv = malloc(sizeof(__auxv_t));
    if (auxv) memcpy(__auxv, auxv, sizeof(__auxv_t));
}

/* Stupid utility function to calculate __argc */
int __get_argc() {
    int i = 0;
    char **p = __argv;
    while (*p) { i++; p++; }
    return i;
}

/* Main libc init function */
__attribute__((constructor)) void __libc_init() {
    __tcb_init_dummy();
    __create_environ(__get_environ());
    __create_auxv(__get_auxv());
    __argv = __get_argv();
    __argc = __get_argc();
}

__attribute__((noreturn)) void __libc_main(int (*main)(int, char**, char**), int argc, char **argv, char **envp, __auxv_t *auxv) {
    if (!__get_argv()) {
        // This returned NULL, so thus __libc_init hasn't been called yet.
        // This indicates that we were loaded from static library
        
        // Set the following variables so __get_environ and __get_argv work
        __argv = argv;
        __envp = envp;
        __auxv = auxv;

        // Now call all the constructors to run __libc_init
extern uintptr_t __init_array_start;
extern uintptr_t __init_array_end;
        for (uintptr_t *i = &__init_array_start; i < &__init_array_end; i++) {
            void (*constructor)() = (void*)*i;
            constructor();
        }

        // Initialize TLS + copy PT_TLS sections
        __tls_init();
        __elf_load_tls();
    }

    // Initialize default constructors
    _init();

    // Go!
    exit(main(__argc, __argv, environ));
    __builtin_unreachable();
}