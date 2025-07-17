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
#include <time.h>

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
 * @brief Main redraw function of Celestial
 */
void celestial_redraw() {
    // Redraw windows
    window_redraw();

    // Render the mouse
    mouse_render();

    // Render the graphics
    gfx_render(WM_GFX);
}

/**
 * @brief Main loop of Celestial
 */
void celestial_main() {
    while (1) {
        // Reset graphics clips
        gfx_resetClips(WM_GFX);

        // Accept new sockets
        socket_accept();

        // Update the mouse cursor (to make clips)
        mouse_update();

        // Update keyboard
        kbd_update();

        // Redraw
        // if (WM_GFX->clip || WM_UPDATE_QUEUE->length) {
            celestial_redraw();
        // }

        list_t *keys = hashmap_keys(WM_SW_MAP);
        foreach(kn, keys) {
            socket_handle((int)(uintptr_t)kn->value);
        }
        list_destroy(keys,false);
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

    while ((c = getopt_long(argc, argv, "dl:hvf", (const struct option*)longopts, &index)) != -1) {
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

    char *launch = "desktop";
    if (argc-optind) {
        launch = argv[optind];
    }

    // If there wasn't a log device, open one.
    if (!__celestial_log_device) {
        __celestial_log_device = fopen("/device/kconsole", "w");
    }

    dup2(__celestial_log_device->fd, STDOUT_FILENO);
    dup2(__celestial_log_device->fd, STDERR_FILENO);
        
    CELESTIAL_LOG("celestial v %d.%d.%d\n", CELESTIAL_VERSION_MAJOR, CELESTIAL_VERSION_MINOR, CELESTIAL_VERSION_LOWER);

    if (!fake) {
        // Initialize the graphics context
        WM_GFX = gfx_createFullscreen(CTX_DEFAULT);
        if (!WM_GFX) {
            CELESTIAL_LOG("error: failed to create graphics context");
            fprintf(stderr, "celestial: Error creating graphics context\n");
            return 1;
        }

        gfx_clear(WM_GFX, GFX_RGB(0,0,0));
        gfx_render(WM_GFX);
    }


    WM_SW_MAP = hashmap_create_int("celestial map", 20);

    // Initialize windows
    window_init();
    CELESTIAL_DEBUG("Created windows successfully\n");

    // Initialize sockets
    socket_init();
    CELESTIAL_DEBUG("Created sockets successfully\n");

    // Initialize mouse
    mouse_init();
    CELESTIAL_DEBUG("Created mouse successfully\n");

    // Initialize keyboard
    kbd_init();
    CELESTIAL_DEBUG("Created keyboard successfully\n");

    if (!fork()) {
        const char *a[] = { launch, NULL };
        execvp(launch, a);
        exit(1);
    }

    // Enter main loop
    celestial_main();
    __builtin_unreachable();
}


/**
 * @brief Fatal error method
 */
void celestial_fatal() {
    CELESTIAL_LOG("FATAL ERROR DETECTED - shutting down\n");
    // TODO
    exit(1);
}