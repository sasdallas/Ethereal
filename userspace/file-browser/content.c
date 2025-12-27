/**
 * @file userspace/file-browser/content.c
 * @brief Content pane 
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "content.h"
#include "file-browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define VIEW_MODE_GRID 1
#define VIEW_MODE_LIST 2
#define VIEW_MODE_TILES 3

static int LIST_DATE_MODIFIED_COL_LEN = 150;
static int LIST_SIZE_COL_LEN = 100;
static int LIST_NAME_COL_LEN = 0;

/* The current hovered tile */
int hovered_offset = -1;

/* The current selected */
int selected_offset = -1;

/* Current view mode */
int current_view_mode = VIEW_MODE_LIST;

/* Time of last click in seconds */
unsigned long last_click = 0;


static void render_file(file_entry_t *ent, int i);

int find_offset(int x, int y) {
    if (current_view_mode == VIEW_MODE_LIST) {
        return ((y / 24)) - 1;
    }
    return 0;
}

void content_mouseButton(uint32_t buttons, int x, int y) {
    if (buttons & CELESTIAL_MOUSE_BUTTON_LEFT) {
        int off = find_offset(x, y);
        if (off != selected_offset) {
            int old = selected_offset;
            selected_offset = off;
            render_file(collector_get(old), old);
            render_file(collector_get(selected_offset), selected_offset);
            gfx_render(fb_ctx_main_view);
            celestial_flip(fb_main_window);

            if (collector_get(selected_offset)) {
                struct timeval t;
                gettimeofday(&t, NULL);
                last_click = (t.tv_sec * 1000 + t.tv_usec / 1000);
            }
        } else if (off == selected_offset) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            uint64_t now = (tv.tv_sec * 1000 + tv.tv_usec / 1000);
            if (now - last_click < 2000) {
                file_entry_t *ent = collector_get(selected_offset);
                if (ent) {
                    printf("Opening %s\n", ent->file_name);
                    if (S_ISDIR(ent->st.st_mode)) {
                        // Switch directories
                        selected_offset = -1;
                        chdir(ent->file_name);
                        collector_collectFiles();
                        fb_redraw();
                    }
                }

                last_click = 0;
            }
        }
    }
}

void content_mouseMotion(int rx, int ry) {
    if (ry < 24) return;
    int offset = find_offset(rx, ry);
    if (offset != hovered_offset) {
        int old = hovered_offset;
        hovered_offset = offset;
        render_file(collector_get(old), old);
        render_file(collector_get(hovered_offset), hovered_offset);
        gfx_render(fb_ctx_main_view);
        celestial_flip(fb_main_window);
    }
}

void content_mouseEnter(int mx, int my) {
}

void content_mouseExit() {
    int old = hovered_offset;
    hovered_offset = -1;

    if (old != hovered_offset) {
        render_file(collector_get(old), old);
        gfx_render(fb_ctx_main_view);
        celestial_flip(fb_main_window);
    }
}

static void render_file_list(file_entry_t *ent, int i) {
    printf("Rendering %s\n", ent->file_name);

    int offy = i / 1;
    int offx = i % 1;

    int x = offx * 100;
    int y = (offy *  24) + 24;

    gfx_drawRectangleFilled(fb_ctx_main_view, &GFX_RECT(x + 2, y + 2, GFX_WIDTH(fb_ctx_main_view) - 4, 24 - 4), GFX_RGB(0xfb,0xfb,0xfb));

    gfx_color_t text = GFX_RGB(0,0,0);


    if (i == selected_offset) {
        gfx_drawRoundedRectangle(fb_ctx_main_view, &GFX_RECT(x + 2, y + 2, GFX_WIDTH(fb_ctx_main_view) - 4, 24 - 4), 5, GFX_RGB(0x93,0x18,0xE4));
        gfx_drawRoundedRectangleGradient(fb_ctx_main_view, &GFX_RECT(x + 3, y + 3, GFX_WIDTH(fb_ctx_main_view) - 6, 24 - 6), 5, GFX_GRADIENT_VERTICAL, GFX_RGB(0xcd, 0x27, 0xf2), GFX_RGB(0xa6, 0x28, 0xfa));
        text = GFX_RGB(255,255,255);
    } else if (i == hovered_offset) {
        gfx_drawRoundedRectangle(fb_ctx_main_view, &GFX_RECT(x + 2, y + 2, GFX_WIDTH(fb_ctx_main_view) - 4, 24 - 4), 3, GFX_RGB(180,180,180));
        gfx_drawRoundedRectangle(fb_ctx_main_view, &GFX_RECT(x + 3, y + 3, GFX_WIDTH(fb_ctx_main_view) - 6, 24 - 6), 4, GFX_RGB(255,255,255));
    }

    gfx_renderSprite(fb_ctx_main_view, icon_missing, x + 4, y + 4);
    gfx_setFontSize(fb_main_font, 13);
    gfx_renderString(fb_ctx_main_view, fb_main_font, ent->file_name, x + 24, y + 17, text);
}

static void render_file(file_entry_t *ent, int i) {
    if (ent == NULL) return;

    if (current_view_mode == VIEW_MODE_LIST) {
        render_file_list(ent, i);
    } else if (current_view_mode == VIEW_MODE_GRID) {

    } else if (current_view_mode == VIEW_MODE_TILES) {

    }
    
}


static void content_renderListView() {
    LIST_NAME_COL_LEN = GFX_WIDTH(fb_ctx_main_view) - LIST_DATE_MODIFIED_COL_LEN - LIST_SIZE_COL_LEN - 2 - 2;

    gfx_drawRectangleFilled(fb_ctx_main_view, &GFX_RECT(LIST_NAME_COL_LEN, 0, 2, 20), GFX_RGB(0xDB, 0xDB, 0xDB));
    gfx_drawRectangleFilled(fb_ctx_main_view, &GFX_RECT(LIST_NAME_COL_LEN+2+LIST_DATE_MODIFIED_COL_LEN, 0, 2, 20), GFX_RGB(0xDB, 0xDB, 0xDB));

    gfx_setFontSize(fb_main_font, 11);
    gfx_renderString(fb_ctx_main_view, fb_main_font, "Name", 4, 14, GFX_RGB(0x7b, 0x7b, 0x7b));
    gfx_renderString(fb_ctx_main_view, fb_main_font, "Date modified", LIST_NAME_COL_LEN+2+4, 14, GFX_RGB(0x7b,0x7b,0x7b));
    gfx_renderString(fb_ctx_main_view, fb_main_font, "Size", LIST_NAME_COL_LEN+2+LIST_DATE_MODIFIED_COL_LEN+2+4, 14, GFX_RGB(0x7b,0x7b,0x7b));

    int i = 0;
    file_entry_t **fl = collector_getFileList();
    while (*fl) {
        file_entry_t *ent = *(file_entry_t**)fl;
        render_file_list(ent, i++);
        fl++;
    }
}

void content_init() {
}

void content_render() {
    if (current_view_mode == VIEW_MODE_LIST) {
        content_renderListView();
    }
}