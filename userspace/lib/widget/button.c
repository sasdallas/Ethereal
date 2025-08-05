/**
 * @file userspace/lib/graphics/button.c
 * @brief Button widget
 * 
 * @todo Clean this code up...
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/widget.h>

/* TEMPORARY: Default font for button */
gfx_font_t *button_default_font = NULL;

/* Default button colors */
#define BUTTON_SHADOW_COLOR_START               GFX_RGB(0xAB, 0xAB, 0xAB)
#define BUTTON_SHADOW_COLOR_END                 GFX_RGB(0xB0, 0xB0, 0xB0)

#define BUTTON_BOTTOM_COLOR                     GFX_RGB(0xFC, 0xFC, 0xFC)

#define BUTTON_MAIN_COLOR_START                 GFX_RGB(0xF6, 0xF6, 0xF6)
#define BUTTON_MAIN_COLOR_END                   GFX_RGB(0xD2, 0xD2, 0xD2)

#define BUTTON_HIGHLIGHT_COLOR_START            GFX_RGB(0xF0, 0xF0, 0xF0)
#define BUTTON_HIGHLIGHT_COLOR_END              GFX_RGB(0xE5, 0xE5, 0xE5)

#define BUTTON_HOLD_COLOR_START                 GFX_RGB(0xD2, 0xD2, 0xD2)
#define BUTTON_HOLD_COLOR_END                   GFX_RGB(0xc2, 0xc2, 0xc2)

#define BUTTON_HOLD_BOTTOM_COLOR                GFX_RGB(0xb4, 0xb4, 0xb4)

#define BUTTON_MIN_WIDTH                        50
#define BUTTON_MIN_HEIGHT                       21

#define STR_OFFSETX                             1

/**
 * @brief Render the button object
 */
static void button_render(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y) {
    widget_button_t *btn = (widget_button_t*)widget->impl;

    // First render all button components
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &r, 4, GFX_GRADIENT_VERTICAL, BUTTON_SHADOW_COLOR_START, BUTTON_SHADOW_COLOR_END);
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + 1, y + 1, widget->width - 2, widget->height - 2), 3, BUTTON_BOTTOM_COLOR);
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 3), 2, GFX_GRADIENT_VERTICAL, BUTTON_MAIN_COLOR_START, BUTTON_MAIN_COLOR_END);
    
    // Now render the text using cursed math
    gfx_string_size_t s; gfx_getStringSize(button_default_font, btn->text, &s);
    int strx = ((r.width - 4 - s.width) / 2);
    int stry = ((r.height - 3 - s.height) / 2);
    gfx_renderString(ctx, button_default_font, btn->text, r.x + strx + STR_OFFSETX, r.y - 3 + r.height - stry, btn->color);
}


/**
 * @brief Button click callback
 * @param widget The widget that was clicked
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 * @param held Held buttons. 
 */
static void button_down(widget_t *widget, gfx_context_t *ctx, int32_t _x, int32_t _y, int held) {
    // Get coordinates
    int x = 0;
    int y = 0;
    widget_getCoordinates(widget, &x, &y);

    // Re-render button
    widget_button_t *btn = (widget_button_t*)widget->impl;

    // First render all button components
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &r, 4, GFX_GRADIENT_VERTICAL, BUTTON_SHADOW_COLOR_START, BUTTON_SHADOW_COLOR_END);
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + 1, y + 1, widget->width - 2, widget->height - 2), 3, BUTTON_HOLD_BOTTOM_COLOR);
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 3), 2, GFX_GRADIENT_VERTICAL, BUTTON_HOLD_COLOR_START, BUTTON_HOLD_COLOR_END);

    // Now render the text using cursed math
    gfx_string_size_t s; gfx_getStringSize(button_default_font, btn->text, &s);
    int strx = ((r.width - 4 - s.width) / 2);
    int stry = ((r.height - 3 - s.height) / 2);
    gfx_renderString(ctx, button_default_font, btn->text, r.x + strx + STR_OFFSETX, r.y - 3 + r.height - stry, btn->color);
}

/**
 * @brief Button up callback
 * @param widget The widget that was released
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 * @param release Released button
 */
