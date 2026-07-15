/**
 * @file userspace/lib/celestial/mercury.c
 * @brief Mercury theme for Celestial
 * 
 * Designed with <3 by @ArtsySquid: https://artsycomms.carrd.co/
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

/* Button sprites */
sprite_t *close_sprite_unhovered = NULL;
sprite_t *maximize_sprite_unhovered = NULL;
sprite_t *minimize_sprite_unhovered = NULL;
sprite_t *close_sprite_hovered = NULL;
sprite_t *maximize_sprite_hovered = NULL;
sprite_t *minimize_sprite_hovered = NULL;

sprite_t *close_sprite = NULL;
sprite_t *maximize_sprite = NULL;
sprite_t *minimize_sprite = NULL;

#define MERCURY_COLOR_LEFT_BORDER (GFX_RGB(0x3f, 0x3b, 0x42))
#define MERCURY_COLOR_RIGHT_BORDER (GFX_RGB(0x94, 0x8f, 0x98))
#define WIN_WIDTH(win) (win->info->width)
#define WIN_HEIGHT(win) (win->info->height)

#define MERCURY_COLOR_TEXT_FOCUSED      (GFX_RGB(255, 255, 255))
#define MERCURY_COLOR_TEXT_UNFOCUSED    (GFX_RGB(0xcc, 0xcc, 0xcc))

/* Border colors */
#define MERCURY_BORDER_GRADIENT_START           GFX_RGB(0x3f, 0x3b, 0x42)
#define MERCURY_BORDER_GRADIENT_END             GFX_RGB(0x95, 0x90, 0x99)

enum ss_pieces {
    CORNER_UL,
    CORNER_UR,
    CORNER_BL,
    CORNER_BR,
    TOP,
    LEFT,
    BOT,
    RIGHT,
    NPIECES
};

static sprite_t *spritesheet[NPIECES];
static sprite_t *sp_base;
static int loaded = 0;

#define CORNER_UL_X 0
#define CORNER_UL_Y 0
#define CORNER_UL_W 10
#define CORNER_UL_H 26

#define TOP_X 8
#define TOP_Y 0
#define TOP_W 1
#define TOP_H 26

#define CORNER_UR_X 11
#define CORNER_UR_Y 0
#define CORNER_UR_W 10
#define CORNER_UR_H 26

#define LEFT_X 0
#define LEFT_Y 26
#define LEFT_W 5
#define LEFT_H 1

#define RIGHT_X 16
#define RIGHT_Y 26
#define RIGHT_W 5
#define RIGHT_H 1

#define CORNER_BL_X 0
#define CORNER_BL_Y 30
#define CORNER_BL_W 5
#define CORNER_BL_H 3

#define CORNER_BR_X 16
#define CORNER_BR_Y 30
#define CORNER_BR_W 5
#define CORNER_BR_H 3

#define BOT_X 5
#define BOT_Y 30
#define BOT_W 1
#define BOT_H 3


/**
 * @brief Load piece
 */
static sprite_t *mercury_loadPiece(sprite_t *base, int x, int y, int w, int h) {
    sprite_t *n = gfx_createSprite(w,h);
    gfx_context_t *ctx = gfx_createContext(CTX_DEFAULT, (uint8_t*)n->bitmap, w, h);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(0,0,w,h), GFX_RGBA(0,0,0,255));

    for (int32_t dy = 0; dy < h; dy++) {
        for (int32_t dx = 0; dx < w; dx++) {
            SPRITE_PIXEL(n, dx, dy) = SPRITE_PIXEL(base, x + dx, y + dy);
        }
    }

    free(ctx);
    return n;
} 

/**
 * @brief Load the spritesheet
 */
