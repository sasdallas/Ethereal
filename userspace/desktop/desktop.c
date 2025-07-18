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

#include <ethereal/celestial.h>
#include <stdio.h>
#include <stdlib.h>
#include <graphics/gfx.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

/* Disable background drawing */
int disable_bg = 0;

/* Background window */
window_t *background_window = NULL;

/* Default startup app */
#define DEFAULT_STARTUP         "termemu"

/* Default wallpaper */
#define DEFAULT_WALLPAPER       "/usr/share/lines.bmp"

/* Current wallpaper sprite */
sprite_t *background_sprite = NULL;

/* Desktop taskbar window */
window_t *taskbar_window = NULL;

/* Fonts */
gfx_font_t *taskbar_font = NULL;

/* Taskbar active */
int menu_active = 0;

/* Menu rectangle */
gfx_rect_t menu_rect = { .x = 0, .y = 0, .width = 300, .height = 0 };

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
 * @brief Create menu gradient
 */
void create_menu_gradient() {
    gfx_color_t grad_start = GFX_RGB(0x66, 0x62, 0x6a);
    gfx_color_t grad_end = GFX_RGB(0x27, 0x24, 0x29);
    gfx_context_t *ctx = celestial_getGraphicsContext(background_window);

    size_t h = menu_rect.y + menu_rect.height;
    float step_a = (float)(GFX_RGB_A(grad_start) - GFX_RGB_A(grad_end)) / (float)h;
    float step_r = (float)(GFX_RGB_R(grad_start) - GFX_RGB_R(grad_end)) / (float)h;
    float step_g = (float)(GFX_RGB_G(grad_start) - GFX_RGB_G(grad_end)) / (float)h;
    float step_b = (float)(GFX_RGB_B(grad_start) - GFX_RGB_B(grad_end)) / (float)h;

    for (uintptr_t y = menu_rect.y; y < menu_rect.y + menu_rect.height; y++) {
        gfx_color_t color = 0 |
            ((uint8_t)(GFX_RGB_A(grad_start) - y * step_a) & 0xFF) << 24 |
            ((uint8_t)(GFX_RGB_R(grad_start) - y * step_r) & 0xFF) << 16 |
            ((uint8_t)(GFX_RGB_G(grad_start) - y * step_g) & 0xFF) << 8 |
            ((uint8_t)(GFX_RGB_B(grad_start) - y * step_b) & 0xFF) << 0;

        uintptr_t x = menu_rect.x;
        uintptr_t x_max = menu_rect.x + menu_rect.width;

        for (; x < x_max; x++) {
            GFX_PIXEL(ctx, x, y) = color;
        }
    }
}

/**
 * @brief Background method
 */
void create_background() {
    celestial_info_t *info = celestial_getServerInformation();
    if (!info) exit(1);

    wid_t bgwid = celestial_createWindowUndecorated(0, info->screen_width, info->screen_height);
    if (bgwid < 0) exit(1);

    background_window = celestial_getWindow(bgwid);
    if (!background_window) exit(1);

    // Set Z order
    celestial_setZArray(background_window, CELESTIAL_Z_BACKGROUND);

    // Unsubscribe from all events
    celestial_unsubscribe(background_window, 0xFFFFFFFF);

    gfx_context_t *bg_ctx = celestial_getGraphicsContext(background_window);

    // TODO: Allow wallpaper customization
    FILE *bg_file = fopen(DEFAULT_WALLPAPER, "r");
    if (!bg_file) {
        goto _fallback;
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
 * @brief Render the menu for programs + apps
 * @warning The menu system will be refactored in future versions of desktop
 */


/**
 * @brief Click event for the taskbar
 * @c menu_active controls whether the menu will spawn
 */
void mouse_event_taskbar(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN && win == taskbar_window) {
        celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t*)event;
        if (down->x >= 0 && down->x < 150) {
            menu_active ^= 1;
            
            gfx_context_t *ctx = celestial_getGraphicsContext(background_window);

            if (menu_active) {
                // Make it
                create_menu_gradient();
            } else {
                // Clear it
                gfx_renderSprite(ctx, background_sprite, 0, 0); // TODO: Only redraw needed part. Use clips?
            }

            gfx_render(ctx);
            celestial_flipRegion(background_window, menu_rect.x, menu_rect.y, menu_rect.width, menu_rect.height);
        }
    }
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        // { .name = "no-background", .has_arg = no_argument, .flag = NULL, .val = 'b' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h', },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
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

    // Create the background
    create_background();

    // Create menu rect
    menu_rect.height = 418;
    menu_rect.y = celestial_getServerInformation()->screen_height - 24 - menu_rect.height;

    // Create the taskbar window
    wid_t taskbar_wid = celestial_createWindowUndecorated(0, celestial_getServerInformation()->screen_width, 25);
    if (taskbar_wid < 0) return 1;
    taskbar_window = celestial_getWindow(taskbar_wid);
    if (!taskbar_window) return 1;
    celestial_setWindowPosition(taskbar_window, 0, celestial_getServerInformation()->screen_height - 24);

    // Set event
    celestial_setHandler(taskbar_window, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, mouse_event_taskbar);

    // Get taskbar + make gradient
    gfx_context_t *taskbar_ctx = celestial_getGraphicsContext(taskbar_window);
    create_taskbar_gradient(taskbar_ctx, 0);

    // Make taskbar button
    gfx_rect_t taskbar_btn_rect = { .x = 0, .y = 0, .width = 150, .height = 25 };
    gfx_drawRectangleFilled(taskbar_ctx, &taskbar_btn_rect, GFX_RGB(26, 24, 27));

    // Load the fonts
    taskbar_font = gfx_loadFont(taskbar_ctx, "/usr/share/DejaVuSans.ttf");

    // Fonts have been loaded, draw them in
    gfx_renderString(taskbar_ctx, taskbar_font, "Ethereal", 10, 17, GFX_RGB(255, 255, 255));
    gfx_render(taskbar_ctx);
    celestial_flip(taskbar_window);

    // Now launch the startup task
    pid_t cpid = fork();
    if (!cpid) {
        // Depends on whether we have an extra argv
        char *start = DEFAULT_STARTUP;
        if (argc-optind) {
            start = argv[optind];
        }       

        char *args[] = { start, NULL };
        execvp(start, (const char **)args);
        return 1;
    }

    char last_time[28] = { 0 };

    while (1) {

        // Update time string
        time_t current_time;
        struct tm *tm;
        char time_str[9];
        time(&current_time);
        tm = localtime(&current_time);
        strftime(time_str, 9, "%I:%M %p", tm);

        if (!last_time[0] || strcmp(last_time, time_str)) {
            strcpy(last_time, time_str);
            
            // Render the string
            gfx_renderString(taskbar_ctx, taskbar_font, time_str, celestial_getServerInformation()->screen_width - 100, 15, GFX_RGB(255, 255, 255));
            gfx_render(taskbar_ctx);
            celestial_flip(taskbar_window);

            // Cover up
            create_taskbar_gradient(taskbar_ctx, celestial_getServerInformation()->screen_width - 100);
        }

        celestial_poll();
    }

    return 0;
}