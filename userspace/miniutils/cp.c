/**
 * @file userspace/miniutils/cp.c
 * @brief weak cp command that needs a rewrite
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
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

void copy_file(const char *src, const char *dest) {
    int src_fd, dest_fd;
    char buffer[4096];
    ssize_t bytes_read, bytes_written;

    src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        perror("open source file");
        exit(EXIT_FAILURE);
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("open destination file");
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write");
            close(src_fd);
            close(dest_fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read < 0) {
        perror("read");
    }

    close(src_fd);
    close(dest_fd);
}

void copy_directory(const char *src, const char *dest) {
    struct stat st;
    struct dirent *entry;
    DIR *dir;

    if (stat(dest, &st) == -1) {
        if (mkdir(dest, 0755) < 0) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }

    dir = opendir(src);
    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        char src_path[4096], dest_path[4096];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            copy_directory(src_path, dest_path);
        } else {
            copy_file(src_path, dest_path);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    struct stat st;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [-r] <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int recursive = 0;
    int src_index = 1;

    if (strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        src_index = 2;
    }

    if (stat(argv[src_index], &st) < 0) {
        perror("stat");
        return EXIT_FAILURE;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n", argv[src_index]);
            return EXIT_FAILURE;
        }
        copy_directory(argv[src_index], argv[src_index + 1]);
    } else {
        copy_file(argv[src_index], argv[src_index + 1]);
    }

    return EXIT_SUCCESS;
}