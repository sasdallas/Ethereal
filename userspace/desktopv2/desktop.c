/**
 * @file userspace/desktopv2/desktop.c
 * @brief Desktop application
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <ethereal/version.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <graphics/gfx.h>
#include <structs/ini.h>
#include <strings.h>
#include "desktop.h"
#include <getopt.h>
#include <unistd.h>

/* Desktop object */
desktop_t __desktop = { 0 };

void usage() {
    printf("Usage: desktop [-h] [-v]\n");
    printf("Main Ethereal desktop interface, providing the background, system clock, etc.\n");
    printf(" -h, --help                 Display this help message\n");
    printf(" -v, --version              Display the version of desktop\n");
    exit(1);
}

void version() {
    printf("desktop v%d.%d.%d\n", DESKTOP_MAJOR, DESKTOP_MINOR, DESKTOP_LOWER);
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(1);
}

void desktop_fatal() {
    TRACE_ERROR("Fatal error encountered.\n");
    exit(EXIT_FAILURE);
}

void desktop_draw_info() {
    if (!DESKTOP->show_info) return;

    const ethereal_version_t *ver = ethereal_getVersion();
    struct utsname utsname;
    uname(&utsname);

    window_t *bg_win = DESKTOP->bg_window;
    gfx_context_t *ctx = celestial_getGraphicsContext(DESKTOP->bg_window);
    static gfx_font_t *font = NULL;

    if (font == NULL) {
        font = gfx_loadFont(ctx, "/usr/share/fonts/DejaVuSans.ttf");
    }

    char str[256];
    gfx_setFontSize(font, 12);
    
    gfx_string_size_t ss;
    snprintf(str, 256, "Ethereal v%d.%d.%d", ver->version_major, ver->version_minor, ver->version_lower);
    gfx_getStringSize(font, str, &ss);
    gfx_renderStringShadow(ctx, font, str, bg_win->width - ss.width - 10, bg_win->height - 80, GFX_RGB(255, 255, 255), 1);
    snprintf(str, 256, "Codename \"%s\"", ver->codename);
    gfx_getStringSize(font, str, &ss);
    gfx_renderStringShadow(ctx, font, str, bg_win->width - ss.width - 10, bg_win->height - 65, GFX_RGB(255, 255, 255), 1);
    snprintf(str, 256, "Kernel: %s %s", utsname.sysname, utsname.release);
    gfx_getStringSize(font, str, &ss);
    gfx_renderStringShadow(ctx, font, str, bg_win->width - ss.width - 10, bg_win->height - 50, GFX_RGB(255, 255, 255), 1);
}

void desktop_init_bg(celestial_info_t *info) {
    wid_t wid = celestial_createWindowUndecorated(0, info->screen_width, info->screen_height);
    if (wid < 0) {
        DESKTOP_FAILED("celestial_createWindowUndecorated");
    }

    DESKTOP->bg_window = celestial_getWindow(wid);
    if (DESKTOP->bg_window == NULL) {
        DESKTOP_FAILED("celestial_getWindow");
    }

    DESKTOP_ASSERT_PERROR(celestial_setZArray(DESKTOP->bg_window, CELESTIAL_Z_BACKGROUND) == 0);
    DESKTOP_ASSERT_PERROR(celestial_setWindowPosition(DESKTOP->bg_window, 0, 0) == 0);

    gfx_context_t *ctx = celestial_getGraphicsContext(DESKTOP->bg_window);

    // Try to load the background
    FILE *bg_f = fopen(DESKTOP->wallpaper_path, "r");
    if (!bg_f) {
        TRACE_WARN("Failed to open %s: %s\n", DESKTOP->wallpaper_path, strerror(errno));
        bg_f = fopen("/usr/share/wallpapers/lines.bmp", "r");
        if (!bg_f) {
            TRACE_ERROR("Failed to open /usr/share/wallpapers/lines.bmp: %s\n", strerror(errno));
            goto _no_bg;
        }
    }

    sprite_t bg = {
        .width = 0,
        .height = 0,
        .bitmap = NULL,
        .alpha = SPRITE_ALPHA_SOLID
    };

    if (gfx_loadSprite(&bg, bg_f) != 0) {
        TRACE_WARN("Failed to load wallpaper sprite\n");
        goto _no_bg;
    }

    // Render bg
    gfx_renderSpriteScaled(ctx, &bg, GFX_RECT(0,0,info->screen_width,info->screen_height));

    desktop_draw_info();
    gfx_render(ctx);
    celestial_flip(DESKTOP->bg_window);
    free(bg.bitmap);

    return;

_no_bg:
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);
    celestial_flip(DESKTOP->bg_window);
}