static void button_up(widget_t *widget, gfx_context_t *ctx, int32_t _x, int32_t _y, int release) {
    // Get coordinates
    int x = 0;
    int y = 0;
    widget_getCoordinates(widget, &x, &y);

    widget_button_t *btn = (widget_button_t*)widget->impl;

    // First render all button components
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &r, 4, GFX_GRADIENT_VERTICAL, BUTTON_SHADOW_COLOR_START, BUTTON_SHADOW_COLOR_END);
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + 1, y + 1, widget->width - 2, widget->height - 2), 3, BUTTON_BOTTOM_COLOR);
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 3), 2, GFX_GRADIENT_VERTICAL, BUTTON_HIGHLIGHT_COLOR_START, BUTTON_HIGHLIGHT_COLOR_END);
    
    // Now render the text using cursed math
    gfx_string_size_t s; gfx_getStringSize(button_default_font, btn->text, &s);
    int strx = ((r.width - 4 - s.width) / 2);
    int stry = ((r.height - 3 - s.height) / 2);
    gfx_renderString(ctx, button_default_font, btn->text, r.x + strx + STR_OFFSETX, r.y - 3 + r.height - stry, btn->color);
}

/**
 * @brief Button enter callback
 * @param widget The widget that was entered
 * @param ctx To render to
 * @param x The X position, widget-relative
 * @param y The Y position, widget-relative
 */
static void button_enter(widget_t *widget, gfx_context_t *ctx, int32_t _x, int32_t _y) {
    // Get coordinates
    int x = 0;
    int y = 0;
    widget_getCoordinates(widget, &x, &y);

    widget_button_t *btn = (widget_button_t*)widget->impl;

    // First render all button components
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &r, 4, GFX_GRADIENT_VERTICAL, BUTTON_SHADOW_COLOR_START, BUTTON_SHADOW_COLOR_END);
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + 1, y + 1, widget->width - 2, widget->height - 2), 3, BUTTON_BOTTOM_COLOR);
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 3), 2, GFX_GRADIENT_VERTICAL, BUTTON_HIGHLIGHT_COLOR_START, BUTTON_HIGHLIGHT_COLOR_END);
    
    // Now render the text using cursed math
    gfx_string_size_t s; gfx_getStringSize(button_default_font, btn->text, &s);
    int strx = ((r.width - 4 - s.width) / 2);
    int stry = ((r.height - 3 - s.height) / 2);
    gfx_renderString(ctx, button_default_font, btn->text, r.x + strx + STR_OFFSETX, r.y - 3 + r.height - stry, btn->color);

}

/**
 * @brief Button exit callback
 * @param widget The button
 * @param ctx Context to render to
 */
static void button_exit(widget_t *widget, gfx_context_t *ctx) {
    // Get coordinates
    int x = 0;
    int y = 0;
    widget_getCoordinates(widget, &x, &y);

    widget_button_t *btn = (widget_button_t*)widget->impl;

    // First render all button components
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &r, 4, GFX_GRADIENT_VERTICAL, BUTTON_SHADOW_COLOR_START, BUTTON_SHADOW_COLOR_END);
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + 1, y + 1, widget->width - 2, widget->height - 2), 3, BUTTON_BOTTOM_COLOR);
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 3), 2, GFX_GRADIENT_VERTICAL, BUTTON_MAIN_COLOR_START, BUTTON_MAIN_COLOR_END);
    
    // Now render the text using cursed math
    gfx_string_size_t s; 
    gfx_getStringSize(button_default_font, btn->text, &s);
    int strx = ((r.width - 4 - s.width) / 2);
    int stry = ((r.height - 3 - s.height) / 2);
    gfx_renderString(ctx, button_default_font, btn->text, r.x + strx + STR_OFFSETX, r.y - 3 + r.height - stry, btn->color);
}

/**
 * @brief Create a new button widget
 * @param frame The frame to put the button into
 * @param text The text of the button
 * @param color The primary color of the button
 * @param state The state of the button, @c BUTTON_ENABLED
 */
widget_t *button_create(widget_t *frame, char *text, gfx_color_t color, int state) {
    if (!button_default_font) {
        // TODO: The graphics API no longer uses context
        button_default_font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");
        gfx_setFontSize(button_default_font, 12);
    }

    widget_t *w = widget_createEmpty();

    widget_button_t *button = malloc(sizeof(widget_button_t));
    button->text = strdup(text);
    button->color = color;
    button->enabled = state;

    // Get bounding box for the text
    gfx_string_size_t s;
    gfx_getStringSize(button_default_font, text, &s);

    // Set components up
    w->width = s.width + 20;
    w->height = s.height + 10;
    w->type = WIDGET_TYPE_BUTTON;
    w->impl = (void*)button;
    w->render = button_render;
    w->down = button_down;
    w->up = button_up;
    w->enter = button_enter;
    w->exit = button_exit;

    // Clamp width and height
    if (w->width < BUTTON_MIN_WIDTH) w->width = BUTTON_MIN_WIDTH;
    if (w->height < BUTTON_MIN_HEIGHT) w->height = BUTTON_MIN_HEIGHT;

    list_append(frame->children, w);
    return w;
}