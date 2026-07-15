/**
 * @file celestial/backend/renderer_linux.c
 * @brief Linux renderer
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifdef BUILDING_LINUX

#define _GNU_SOURCE
#include <unistd.h>

#include "../celestial.h"
#include <time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


Display *display;
Window w;
GC gc;
XImage *img;


gfx_font_t *font;
static pthread_t x11_pthread;
uint64_t last_redraw = 0;

void *render_main(void *arg) {
    TRACE_DEBUG("Render thread TID: %d\n", gettid());

    for (;;) {
        render_request_t *req = damage_build();
        if (!req) continue;
        int cursor_x = 0;
        int cursor_y = 0;
        bool draw_cursor = input_frameCursor(req, &cursor_x, &cursor_y);

        while (req) {
            render_request(req);

            render_request_t *next = req->next;
            free(req);
            req = next;
        }

        if (draw_cursor) input_draw_at(cursor_x, cursor_y);
        gfx_render(RENDERER->ctx);
        XPutImage(display, w, gc, img, 0, 0, 0, 0, 1920, 1080);
        XFlush(display);

        uint64_t now = celestial_now();
        if (now - last_redraw < 15000) {
            usleep(15000 - (now - last_redraw));
        }

        last_redraw = now;
    }
}

void *x11_thread(void *arg) {
    TRACE_DEBUG("X11 thread spawned\n");
    bool run = true;

    // Setup clientMessage
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, w, &wmDelete, 1);

    extern void mouse_process_motion(XMotionEvent *e);
    extern void mouse_process_press(XButtonEvent *e);
    extern void mouse_process_release(XButtonEvent *e);

    while (run) {
        XEvent e;
        XNextEvent(display, &e);

        bool have_motion = false;
        XMotionEvent latest_motion = {0};

        for (;;) {
            if (e.type == MotionNotify) {
                latest_motion = e.xmotion;
                have_motion = true;
            } else {
                if (have_motion) {
                    mouse_process_motion(&latest_motion);
                    have_motion = false;
                }

                if (e.type == DestroyNotify) {
                    run = false;
                } else if (e.type == ClientMessage) {
                    if (e.xclient.data.l[0] == wmDelete) {
                        run = false;
                    }
                } else if (e.type == ButtonPress) {
                    mouse_process_press(&e.xbutton);
                } else if (e.type == ButtonRelease) {
                    mouse_process_release(&e.xbutton);
                } else if (e.type == Expose) {
                    damage_add_locked(GFX_RECT(0, 0, 1920, 1080));
                }
            }

            if (!run || !XPending(display)) {
                break;
            }

            XNextEvent(display, &e);
        }

        if (have_motion) {
            mouse_process_motion(&latest_motion);
        }
    }

    extern void celestial_shutdown();
    celestial_shutdown();
}

int renderer_init() {
    XInitThreads();

    display = XOpenDisplay(NULL);
    XVisualInfo visual_info;
    XMatchVisualInfo(display, DefaultScreen(display), 24, TrueColor, &visual_info);

    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.colormap = XCreateColormap(display, DefaultRootWindow(display), visual_info.visual, AllocNone);
    attr.event_mask = StructureNotifyMask;
    w = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 1920, 1080, 0, visual_info.depth, 0, visual_info.visual, CWBackPixel | CWColormap | CWEventMask, &attr);
    XMapWindow(display, w);
    XSelectInput(display, w, ExposureMask | StructureNotifyMask | ButtonMotionMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    XFlush(display);

    XSizeHints *size_hints = XAllocSizeHints();
    size_hints->flags = PMinSize | PMaxSize;
    size_hints->min_width = size_hints->max_width = 1920;
    size_hints->min_height = size_hints->max_height = 1080;
    XSetWMNormalHints(display, w, size_hints);
    XFree(size_hints);

    uint8_t *render_buffer = malloc(1920 * 1080 * 4);
    img = XCreateImage(display, visual_info.visual, visual_info.depth, ZPixmap, 0, (char*)render_buffer, 1920, 1080, 32, 0);
    
    XGCValues gcv;
    gcv.graphics_exposures = 0;
    gc = XCreateGC(display, w, GCGraphicsExposures, &gcv);

    RENDERER->ctx = gfx_createContext(CTX_DEFAULT, render_buffer, 1920, 1080);
    gfx_clear(RENDERER->ctx, GFX_RGB(255,0,0));
    gfx_render(RENDERER->ctx);
    XPutImage(display, w, gc, img, 0, 0, 0, 0, 1920, 1080);
    XFlush(display);

    font = gfx_loadFont(RENDERER->ctx, "/usr/share/DejaVuSans.ttf");
    
    if (pthread_create(&SERVER->render_thread, NULL, render_main, NULL) < 0) {
        FATAL("Could not create render thread: %s\n", strerror(errno));
        return 1;
    }

    if (pthread_create(&x11_pthread, NULL, x11_thread, NULL) < 0) {
        FATAL("Could not create X11 thread: %s\n", strerror(errno));
        return 1;
    }

    TRACE_DEBUG("Renderer initialized\n");

    return 0;
}

void renderer_shutdown() {
    TRACE_INFO("Shutting down renderer...\n");

    pthread_cancel(SERVER->render_thread);
    pthread_cancel(x11_pthread);

    if (RENDERER->ctx) {
        free(RENDERER->ctx->backbuffer);
        free(RENDERER->ctx);
    }

    TRACE_DEBUG("Renderer shutdown successfully.\n");
}

inline size_t renderer_getWidth() { return GFX_WIDTH(RENDERER->ctx); }
inline size_t renderer_getHeight() { return GFX_HEIGHT(RENDERER->ctx); }

#endif
