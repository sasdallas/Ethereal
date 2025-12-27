/**
 * @file userspace/file-browser/sidebar.h
 * @brief Sidebar code for menu
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _SIDEBAR_H
#define _SIDEBAR_H

#define SIDEBAR_TYPE_ITEM 0
#define SIDEBAR_TYPE_SEPARATOR 1
#define SIDEBAR_TYPE_SUBMENU 2
#define SIDEBAR_TYPE_SUBMENU_END 3

typedef struct sidebar_entry {
    unsigned char type;
    char *item_name;
    char *path;
} sidebar_entry_t;

void sidebar_init();
void sidebar_render();

#endif