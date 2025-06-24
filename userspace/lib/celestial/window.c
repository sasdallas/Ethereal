/**
 * @file userspace/lib/celestial/window.c
 * @brief Celestial window creation
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
#include <sys/ethereal/shared.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <errno.h>
#include <structs/hashmap.h>

/* Global celestial window map (WID -> Window object) */
hashmap_t *celestial_window_map = NULL;

/**
 * @brief Create a new window in Ethereal (undecorated)
 * @param flags The flags to use when creating the window
 * @param width Width of the window
 * @param height Height of the window
 * @returns A window ID or -1
 */
wid_t celestial_createWindowUndecorated(int flags, size_t width, size_t height) {
    if (!width) width = CELESTIAL_DEFAULT_WINDOW_WIDTH;
    if (!height) height = CELESTIAL_DEFAULT_WINDOW_HEIGHT;

    // Build a new request
    celestial_req_create_window_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_CREATE_WINDOW,
        .width = width,
        .height = height,
        .size = sizeof(celestial_req_create_window_t),
        .flags = flags
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_create_window_t *resp = celestial_getResponse(CELESTIAL_REQ_CREATE_WINDOW);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    wid_t wid = resp->id;
    free(resp);
    return wid;
}

/**
 * @brief Create a new window in Ethereal (decorated)
 * @param flags The flags to use when creating the window
 * @param width Width of the window
 * @param height Height of the window
 * @returns A window ID or -1
 */
wid_t celestial_createWindow(int flags, size_t width, size_t height) {
    if (!width) width = CELESTIAL_DEFAULT_WINDOW_WIDTH;
    if (!height) height = CELESTIAL_DEFAULT_WINDOW_HEIGHT;

    // We need to adjust width and height for our decorations
    // Get decoration borders
    decor_borders_t borders = celestial_getDecorationBorders(celestial_getDefaultDecorations());

    // Now that we have borders, we can start working
    size_t width_real = width + borders.left_width + borders.right_width;
    size_t height_real = height + borders.top_height + borders.bottom_height;

    // Build a new request
    celestial_req_create_window_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_CREATE_WINDOW,
        .width = width_real,
        .height = height_real,
        .size = sizeof(celestial_req_create_window_t),
        .flags = flags
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_create_window_t *resp = celestial_getResponse(CELESTIAL_REQ_CREATE_WINDOW);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    wid_t wid = resp->id;
    free(resp);

    // Now we need to create a window object
    window_t *win = celestial_getWindow(wid);
    win->flags |= CELESTIAL_WINDOW_FLAG_DECORATED;

    // Setup real info
    win->info = malloc(sizeof(decor_window_info_t));
    win->info->width = width_real;
    win->info->height = height_real;

    // Get the buffer for the window
    win->decor_buffer = celestial_getFramebuffer(win);
    
    // Initialize decorations sets up win->decor and win->buffer
    celestial_initDecorationsDefault(win);

    // Fix window width and height
    win->width = width;
    win->height = height;

    return wid;
}

/**
 * @brief Set the title of a decorated window
 * @param window The window to set title of decorated
 * @param title The title to set
 */
void celestial_setTitle(window_t *win, char *title) {
    win->decor->titlebar = title;
    win->decor->render(win);
}

/**
 * @brief Get a window object from an ID
 * @param wid The window ID to get the window for
 * @returns A window object or -1 (errno set)
 */
