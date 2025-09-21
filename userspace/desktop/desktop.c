/**
 * @file userspace/desktop/desktop.c
 * @brief Main desktop interface of Ethereal
 * 
 * The desktop interface! Handles the system clock, background, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "menu.h"
#include "widget.h"
#include <ethereal/celestial.h>
#include <stdio.h>
#include <stdlib.h>
#include <graphics/gfx.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <structs/ini.h>
#include <sys/signal.h>
#include <errno.h>
#include <poll.h>

/* Disable background drawing */
int disable_bg = 0;

/* Background window */
window_t *background_window = NULL;

/* Default startup app */
#define DEFAULT_STARTUP         "termemu"

/* Default wallpaper */
#define DEFAULT_WALLPAPER       "/usr/share/wallpapers/lines.bmp"

/* Current wallpaper sprite */
sprite_t *background_sprite = NULL;

/* Desktop taskbar window */
window_t *taskbar_window = NULL;

/* Fonts */
gfx_font_t *taskbar_font = NULL;

/* Taskbar active */
int menu_active = 0;


/* Current wallpaper */
char *wallpaper = DEFAULT_WALLPAPER;

void config_load();

/**
 * @brief User frame callback
 */
static void anim_uframe(gfx_context_t *ctx, gfx_anim_t *anim) {
    gfx_render(ctx);
    celestial_flip(background_window);
} 


/**
 * @brief User frame callback
 */
static void anim_end(gfx_context_t *ctx, gfx_anim_t *anim) {
    gfx_destroySprite(anim->sprite);
} 

/**
 * @brief Reload signal
 * 
 * Desktop uses SIGUSR2 to signal a reload
 */
void reload_signal(int signum) {
    // Refresh signal means we should check /tmp/wallpaper to see if it 1. exists and 2. has a valid wallpaper
    fprintf(stderr, "Reloading desktop environment\n");
    FILE *wallpaper_file = fopen("/tmp/wallpaper", "r");
    if (wallpaper_file) {
        char tmp_buffer[256] = { 0 };
        fread(tmp_buffer, 256, 1, wallpaper_file);
        wallpaper = strdup(tmp_buffer); 
        fclose(wallpaper_file);
    }

    gfx_context_t *bg_ctx = celestial_getGraphicsContext(background_window);

    fprintf(stderr, "Loading wallpaper: %s\n", wallpaper);
    FILE *bg_file = fopen(wallpaper, "r");
    if (!bg_file) {
        fprintf(stderr, "Error loading wallpaper %s, fallback to default wallpaper\n", wallpaper);

        bg_file = fopen(DEFAULT_WALLPAPER, "r");
        if (!bg_file) goto _fallback;
    }

    // Load from file
    sprite_t *new = gfx_createSprite(0, 0);

    if (gfx_loadSprite(new, bg_file)) {
        fclose(bg_file);
        goto _fallback;
    }

    fclose(bg_file);
    background_sprite = new;
    gfx_renderSprite(bg_ctx, background_sprite, 0, 0); // TODO: SCALING!
    gfx_render(bg_ctx);
    celestial_flip(background_window);
    return;

_fallback:
    gfx_clear(bg_ctx, GFX_RGB(0, 0, 0));
    gfx_render(bg_ctx);
    celestial_flip(background_window);
}

/**
 * @brief Usage
 */
void usage() {
    printf("Usage: desktop [-b] [PROGRAM]\n");
    printf("Main Ethereal desktop interface, providing the background, system clock, etc.\n");
    // printf(" -b, --no-background        Disable the background entirely\n");
    printf(" -h, --help                 Display this help message\n");
    printf(" -v, --version              Display the version of desktop\n");
    exit(1);
}

/**
 * @brief Version
 */
