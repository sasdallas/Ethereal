/**
 * @file userspace/miniutils/migrate.c
 * @brief Migrate initial ramdisk utility
 * 
 * Stupid little migrate utility
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
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int verbose = 0;

#define VERBOSE(...) if (verbose) printf(__VA_ARGS__)

/**
 * @brief Copy a file
 * @param src The source filename
 * @param dst The destination filename
 * @param st The stat of the source
 */
void file_copy(char *src, char *dst, struct stat *st) {
    VERBOSE("Copying file \"%s\" to \"%s\"...\n", src, dst);

    // Open both files
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "migrate: %s: %s\n", src, strerror(errno));
        return;
    }

    int dst_fd = open(dst, O_CREAT | O_RDWR);
    if (dst_fd < 0) {
        fprintf(stderr, "migrate: %s: %s\n", dst, strerror(errno));
        return;
    }

    // Let's copy the file over
    char buf[4096];

    ssize_t remaining = st->st_size;
    while (remaining) {
        ssize_t r = read(src_fd, buf, 4096);
        if (!r) break;
        write(dst_fd, buf, 4096);
        remaining -= r;
    }

    close(src_fd);
    close(dst_fd);
}

/**
 * @brief Copy a directory
 * @param directory The directory to copy
 * @param destination The destination of the directory
 * @param mode The mode to create the directory with
 */
void directory_copy(char *directory, char *destination, mode_t mode) {
    if (strcmp(destination, "/"))  {
        mkdir(destination, mode);
    }

    VERBOSE("Copying directory \"%s\" to \"%s\"...\n", directory, destination);
    DIR *dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "migrate: %s: %s\n", dir, strerror(errno));
        return;
    }

    struct dirent *ent = readdir(dir);
    while (ent) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) goto _next_entry;

        // Create the filenames
        size_t srclen = strlen(ent->d_name) + strlen(directory) + 2;
        char *src_filename = malloc(srclen);
        snprintf(src_filename, srclen, "%s/%s", directory, ent->d_name);

        size_t dstlen = strlen(ent->d_name) + strlen(destination) + 2;
        char *dst_filename = malloc(dstlen);
        snprintf(dst_filename, dstlen, "%s/%s", destination, ent->d_name);

        struct stat st;
        if (lstat(src_filename, &st) < 0) {
            fprintf(stderr, "migrate: stat: %s\n", strerror(errno));
            goto _next_entry;
        }


        // Depending on the type..
        if (S_ISDIR(st.st_mode)) {
            directory_copy(src_filename, dst_filename, st.st_mode & 07777);
        } else if (S_ISREG(st.st_mode)) {
            file_copy(src_filename, dst_filename, &st);
        } else {
            printf("WARNING: Unknown type (st_mode = 0x%x) on %s\n", st.st_mode, src_filename);
        }


        free(src_filename);
        free(dst_filename);
    _next_entry:
        ent = readdir(dir);
    }
}

int main(int argc, char *argv[]) {
    // TODO: getopt() proper?
    if (argc > 1 && !strcmp(argv[1], "-v")) verbose = 1;

    printf("Copying rootfs image to RAM...\n");
    directory_copy("/device/initrd", "/", 0);

    // TODO: Free initrd? How? Maybe via unmount?

    return 0;
}