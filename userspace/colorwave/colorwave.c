/**
 * @file userspace/colorwave/colorwave.c
 * @brief Test for celestial
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <graphics/gfx.h>
#include <math.h>

#define FREQ       .05
#define PHASE1      00
#define PHASE2      02
#define PHASE3      04
#define CENTER      128

gfx_color_t color_gradient(int i, double t) {
    uint32_t r = sin(FREQ * i + (PHASE1) + t) * 127 + 128;
    uint32_t g = sin(FREQ * i + (PHASE2) + t) * 127 + 128;
    uint32_t b = sin(FREQ * i + (PHASE3) + t) * 127 + 128;

    return GFX_RGB(r, g, b);
}

int main(int argc, char *argv[]) {
    wid_t wid = celestial_createWindow(CELESTIAL_WINDOW_FLAG_EXIT_ON_CLOSE, 300, 300);
    window_t *win = celestial_getWindow(wid);
    celestial_setTitle(win, "Color Wave");

    double t = 0.0;

    while (celestial_running()) {
        for (uint32_t y = 0; y < 150; y++) {
            celestial_poll();
            gfx_drawRectangleFilled(celestial_getGraphicsContext(win), &GFX_RECT(0, y*2, 300, 2), color_gradient(y, t));
        }

        gfx_render(celestial_getGraphicsContext(win));
        celestial_flip(win);
        usleep(16000);
        t += 0.05;
    }

    return 0;
}