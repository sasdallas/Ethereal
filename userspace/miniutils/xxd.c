/**
 * @file userspace/miniutils/xxd.c
 * @brief xxd clone
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
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // TODO cli args and stdin mode
    if (argc < 2) {
        printf("Usage: xxd [filename]\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "xxd: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }


    unsigned char buf[16];
    size_t n;
    unsigned long offset = 0;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        printf("%08lx: ", offset);

        for (size_t i = 0; i < 16; i++) {
            if (i < n) {
                printf("%02x", buf[i]);
            } else {
                printf("  ");
            }

            if (i % 2 == 1) {
                putchar(' ');
            }
        }

        putchar(' ');
        for (size_t i = 0; i < n; i++) {
            unsigned char c = buf[i];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }
        putchar('\n');

        offset += (unsigned long)n;
    }

    if (ferror(f)) {
        fprintf(stderr, "xxd: read error: %s\n", strerror(errno));
        fclose(f);
        return 1;
    }

    fclose(f);
    return 0;
}
