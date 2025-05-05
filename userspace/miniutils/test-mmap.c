/**
 * @file userspace/miniutils/test-mmap.c
 * @brief mmap test
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    // Get some memory!
    void *addr = mmap(0, 0x4000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        printf("test-mmap: error while mapping memory: %s\n", strerror(errno));
        return 1;
    }

    printf("mmap: acquired 0x4000 bytes at %p\n", addr);
    memset(addr, 0, 0x4000);    

    munmap(addr, 0x4000);
    printf("mmap: allocation destroyed\n");

    // Open file
    char *file = "/test.txt";
    if (argc > 1) file = argv[1];


    int f = open(file, O_RDWR | O_CREAT);
    if (f < 0) {
        printf("test-mmap: %s: %s\n", file, strerror(errno));
        return 1;
    }
    
    // Write some bare minimum content

    printf("mmap: mapping file %s\n", file);
    void *file_map = mmap(NULL, 128, PROT_READ | PROT_WRITE, MAP_PRIVATE, f, 0);
    if (file_map == MAP_FAILED) {
        printf("test-mmap: %s: %s\n", file, strerror(errno));
        return 1;
    }

    printf("mmap: acquired 128 bytes for file %s at %p\n", file, file_map);

    strcpy((char*)file_map, "This is a test program for mmap()");
    printf("mmap: wrote bytes\n");

    munmap(file_map, 128);
    printf("mmap: unmapped file\n");

    
    printf("mmap: mapping fb\n");
    int fb = open("/device/fb0", O_RDWR);
    if (fb < 0) {
        printf("test-mmap: fbopen failed: /device/fb0: %s\n", strerror(errno));
        return 1;
    }

#include <kernel/gfx/video.h>

    video_info_t info;
    if (ioctl(fb, IO_VIDEO_GET_INFO, &info) < 0) {
        printf("test-mmap: ioctl failed: /device/fb0: %s\n", strerror(errno));
        return 1;
    }

    size_t bufsz = (info.screen_width * 4) + (info.screen_height * info.screen_pitch);

    void *fb_map = mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_PRIVATE, fb, 0);
    if (fb_map == MAP_FAILED) {
        printf("test-mmap: fbmap failed: /device/fb0: %s\n", strerror(errno));
        return 1;
    }

    memset(fb_map, 0xFF, bufsz);
    munmap(fb_map, bufsz);

    return 0;
}