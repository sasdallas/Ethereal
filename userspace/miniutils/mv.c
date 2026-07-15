/**
 * @file userspace/miniutils/mv.c
 * @brief mv command
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>

static int force = 0;
static int verbose = 0;

static char *make_dest_path(const char *src, const char *dest) {
    struct stat st;

    if (stat(dest, &st) == 0 && S_ISDIR(st.st_mode)) {
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;

        size_t len = strlen(dest) + 1 + strlen(base) + 1;
        char *path = malloc(len);
        if (!path) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        snprintf(path, len, "%s/%s", dest, base);
        return path;
    }

    return strdup(dest);
}

static int copy_fallback(const char *src, const char *dest, const struct stat *st) {
    if (S_ISLNK(st->st_mode)) {
        char target[PATH_MAX];
        ssize_t len = readlink(src, target, sizeof(target) - 1);
        if (len < 0) {
            perror("readlink");
            return -1;
        }
        target[len] = '\0';

        if (force) {
            unlink(dest);
        }

        if (symlink(target, dest) < 0) {
            perror("symlink");
            return -1;
        }

        return 0;
    }

    if (!S_ISREG(st->st_mode)) {
        errno = EISDIR;
        return -1;
    }

    // Copy regular file fallback
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        perror("open source file");
        return -1;
    }

    if (!force && access(dest, F_OK) == 0) {
        fprintf(stderr, "mv: cannot overwrite '%s'\n", dest);
        close(src_fd);
        return -1;
    }

    int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st->st_mode & 07777);
    if (dest_fd < 0) {
        perror("open destination file");
        close(src_fd);
        return -1;
    }

    char buffer[4096];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write");
            close(src_fd);
            close(dest_fd);
            return -1;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(src_fd);
        close(dest_fd);
        return -1;
    }

    close(src_fd);
    close(dest_fd);

    // Preserve permissions
    chmod(dest, st->st_mode & 07777);

    return 0;
}

void move_file(const char *src, const char *dest) {
    struct stat src_st;
    struct stat dest_st;
    int dest_exists = (lstat(dest, &dest_st) == 0);

    if (lstat(src, &src_st) < 0) {
        fprintf(stderr, "mv: cannot stat '%s': %s\n", src, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (rename(src, dest) == 0) {
        if (verbose) {
            printf("renamed '%s' -> '%s'\n", src, dest);
        }
        return;
    }

    if (dest_exists && !S_ISDIR(dest_st.st_mode)) {
        if (force) {
            unlink(dest);
            if (rename(src, dest) == 0) {
                if (verbose) {
                    printf("renamed '%s' -> '%s'\n", src, dest);
                }
                return;
            }
        }
    }

    fprintf(stderr, "mv: cannot move '%s' -> '%s': %s\n", src, dest, strerror(errno));
    exit(EXIT_FAILURE);
}

void usage() {
    printf("Usage: mv [OPTION] [-fv] [SOURCE...] [DESTINATION]\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("mv (Ethereal miniutils) 1.10\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int opt;

    static struct option long_options[] = {
        { "force", no_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'V' },
        { "verbose", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "fhVv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                force = 1;
                break;
            case 'h':
                usage();
            case 'V':
                version();
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-fv] <source> <destination>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    int remaining = argc - optind;
    if (remaining < 2) {
        fprintf(stderr, "Usage: %s [-fv] <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *dest_arg = argv[argc - 1];
    struct stat dest_st;
    int dest_exists = (stat(dest_arg, &dest_st) == 0);

    if (remaining > 2) {
        if (!dest_exists || !S_ISDIR(dest_st.st_mode)) {
            fprintf(stderr, "mv: target '%s' is not a directory\n", dest_arg);
            return EXIT_FAILURE;
        }

        for (int i = optind; i < argc - 1; i++) {
            char *dest_path = make_dest_path(argv[i], dest_arg);
            move_file(argv[i], dest_path);
            free(dest_path);
        }
    } else {
        char *dest_path = make_dest_path(argv[optind], dest_arg);
        move_file(argv[optind], dest_path);
        free(dest_path);
    }

    return EXIT_SUCCESS;
}