/**
 * @file userspace/celestial/main.c
 * @brief Celestial Window Manager main file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>

/* Log device */
FILE *__celestial_log_device = NULL; 

/* Debug mode */
int __celestial_debug = 0;

/* Graphics context */
gfx_context_t *__celestial_gfx = NULL;

/* Celestial socket -> window ID mapper table */
hashmap_t *__celestial_map = NULL;
int __celestial_client_count = 0;

/* Versioning information */
#define CELESTIAL_VERSION_MAJOR     1
#define CELESTIAL_VERSION_MINOR     0
#define CELESTIAL_VERSION_LOWER     0

/**
 * @brief Help function
 */
void usage() {
    printf("Usage: celestial [-h] [-v] [-l LOGFILE] [-d]\n");
    printf("Celestial window manager\n\n");
    printf(" -h, --help         Display this help message\n");
    printf(" -l, --log file     Change the default log device (default: /device/kconsole)\n");
    printf(" -d, --debug        Enable debug mode\n");
    printf(" -v, --version      Print the version of celestial\n\n");
    exit(1);
}

/** 
 * @brief Version function
 */
void version() {
    printf("celestial version %d.%d.%d\n", CELESTIAL_VERSION_MAJOR, CELESTIAL_VERSION_MINOR, CELESTIAL_VERSION_LOWER);
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}


/**
 * @brief Main loop of Celestial
 */
void celestial_main() {
    while (1) {
        socket_accept();
    
        if (!__celestial_client_count) continue;

        // !!!: Sorta slow, but probably the easiest way with the client map
        struct pollfd p[__celestial_client_count];
        int i = 0;
        foreach (kn, hashmap_keys(WM_SW_MAP)) {
            p[i].fd = (int)(uintptr_t)kn->value;
            p[i].events = POLLIN;
            i++;
        } 

        if (!i) continue;

        // Wait for events
        int r = poll(p, __celestial_client_count, 0);
        if (r < 0) {
            CELESTIAL_PERROR("poll");
            celestial_fatal();
        }

        for (int i = 0; i < __celestial_client_count; i++) {
            if (p[i].revents & POLLIN) {
                // INPUT available
                CELESTIAL_DEBUG("Input available on fd %d\n", p[i].fd);
                socket_handle(p[i].fd);
            }
        }

        if (!r) continue;
    }
}

/**
 * @brief Add a new client to the poll queue
 * @param fd The file descriptor of the client in the queue
 * @param win The window ID
 * @returns 0 on success
 */
int celestial_addClient(int fd, int win) {
    if (hashmap_has(WM_SW_MAP, (void*)(uintptr_t)fd)) return 1;
    hashmap_set(WM_SW_MAP, (void*)(uintptr_t)fd, (void*)(uintptr_t)win);
    __celestial_client_count++;
    return 0;
}

/**
 * @brief Remove a client from the poll queue
 * @param fd The file descriptor of the client to drop
 * @returns 0 on success
 */
int celestial_removeClient(int fd) {
    hashmap_remove(WM_SW_MAP, (void*)(uintptr_t)fd);
    __celestial_client_count--;
    return 0;
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[]) {
    int c;
    int index;
    int fake = 0;

    struct option longopts[] = {
        { .name = "debug", .has_arg = no_argument, .flag = NULL, .val = 'd' },
        { .name = "log", .has_arg = required_argument, .flag = NULL, .val = 'l' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
        { .name = "virtual", .has_arg = no_argument, .flag = NULL, .val = 'f'}
    };

    while ((c = getopt_long(argc, argv, "dlhvf", (const struct option*)longopts, &index)) != -1) {
        if (!c && longopts[index].flag == 0) {
            c = longopts[index].val;
        }

        switch (c) {
            case 'd':
                __celestial_debug = 1;
                break;
            
            case 'l':
                if (!optarg) {
                    fprintf(stderr, "celestial: option \'-l\' requires an argument\n");
                    return 1;
                }

                __celestial_log_device = fopen(optarg, "w+");
                if (!__celestial_log_device) {
                    perror(optarg);
                    return 1;
                }

                break;

            case 'h':
                usage();
                break;


            case 'v':
                version();
                break;
            
            case 'f':
                fake = 1;
                break;

            case '?':
            default:
                usage();
                break;
        }
    }

    // If there wasn't a log device, open one.
    if (!__celestial_log_device) __celestial_log_device = fopen("/device/kconsole", "w");
    CELESTIAL_LOG("celestial v %d.%d.%d\n", CELESTIAL_VERSION_MAJOR, CELESTIAL_VERSION_MINOR, CELESTIAL_VERSION_LOWER);

    if (!fake) {
        // Initialize the graphics context
        WM_GFX = gfx_createFullscreen(CTX_DEFAULT);
        if (!WM_GFX) {
            CELESTIAL_LOG("error: failed to create graphics context");
            fprintf(stderr, "celestial: Error creating graphics context\n");
            return 1;
        }

        gfx_clear(WM_GFX, GFX_RGB(255, 255, 255));
        gfx_render(WM_GFX);
    }


    WM_SW_MAP = hashmap_create_int("celestial map", 20);

    // Initialize windows
    window_init();
    CELESTIAL_DEBUG("Created windows successfully\n");

    // Initialize sockets
    socket_init();
    CELESTIAL_DEBUG("Created sockets successfully\n");

    // if (!fork()) {
    //     const char *a[] = { "/device/initrd/bin/wmtest", NULL };
    //     execvp("/device/initrd/bin/wmtest", a);
    // }

    // Enter main loop
    celestial_main();
}


/**
 * @brief Fatal error method
 */
void celestial_fatal() {
    CELESTIAL_LOG("FATAL ERROR DETECTED - shutting down\n");
    // TODO
    exit(1);
}