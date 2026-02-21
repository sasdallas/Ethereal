/**
 * @file userspace/miniutils/rm.c
 * @brief rm
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

int rm_force = 0;
int rm_interactive = 0;
int rm_recursive = 0;
int rm_verbose = 0;

void usage() {
    printf("Usage: rm [OPTION]... [FILE]...\n");
    printf("Remove (unlink) the FILE(s)\n");
    printf("\n");
    printf(" -f, --force        Ignore nonexistent files and arguments, never prompt\n");
    printf(" -i                 Prompt before removal\n");
    printf(" -r, --recursive    Removes directories and their contents\n");
    printf(" -v, --verbose      Show verbose\n");
    printf(" -h, --help         Show this help message\n");
    printf("     --version      Show version\n\n");
    exit(1);
}

void version() {
    printf("rm version 1.0.0\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(0);
}

int rm_unlink(char *p) {
    if (rm_interactive) {
        printf("rm: remove regular file '%s'?", p);
        fflush(stdout);

        char resp[32];
        fgets(resp, 32, stdin);
        if (strcmp(resp, "yes") && strcmp(resp, "y")) {
            return 0; // user chose not to
        } 
    }

    if (rm_verbose) printf("removing '%s'\n", p);
    int r = unlink(p);
    return r;
}

int remove_dir(char *p) {
    DIR *dirp = opendir(p);
    if (!dirp) { fprintf(stderr, "rm: %s: %s\n", p, strerror(errno)); return 1; }

    struct dirent *ent = readdir(dirp);
    while (ent) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
            ent = readdir(dirp); continue;
        }

        size_t pathlen = strlen(p) + strlen(ent->d_name) + 2;
        char path[pathlen];
        snprintf(path, pathlen+2, "%s/%s", p, ent->d_name);

        int r = rm_unlink(path);
        if (r != 0) { closedir(dirp); return r; }

        ent = readdir(dirp);
    }

    closedir(dirp);

    // rm the directory
    if (rm_interactive) {
        printf("rm: remove directory '%s'?", p);
        fflush(stdout);

        char resp[32];
        fgets(resp, 32, stdin);
        if (strcmp(resp, "yes") && strcmp(resp, "y")) {
            return 0; // user chose not to
        } 
    }

    if (rm_verbose) printf("removing '%s'\n", p);
    int r = rmdir(p);
    if (r != 0) {
        fprintf(stderr, "rm: %s: %s\n", p, strerror(errno));
        return 1;
    }

    return 0;
    
}

int do_remove(char *p) {
    // try to stat it
    struct stat st;
    int r = lstat(p, &st);
    if (r < 0) {
        if (!rm_force) {
            fprintf(stderr, "rm: %s: %s\n", p, strerror(errno));
            return 1;
        } 

        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        if (rm_recursive) {
            return remove_dir(p);
        }

        fprintf(stderr, "rm: %s: Is a directory\n", p);
        return 1;
    }

    return rm_unlink(p);
}

int main(int argc, char *argv[]) {
    const struct option options[] = {
        { .name = "force", .has_arg = no_argument, .flag = NULL, .val = 'f' },
        { .name = "interactive", .has_arg = no_argument, .flag = NULL, .val = 'i' },
        { .name = "recursive", .has_arg = no_argument, .flag = NULL, .val = 'r'},
        { .name = "verbose", .has_arg = no_argument, .flag = NULL, .val = 'v'},
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'V'},
        { 0,0,0,0 },
    };

    int longidx;
    int ch;
    opterr = 1;
    while ((ch = getopt_long(argc, argv, "firvhV", options, &longidx)) != -1) {
        if (!ch && options[longidx].flag == NULL) ch = options[longidx].val;

        switch (ch ){
            case 'f':
                rm_force = 1;
                break;
            
            case 'i':
                rm_interactive = 1;
                break;
            
            case 'r':
                rm_recursive = 1;
                break;
            
            case 'v':
                rm_verbose = 1;
                break;
            
            case 'V':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    if (argc - optind < 1) {
        fprintf(stderr, "rm: missing operand\n");
        fprintf(stderr, "Try 'rm --help' for more information\n");
        return 1;
    }


    int ret = 0;

    for (int i = optind; i < argc; i++) {
        ret = do_remove(argv[i]);
        if (ret != 0) break;
    }

    return ret;
}