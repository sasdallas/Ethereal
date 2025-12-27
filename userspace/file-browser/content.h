/**
 * @file userspace/file-browser/content.h
 * @brief Content pane of the file browser
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CONTENT_H
#define _CONTENT_H

#include <stdint.h>

void content_init();
void content_render();

void content_mouseEnter(int mx, int my);
void content_mouseExit();
void content_mouseMotion(int rx, int ry);
void content_mouseButton(uint32_t buttons, int x, int y);

#endif