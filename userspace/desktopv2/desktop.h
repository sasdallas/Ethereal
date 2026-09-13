/**
 * @file userspace/desktopv2/desktop.h
 * @brief Desktop
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DESKTOP_H
#define _DESKTOP_H

#include <ethereal/celestial.h>
#include <graphics/gfx.h>
#include <structs/ini.h>
#include <errno.h>

#define APP_NAME "desktop"
#include <ethereal/log.h>

#define APP_BTN_X 10
#define APP_BTN_Y ((DESKTOP->taskbar_window->height - DESKTOP->ethereal_logo->height) / 2)

#define DESKTOP_MAJOR 2
#define DESKTOP_MINOR 0
#define DESKTOP_LOWER 0
#define DESKTOP_CFG(section, val, backup) ({ char *v = ini_get(DESKTOP->desktop_cfg, section, val); if (!v) { v = (backup); }; v; })

#define DESKTOP_FAILED(fn) ({ TRACE_ERROR("%s: %s\n", fn, strerror(errno)); desktop_fatal(); }) 

#define DESKTOP_ASSERT_PERROR(stmt) ({ if (!(stmt)) { DESKTOP_FAILED(#stmt); } })

/* primary desktop state */
typedef struct desktop {
    // Windows
    window_t *taskbar_window;
    window_t *bg_window;

    // Settings
    ini_t *desktop_cfg;
    char *wallpaper_path;
    int screen_width;
    int screen_height;
    bool show_info;
} desktop_t;

extern desktop_t __desktop;
#define DESKTOP (&__desktop)
#define TASKBAR (&DESKTOP->tb)

void desktop_fatal();

#endif
