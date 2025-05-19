/**
 * @file libpolyhedron/unistd/exec.c
 * @brief execv, execvp, execvpe, execl, execlp, execle
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

int execvpe(const char *file, const char *argv[], char *envp[]) {
    // Does file contain a "/" anywhere?
    if (strchr(file, '/')) {
        return execve(file, argv, envp);
    }

    // Get PATH
    const char *path = getenv("PATH");
    if (!path) {
        // Supposed to default to a path.. Unimplemented
        errno = ENOENT;
        return -1;
    }

    // Start looping through path
    char *p = (char*)path;
    
    char exec_path[strlen(file) + 128 + 1]; // PATH_MAX?
    while (*p) {
        // Get the next occurance of a :
        char *p_next = strchrnul(p, ':');
        if (*p_next == 0) break;

        if (strlen(p) > 128) continue; // Next iteration

        // Construct a basic path
        // !!!: bad?
        size_t p_len = p_next - p;
        strncpy(exec_path, p, p_len);
        exec_path[p_len] = '/';
        strncpy(exec_path + p_len +  1, file, 128); 

        // Try to execute
        execve(exec_path, argv, envp);

        // Ok, something didn't work.
        switch (errno) {
            // Errors on which to ignore and continue, taken from glibc
            case EACCES:
            case ENOENT:
            case ESTALE:
            case ENOTDIR:
            case ENODEV:
            case ETIMEDOUT:
                break;

            default:
                // Some other critical error must've occurred
                return -1;
        }

        // Ok, make sure p_next is valid
        if (!p_next[1]) break; 
        p = p_next;
    }

    return -1;
}

int execvp(const char *file, const char *argv[]) {
    return execvpe(file, argv, environ);
}

int execl(const char *pathname, const char *arg, ...) {
    int argc = 0;
    va_list ap;
    va_start(ap, arg);

    // Get argc
    while (1) {
        argc++;
        if (!va_arg(ap, char *)) {
            break;
        }
    }

    // End the varargs list
    va_end(ap);

    char *argv[argc+1];

    // Restart varargs list
    va_start(ap, arg);
    argv[0] = (char*)arg;
    for (int i = 1; i < argc; i++) {
        argv[i] = va_arg(ap, char*);
    }
    va_end(ap);

    return execve(pathname, (const char**)argv, environ);
}

int execle(const char *path, const char *arg, ...) {
    int argc = 0;
    va_list ap;
    va_start(ap, arg);

    // Get argc
    while (1) {
        argc++;

        // NULL, afterwards is envp
        if (!va_arg(ap, char *)) {
            break;
        }
    }

    // End the varargs list
    va_end(ap);

    char *argv[argc+1];

    // Restart varargs list
    va_start(ap, arg);
    argv[0] = (char*)arg;
    for (int i = 1; i < argc; i++) {
        argv[i] = va_arg(ap, char*);
    }
 
    // One more for envp..
    char **envp = va_arg(ap, char**);
    va_end(ap);

    return execve(path, (const char**)argv, envp);
}

int execlp(const char *file, const char *arg, ...) {
    int argc = 0;
    va_list ap;
    va_start(ap, arg);

    // Get argc
    while (1) {
        argc++;
        if (!va_arg(ap, char *)) {
            break;
        }
    }

    // End the varargs list
    va_end(ap);

    char *argv[argc+1];

    // Restart varargs list
    va_start(ap, arg);
    argv[0] = (char*)arg;
    for (int i = 1; i < argc; i++) {
        argv[i] = va_arg(ap, char*);
    }
    va_end(ap);

    return execvpe(file, (const char**)argv, environ);
}

