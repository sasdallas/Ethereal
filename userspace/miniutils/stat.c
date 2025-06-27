/**
 * @file userspace/miniutils/stat.c
 * @brief stat test
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: stat <filename>\n");
        exit(EXIT_FAILURE);
    }
    struct stat st;
    if (stat(argv[1], &st) < 0) {
        printf("stat: %s: %s\n", argv[1], strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("stat for %s:\n", argv[1]);
    printf("\tst_dev: %d\n", st.st_dev);
    printf("\tst_ino: %d\n", st.st_ino);
    printf("\tst_mode: %d\n", st.st_mode);
    printf("\tst_nlink: %d\n", st.st_nlink);
    printf("\tst_uid: %d\n", st.st_uid);
    printf("\tst_gid: %d\n", st.st_gid);
    printf("\tst_rdev: %d\n", st.st_rdev);
    printf("\tst_size: %d\n", st.st_size);
    printf("\tst_atime: %d\n", st.st_atime);
    printf("\tst_mtime: %d\n", st.st_mtime);
    printf("\tst_ctime: %d\n", st.st_ctime);
    printf("\tst_blksize: %d\n", st.st_blksize);
    printf("\tst_blocks: %d\n", st.st_blocks);

    printf("type: ");
    if (S_ISBLK(st.st_mode)) printf("blk\n");
    else if (S_ISCHR(st.st_mode)) printf("chr\n");
    else if (S_ISDIR(st.st_mode)) printf("dir\n");
    else if (S_ISFIFO(st.st_mode)) printf("fifo\n");
    else if (S_ISREG(st.st_mode)) printf("reg\n");
    else if (S_ISLNK(st.st_mode)) printf("lnk\n");
    else if (S_ISSOCK(st.st_mode)) printf("sock\n");
    else printf("not found\n");
    return 0;
}