/**
 * @file userspace/font-viewer/font-viewer.c
 * @brief Font viewer
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
#include <graphics/gfx.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        // TODO: Accept custom strings and whatnot
        fprintf(stderr, "Usage: font-viewer [FONT]\n");
        fprintf(stderr, "Show a demo window of the font file\n");

        return 1;
    }

    char *f = argv[1];
    gfx_font_t *font = gfx_loadFont(NULL, f);
    if (!font) {
        fprintf(stderr, "font-viewer: %s: %s\n", f, strerror(errno));
        return 1;
    }

    // Now initialize the window
    wid_t wid = celestial_createWindow(0, 640, 480);
    window_t *win = celestial_getWindow(wid);
    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    celestial_setTitle(win, "Font Viewer");

    gfx_clear(ctx, GFX_RGB(255, 255, 255));


    int y = 30;
    gfx_setFontSize(font, 26);

    // TODO: Proper name
    gfx_renderString(ctx, font, f, 10, y, GFX_RGB(0,0,0));
    y += 30;

    // Draw font string
    gfx_setFontSize(font, 22);

    gfx_renderString(ctx, font, "abcdefghijklmnopqrstuvwxyz", 10, y, GFX_RGB(0,0,0));
    y += 26;
    gfx_renderString(ctx, font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 10, y, GFX_RGB(0,0,0));
    y += 26;
    gfx_renderString(ctx, font, "0123456789.:,;(*!?')", 10, y, GFX_RGB(0,0,0));
    y += 26;

    for (int i = 2; i < 26; i += 2) {
        gfx_setFontSize(font, i);

        gfx_renderString(ctx, font, "The quick brown fox jumps over the lazy dog.", 10, y, GFX_RGB(0,0,0));
        y += i + 4;
    }

    gfx_render(ctx);
    celestial_flip(win);
    celestial_mainLoop();
    return 0;
}