/**
 * @file userspace/miniutils/memstat.c
 * @brief Memory stat command
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
#include <getopt.h>
#include <stdlib.h>

void usage() {
    printf("Usage: memstat [-k] [-f] [-u] [-h] [-v]\n");
    printf("Print memory statistics\n\n");

    printf(" -k, --kernel   Print out kernel memory usage\n");
    printf(" -f, --free     Print free physical memory\n");
    printf(" -u, --used     Print used physical memory\n");
    printf(" -t, --total    Print total physical memory\n");
    printf(" -p, --pretty   Pretty print\n");
    printf(" -h, --help     Print out this help and exit\n");
    printf(" -v, --version  Print out version and exit\n");
    exit(0);
}

void version() {
    printf("memstat (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(0);
}

void pretty_print(int pretty, unsigned long long memory, char *prefix) {
    if (pretty) {
        char *unit = "bytes";
        if (memory > 1000) {
            unit = "kB";
            memory /= 1000;

            if (memory > 1000) {
                unit = "MB";
                memory /= 1000;
                
                if (memory > 1000) {
                    unit = "GB";
                    memory /= 1000;
                }
            }
        }

        printf("%s%lld %s\n", prefix, memory, unit);
    } else {
        printf("%lld\n", memory);
    }
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "used", .flag = NULL, .has_arg = no_argument, .val = 'u'},
        { .name = "total", .flag = NULL, .has_arg = no_argument, .val = 't'},
        { .name = "free", .flag = NULL, .has_arg = no_argument, .val = 'f'},
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h'},
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v'},
        { .name = "kernel", .flag = NULL, .has_arg = no_argument, .val = 'k'},
        { .name = "pretty", .flag = NULL, .has_arg = no_argument, .val = 'p'},
        { 0, 0, 0, 0 }
    };

    int ch;
    int index;
    int used = 0;
    int free = 0;
    int kernel = 0;
    int pretty = 0;
    int total = 0;
    while ((ch = getopt_long(argc, argv, "ufhvk", (const struct option*)options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) ch = options[index].val;

        switch (ch) {
            case 'u': used = 1; break;
            case 'f': free = 1; break;
            case 'k': kernel = 1; break;
            case 'p': pretty = 1; break;
            case 't': total = 1; break;

            case 'v':
                version();
                return 0;

            case 'h':
            default:
                usage();
                return 0;
        }
    }

    FILE *kmem = fopen("/kernel/memory", "r");
    if (!kmem) {
        perror("/kernel/memory");
        return 1;
    }

    unsigned long long total_memory, free_memory, used_memory, kernel_memory;
    fscanf(kmem, "TotalPhysBlocks:%*lld\nTotalPhysMemory:%lld kB\nUsedPhysMemory:%lld kB\nFreePhysMemory:%lld kB\nKernelMemoryAllocator:%lld", &total_memory, &used_memory, &free_memory, &kernel_memory);
    fclose(kmem);

    total_memory *= 1000;
    free_memory *= 1000;
    used_memory *= 1000;


    if (used) {
        pretty_print(pretty, used_memory, "Used: ");
    }

    if (free) {
        pretty_print(pretty, free_memory, "Free: ");
    }

    if (total) {
        pretty_print(pretty, total_memory, "Total: ");
    }

    if (kernel) {
        pretty_print(pretty, kernel_memory, "Kernel: ");
    }


    if (!used && !free && !total && !kernel) {
        pretty_print(1, used_memory, "Used: ");
        pretty_print(1, free_memory, "Free: ");
        pretty_print(1, total_memory, "Total: ");
        pretty_print(1, kernel_memory, "Kernel: ");
    }

    return 0;
}
