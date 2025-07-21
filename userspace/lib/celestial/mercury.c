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

/**
 * @brief Initialize the Mercury theme
 * @param win The window to initialize Mercury on
 */
int celestial_initMercury(window_t *win) {
    gfx_clear(win->decor->ctx, GFX_RGB(255,255,255));
    gfx_render(win->decor->ctx);

    // Load font object
    win->decor->font = gfx_loadFont(win->decor->ctx, "/usr/share/DejaVuSans.ttf");

    return 0;
}

/**
 * @brief Render the top/bottom gradient of Mercury
 * @param win The window to render the top gradient on
 * @param bottom Toggle to draw the gradient on the bottom
 */
static void celestial_renderMercuryGradient(window_t *win, int bottom) {
    // https://stackoverflow.com/questions/72756900/color-gradient-in-c (LMAO)
    // Calculate the steps

    gfx_color_t grad_start = GFX_RGB(0x3f, 0x3b, 0x42);
    gfx_color_t grad_end = GFX_RGB(0x95, 0x90, 0x99);

    float step_a = (float)(GFX_RGB_A(grad_end) - GFX_RGB_A(grad_start)) / (float)WIN_WIDTH(win);
    float step_r = (float)(GFX_RGB_R(grad_end) - GFX_RGB_R(grad_start)) / (float)WIN_WIDTH(win);
    float step_g = (float)(GFX_RGB_G(grad_end) - GFX_RGB_G(grad_start)) / (float)WIN_WIDTH(win);
    float step_b = (float)(GFX_RGB_B(grad_end) - GFX_RGB_B(grad_start)) / (float)WIN_WIDTH(win);

    for (uintptr_t x = 0; x < WIN_WIDTH(win); x++) {
        gfx_color_t color = 0 |
            ((uint8_t)(GFX_RGB_A(grad_start) + x * step_a) & 0xFF) << 24 |
            ((uint8_t)(GFX_RGB_R(grad_start) + x * step_r) & 0xFF) << 16 |
            ((uint8_t)(GFX_RGB_G(grad_start) + x * step_g) & 0xFF) << 8 |
            ((uint8_t)(GFX_RGB_B(grad_start) + x * step_b) & 0xFF) << 0;

        uintptr_t y = (bottom ? WIN_HEIGHT(win) - win->decor->borders.bottom_height : 0);
        uintptr_t y_max = (bottom ? WIN_HEIGHT(win) : (size_t)win->decor->borders.top_height);

        for (; y < y_max; y++) {
            GFX_PIXEL(win->decor->ctx, x, y) = color;
        }
    }
}

/**
 * @brief Render the Mercury theme
 * @param win The window to render the Mercury theme on
 */
int celestial_renderMercury(window_t *win) {
    decor_t *d = win->decor;

    // Render our borders
    
    // General window border
    gfx_rect_t rect_window_border = { .x = 0, .y = 0, .width = WIN_WIDTH(win) - 1, .height = WIN_HEIGHT(win) - 1};
    gfx_rect_t rect_inner_window_border = { .x = d->borders.left_width - 1, .y = d->borders.top_height - 1, .width = WIN_WIDTH(win)  - d->borders.left_width - d->borders.right_width + 1, .height = WIN_HEIGHT(win) - d->borders.top_height - d->borders.bottom_height + 1 };

    // Left border
    gfx_rect_t rect_left = { .x = 0, .y = d->borders.top_height, .width = d->borders.left_width, .height = WIN_HEIGHT(win) - d->borders.top_height - d->borders.bottom_height };

    // Right border
    gfx_rect_t rect_right = { .x = WIN_WIDTH(win) - d->borders.right_width, .y = d->borders.top_height, .width = d->borders.right_width - 1, .height = WIN_HEIGHT(win) - d->borders.top_height - d->borders.bottom_height, };


    celestial_renderMercuryGradient(win, 0);
    celestial_renderMercuryGradient(win, 1);
    gfx_drawRectangleFilled(win->decor->ctx, &rect_left, MERCURY_COLOR_LEFT_BORDER);
    gfx_drawRectangleFilled(win->decor->ctx, &rect_right, MERCURY_COLOR_RIGHT_BORDER);
    gfx_drawRectangle(win->decor->ctx, &rect_window_border, GFX_RGB(16,16,16));
    gfx_drawRectangle(win->decor->ctx, &rect_inner_window_border, GFX_RGB(16,16,16));
    gfx_renderSprite(win->decor->ctx, close_sprite, win->decor->ctx->width - close_sprite->width - 4, 2);
    gfx_renderSprite(win->decor->ctx, maximize_sprite, win->decor->ctx->width - close_sprite->width - maximize_sprite->width - 5, 2);
    gfx_renderSprite(win->decor->ctx, minimize_sprite, win->decor->ctx->width - close_sprite->width - maximize_sprite->width - minimize_sprite->width - 6, 2);
    gfx_renderString(win->decor->ctx, win->decor->font, win->decor->titlebar, 6, win->decor->borders.top_height - 6, (win->decor->focused ? MERCURY_COLOR_TEXT_FOCUSED : MERCURY_COLOR_TEXT_UNFOCUSED));
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
    if ((size_t)x >=  win->decor->ctx->width - close_sprite->width - maximize_sprite->width - minimize_sprite->width - 6 && (size_t)x < win->decor->ctx->width - close_sprite->width - maximize_sprite->width - 6) {
        return DECOR_BTN_MINIMIZE;
    } else if ((size_t)x >= win->decor->ctx->width - close_sprite->width - maximize_sprite->width - 5 && (size_t)x < win->decor->ctx->width - close_sprite->width - 5) {
        return DECOR_BTN_MAXIMIZE;
    } else if ((size_t)x >= win->decor->ctx->width - close_sprite->width - 4 && (size_t)x < win->decor->ctx->width - 4) {
        return DECOR_BTN_CLOSE;
    } else {
        return DECOR_BTN_NONE;
    }
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
    d->borders.top_height = 24;
    d->borders.bottom_height = 4;
    d->borders.right_width = 4;
    d->borders.left_width = 4;
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
        .top_height = 24,
        .bottom_height = 4,
        .right_width = 4,
        .left_width = 4,
    };

    return borders;
}