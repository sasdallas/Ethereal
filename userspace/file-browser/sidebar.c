/**
 * @file userspace/file-browser/sidebar.c
 * @brief Sidebar code for the file browser
 * 
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

sidebar_entry_t **entry_list = NULL;

#define SIDEBAR_ITEM_HEIGHT 26

void sidebar_init() {
    // temporary, fill with fake data
    entry_list = malloc(8 * sizeof(sidebar_entry_t));


    static sidebar_entry_t ent1 = { .type = SIDEBAR_TYPE_ITEM, .item_name = "Test1" };
    entry_list[0] = &ent1;
    static sidebar_entry_t ent2 = { .type = SIDEBAR_TYPE_SUBMENU, .item_name = "TestSubmenu"};
    entry_list[1] = &ent2;
    static sidebar_entry_t ent3 = { .type = SIDEBAR_TYPE_ITEM, .item_name = "TestSubitem1"};
    entry_list[2] = &ent3;
    static sidebar_entry_t ent4 = { .type = SIDEBAR_TYPE_ITEM, .item_name = "TestSubitem2" };
    entry_list[3] = &ent4;
    static sidebar_entry_t ent5 = { .type = SIDEBAR_TYPE_SUBMENU_END};
    entry_list[4] = &ent5;
    static sidebar_entry_t ent6 = { .type = SIDEBAR_TYPE_SEPARATOR };
    entry_list[5] = &ent6;
    static sidebar_entry_t ent7 = { .type = SIDEBAR_TYPE_ITEM, .item_name = "Test2"};
    entry_list[6] = &ent7;
    entry_list[7] = NULL;
}

void sidebar_render() {
    int y = 2;

    sidebar_entry_t **entl = entry_list;
    while (*entl++) {
        // sidebar_entry_t *ent = *entl++;
        gfx_setFontSize(fb_main_font, 12);

        gfx_drawRoundedRectangle(fb_ctx_sidebar, &GFX_RECT(2, y, GFX_WIDTH(fb_ctx_sidebar)-4, SIDEBAR_ITEM_HEIGHT), 4, GFX_RGB(0, 0, 255));

        gfx_renderSprite(fb_ctx_sidebar, icon_missing, 4, y+(26-16)/2);
        gfx_renderString(fb_ctx_sidebar, fb_main_font, "Testing", 22, y+(26-14)/2+12, GFX_RGB(0,0,0));
        y += SIDEBAR_ITEM_HEIGHT + 2;
    }
}