static void mercury_loadSpritesheet(sprite_t *sp) {
    spritesheet[CORNER_UL] = mercury_loadPiece(sp, CORNER_UL_X, CORNER_UL_Y, CORNER_UL_W, CORNER_UL_H);
    spritesheet[CORNER_UR] = mercury_loadPiece(sp, CORNER_UR_X, CORNER_UR_Y, CORNER_UR_W, CORNER_UR_H);
    spritesheet[CORNER_BL] = mercury_loadPiece(sp, CORNER_BL_X, CORNER_BL_Y, CORNER_BL_W, CORNER_BL_H);
    spritesheet[CORNER_BR] = mercury_loadPiece(sp, CORNER_BR_X, CORNER_BR_Y, CORNER_BR_W, CORNER_BR_H);
    spritesheet[TOP] = mercury_loadPiece(sp, TOP_X, TOP_Y, TOP_W, TOP_H);
    spritesheet[BOT] = mercury_loadPiece(sp, BOT_X, BOT_Y, BOT_W, BOT_H);
    spritesheet[LEFT] = mercury_loadPiece(sp, LEFT_X, LEFT_Y, LEFT_W, LEFT_H);
    spritesheet[RIGHT] = mercury_loadPiece(sp, RIGHT_X, RIGHT_Y, RIGHT_W, RIGHT_H);
    loaded = 1;
}

/**
 * @brief Initialize the Mercury theme
 * @param win The window to initialize Mercury on
 */
int celestial_initMercury(window_t *win) {
    gfx_clear(win->decor->ctx, GFX_RGB(255,255,255));
    gfx_render(win->decor->ctx);

    // Load font object
    win->decor->font = gfx_loadFont(win->decor->ctx, "/usr/share/DejaVuSans.ttf");

    // Load the mercury borders
    if (!loaded) {
        FILE *f = fopen("/usr/share/mercury/borders.bmp", "r");
        if (!f) {
            fprintf(stderr, "mercury: Error loading /usr/share/mercury/borders.bmp\n");
            fprintf(stderr, "mercury: Trying to continue anyways, this won't work well...\n");
            return 1;
        }   
        
        sp_base = gfx_createSprite(0,0);
        gfx_loadSprite(sp_base, f);
        fclose(f);
        mercury_loadSpritesheet(sp_base);
    }

    return 0;
}

/**
 * @brief Render the Mercury theme
 * @param win The window to render the Mercury theme on
 */
