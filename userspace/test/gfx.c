/**
 * @file userspace/test/gfx.c
 * @brief GFX test program
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <graphics/gfx.h>
#include <graphics/draw.h>
#include <stdio.h>
#include <kernel/fs/periphfs.h>
#include <unistd.h>
#include <string.h>

/* Background sprite */
sprite_t *bg_sprite = NULL;

int nobg = 0;

extern int gfx_renderSpriteInTheHoles(gfx_context_t *ctx, sprite_t *sprite);


void draw_background(gfx_context_t *ctx) {
    if (nobg) return;
    if (!bg_sprite) {
        bg_sprite = gfx_createSprite(0,0);
        FILE *f = fopen("/device/initrd/lines.bmp", "r");
        if (!f) {
            perror("fopen");
            exit(1);
        }

        if (gfx_loadSprite(bg_sprite, f)) {
            printf("Failed to load sprite\n");
            exit(1);
        }
    }

    gfx_renderSpriteInTheHoles(ctx, bg_sprite);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (!strcmp(argv[1], "nobg")) nobg = 1;
    }

    // Load sprite
    sprite_t *spr = gfx_createSprite(0, 0);
    FILE *f = fopen("/device/initrd/cursor.bmp", "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    if (gfx_loadSprite(spr, f)) {
        printf("Failed to load sprite\n");
        return 1;
    }

    // Create context
    gfx_context_t *ctx = gfx_createFullscreen(CTX_DEFAULT);
    if (!ctx) {
        printf("Failed to create graphics context\n");
        return 1;
    }
    
    // Clear screen
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);

    // Get mouse
    int mouse = open("/device/mouse", O_RDONLY);

    // Mouse parameters
    unsigned int x = GFX_WIDTH(ctx) / 2;
    unsigned int y = GFX_HEIGHT(ctx) / 2;
    long x_offset = 0;
    long y_offset = 0;
    int dragged = 0;


    if (!nobg) {
        gfx_createClip(ctx, 0, 0, ctx->width - 2, ctx->height - 2);
        draw_background(ctx);
        gfx_renderSprite(ctx, bg_sprite, 0, 0);
        gfx_render(ctx);
        gfx_resetClips(ctx);
    }

    // Test rect
    gfx_rect_t rect = GFX_RECT(100, 100, 250, 150);
    
    

    // main loop
    while (1) {
        mouse_event_t event;
        while (!read(mouse, &event, sizeof(mouse_event_t)));
        if (!(event.x_difference || event.y_difference)) {
            continue;
        }

        if (!x || x + event.x_difference >= GFX_WIDTH(ctx) || !y || y - event.y_difference >= GFX_HEIGHT(ctx)) {
            fprintf(stderr, "Ignoring event\n");
            continue;
        }

        gfx_createClip(ctx, x, y, spr->width, spr->height);
        // gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y, spr->width, spr->height), GFX_RGBA(0, 0, 0, 255));

        x += event.x_difference;
        y -= event.y_difference;

        if (x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.width && event.buttons & MOUSE_BUTTON_LEFT && !dragged) {
            // Determine the offset
            x_offset = rect.x - x;
            y_offset = rect.y - y;
            dragged = 1;
        }

        if (!(event.buttons & MOUSE_BUTTON_LEFT) && dragged) {
            dragged = 0;
        }


        if (dragged) {
            gfx_createClip(ctx, rect.x, rect.y, rect.width, rect.height);
            gfx_drawRectangleFilled(ctx, &rect, GFX_RGB(0,0,0));
            rect.x = x + x_offset;
            rect.y = y + y_offset;
        }

        // We've made collector holes for each
        draw_background(ctx);

        gfx_createClip(ctx, x, y, spr->width, spr->height);
        gfx_createClip(ctx, rect.x, rect.y, rect.width, rect.height);

        gfx_drawRectangleFilled(ctx, &rect, GFX_RGB(0, 255, 0));
        gfx_renderSprite(ctx, spr, x, y);

        fprintf(stderr, "%d %d\n", x, y);
        gfx_render(ctx);

        gfx_resetClips(ctx);
        continue;

    }

    return 0;
}