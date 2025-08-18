/**
 * @file userspace/desktop/menu.c
 * @brief Menu system for taskbar
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "menu.h"
#include <ethereal/celestial.h>
#include <stdio.h>
#include <stdlib.h>
#include <graphics/gfx.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <structs/ini.h>
#include <fcntl.h>
#include <dirent.h>

/* Menu window */
window_t *menu_window = NULL;

/* Menu graphics */
gfx_context_t *menu_ctx = NULL;

/* List of *all* menu entries */
list_t *menu_entries = NULL;

/* Large menu entries */
menu_entry_t *menu_large_entries[MENU_ENTRY_LARGE_COUNT] = { NULL };

/* Currently highlighted large menu entry */
menu_entry_t *highlighted_large_menu_entry = NULL;

/* Count of entries */
int menu_large_entry_count = 0;

/* Menu active */
extern int menu_active;

extern gfx_font_t *taskbar_font;

/**
 * @brief Draw large menu entry (expects ent->lg_idx to be set)
 * @param ent The entry to draw
 * @param highlighted Whether to draw it as highlighted
 */
void menu_drawLargeEntry(menu_entry_t *ent, int highlighted) {
    // Render it
    size_t ent_x = MENU_ENTRY_START_X;
    size_t ent_y = MENU_ENTRY_START_Y + 0 + (ent->lg_idx * MENU_ENTRY_HEIGHT);

    // Draw rectangle
    if (!highlighted) {   
        gfx_rect_t r = { .x = ent_x, .y = ent_y, .width = MENU_ENTRY_WIDTH, .height = MENU_ENTRY_HEIGHT };
        gfx_drawRectangleFilled(menu_ctx, &r, MENU_ENTRY_COLOR_UNHIGHLIGHTED);
    } else {
        gfx_rect_t r = { .x = ent_x, .y = ent_y, .width = MENU_ENTRY_WIDTH, .height = MENU_ENTRY_HEIGHT };
        gfx_drawRectangleFilledGradient(menu_ctx, &r, GFX_GRADIENT_HORIZONTAL, GFX_RGB(0x5d, 0x93, 0xcb), GFX_RGB(0x4f, 0x7a, 0xbf));
    }
    

    // Render the icon and the string
    gfx_renderSprite(menu_ctx, ent->icon, ent_x + MENU_ENTRY_ICON_START_X, MENU_ENTRY_ICON_START_Y + ent_y);
    gfx_setFontSize(taskbar_font, 14);
    gfx_renderString(menu_ctx, taskbar_font, ent->name, ent_x + MENU_ENTRY_TEXT_START_X, ent_y + MENU_ENTRY_TEXT_START_Y - 2, MENU_ENTRY_TEXT_COLOR);
    gfx_setFontSize(taskbar_font, 12);
}

/**
 * @brief Run the command registered with an entry
 * @param ent The entry to run the command for
 */
void menu_execute(menu_entry_t *ent) {
    if (!fork()) {
        // TODO: This is kinda stupid
        char *args[] = {
            "/usr/bin/essence",
            "-c",
            ent->exec,
            NULL
        };
        execvpe("/usr/bin/essence", args, environ);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Render the menu and enable it
 */
void menu_render() {
    gfx_createClip(menu_ctx, 0, 0, MENU_WIDTH, MENU_HEIGHT);
    gfx_drawRectangleFilledGradient(menu_ctx, &GFX_RECT(0, 0, MENU_WIDTH, MENU_HEIGHT), GFX_GRADIENT_VERTICAL, GFX_RGB(0x66, 0x62, 0x6a), GFX_RGB(0x27, 0x24, 0x29));


    // Draw the inner rect
    gfx_rect_t menu_inner_rect = { .x = 0 + MENU_ENTRY_START_X, .y = 0 + MENU_ENTRY_START_Y, .width = MENU_ENTRY_WIDTH, .height = MENU_ENTRY_LIST_HEIGHT };
    gfx_drawRectangleFilled(menu_ctx, &menu_inner_rect, MENU_ENTRY_LIST_COLOR);

    for (int i = 0; i < menu_large_entry_count; i++) {
        menu_drawLargeEntry(menu_large_entries[i], menu_large_entries[i] == highlighted_large_menu_entry);
    }

    gfx_render(menu_ctx);
    celestial_flip(menu_window);
}

/**
 * @brief Menu mouse callback
 * @param win The window the event occurred in
 * @param event_type The type of event that occurred
 * @param event The event object 
 */
void menu_mouseCallback(window_t *win, uint32_t event_type, void *event) {
    if (!menu_active) return;

    if (event_type == CELESTIAL_EVENT_MOUSE_MOTION) {
        celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t*)event;

        // In bounds?
        if (motion->x < MENU_ENTRY_START_X || motion->x > MENU_ENTRY_START_X + MENU_ENTRY_WIDTH) goto _unhighlight;
        if (motion->y < MENU_ENTRY_START_Y || motion->y > MENU_ENTRY_START_Y + MENU_ENTRY_LIST_HEIGHT) goto _unhighlight;

        // Calculate the entry that holds the cursor
        int32_t fixed_y = motion->y - MENU_ENTRY_START_Y;

        int idx = fixed_y / MENU_ENTRY_HEIGHT;
        if (idx >= menu_large_entry_count) goto _unhighlight;

        if (highlighted_large_menu_entry != menu_large_entries[idx]) {
            // We should switch
            if (highlighted_large_menu_entry) {
                menu_drawLargeEntry(highlighted_large_menu_entry, 0);
            }


            highlighted_large_menu_entry = menu_large_entries[idx];
            menu_drawLargeEntry(highlighted_large_menu_entry, 1);

            gfx_render(menu_ctx);
            celestial_flip(menu_window);
        }

        return;
        
_unhighlight:
        if (highlighted_large_menu_entry) {
            menu_drawLargeEntry(highlighted_large_menu_entry, 0);
            highlighted_large_menu_entry = NULL;
            gfx_render(menu_ctx);
            celestial_flip(menu_window);
        }
    } else if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN) {
        celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t *)event;
        if (down->held & CELESTIAL_MOUSE_BUTTON_LEFT && highlighted_large_menu_entry) {
            // Execute
            menu_execute(highlighted_large_menu_entry);
            menu_show(0);
        }
    }
}

