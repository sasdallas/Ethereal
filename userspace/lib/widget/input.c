/**
 * @file userspace/lib/widget/input.c
 * @brief Text box
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

/* TEMPORARY: Default font for text */
gfx_font_t *input_default_font = NULL;

/**
 * @brief Common render method
 */
static void input_renderCommon(widget_t *widget, gfx_context_t *ctx) {
    int x = 0;
    int y = 0;
    widget_getCoordinates(widget, &x, &y);

    widget_input_t *input = (widget_input_t*)widget->impl;

    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x, r.y, r.width, r.height), 4, GFX_GRADIENT_HORIZONTAL, GFX_RGB(0xE2, 0xE3, 0xEA), GFX_RGB(0xDB, 0xDF, 0xE6));
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(r.x+1, r.y+1, r.width-2, r.height-2), 4, GFX_RGB(255, 255, 255));
    gfx_drawRectangleFilled(ctx, &GFX_RECT(r.x+2, r.y, r.width-4, 1), GFX_RGB(0xab, 0xad, 0xa3));


    // Adjust for text input
    int tx = 4;

    // Do text if we have it
    if (input->idx) {
        gfx_context_t *clipped = gfx_createContextSubrect(ctx, &GFX_RECT(r.x, y+4, r.width-2, r.height-2));
        if (input->type == INPUT_TYPE_PASSWORD) {
            char buf[512];
            strcpy(buf, input->buffer);
    
            *buf = 0;
        
            for (unsigned i = 0; i < strlen(input->buffer); i++) {
                strcat(buf, "●");
            }

            gfx_renderString(ctx, input_default_font, (char*)buf, r.x+4, y+14, GFX_RGB(0,0,0));
            gfx_string_size_t s;
            gfx_getStringSize(input_default_font, buf, &s);
            tx += s.width ;
        } else {
            gfx_renderString(clipped, input_default_font, input->buffer, 4, 10, GFX_RGB(0,0,0));
            gfx_string_size_t s;
            gfx_getStringSize(input_default_font, input->buffer, &s);
            tx += s.width ;
        }

    } else if (!input->focused && input->placeholder) {
        // Not focused, draw placeholder
        gfx_context_t *clipped = gfx_createContextSubrect(ctx, &GFX_RECT(r.x, y+4, r.width-2, r.height-2));
        gfx_renderString(clipped, input_default_font, input->placeholder, 4, 10, GFX_RGB(0x82, 0x82, 0x82));
    }

    if (input->focused && input->csr_state && tx < (int)widget->width) {
        gfx_drawRectangleFilled(ctx, &GFX_RECT(x+tx, r.y+4, 1, r.height - 8), GFX_RGB(0,0,0));
    }
}

/**
 * @brief Render method
 */
void input_render(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y) {
    // widget_input_t *input = (widget_input_t*)widget->impl;

    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(r.x, r.y, r.width, r.height), 4, GFX_GRADIENT_HORIZONTAL, GFX_RGB(0xE2, 0xE3, 0xEA), GFX_RGB(0xDB, 0xDF, 0xE6));
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(r.x+1, r.y+1, r.width-2, r.height-2), 4, GFX_RGB(255, 255, 255));
    gfx_drawRectangleFilled(ctx, &GFX_RECT(r.x+2, r.y, r.width-4, 1), GFX_RGB(0xab, 0xad, 0xa3));

}

static void input_enter(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y) {
    celestial_setMouseCursor(CELESTIAL_MOUSE_TEXT);
}

static void input_exit(widget_t *widget, gfx_context_t *ctx) {
    celestial_setMouseCursor(CELESTIAL_MOUSE_DEFAULT);
}


static void input_up(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y, int32_t held) {
    if (held & CELESTIAL_MOUSE_BUTTON_LEFT) {

        widget_input_t *input = (widget_input_t*)widget->impl;
        input->focused = true;
        input->csr_state = true;
    
        struct timeval tv;
        gettimeofday(&tv, NULL);
        input->last_csr_update = tv.tv_sec * 1000000 + tv.tv_usec;

        // Render a cursor
        input_renderCommon(widget, ctx);
    }
}

static int input_update(widget_t *widget, gfx_context_t *ctx, uint64_t ticks) {
    widget_input_t *input = (widget_input_t*)widget->impl;

    if (!input->focused) return 0;

    if (ticks - input->last_csr_update >= 600000) {
        input->csr_state = !input->csr_state;
        input->last_csr_update = ticks;
        input_renderCommon(widget, ctx);
        return 1;
    }


    return 0;
}

static void input_key(widget_t *widget, gfx_context_t *ctx, keyboard_event_t *event) {
    if (event->type == KEYBOARD_EVENT_RELEASE) return;

    widget_input_t *input = (widget_input_t*)widget->impl;

    if (!input->focused) {
        fprintf(stderr, "input not focused!\n");
        return;
    }

    if (event->ascii == '\b' || event->ascii == 0x7F) {
        if (input->idx == 0) return;

        input->buffer[input->idx-1] = 0;
        input->idx--;
        input_renderCommon(widget, ctx);
        return;
    }

    if (event->ascii == '\r' || event->ascii == '\n') {
        // Newline, ignore if no NL
        if (input->nl) {
            input->nl(widget, input->nl_ctx);
        }

        return;
    }

    // input->buffer = realloc(input->buffer, input->idx+2);
    input->buffer[input->idx] = event->ascii;
    input->idx++;
    input->buffer[input->idx] = 0;

    input_renderCommon(widget, ctx);
}

static void input_clickAway(widget_t *widget, gfx_context_t *ctx) {
    
    widget_input_t *input = (widget_input_t*)widget->impl;
    input->focused = false;
    input_renderCommon(widget, ctx);

}

/**
 * @brief Create a text input box
 * @param frame The frame to create the input box on
 * @param type Type of textbox
 * @param placeholder Optional placeholder
 * @param width Width
 * @param height Height
 */
widget_t *input_create(widget_t *frame, int type, char *placeholder, size_t width, size_t height) {
    if (!input_default_font) {
        // TODO: The graphics API no longer uses context
        input_default_font = gfx_loadFont(NULL, LABEL_DEFAULT_FONT);
        gfx_setFontSize(input_default_font, 11);
    }

    widget_t *w = widget_createEmpty();

    widget_input_t *inp = malloc(sizeof(widget_input_t));
    memset(inp, 0, sizeof(widget_input_t));
    inp->buffer = malloc(128);
    inp->buffer[0] = 0;
    if (placeholder) inp->placeholder = strdup(placeholder);
    else inp->placeholder = NULL;
    inp->type = type;
    inp->focused = false;
    inp->csr_state = true;
    inp->idx = 0;

    w->width = width;
    w->height = height;

    w->impl = (void*)inp;    
    w->type = WIDGET_TYPE_INPUT;
    w->render = input_render;
    w->enter = input_enter;
    w->exit = input_exit;
    w->up = input_up;
    w->key = input_key;
    w->click_away = input_clickAway;
    w->update = input_update;
    list_append(frame->children, w);
    return w;
}

/**
 * @brief Register newline handler
 * @param input Input box
 * @param handler Handler
 * @param context Context
 */
void input_onNewline(widget_t *input, input_newline_handler_t handler, void *context) {
    widget_input_t *inp = (widget_input_t*)input->impl;
    inp->nl = handler;
    inp->nl_ctx = context;
}