void version() {
    printf("desktop version 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Create taskbar gradient
 * @param ctx The context to draw the taskbar gradient in
 * @param start_x The starting X to use (for covering things up)
 */
void create_taskbar_gradient(gfx_context_t *ctx, uint16_t start_x) {
    gfx_color_t grad_start = GFX_RGB(0x66, 0x62, 0x6a);
    gfx_color_t grad_end = GFX_RGB(0x27, 0x24, 0x29);

    float step_a = (float)(GFX_RGB_A(grad_start) - GFX_RGB_A(grad_end)) / (float)GFX_HEIGHT(ctx);
    float step_r = (float)(GFX_RGB_R(grad_start) - GFX_RGB_R(grad_end)) / (float)GFX_HEIGHT(ctx);
    float step_g = (float)(GFX_RGB_G(grad_start) - GFX_RGB_G(grad_end)) / (float)GFX_HEIGHT(ctx);
    float step_b = (float)(GFX_RGB_B(grad_start) - GFX_RGB_B(grad_end)) / (float)GFX_HEIGHT(ctx);

    for (uintptr_t y = 0; y < GFX_HEIGHT(ctx); y++) {
        gfx_color_t color = 0 |
            ((uint8_t)(GFX_RGB_A(grad_start) - y * step_a) & 0xFF) << 24 |
            ((uint8_t)(GFX_RGB_R(grad_start) - y * step_r) & 0xFF) << 16 |
            ((uint8_t)(GFX_RGB_G(grad_start) - y * step_g) & 0xFF) << 8 |
            ((uint8_t)(GFX_RGB_B(grad_start) - y * step_b) & 0xFF) << 0;

        uintptr_t x = start_x;
        uintptr_t x_max = GFX_WIDTH(ctx);

        for (; x < x_max; x++) {
            GFX_PIXEL(ctx, x, y) = color;
        }
    }
}

/**
 * @brief Load configuration method
 */
void config_load() {
    ini_t *ini = ini_load("/etc/desktop.ini");
    if (!ini) {
        fprintf(stderr, "Error loading /etc/desktop.ini\n");
        return; // Hopefully default values are enough
    }


    // Load sections
    char *wp = ini_get(ini, "wallpaper", "file");
    if (wp) wallpaper = wp;
    else fprintf(stderr, "Missing directive: section=\"wallpaper\" value=\"file\"\n");

    // TODO: More stuff
    hashmap_free(ini->sections);
    free(ini);
}


/**
 * @brief Background method
 */
void create_background() {
    celestial_info_t *info = celestial_getServerInformation();
    if (!info) {
        perror("celestial_getServerInformation");
        exit(1);
    }

    wid_t bgwid = celestial_createWindowUndecorated(0, info->screen_width, info->screen_height);
    if (bgwid < 0) {
        perror("celestial_createWindowUndecorated");
        exit(1);
    }

    background_window = celestial_getWindow(bgwid);
    if (!background_window) {
        perror("celestial_getWindow");
        exit(1);
    }

    // Set Z order
    celestial_setZArray(background_window, CELESTIAL_Z_BACKGROUND);

    // Unsubscribe from all events
    celestial_unsubscribe(background_window, 0xFFFFFFFF);

    gfx_context_t *bg_ctx = celestial_getGraphicsContext(background_window);

    FILE *bg_file = fopen(wallpaper, "r");
    if (!bg_file) {
        fprintf(stderr, "Error loading wallpaper %s, fallback to default wallpaper\n", wallpaper);

        bg_file = fopen(DEFAULT_WALLPAPER, "r");
        if (!bg_file) goto _fallback;
    }

    // Load from file
    background_sprite = gfx_createSprite(0, 0);
    if (gfx_loadSprite(background_sprite, bg_file)) {
        fclose(bg_file);
        goto _fallback;
    }

    fclose(bg_file);
    gfx_renderSprite(bg_ctx, background_sprite, 0, 0); // TODO: SCALING!
    gfx_render(bg_ctx);
    celestial_flip(background_window);
    return;

_fallback:
    gfx_clear(bg_ctx, GFX_RGB(0, 0, 0));
    gfx_render(bg_ctx);
    celestial_flip(background_window);
}

/**
 * @brief Click event for the taskbar
 * @c menu_active controls whether the menu will spawn
 */
void mouse_event_taskbar(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN && win == taskbar_window) {
        celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t*)event;
        if (down->x >= 0 && down->x < 150 && down->held & CELESTIAL_MOUSE_BUTTON_LEFT) {
            menu_active ^= 1;
        
            // TODO: Menu system completion
            menu_show(menu_active);
        } else {
            if (down->held & CELESTIAL_MOUSE_BUTTON_LEFT) {
                widget_mouseClick(down->x, down->y);
            }
        }
    } else if (event_type == CELESTIAL_EVENT_MOUSE_MOTION && win == taskbar_window) {
        celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t*)event;
        widget_mouseMovement(motion->x, motion->y);
    } else if (event_type == CELESTIAL_EVENT_MOUSE_EXIT) {
        widget_mouseExit();
    } else if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_UP) {
        celestial_event_mouse_button_up_t *up = (celestial_event_mouse_button_up_t*)event;
        widget_mouseRelease(up->x, up->y);
    }
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h', },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "vh", (const struct option*)options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) {
            ch = options[index].val;
        }
        
        switch (ch) {
            case 'v':
                version();
                break;
            
            case 'h':
                usage();
                break;
        }
    }

    // Create helpful little file
    FILE *pid_file = fopen("/comm/desktop.pid", "w+");
    if (pid_file) {
        char buf[25] = { 0 };
        snprintf(buf, 25, "%ld", getpid());
        fwrite(buf, strlen(buf), 1, pid_file);
        fclose(pid_file);
    }

    // Set reload signal
    signal(SIGUSR2, reload_signal);

    // Load config
    config_load();

    // Create the background
    create_background();

    // Create the taskbar window
    wid_t taskbar_wid = celestial_createWindowUndecorated(0, celestial_getServerInformation()->screen_width, TASKBAR_HEIGHT);
    if (taskbar_wid < 0) {
        fprintf(stderr, "desktop: Create window failed with error %s\n", strerror(errno));
        return 1;
    }

    taskbar_window = celestial_getWindow(taskbar_wid);
    if (!taskbar_window) return 1;
    celestial_setWindowPosition(taskbar_window, 0, celestial_getServerInformation()->screen_height - TASKBAR_HEIGHT);

    // Set event
    celestial_setHandler(taskbar_window, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, mouse_event_taskbar);
    celestial_setHandler(taskbar_window, CELESTIAL_EVENT_MOUSE_MOTION, mouse_event_taskbar);
    celestial_setHandler(taskbar_window, CELESTIAL_EVENT_MOUSE_EXIT, mouse_event_taskbar);
    celestial_setHandler(taskbar_window, CELESTIAL_EVENT_MOUSE_BUTTON_UP, mouse_event_taskbar);

    // Get taskbar + make gradient
    gfx_context_t *taskbar_ctx = celestial_getGraphicsContext(taskbar_window);
    create_taskbar_gradient(taskbar_ctx, 0);

    // Load the fonts
    taskbar_font = gfx_loadFont(taskbar_ctx, "/usr/share/DejaVuSans.ttf");

    sprite_t *start_btn = gfx_createSprite(0, 0);
    gfx_loadSprite(start_btn, fopen("/usr/share/EtherealStartButton.bmp", "r"));
    gfx_renderSprite(taskbar_ctx, start_btn, 10, 4);

    // Fonts have been loaded, draw them in
    gfx_render(taskbar_ctx);
    celestial_flip(taskbar_window);

    // Init menu
    menu_init();

    // Now launch the startup task
    pid_t cpid = fork();
    if (!cpid) {
        // Depends on whether we have an extra argv
        char *start = DEFAULT_STARTUP;
        if (argc-optind) {
            start = argv[optind];
        }       

        char *args[] = { start, NULL };
        execvp(start, args);
        return 1;
    }

    // Load the widgets
    widgets_load();
    
    // Launch toast-server
    cpid = fork();
    if (!cpid) {
        char *start = "toast-server";
        if (argc-optind) {
            start = argv[optind];
        }       

        char *args[] = { start, NULL };
        execvp(start, args);
        return 1;
    }

    // Say hi!
    system("show-toast --text=\"Welcome to Ethereal!\nThank you for supporting development!\" --title=\"Welcome to Ethereal\"");

    while (1) {
        struct pollfd fds[] = {{ .fd = celestial_getSocketFile(), .events = POLLIN }};
        int p = poll(fds, 1, 1000);
        if (!p) continue;

        create_taskbar_gradient(taskbar_ctx, 0);
        celestial_poll();

        widgets_update();

        if (taskbar_ctx->clip) {
            celestial_flip(taskbar_window);
        }
    }

    return 0;
}