window_t *celestial_getWindow(wid_t wid) {
    // Do we have a map?
    if (celestial_window_map) {
        window_t *w = (window_t*)hashmap_get(celestial_window_map, (void*)(uintptr_t)wid);
        if (w) return w;
    } else {
        celestial_window_map = hashmap_create_int("celestial window map", 20);
    }
    
    // Build a new request
    celestial_req_get_window_info_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_GET_WINDOW_INFO,
        .size = sizeof(celestial_req_get_window_info_t),
        .wid = wid,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return NULL;

    // Wait for a response
    celestial_resp_get_window_info_t *resp = celestial_getResponse(CELESTIAL_REQ_GET_WINDOW_INFO);
    if (!resp) return NULL;

    // Handle error
    CELESTIAL_HANDLE_RESP_ERROR(resp, NULL);

    window_t *win = malloc(sizeof(window_t));
    win->shmfd = -1;
    win->buffer = NULL;
    win->wid = wid;
    win->key = resp->buffer_key;
    win->x = resp->x;
    win->y = resp->y;
    win->width = resp->width;
    win->height = resp->height;
    win->event_handler_map = hashmap_create_int("event handler map", 20);
    win->ctx = NULL;
    win->decor = NULL;
    win->decor_buffer = NULL;
    win->info = NULL;

    hashmap_set(celestial_window_map, (void*)(uintptr_t)wid, win);

    free(resp);
    return win;
}

/**
 * @brief Get a raw framebuffer that you can draw to
 * @param win The window object to get a framebuffer for
 * @returns A framebuffer object or NULL (errno set)
 */
uint32_t *celestial_getFramebuffer(window_t *win) {
    // Obtain SHM mapping for window
    if (win->shmfd < 0) {
        // We need to get the shared mapping
        win->shmfd = shared_open(win->key);
        if (win->shmfd < 0) {
            return NULL;
        }

        // Map it into memory
        win->buffer = mmap(NULL, win->width * win->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, win->shmfd, 0);
        if (win->buffer == MAP_FAILED) {
            return NULL;
        }
    }

    return win->buffer;
}

/**
 * @brief Start dragging a window
 * @param win The window object to start dragging
 * @returns 0 on success or -1 
 * 
 * This will lock the mouse cursor in place and have it continuously drag the window.
 * Usually, unless you have an undecorated window, don't use this.
 */
int celestial_startDragging(window_t *win) {
    // Build a new request
    celestial_req_drag_start_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_DRAG_START,
        .size = sizeof(celestial_req_create_window_t),
        .wid = win->wid,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_ok_t *resp = celestial_getResponse(CELESTIAL_REQ_DRAG_START);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    free(resp);
    return 0;
}

/**
 * @brief Stop dragging a window
 * @param win The window object to stop dragging
 * @returns 0 on success
 *  
 * This will unlock the mouse cursor.
 * Usually, unless you have an undecorated window, don't use this.
 */
int celestial_stopDragging(window_t *win) {
    // Build a new request
    celestial_req_drag_stop_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_DRAG_STOP,
        .size = sizeof(celestial_req_create_window_t),
        .wid = win->wid,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_ok_t *resp = celestial_getResponse(CELESTIAL_REQ_DRAG_STOP);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    free(resp);
    return 0;
}

/**
 * @brief Initialize graphics for a window
 * @param win The window object to initialize graphics for
 * @param flags Flags to use during context creation
 * @returns Graphics object
 */
gfx_context_t *celestial_initGraphics(window_t *win, int flags) {
    if (win->ctx) {
        // Either they've done this before or Celestial set up some decorations for it
        // Although, we need to adjust the context if flags are different
        // Do not adjust buffers for windows that are decorated. Celestial will fill the proper details in.
        if (win->ctx->flags != flags) {
            if (flags & CTX_NO_BACKBUFFER) {
                win->ctx->flags |= (CTX_NO_BACKBUFFER);
                if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED)) free(win->ctx->backbuffer);
            } else {
                win->ctx->flags &= ~(CTX_NO_BACKBUFFER);
                if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED)) win->ctx->backbuffer = malloc(GFX_WIDTH(win->ctx) * GFX_HEIGHT(win->ctx) + (win->ctx->bpp/8));
            }
        }

        return win->ctx; 
    }

    win->ctx = gfx_createContext(flags, (uint8_t*)celestial_getFramebuffer(win), win->width, win->height);
    return win->ctx;
}


/**
 * @brief Get graphical context for a window
 * @param win The window object to get the graphics context for
 * @returns Graphics context
 */
gfx_context_t *celestial_getGraphicsContext(window_t *win) {
    if (!win->ctx) return celestial_initGraphics(win, CTX_DEFAULT);
    return win->ctx;
}