/**
 * @brief Focus lost event
 */
void menu_unfocusCallback(window_t *win, uint32_t event_type, void *event) {
    menu_show(0);
}

/**
 * @brief Entry list sort function
 */
static int sort_fn(const void *s1, const void *s2) {
    return strcmp(*(const char**)s1, *(const char**)s2);
}

/**
 * @brief Initialize the menu window
 */
void menu_init() {
    int f = open("/device/log", O_RDWR);
    dup2(f, STDERR_FILENO);

    wid_t menu_wid = celestial_createWindowUndecorated(0, MENU_WIDTH, MENU_HEIGHT);
    menu_window = celestial_getWindow(menu_wid);
    celestial_setWindowPosition(menu_window, 0, celestial_getServerInformation()->screen_height - 40 - MENU_HEIGHT);
    menu_ctx = celestial_getGraphicsContext(menu_window);

    celestial_setHandler(menu_window, CELESTIAL_EVENT_MOUSE_MOTION, menu_mouseCallback);
    celestial_setHandler(menu_window, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, menu_mouseCallback);


    // Disabled originally
    menu_show(0);

    // Create menu entries list
    menu_entries = list_create("menu entries");
    memset(menu_large_entries, 0, sizeof(menu_entry_t*) * MENU_ENTRY_LARGE_COUNT);

    // First, we gather all the menu entries in /etc/desktop.d
    char **ent_list = malloc(sizeof(char*));
    ent_list[0] = NULL;
    size_t idx = 0;
    DIR *dirp = opendir("/etc/desktop.d");

    if (dirp) {
        struct dirent *ent = readdir(dirp);
        while (ent) {
            if (ent->d_name[0] == '.') {
                ent = readdir(dirp);
                continue;
            }

            ent_list[idx] = strdup(ent->d_name);
            idx++;
            ent_list = realloc(ent_list, sizeof(char*) * (idx + 1));
            ent_list[idx] = NULL;

            ent = readdir(dirp);
        }

        // Entry list has been built up, let's sort it
        if (idx) {
            qsort(ent_list, idx, sizeof(char*), sort_fn);
        }

        // Now iterate through each
        for (unsigned i = 0; i < idx; i++) {
            // Using ini, load it
            char path[128] = { 0 };
            snprintf(path, 128, "/etc/desktop.d/%s", ent_list[i]);
            ini_t *ini = ini_load(path);

            if (!ini) {
                fprintf(stderr, "Load failed for desktop entry: %s\n", ent_list[i]);
                continue;
            }

            menu_entry_t *ent = malloc(sizeof(menu_entry_t));
            ent->name = ini_get(ini, "Desktop", "Name");
            
            // Load the icon
            ent->icon = gfx_createSprite(0, 0);
            
            char *icon_path = ini_get(ini, "Desktop", "Icon");
            if (icon_path) {
                FILE *f = fopen(icon_path, "r");
            
                if (f) {
                    gfx_loadSprite(ent->icon, f);
                }
            }
            
            ent->exec = ini_get(ini, "Desktop", "Exec");
            ent->lg_idx = 0;
            list_append(menu_entries, ent);

            ini_destroy(ini);
        }
    }

    foreach(entry, menu_entries) {
        if (menu_large_entry_count >= MENU_ENTRY_LARGE_COUNT) break;

        // Get the entry
        menu_entry_t *ent = (menu_entry_t*)entry->value;
        ent->lg_idx = menu_large_entry_count;

        // Next entry
        menu_large_entries[menu_large_entry_count] = ent;
        menu_large_entry_count++;
    }
}

/**
 * @brief Show/hide the menu window
 * @param show 1 to show
 */
void menu_show(int show) {
    if (!show) {
        gfx_clear(menu_ctx, GFX_RGBA(0, 0, 0, 0));
        gfx_render(menu_ctx);
    } else {
        menu_render();
    }

    celestial_flip(menu_window);
    menu_active = show;
}