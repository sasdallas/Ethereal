/**
 * @file userspace/miniutils/ln.c
 * @brief ln command
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
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>

void usage() {
    printf("Usage: ln [OPTION]... TARGET LINK_NAME\n");
    printf("  or:  ln [OPTION]... TARGET\n");
    printf("  or:  ln [OPTION]... TARGET... DIRECTORY\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("ln (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}


static char *join_path(const char *dir, const char *base) {
    size_t n = strlen(dir);
    size_t m = strlen(base);
    int need_slash = (n > 0 && dir[n-1] != '/') ? 1 : 0;
    char *buf = malloc(n + need_slash + m + 1);
    if (!buf) return NULL;
    if (n > 0) memcpy(buf, dir, n);
    if (need_slash) buf[n++] = '/';
    memcpy(buf + n, base, m);
    buf[n + m] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "symbolic", .has_arg = no_argument, .flag = NULL, .val = 's' },
        { .name = "force", .has_arg = no_argument, .flag = NULL, .val = 'f' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        {0,0,0,0}
    };

    bool opt_symbolic = false;
    bool opt_force = false;

    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "sfhv", (const struct option*)options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) {
            ch = options[index].val;
        }

        switch (ch) {
            case 's': opt_symbolic = true; break;
            case 'f': opt_force = true; break;
            case 'v': version(); break;
            case 'h': default: usage(); break;
        }
    }

    int remaining = argc - optind;
    if (remaining < 1) {
        usage();
    }


    int exit_status = 0;

    if (remaining == 1) {
        const char *src = argv[optind];
        char *srcdup = strdup(src);
        if (!srcdup) {
            perror("strdup");
            return EXIT_FAILURE;
        }
        char *base = basename(srcdup);
        char *dest = join_path(".", base);
        free(srcdup);
        if (!dest) {
            perror("malloc");
            return EXIT_FAILURE;
        }

        struct stat st;
        if (lstat(dest, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                fprintf(stderr, "ln: cannot overwrite directory '%s'\n", dest);
                free(dest);
                return EXIT_FAILURE;
            }
            if (opt_force) {
                if (unlink(dest) != 0) {
                    fprintf(stderr, "ln: cannot remove '%s': %s\n", dest, strerror(errno));
                    free(dest);
                    return EXIT_FAILURE;
                }
            } else {
                fprintf(stderr, "ln: failed to create link '%s': File exists\n", dest);
                free(dest);
                return EXIT_FAILURE;
            }
        }

        if (opt_symbolic) {
            if (symlink(src, dest) != 0) {
                fprintf(stderr, "ln: failed to create symbolic link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                free(dest);
                return EXIT_FAILURE;
            }
        } else {
            if (link(src, dest) != 0) {
                fprintf(stderr, "ln: failed to create hard link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                free(dest);
                return EXIT_FAILURE;
            }
        }
        free(dest);
        return EXIT_SUCCESS;
    }

    const char *possible_dir = argv[argc - 1];
    struct stat st;
    bool dest_is_dir = (stat(possible_dir, &st) == 0 && S_ISDIR(st.st_mode));

    if (remaining > 2 && !dest_is_dir) {
        fprintf(stderr, "ln: when creating multiple links, last argument must be a directory\n");
        return EXIT_FAILURE;
    }

    if (dest_is_dir) {
        const char *dir = possible_dir;
        for (int i = optind; i < argc - 1; ++i) {
            const char *src = argv[i];
            char *srcdup = strdup(src);
            if (!srcdup) {
                perror("strdup");
                exit_status = 1;
                continue;
            }
            char *base = basename(srcdup);
            char *dest = join_path(dir, base);
            free(srcdup);
            if (!dest) {
                perror("malloc");
                exit_status = 1;
                continue;
            }

            struct stat dstst;
            if (lstat(dest, &dstst) == 0) {
                if (S_ISDIR(dstst.st_mode)) {
                    fprintf(stderr, "ln: cannot overwrite directory '%s'\n", dest);
                    free(dest);
                    exit_status = 1;
                    continue;
                }
                if (opt_force) {
                    if (unlink(dest) != 0) {
                        fprintf(stderr, "ln: cannot remove '%s': %s\n", dest, strerror(errno));
                        free(dest);
                        exit_status = 1;
                        continue;
                    }
                } else {
                    fprintf(stderr, "ln: failed to create link '%s': File exists\n", dest);
                    free(dest);
                    exit_status = 1;
                    continue;
                }
            }

            if (opt_symbolic) {
                if (symlink(src, dest) != 0) {
                    fprintf(stderr, "ln: failed to create symbolic link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                    exit_status = 1;
                }
            } else {
                if (link(src, dest) != 0) {
                    fprintf(stderr, "ln: failed to create hard link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                    exit_status = 1;
                }
            }
            free(dest);
        }
        return exit_status ? 1 : 0;
    } else {
        const char *src = argv[optind];
        const char *dest = argv[optind + 1];

        struct stat dstst;
        if (lstat(dest, &dstst) == 0) {
            if (S_ISDIR(dstst.st_mode)) {
                fprintf(stderr, "ln: cannot overwrite directory '%s'\n", dest);
                return 1;
            }
            if (opt_force) {
                if (unlink(dest) != 0) {
                    fprintf(stderr, "ln: cannot remove '%s': %s\n", dest, strerror(errno));
                    return 1;
                }
            } else {
                fprintf(stderr, "ln: failed to create link '%s': File exists\n", dest);
                return 1;
            }
        }

        if (opt_symbolic) {
            if (symlink(src, dest) != 0) {
                fprintf(stderr, "ln: failed to create symbolic link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                return 1;
            }
        } else {
            if (link(src, dest) != 0) {
                fprintf(stderr, "ln: failed to create hard link '%s' -> '%s': %s\n", dest, src, strerror(errno));
                return 1;
            }
        }
        return 0;
    }
}