/**
 * @file userspace/lib/celestial/mintia.c
 * @brief MINTIA theme for Celestial
 * 
 * This uses MINTIA1's source code to draw:
 * https://github.com/xrarch/mintia/tree/main
 * 
 * See CoVideoConsoleDrawWindowBorder and CoVideoConsoleDrawTitleBar
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 */

#include <ethereal/celestial.h>
#include <graphics/gfx.h>
#include <stdlib.h>

#define MINTIA_TITLE_HEIGHT 22
#define MINTIA_SIDE_WIDTH 6
#define MINTIA_BOTTOM_HEIGHT 6
#define MINTIA_BODY GFX_RGB(204, 204, 204)
#define MINTIA_LIGHT GFX_RGB(255, 255, 255)
#define MINTIA_DARK GFX_RGB(136, 136, 136)
#define MINTIA_BLACK GFX_RGB(0, 0, 0)

/**
 * @brief Draw an outline
 * Yes this is needed. Yes I hate it. It's because gfx_drawRectangle also draws the far sides.
 */
static void mintia_drawOutline(gfx_context_t *ctx, int x, int y, int width, int height, gfx_color_t color) {
    if (width < 2 || height < 2) return;
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y, width, 1), color);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y + height - 1, width, 1), color);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y + 1, 1, height - 2), color);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x + width - 1, y + 1, 1, height - 2), color);
}

/**
 * @brief Get the X coordinate of a button
 */
static int mintia_getButtonX(int button) {
    if (button == DECOR_BTN_CLOSE) return 5;
    if (button == DECOR_BTN_MINIMIZE) return 20;
    return 35;
}

/**
 * @brief Draw a button
 * 
 * Slower since unlike mercury it just draws them out, and I'm lazy and that's what Co does anyways
 */
static void mintia_drawButton(window_t *win, int button, int hover) {
    gfx_context_t *ctx = win->decor->ctx;
    int x = mintia_getButtonX(button);
    int y = 4;

    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y, 14, 14), hover ? MINTIA_LIGHT : MINTIA_BODY);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y, 13, 1), MINTIA_LIGHT);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x, y + 1, 1, 12), MINTIA_LIGHT);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 1, y + 13, 13, 1), MINTIA_DARK);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 13, y + 1, 1, 12), MINTIA_DARK);

    if (button == DECOR_BTN_MINIMIZE) {
        gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 5, y + 7, 5, 1), MINTIA_BLACK);
    } else if (button == DECOR_BTN_MAXIMIZE) {
        gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 5, y + 7, 5, 1), MINTIA_BLACK);
        gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 7, y + 5, 1, 5), MINTIA_BLACK);
    } else {
        for (int i = 0; i < 5; i++) {
            gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 5 + i, y + 5 + i, 1, 1), MINTIA_BLACK);
            gfx_drawRectangleFilled(ctx, &GFX_RECT(x + 9 - i, y + 5 + i, 1, 1), MINTIA_BLACK);
        }
    }
}

/**
 * @brief Init MINTIA
 */
static int celestial_initMintia(window_t *win) {
    win->decor->font = gfx_loadFont(win->decor->ctx, "/usr/share/fonts/DejaVuSansMono.ttf");
    if (win->decor->font) gfx_setFontSize(win->decor->font, 10);
    return 0;
}

/**
 * @brief Render MINTIA
 */