int celestial_renderMercury(window_t *win) {
    decor_t *d = win->decor;

    // Left and right borders
    gfx_rect_t rect_left = { .x = 0, .y = d->borders.top_height, .width = d->borders.left_width, .height = WIN_HEIGHT(win) - d->borders.top_height - d->borders.bottom_height };
    gfx_rect_t rect_right = { .x = WIN_WIDTH(win) - d->borders.right_width + 1, .y = d->borders.top_height, .width = d->borders.right_width - 1, .height = WIN_HEIGHT(win) - d->borders.top_height - d->borders.bottom_height, };

    gfx_drawRectangleFilled(win->decor->ctx, &GFX_RECT(0, 0, win->info->width, d->borders.top_height), GFX_RGBA(0,0,0,0));

    gfx_drawRectangleFilled(win->decor->ctx, &GFX_RECT(0, win->info->height - d->borders.bottom_height, win->info->width, d->borders.bottom_height), GFX_RGBA(0,0,0,0));

    // Upper left
    gfx_renderSprite(win->decor->ctx, spritesheet[CORNER_UL], 0, 0);

    // Left
    gfx_drawRectangleFilled(win->decor->ctx, &rect_left, GFX_RGBA(0,0,0,0));
    for (size_t i = d->borders.top_height; i <= WIN_HEIGHT(win) - d->borders.bottom_height; i++) {
        gfx_renderSprite(win->decor->ctx, spritesheet[LEFT], 0, i);
    }

    // Right
    gfx_drawRectangleFilled(win->decor->ctx, &rect_right, GFX_RGBA(0,0,0,0));
    for (size_t i = d->borders.top_height; i <= WIN_HEIGHT(win) - d->borders.bottom_height; i++) {
        gfx_renderSprite(win->decor->ctx, spritesheet[RIGHT], WIN_WIDTH(win) - d->borders.right_width, i);
    }

    // Top
    for (size_t i = CORNER_UL_W; i < WIN_WIDTH(win) - CORNER_UR_W; i++) {
        gfx_renderSprite(win->decor->ctx, spritesheet[TOP], i, 0);
    }

    // Upper right now
    gfx_renderSprite(win->decor->ctx, spritesheet[CORNER_UR], WIN_WIDTH(win) - CORNER_UR_W, 0);

    gfx_drawRectangleFilled(win->decor->ctx, &GFX_RECT(0,WIN_HEIGHT(win) - BOT_H, WIN_WIDTH(win), BOT_H), GFX_RGBA(0,0,0,0));
    for (size_t i = CORNER_BL_W; i < WIN_WIDTH(win) - CORNER_BR_W; i++) {
        gfx_renderSprite(win->decor->ctx, spritesheet[BOT], i, WIN_HEIGHT(win) - BOT_H);
    }    

    // Bottom corners
    gfx_renderSprite(win->decor->ctx, spritesheet[CORNER_BL], 0, WIN_HEIGHT(win) - d->borders.bottom_height);
    gfx_renderSprite(win->decor->ctx, spritesheet[CORNER_BR], WIN_WIDTH(win) - CORNER_BR_W, WIN_HEIGHT(win) - d->borders.bottom_height);

    // The spritesheet doesn't actually have gradients, so...
    gfx_drawRectangleFilledGradient(win->decor->ctx, &GFX_RECT(CORNER_UL_W, 3, WIN_WIDTH(win) - CORNER_UR_W - CORNER_UL_W, 23), GFX_GRADIENT_HORIZONTAL, MERCURY_BORDER_GRADIENT_START, MERCURY_BORDER_GRADIENT_END);
    gfx_drawRectangleFilledGradient(win->decor->ctx, &GFX_RECT(CORNER_BL_W, WIN_HEIGHT(win) - 3, WIN_WIDTH(win) - CORNER_BL_W - CORNER_BR_W, 1), GFX_GRADIENT_HORIZONTAL, MERCURY_BORDER_GRADIENT_START, MERCURY_BORDER_GRADIENT_END);

    // The rest of the junk
#define BTN_OFFSET_INITIAL              10
    gfx_renderSprite(win->decor->ctx, close_sprite, win->decor->ctx->width - close_sprite->width - BTN_OFFSET_INITIAL, 4);
    gfx_renderSprite(win->decor->ctx, maximize_sprite, win->decor->ctx->width - close_sprite->width - maximize_sprite->width - BTN_OFFSET_INITIAL - 1, 4);
    gfx_renderSprite(win->decor->ctx, minimize_sprite, win->decor->ctx->width - close_sprite->width - maximize_sprite->width - minimize_sprite->width - BTN_OFFSET_INITIAL - 2, 4);
    gfx_renderString(win->decor->ctx, win->decor->font, win->decor->titlebar, 8, win->decor->borders.top_height - 6, (win->decor->focused ? MERCURY_COLOR_TEXT_FOCUSED : MERCURY_COLOR_TEXT_UNFOCUSED));
    gfx_render(win->decor->ctx);
    celestial_flip(win); // TODO: Only flip decorations

    return 0;
}

/**
 * @brief Check whether the mouse is inside a button and what button
 * @param win The window to check
 * @param x The X coordinate
 * @param y The Y coordinate
 * @returns Button ID in bounds
 */
int celestial_inBoundsMercury(struct window *win, int32_t x, int32_t y) {
    if (x < 0 || y < 0) return DECOR_BTN_NONE;

    size_t win_width = win->decor->ctx->width;

    size_t close_start = win_width - close_sprite->width - 10;
    size_t max_start   = close_start - maximize_sprite->width - 1;
    size_t min_start   = max_start - minimize_sprite->width - 1;

    if ((size_t)x >= close_start && (size_t)x < win_width - 8) {
        return DECOR_BTN_CLOSE;
    }
    
    if ((size_t)x >= max_start && (size_t)x < close_start - 1) {
        return DECOR_BTN_MAXIMIZE;
    }
    
    if ((size_t)x >= min_start && (size_t)x < max_start - 1) {
        return DECOR_BTN_MINIMIZE;
    }

    return DECOR_BTN_NONE;
}

