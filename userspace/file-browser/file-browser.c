/**
 * @file userspace/file-browser/file-browser.c
 * @brief File browser source code
 * 
 * The file browser is split into 3 parts:
 * - Topbar, with the address bar and options
 * - Sidebar, with the file view system
 * - Content pane, with the actual files themselves
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "sidebar.h"
#include "file-browser.h"
#include "content.h"
#include "topbar.h"
#include <ethereal/celestial.h>
#include <assert.h>
#include <ethereal/widget.h>
#include <graphics/gfx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

window_t *fb_main_window = NULL;
gfx_context_t *fb_ctx = NULL;
gfx_font_t *fb_main_font = NULL;
gfx_font_t *fb_bold_font = NULL;

gfx_context_t *fb_ctx_topbar = NULL;
gfx_context_t *fb_ctx_sidebar = NULL;
gfx_context_t *fb_ctx_main_view = NULL;
sprite_t *icon_missing = NULL;
sprite_t *icon_missing_24 = NULL;

#define MERCURY_START GFX_RGB(0x3f, 0x3b, 0x42)
#define MERCURY_END  GFX_RGB(0x95, 0x90, 0x99)

#define FB_DBG(...) printf("file-browser: " __VA_ARGS__)

#define REGION_UNK -1
#define REGION_TOPBAR 0
#define REGION_SIDEBAR 1
#define REGION_MAIN 2

int mregion = REGION_UNK;

void usage() {
    printf("Usage: file-browser [-h] [-v] [DIR]\n");
    printf("File browser\n\n");
    printf(" -h, --help         Display this help message\n");
    printf(" -v, --version      Print the version of file-browser\n\n");
    printf(" -p, --picker       Run the file-browser as a picker (it will print whatever you type)\n");
    exit(1);
}

void version() {
    printf("file-browser version 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

void fb_redraw() {
    // First, clear the entire context
    gfx_clear(fb_ctx, GFX_RGB(0xfb,0xfb,0xfb));
    
    // Initiate subcontexts
    fb_ctx_topbar = gfx_createContextSubrect(fb_ctx, &GFX_RECT(0, 0, GFX_WIDTH(fb_ctx), 35));
    fb_ctx_sidebar = gfx_createContextSubrect(fb_ctx, &GFX_RECT(0, 35, 200, GFX_HEIGHT(fb_ctx)-35));
    fb_ctx_main_view = gfx_createContextSubrect(fb_ctx, &GFX_RECT(202, 35, GFX_WIDTH(fb_ctx)-202, GFX_HEIGHT(fb_ctx)-35));

    gfx_drawRectangleFilledGradient(fb_ctx_topbar, &GFX_RECT(0,0,GFX_WIDTH(fb_ctx_topbar), GFX_HEIGHT(fb_ctx_topbar)), GFX_GRADIENT_HORIZONTAL, MERCURY_START, MERCURY_END);
    gfx_drawRectangleFilled(fb_ctx, &GFX_RECT(200, 35, 2, GFX_HEIGHT(fb_ctx)-35), GFX_RGB(75,75,75));

    // Render
    sidebar_render();
    content_render();
    topbar_render();

    gfx_render(fb_ctx_topbar);
    gfx_render(fb_ctx_sidebar);
    gfx_render(fb_ctx_main_view);

    celestial_flip(fb_main_window);
}

void onexit(int region) {
    if (region == REGION_TOPBAR) return;
    else if (region == REGION_SIDEBAR) return;
    else if (region == REGION_MAIN) content_mouseExit();
}

void onenter(int region, int mx, int my) {
    if (region == REGION_TOPBAR) {
        // int rx = mx;
        // int ry = my;
    } else if (region == REGION_SIDEBAR) {
        // int rx = mx;
        // int ry = my - 35;
    } else if (region == REGION_MAIN) {
        int rx = mx - 202;
        int ry = my - 35;
        content_mouseEnter(rx, ry);
    }

    mregion = region;
}

void onmoved(int mx, int my) {
    if (mregion == REGION_TOPBAR) {
        // int rx = mx;
        // int ry = my;
    } else if (mregion == REGION_SIDEBAR) {
        // int rx = mx;
        // int ry = my - 35;
    } else if (mregion == REGION_MAIN) {
        int rx = mx - 202;
        int ry = my - 35;
        content_mouseMotion(rx, ry);
    }
}


void mouse_event_handler(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_MOUSE_ENTER) {
        // Find the region it entered into
        celestial_event_mouse_enter_t *ev = (celestial_event_mouse_enter_t*)event;
        if (ev->x >= 0 && ev->x < (int32_t)GFX_WIDTH(fb_ctx) && ev->y >= 0 && ev->y < 35) {
            // Topbar
            if (mregion != REGION_TOPBAR) { onexit(mregion); onenter(REGION_TOPBAR, ev->x, ev->y); }
        } else if (ev->x >= 0 && ev->x < 200 && ev->y >= 35 && ev->y < (int32_t)GFX_HEIGHT(fb_ctx)) {
            // Sidebar
            if (mregion != REGION_SIDEBAR) { onexit(mregion); onenter(REGION_SIDEBAR, ev->x, ev->y); }
        } else if (ev->x >= 202 && ev->x < (int32_t)GFX_WIDTH(fb_ctx) && ev->y >= 35 && ev->y < (int32_t)GFX_HEIGHT(fb_ctx)) {
            // Content view
            if (mregion != REGION_MAIN) { onexit(mregion); onenter(REGION_MAIN, ev->x, ev->y); }
        } else {
            // Unknown region
            if (mregion != REGION_UNK) { onexit(mregion); onenter(REGION_UNK, ev->x, ev->y); }
        }
    } else if (event_type == CELESTIAL_EVENT_MOUSE_EXIT) {
        onexit(mregion);
        mregion = REGION_UNK;
    } else if (event_type == CELESTIAL_EVENT_MOUSE_MOTION) {
        // Check if we moved out of a region
        celestial_event_mouse_motion_t *ev = (celestial_event_mouse_motion_t*)event;
        if (ev->x >= 0 && ev->x < (int32_t)GFX_WIDTH(fb_ctx) && ev->y >= 0 && ev->y < 35) {
            // Topbar
            if (mregion != REGION_TOPBAR) { onexit(mregion); onenter(REGION_TOPBAR, ev->x, ev->y); }
        } else if (ev->x >= 0 && ev->x < 200 && ev->y >= 35 && ev->y < (int32_t)GFX_HEIGHT(fb_ctx)) {
            // Sidebar
            if (mregion != REGION_SIDEBAR) { onexit(mregion); onenter(REGION_SIDEBAR, ev->x, ev->y); }
        } else if (ev->x >= 202 && ev->x < (int32_t)GFX_WIDTH(fb_ctx) && ev->y >= 35 && ev->y < (int32_t)GFX_HEIGHT(fb_ctx)) {
            // Content view
            if (mregion != REGION_MAIN) { onexit(mregion); onenter(REGION_MAIN, ev->x, ev->y); }
        } else {
            // Unknown region
            if (mregion != REGION_UNK) { onexit(mregion); onenter(REGION_UNK, ev->x, ev->y); }
        }

        onmoved(ev->x, ev->y);
    } else if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN) {
        celestial_event_mouse_button_down_t *ev = (celestial_event_mouse_button_down_t*)event;
        if (mregion == REGION_TOPBAR) {

        } else if (mregion == REGION_SIDEBAR) {
        } else if (mregion == REGION_MAIN) {
            int rx = ev->x - 202;
            int ry = ev->y - 35;
            content_mouseButton(ev->held, rx, ry);
        }
         
    }
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v' },
        { .name = "picker", .flag = NULL, .has_arg = no_argument, .val = 'p', },
        { 0, 0, 0, 0 }
    };

    int c;
    int index;
    while ((c = getopt_long(argc, argv, "hv", options, &index)) != -1) {
        if (!c) c = options[index].val;

        switch (c) {
            case 'v':
                version();
                break;
            
            case 'p':  
                printf("not yet\n");
                return 1;
            
            case 'h':
            default:
                usage();
                break;
        }
    }

    wid_t wid = celestial_createWindow(CELESTIAL_WINDOW_FLAG_EXIT_ON_CLOSE, 800, 600);
    if (wid < 0) {
        fprintf(stderr, "celestial_createWindow failed: %s\n", strerror(errno));
        return 1;
    }

    fb_main_window = celestial_getWindow(wid);
    if (fb_main_window == NULL) {
        fprintf(stderr, "celestial_getWindow(%d) failed: %s\n", wid, strerror(errno));
    }

    celestial_setTitle(fb_main_window, "File Browser");
    fb_ctx = celestial_getGraphicsContext(fb_main_window);

    // Load fonts
    fb_main_font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");
    if (!fb_main_font) {
        fprintf(stderr, "/usr/share/DejaVuSans.ttf: %s\n", strerror(errno));
        return 1;
    }

    fb_bold_font = gfx_loadFont(NULL, "/usr/share/DejaVuSans-Bold.ttf");
    if (!fb_bold_font) {
        fprintf(stderr, "/usr/share/DejaVuSans-Bold.ttf: %s\n", strerror(errno));
        return 1;
    }

    // Load icon
    icon_missing = gfx_createSprite(0,0);
    FILE *i = fopen("/usr/share/icons/16/Missing.bmp", "r");
    assert(i);
    gfx_loadSprite(icon_missing, i);
    fclose(i);

    icon_missing_24 = gfx_createSprite(0,0);
    i = fopen("/usr/share/icons/24/Missing.bmp", "r");
    assert(i);
    gfx_loadSprite(icon_missing_24, i);
    fclose(i);

    // Init
    collector_collectFiles();
    topbar_init();
    sidebar_init();
    content_init();

    // Event
    celestial_setHandler(fb_main_window, CELESTIAL_EVENT_MOUSE_ENTER, mouse_event_handler);
    celestial_setHandler(fb_main_window, CELESTIAL_EVENT_MOUSE_EXIT, mouse_event_handler);
    celestial_setHandler(fb_main_window, CELESTIAL_EVENT_MOUSE_MOTION, mouse_event_handler);
    celestial_setHandler(fb_main_window, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, mouse_event_handler);
    celestial_setHandler(fb_main_window, CELESTIAL_EVENT_MOUSE_BUTTON_UP, mouse_event_handler);

    // Redraw!
    fb_redraw();

    gfx_render(fb_ctx);
    celestial_flip(fb_main_window);

    celestial_mainLoop();


    return 0;
}