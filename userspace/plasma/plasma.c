/**
 * @file userspace/plasma/plasma.c
 * @brief Plasma drwaer
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


#define CENTER      128
#define AMPLIFIER   127

gfx_color_t plasma(int x, int y, double t) {
    double value = 0.0;

    value += sin(x * 0.06 + t);
    value += sin(y * 0.07 + t * 1.3);
    value += sin((x + y) * 0.04 + t * 0.7);
    value += sin(sqrt(x * x + y * y) * 0.05 + t);

    // normalize
    value = (value + 4) / 8.0;
    uint32_t r = (uint32_t)(sin(value * M_PI) * AMPLIFIER + CENTER);
    uint32_t g = (uint32_t)(sin(value * M_PI + 2) * AMPLIFIER + CENTER);
    uint32_t b = (uint32_t)(sin(value * M_PI + 4) * AMPLIFIER + CENTER);
    return GFX_RGB(r, g, b);
}

int main(int argc, char *argv[]) {
    wid_t wid = celestial_createWindow(CELESTIAL_WINDOW_FLAG_SOLID, 300, 300);
    window_t *win = celestial_getWindow(wid);
    celestial_setTitle(win, "Plasma");

    double t = 0.0;

    gfx_context_t *ctx = celestial_getGraphicsContext(win);

    while (1) {
        celestial_poll();
        for (uint32_t y = 0; y < 300; y++) {
            for (uint32_t x = 0; x < 300; x++) {
                GFX_PIXEL(ctx, x, y) = plasma(x, y, t);
            }
        }

        gfx_render(ctx);
        celestial_flip(win);
        usleep(16000);
        t += 0.05;
    }

    return 0;
}