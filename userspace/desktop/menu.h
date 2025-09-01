/**
 * @file userspace/desktop/menu.h
 * @brief Ethereal menu system
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DESKTOP_MENU_H
#define _DESKTOP_MENU_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

/* Taskbar settings */
#define TASKBAR_HEIGHT              40

/* Menu width and height */
#define MENU_WIDTH                  300
#define MENU_HEIGHT                 465

/* Menu entry configs */
#define MENU_ENTRY_START_X          5
#define MENU_ENTRY_START_Y          10

#define MENU_ENTRY_WIDTH            290
#define MENU_ENTRY_HEIGHT           35

#define MENU_ENTRY_LARGE_COUNT      13

#define MENU_ENTRY_ICON_START_X     10
#define MENU_ENTRY_ICON_START_Y     5

#define MENU_ENTRY_TEXT_START_X     40
#define MENU_ENTRY_TEXT_START_Y     24

/* Menu colors */
#define MENU_ENTRY_COLOR_HIGHLIGHTED    GFX_RGB(0, 0, 255)
#define MENU_ENTRY_COLOR_UNHIGHLIGHTED  GFX_RGB(255, 255, 255)
#define MENU_ENTRY_TEXT_COLOR           GFX_RGB(0, 0, 0)

#define MENU_ENTRY_LIST_HEIGHT      (MENU_HEIGHT-MENU_ENTRY_START_Y-5)
#define MENU_ENTRY_LIST_COLOR       GFX_RGB(255, 255, 255)

/**** TYPES ****/

typedef struct menu_entry {
    char *name;                 // Name
    sprite_t *icon;             // Icon sprite
    char *exec;                 // Command to execute
    long lg_idx;                // Long index (very hacky and stupid way of ding things)
} menu_entry_t;


/**** FUNCTIONS ****/

/**
 * @brief Initialize the menu window
 */
void menu_init();

/**
 * @brief Show/hide the menu window
 * @param show 1 to show
 */
void menu_show(int show);

#endif