void reload_signal(int signum) {
    TRACE_DEBUG("Reloading desktop environment.\n");

    FILE *wallpaper_file = fopen("/comm/wallpaper", "r");
    if (wallpaper_file) {
        char tmp_buffer[256] = { 0 };
        fread(tmp_buffer, 256, 1, wallpaper_file);
        free(DESKTOP->wallpaper_path);
        DESKTOP->wallpaper_path = strdup(tmp_buffer);
        fclose(wallpaper_file);
    }

    gfx_context_t *ctx = celestial_getGraphicsContext(DESKTOP->bg_window);

    TRACE_DEBUG("Loading wallpaper: %s\n", DESKTOP->wallpaper_path);

    // Try to load the background
    FILE *bg_f = fopen(DESKTOP->wallpaper_path, "r");
    if (!bg_f) {
        TRACE_WARN("Failed to open %s: %s\n", DESKTOP->wallpaper_path, strerror(errno));
        bg_f = fopen("/usr/share/wallpapers/lines.bmp", "r");
        if (!bg_f) {
            TRACE_ERROR("Failed to open /usr/share/wallpapers/lines.bmp: %s\n", strerror(errno));
            goto _no_bg;
        }
    }

    sprite_t bg = {
        .width = 0,
        .height = 0,
        .bitmap = NULL,
        .alpha = SPRITE_ALPHA_SOLID
    };

    if (gfx_loadSprite(&bg, bg_f) != 0) {
        TRACE_WARN("Failed to load wallpaper sprite\n");
        goto _no_bg;
    }

    fclose(bg_f);

    size_t size = GFX_SIZE(ctx);
    gfx_color_t *old_bg = malloc(size);
    gfx_color_t *new_bg = malloc(size);
    
    memcpy(old_bg, ctx->backbuffer, size);

    gfx_context_t new_ctx = *ctx;
    new_ctx.backbuffer = new_bg;
    gfx_renderSpriteScaled(&new_ctx, &bg, GFX_RECT(0,0,DESKTOP->screen_width,DESKTOP->screen_height));

    // super sick transition that took surprisingly long to implement
    sprite_t transition = {
        .width = DESKTOP->screen_width,
        .height = DESKTOP->screen_height,
        .bitmap = new_bg,
        .alpha = SPRITE_ALPHA_BLEND
    };

    for (int alpha = 16; alpha < 255; alpha += 16) {
        memcpy(ctx->backbuffer, old_bg, size);
        gfx_renderSpriteAlpha(ctx, &transition, 0, 0, alpha);
        desktop_draw_info();

        gfx_render(ctx);
        celestial_flip(DESKTOP->bg_window);

        usleep(25000);
    }
    
    memcpy(ctx->backbuffer, new_bg, size);
    desktop_draw_info();
    gfx_render(ctx);

    celestial_flip(DESKTOP->bg_window);

    free(old_bg);
    free(new_bg);
    free(bg.bitmap);

    return;

_no_bg:
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);
    celestial_flip(DESKTOP->bg_window);
}

int main(int argc, char *argv[]) {
    struct option opts[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v' },
        { 0,0,0,0 }
    };

    int optindex;
    char c;
    while ((c = getopt_long(argc, argv, "hv", (const struct option*)opts, &optindex)) != -1) {
        if (!c) c = opts[optindex].val;

        switch (c) {
            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    // hello!
    TRACE_INFO("desktop v%d.%d.%d\n", DESKTOP_MAJOR, DESKTOP_MINOR, DESKTOP_LOWER);

    // Config parser
    DESKTOP->desktop_cfg = ini_load("/etc/desktop.conf");
    if (!DESKTOP->desktop_cfg) {
        TRACE_ERROR("Failed to load /etc/desktop.conf. Bailing.\n");
        return 1;
    }

    // Load the settings
    DESKTOP->wallpaper_path = DESKTOP_CFG("desktop", "wallpaper", "/usr/share/wallpapers/EtherealWallpaper1.bmp");
    char *launch_path = DESKTOP_CFG("desktop", "launch_path", "termemu");

    // Begin!
    celestial_info_t *info = celestial_getServerInformation();
    if (!info) {
        DESKTOP_FAILED("celestial_getServerInformation");
    }

    DESKTOP->screen_width = info->screen_width;
    DESKTOP->screen_height = info->screen_height;
    TRACE_DEBUG("%dx%d screen\n", info->screen_width, info->screen_height);

    char *info_enable = DESKTOP_CFG("desktop", "show_build_info", "true");
    if (!strcasecmp(info_enable, "true")) {
        DESKTOP->show_info = true;
    } else {
        DESKTOP->show_info = false;
    }

    // Create windows
    desktop_init_bg(info);
    free(info);

    // Initialize the background switching runtime
    FILE *pid_file = fopen("/comm/desktop.pid", "w+");
    if (pid_file) {
        char buf[25] = { 0 };
        snprintf(buf, 25, "%ld", getpid());
        fwrite(buf, strlen(buf), 1, pid_file);
        fclose(pid_file);
    }

    signal(SIGUSR2, reload_signal);

    pid_t cpid = fork();
    if (cpid == 0) {
        char *argv[] = {"desktopmgr", NULL};
        execvp("desktopmgr", (char *const*)argv);
        TRACE_WARN("Failed to launch desktopmgr\n");
        exit(EXIT_FAILURE);
    }

    cpid = fork();
    if (cpid == 0) {
        char *argv[] = {"taskbar", NULL};
        execvp("taskbar", (char *const*)argv);
        TRACE_WARN("Failed to launch taskbar\n");
        exit(EXIT_FAILURE);  
    }

    // Launch the initial application
    cpid = fork();
    if (cpid == 0) {
        char *argv[] = {launch_path, NULL};
        execvp(launch_path, (char *const*)argv);
        TRACE_WARN("Failed to launch %s\n", launch_path);
        exit(EXIT_FAILURE);    
    }

    while (celestial_running()) {
        celestial_pollIndefinite();
    }

    TRACE_DEBUG("TODO: desktop_shutdown");
    return 0;
}
