/**
 * @file userspace/lib/include/ethereal/widget/menu.h
 * @brief Menu system
 * 
 * Provides context menus, like the ones you see in menubars and on right click
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_MENU_H
#define _WIDGET_MENU_H

/**** INCLUDES ****/
#include <ethereal/widget/widget.h>
#include <ethereal/widget/event.h>
#include <ethereal/celestial/window.h>
#include <structs/list.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

#define MENU_ITEM_DEFAULT       0
#define MENU_ITEM_SEPARATOR     1
#define MENU_ITEM_SUBMENU       2

/**** TYPES ****/


/**
 * @brief Menu click event
 * @param menu The menu that was clicked
 * @param context Additional context
 */
typedef void (*menu_click_callback_t)(widget_t*, void*);

typedef struct widget_menu_item {
    int type;
    char *text;
    sprite_t *icon;
    menu_click_callback_t callback;
} widget_menu_item_t;

typedef struct widget_menu {
    list_t *items;
    window_t *window;
} widget_menu_t;


/**** FUNCTIONS ****/

/**
 * @brief Create a new menu widget
 * @param frame The frame to attach the menu to
 */
widget_t *menu_create(widget_t *frame);

/**
 * @brief Add a new item to a menu
 * @param menu The menu to add to
 * @param text The text of the icon
 * @param icon (Optional) Icon path
 * @param callback On click callback
 */
void menu_addItem(widget_t *menu, char *text, char *icon, menu_click_callback_t callback);

/**
 * @brief Bind to right click of frame
 * @param menu Menu to bind
 * @param frame The frame to bind to right click of
 */
void menu_bind(widget_t *menu, widget_t *frame);

#endif