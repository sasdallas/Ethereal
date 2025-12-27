/**
 * @file userspace/file-browser/topbar.c
 * @brief Topbar of the file explorer
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */


#include "topbar.h"
#include "file-browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ethereal/widget.h>

widget_t *input;

void topbar_mouseEnter(int mx, int my) {

}

void topbar_mouseExit() {}

void topbar_init() {
    input = input_create(NULL, INPUT_TYPE_DEFAULT, NULL, GFX_WIDTH(fb_ctx_topbar) - (10+(24*4)+(16*4)), 24);
}

void topbar_render() {
    gfx_renderSprite(fb_ctx_topbar, icon_missing_24, 10, 5);
    gfx_renderSprite(fb_ctx_topbar, icon_missing_24, 10+24+16, 5);
    gfx_renderSprite(fb_ctx_topbar, icon_missing_24, 10+24+24+16+16, 5);
    gfx_renderSprite(fb_ctx_topbar, icon_missing_24, 10+24+24+24+16+16+16, 5);
    input->render(input, fb_ctx_topbar, 10+(24*4)+(16*4), 5);
}