static int celestial_renderMintia(window_t *win) {
    decor_t *d = win->decor;
    gfx_context_t *ctx = d->ctx;
    int width = (int)win->info->width;
    int height = (int)win->info->height;

    gfx_drawRectangleFilled(ctx, &GFX_RECT(0, 0, width, MINTIA_TITLE_HEIGHT), MINTIA_BODY);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(0, MINTIA_TITLE_HEIGHT, MINTIA_SIDE_WIDTH, height - MINTIA_TITLE_HEIGHT), MINTIA_BODY);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(width - MINTIA_SIDE_WIDTH, MINTIA_TITLE_HEIGHT, MINTIA_SIDE_WIDTH, height - MINTIA_TITLE_HEIGHT), MINTIA_BODY);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(0, height - MINTIA_BOTTOM_HEIGHT, width, MINTIA_BOTTOM_HEIGHT), MINTIA_BODY);

    mintia_drawOutline(ctx, 0, 0, width, height, MINTIA_BLACK);
    mintia_drawOutline(ctx, 1, 1, width - 2, height - 2, MINTIA_DARK);
    mintia_drawOutline(ctx, 1, 1, width - 3, height - 3, MINTIA_LIGHT);
    mintia_drawOutline(ctx, 4, 4, width - 8, height - 8, MINTIA_LIGHT);
    mintia_drawOutline(ctx, 4, 4, width - 9, height - 9, MINTIA_DARK);
    mintia_drawOutline(ctx, 2, 2, width - 4, height - 4, MINTIA_BODY);
    mintia_drawOutline(ctx, 3, 3, width - 6, height - 6, MINTIA_BODY);
    mintia_drawOutline(ctx, 5, 5, width - 10, height - 10, MINTIA_BLACK);

    gfx_drawRectangleFilled(ctx, &GFX_RECT(4, 3, width - 8, 19), MINTIA_BODY);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(4, 20, width - 8, 2), MINTIA_DARK);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(width - 5, 20, 1, 2), MINTIA_LIGHT);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(5, 21, width - 10, 1), MINTIA_BLACK);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(4, 3, width - 9, 1), MINTIA_DARK);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(4, 4, 1, 14), MINTIA_DARK);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(4, 18, width - 9, 1), MINTIA_LIGHT);
    gfx_drawRectangleFilled(ctx, &GFX_RECT(width - 5, 3, 1, 16), MINTIA_LIGHT);

    int title_width = 0;
    if (d->titlebar && d->font) {
        for (const char *p = d->titlebar; *p; p++) {
            title_width += gfx_getAdvanceX(ctx, d->font, *p);
        }
    }

    int stripe_x = 57 + title_width;
    for (int i = 0; i < 5; i++) {
        int y = 6 + i * 2;
        gfx_drawRectangleFilled(ctx, &GFX_RECT(stripe_x, y, width - stripe_x - 12, 1), MINTIA_LIGHT);
        gfx_drawRectangleFilled(ctx, &GFX_RECT(stripe_x + 1, y + 1, width - stripe_x - 12, 1), MINTIA_DARK);
    }

    if (d->titlebar && d->font) {
        gfx_renderString(ctx, d->font, d->titlebar, 53, 16, d->focused ? MINTIA_BLACK : MINTIA_DARK);
    }

    mintia_drawButton(win, DECOR_BTN_CLOSE, 0);
    mintia_drawButton(win, DECOR_BTN_MINIMIZE, 0);
    mintia_drawButton(win, DECOR_BTN_MAXIMIZE, 0);
    gfx_render(ctx);
    celestial_flip(win);
    return 0;
}

/**
 * @brief In bounds MINTIA
 */
static int celestial_inBoundsMintia(window_t *win, int32_t x, int32_t y) {
    if (y < 4 || y >= 18) return DECOR_BTN_NONE;
    if (x >= 5 && x < 19) return DECOR_BTN_CLOSE;
    if (x >= 20 && x < 34) return DECOR_BTN_MINIMIZE;
    if (x >= 35 && x < 49) return DECOR_BTN_MAXIMIZE;
    return DECOR_BTN_NONE;
}

/**
 * @brief Update state MINTIA
 */
static int celestial_updateStateMintia(window_t *win, int button, int state) {
    if (button >= DECOR_BTN_CLOSE && button <= DECOR_BTN_MINIMIZE) {
        mintia_drawButton(win, button, state == DECOR_BTN_STATE_HOVER);
        gfx_render(win->decor->ctx);
        celestial_flip(win);
    }
    return 0;
}

/**
 * @brief Load MINTIA
 */
decor_t *celestial_loadMintia(decor_handler_t *handler, window_t *win) {
    decor_t *d = malloc(sizeof(decor_t));
    memset(d, 0, sizeof(decor_t));

    d->borders.top_height = MINTIA_TITLE_HEIGHT;
    d->borders.bottom_height = MINTIA_BOTTOM_HEIGHT;
    d->borders.left_width = MINTIA_SIDE_WIDTH;
    d->borders.right_width = MINTIA_SIDE_WIDTH;

    d->init = celestial_initMintia;
    d->render = celestial_renderMintia;
    d->inbtn = celestial_inBoundsMintia;
    d->state = celestial_updateStateMintia;
    d->handler = handler;
    d->win = win;
    return d;
}

/**
 * @brief Get borders MINTIA
 */
decor_borders_t celestial_getBordersMintia(decor_handler_t *handler) {
    return (decor_borders_t) {
        .top_height = MINTIA_TITLE_HEIGHT,
        .bottom_height = MINTIA_BOTTOM_HEIGHT,
        .left_width = MINTIA_SIDE_WIDTH,
        .right_width = MINTIA_SIDE_WIDTH,
    };
}
