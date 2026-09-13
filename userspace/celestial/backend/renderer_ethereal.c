/**
 * @file userspace/celestial/backend/renderer_ethereal.c
 * @brief Celestial renderer
 * 
 * Runs in its own thread and collects @c render_request_t objects
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */


#ifndef BUILDING_LINUX
#define _GNU_SOURCE
#include <unistd.h>

#include "../celestial.h"
#include <time.h>
#include <signal.h>

void render_copyRect(gfx_rect_t *r) {
    size_t pitch = RENDERER->ctx->pitch;
    uint8_t *buffer = ((uint8_t*)RENDERER->ctx->buffer + (r->y * pitch + r->x * 4));
    uint8_t *backbuffer = ((uint8_t*)RENDERER->ctx->backbuffer + (r->y * pitch + r->x * 4));
    for (unsigned y = 0; y < r->height; y++) {
        memcpy(buffer, backbuffer, r->width * 4);
        buffer += RENDERER->ctx->pitch;
        backbuffer += RENDERER->ctx->pitch;
    }
}

void *render_main(void *arg) {
    TRACE_DEBUG("Render thread TID: %d\n", gettid());
    
    for (;;) {
        RENDERER->frame = damage_build();
        if (!RENDERER->frame) continue;
        int cursor_x = 0;
        int cursor_y = 0;
        bool draw_cursor = input_frameCursor(RENDERER->frame, &cursor_x, &cursor_y);

        render_request_t *req = RENDERER->frame;
        while (req) {
            render_request(req);

            render_request_t *next = req->next;
            req = next;
        }

        if (draw_cursor) {
            input_draw_at(cursor_x, cursor_y);
        }

        req = RENDERER->frame;
        while (req) {
            render_copyRect(&req->rect);

            render_request_t *next = req->next;
            free(req);
            req = next;
        }

        // this alleviates some visual artifacting while blurring/other effects.
        // because the compositor doesn't flush correctly or something
        // (i actually have no idea why this works)
        if (draw_cursor) {
            input_restore_at(cursor_x, cursor_y);
        }
    }
}

int renderer_init() {
    RENDERER->ctx = gfx_createFullscreen(CTX_DEFAULT);
    if (!RENDERER->ctx) {
        FATAL("Could not create graphics context\n");
        return 1;
    }
    gfx_clear(RENDERER->ctx, GFX_RGB(0,0,0));
    gfx_render(RENDERER->ctx);

    RENDERER->frame = NULL;

    // initialize the generic blur context
    renderer_initGeneric();
    
    if (pthread_create(&SERVER->render_thread, NULL, render_main, NULL) < 0) {
        FATAL("Could not create render thread: %s\n", strerror(errno));
        return 1;
    }

    TRACE_DEBUG("Renderer initialized\n");

    return 0;
}

void renderer_shutdown() {
    TRACE_INFO("Shutting down renderer...\n");

    pthread_cancel(SERVER->render_thread);
    renderer_shutdownGeneric();
    
    // TODO graphics API for this

    close(RENDERER->ctx->fb_fd);
    free(RENDERER->ctx->backbuffer);
    // TODO munmap and the rest of the shit
    free(RENDERER->ctx);

    TRACE_DEBUG("Renderer shutdown successfully.\n");
}

inline size_t renderer_getWidth() { return GFX_WIDTH(RENDERER->ctx); }
inline size_t renderer_getHeight() { return GFX_HEIGHT(RENDERER->ctx); }
#endif
