/**
 * @file userspace/etherealver/etherealver.c
 * @brief Ethereal version message box
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
#include <stdio.h>
#include <graphics/gfx.h>
#include <sys/utsname.h>

int main(int argc, char *argv[]) {
    wid_t w = celestial_createWindow(0, 350, 250);
    window_t *win = celestial_getWindow(w);
    celestial_setTitle(win, "Ethereal Version");

    // Load font we need
    gfx_font_t *f = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");

    // Load sprite we need
    sprite_t *s = gfx_createSprite(0, 0);
    gfx_loadSprite(s, fopen("/usr/share/EtherealLogo.bmp", "r"));

    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    gfx_clear(ctx, GFX_RGB(0xFB, 0xFB, 0xFB));

    // Render logo and draw Ethereal text
    gfx_renderSprite(ctx, s, 50, 10);
    gfx_setFontSize(f, 32);
    gfx_renderString(ctx, f, "Ethereal", 120, 55, GFX_RGB(0,0,0));
    gfx_setFontSize(f, 12);

    // Draw separator
    gfx_drawRectangleFilled(ctx, &GFX_RECT(10, 80, 330, 3), GFX_RGB(0xdd, 0xdd, 0xdd));

    // Get kernel info
    struct utsname name;
    uname(&name);

    // TODO: Way to get Ethereal version. Perhaps command, perhaps otherwise
    char kernel_build_str[256] = { 0 };
    snprintf(kernel_build_str, 256, "Version 1.0.0 (Kernel Build %s)", name.release);

    // Draw info lines
    gfx_renderString(ctx, f, "Ethereal Operating System", 10, 100, GFX_RGB(0,0,0));
    gfx_renderString(ctx, f, kernel_build_str, 10, 120, GFX_RGB(0,0,0));
    gfx_renderString(ctx, f, "Copyright Samuel Stuart, 2024 - 2025", 10, 140, GFX_RGB(0,0,0));
    gfx_renderString(ctx, f, "Ethereal is licensed under the BSD 3-clause license.", 10, 180, GFX_RGB(0, 0, 0));
    gfx_renderString(ctx, f, "https://github.com/sasdallas/Ethereal", 10, 200, GFX_RGB(0,0,0));
    gfx_render(ctx);
    celestial_flip(win);

    celestial_mainLoop();
    return 0;
}