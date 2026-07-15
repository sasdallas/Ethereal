/**
 * @file userspace/celestialv2/backend/renderer_ethereal.c
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
            free(req);
            req = next;
        }

        
        if (draw_cursor) input_draw_at(cursor_x, cursor_y);
        memcpy(RENDERER->ctx->buffer, RENDERER->ctx->backbuffer, GFX_SIZE(RENDERER->ctx));
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
