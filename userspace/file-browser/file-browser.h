/**
 * @file userspace/file-browser/file-browser.h
 * @brief File browser
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _FILE_BROWSER_H
#define _FILE_BROWSER_H

#include <graphics/gfx.h>
#include <ethereal/celestial.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct file_entry {
    char file_name[256];
    char icon[256];
    struct stat st;
} file_entry_t;

#define PRINT_ERROR_AND_DIE(fn) ({ fprintf(stderr, "%s:%d (in function %s): %s: %s", __FILE__, __LINE__, __FUNCTION__, (fn), strerror(errno)); exit(1); })

extern gfx_context_t *fb_ctx_topbar;
extern gfx_context_t *fb_ctx_sidebar;
extern gfx_context_t *fb_ctx_main_view;
extern window_t *fb_main_window;

extern gfx_font_t *fb_main_font;
extern gfx_font_t *fb_bold_font;
extern sprite_t *icon_missing;
extern sprite_t *icon_missing_24;

void collector_collectFiles();
file_entry_t **collector_getFileList();
file_entry_t *collector_get(int index);

void fb_redraw();

#endif