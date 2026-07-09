/**
 * @file userspace/miniutils/cp.c
 * @brief cp command
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
#include <getopt.h>
#include <limits.h>

static int preserve = 0;
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

void copy_file(const char *src, const char *dest) {
    int src_fd, dest_fd;
    char buffer[4096];
    ssize_t bytes_read, bytes_written;
    struct stat st;

    printf("%s -> %s\n", src, dest);

    src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        perror("open source file");
        exit(EXIT_FAILURE);
    }

    if (!force && access(dest, F_OK) == 0) {
        fprintf(stderr, "cp: cannot overwrite '%s'\n", dest);
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    if (preserve && stat(src, &st) < 0) {
        perror("stat");
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC,
        preserve ? (st.st_mode & 0777) : 0644);
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

    if (preserve) {
        chmod(dest, st.st_mode & 07777);
    }
}

void copy_symlink(const char *src, const char *dest) {
    char target[PATH_MAX];
    ssize_t len;

    if (!force && access(dest, F_OK) == 0) {
        fprintf(stderr, "cp: cannot overwrite '%s'\n", dest);
        exit(EXIT_FAILURE);
    }

    if (force)
        unlink(dest);

    len = readlink(src, target, sizeof(target) - 1);
    if (len < 0) {
        perror("readlink");
        exit(EXIT_FAILURE);
    }

    target[len] = '\0';

    if (symlink(target, dest) < 0) {
        perror("symlink");
        exit(EXIT_FAILURE);
    }

    if (verbose)
        printf("%s -> %s\n", src, dest);
}

void copy_directory(const char *src, const char *dest) {
    struct stat st;
    struct dirent *entry;
    DIR *dir;

    if (stat(dest, &st) == -1) {
        mode_t mode = 0755;

        if (preserve && stat(src, &st) == 0) {
            mode = st.st_mode & 07777;
        }

        if (mkdir(dest, mode) < 0) {
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

        if (lstat(src_path, &st) != 0) {
            fprintf(stderr, "cp: %s: %s\n", src_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            copy_directory(src_path, dest_path);
        } else if (S_ISLNK(st.st_mode)) {
            copy_symlink(src_path, dest_path);
        } else {
            copy_file(src_path, dest_path);
        }
    }

    closedir(dir);
}

void usage() {
    printf("Usage: cp [OPTION] [-rpf] [SOURCE] [DESTINATION]\n");
    exit(EXIT_FAILURE); // todo
}

void version() {
    printf("cp (Ethereal miniutils) 1.10\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
} 

int main(int argc, char *argv[]) {
    struct stat st;
    int recursive = 0;
    int opt;

    static struct option long_options[] = {
        { "recursive", no_argument, 0, 'r' },
        { "preserve",  no_argument, 0, 'p' },
        { "force",     no_argument, 0, 'f' },
        { "help",      no_argument, 0, 'h' },
        { "version",   no_argument, 0, 'V' },
        { "verbose",   no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "rpfhv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                recursive = 1;
                break;
            case 'p':
                preserve = 1;
                break;
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
                fprintf(stderr, "Usage: %s [-rpf] <source> <destination>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc - optind < 2) {
        fprintf(stderr, "Usage: %s [-rpf] <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (lstat(argv[optind], &st) < 0) {
        fprintf(stderr, "cp: %s: %s", argv[optind], strerror(errno));
        return EXIT_FAILURE;
    }

    if (S_ISLNK(st.st_mode)) {
        copy_symlink(argv[optind], argv[optind+1]);
    } else if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n", argv[optind]);
            return EXIT_FAILURE;
        }
        copy_directory(argv[optind], argv[optind + 1]);
    } else {
        char *dest = make_dest_path(argv[optind], argv[optind + 1]);
        copy_file(argv[optind], dest);
        free(dest);
    }

    return EXIT_SUCCESS;
}