/**
 * @brief Update the state of a button
 * @param win The window
 * @param btn The button ID to update
 * @param state The new state to use
 * @returns 0 on success
 */
int celestial_updateStateMercury(struct window *win, int btn, int state) {
    sprite_t *old;
    switch (btn) {
        case DECOR_BTN_CLOSE:
            old = close_sprite;
            close_sprite = (state == DECOR_BTN_STATE_HOVER) ? close_sprite_hovered : close_sprite_unhovered;
            if (close_sprite != old) win->decor->render(win);
            break;

        case DECOR_BTN_MAXIMIZE:
            old = maximize_sprite;
            maximize_sprite = (state == DECOR_BTN_STATE_HOVER) ? maximize_sprite_hovered : maximize_sprite_unhovered;
            if (maximize_sprite != old) win->decor->render(win);
            break;

        case DECOR_BTN_MINIMIZE:
            old = minimize_sprite;
            minimize_sprite = (state == DECOR_BTN_STATE_HOVER) ? minimize_sprite_hovered : minimize_sprite_unhovered;
            if (minimize_sprite != old) win->decor->render(win);
            break;
    }
    
    return 0;
}

/**
 * @brief Load the Mercury theme
 * @param win The window to load Mercury on
 */
decor_t *celestial_loadMercury(decor_handler_t *handler, window_t *win) {
    if (!close_sprite) {
        FILE *f = fopen("/usr/share/mercury/close.bmp", "r");
        if (f) {
            close_sprite_unhovered = gfx_createSprite(0,0);
            gfx_loadSprite(close_sprite_unhovered, f);
            fclose(f);

            close_sprite = close_sprite_unhovered;
        }

        f = fopen("/usr/share/mercury/close-hover.bmp", "r");
        if (f) {
            close_sprite_hovered = gfx_createSprite(0,0);
            gfx_loadSprite(close_sprite_hovered, f);
            fclose(f);
        }
    }
    
    if (!maximize_sprite) {
        FILE *f = fopen("/usr/share/mercury/maximize.bmp", "r");
        if (f) {
            maximize_sprite_unhovered = gfx_createSprite(0,0);
            gfx_loadSprite(maximize_sprite_unhovered, f);
            fclose(f);

            maximize_sprite = maximize_sprite_unhovered;
        }

        f = fopen("/usr/share/mercury/maximize-hover.bmp", "r");
        if (f) {
            maximize_sprite_hovered = gfx_createSprite(0,0);
            gfx_loadSprite(maximize_sprite_hovered, f);
            fclose(f);
        }
    }

    if (!minimize_sprite) {
        FILE *f = fopen("/usr/share/mercury/minimize.bmp", "r");

        if (f) {
            minimize_sprite_unhovered = gfx_createSprite(0,0);
            gfx_loadSprite(minimize_sprite_unhovered, f);
            fclose(f);

            minimize_sprite = minimize_sprite_unhovered;
        }

        f = fopen("/usr/share/mercury/minimize-hover.bmp", "r");
        if (f) {
            minimize_sprite_hovered = gfx_createSprite(0,0);
            gfx_loadSprite(minimize_sprite_hovered, f);
            fclose(f);
        }
    }

    // Create decor object
    decor_t *d = malloc(sizeof(decor_t));
    d->borders.top_height = 26;
    d->borders.bottom_height = 3;
    d->borders.right_width = 5;
    d->borders.left_width = 5;
    d->flags = 0;
    d->init = celestial_initMercury;
    d->render = celestial_renderMercury;
    d->inbtn = celestial_inBoundsMercury;
    d->state = celestial_updateStateMercury;
    d->handler = handler;
    d->win = win;

    return d;
}

/**
 * @brief Get borders for the Mercury theme
 * @param handler The handler to use
 * @returns The border objects
 */
decor_borders_t celestial_getBordersMercury(decor_handler_t *handler) {
    decor_borders_t borders = {
        .top_height = 26,
        .bottom_height = 3,
        .right_width = 5,
        .left_width = 5,
    };

    return borders;
}