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
#include <unistd.h>
#include <assert.h>

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

    // Get window object
    window_t *win = celestial_getWindow(wid);

    // Ready to start receiving events
    celestial_subscribe(win, CELESTIAL_EVENT_DEFAULT_SUBSCRIBED);

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

    // Now update flags so decoration functions start running
    win->flags |= CELESTIAL_WINDOW_FLAG_DECORATED;

    // Ready to start receiving events
    celestial_subscribe(win, CELESTIAL_EVENT_DEFAULT_SUBSCRIBED);

    return wid;
}

/**
 * @brief Set the title of a decorated window
 * @param window The window to set title of decorated
 * @param title The title to set
 */
void celestial_setTitle(window_t *win, char *title) {
    if (!(win->flags & CELESTIAL_WINDOW_FLAG_DECORATED)) return;
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
    win->flags = 0;

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

/**
 * @brief Set the position of a window
 * @param win The window object to set position of
 * @param x The X position to set for the window
 * @param y The Y position to set for the window
 * @returns 0 on success
 */
int celestial_setWindowPosition(window_t *win, int32_t x, int32_t y) {
    int32_t x_adjusted = (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) ? x - win->decor->borders.left_width : x;
    int32_t y_adjusted = (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) ? y - win->decor->borders.top_height : y;
    
    celestial_req_set_window_pos_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_set_window_pos_t),
        .type = CELESTIAL_REQ_SET_WINDOW_POS,
        .wid = win->wid,
        .x = x_adjusted,
        .y = y_adjusted,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_set_window_pos_t *resp = celestial_getResponse(CELESTIAL_REQ_SET_WINDOW_POS);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    // Update window positioning
    if (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) {
        win->info->x = resp->x;
        win->info->y = resp->y;

        win->x = resp->x + win->decor->borders.left_width;
        win->y = resp->y + win->decor->borders.top_height; 
    } else {
        win->x = resp->x;
        win->y = resp->y;
    }

    free(resp);
    return 0;
}


/**
 * @brief Set the Z array of a window
 * @param win The window to set the Z array of
 * @param z The Z array to set
 * @returns 0 on success
 */
int celestial_setZArray(window_t *win, int z) {
    celestial_req_set_z_array_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_set_z_array_t),
        .type = CELESTIAL_REQ_SET_Z_ARRAY,
        .wid = win->wid,
        .array = z,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1;

    // Wait for a resonse
    celestial_resp_ok_t *resp = celestial_getResponse(CELESTIAL_REQ_SET_Z_ARRAY);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    free(resp);
    return 0;
}

/**
 * @brief Flip a window (specific region)
 * @param win The window to flip
 * @param x The X-coordinate of start region
 * @param y The Y-coordinate of start region
 * @param width The width of the region
 * @param heigth The height of the region
 */
void celestial_flipRegion(window_t *win, int32_t x, int32_t y, size_t width, size_t height) {
    // if (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) x += win->decor->borders.left_width;
    // if (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) y += win->decor->borders.top_height;
    
    celestial_req_flip_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_flip_t),
        .type = CELESTIAL_REQ_FLIP,
        .wid = win->wid,
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };

    // Send the request
    // For a flip request, we don't wait for a response
    celestial_sendRequest(&req, req.size);
}

/**
 * @brief Flip/update a window
 * @param win The window to flip
 */
void celestial_flip(window_t *win) {
    // Render everything
    if (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) celestial_flipRegion(win, 0, 0, win->info->width, win->info->height);
    else celestial_flipRegion(win, 0, 0, win->width, win->height);
}

/**
 * @brief Close a window
 * @param win The window to close
 * 
 * @warning Any attempts to update the window from here might crash.
 */
void celestial_closeWindow(window_t *win) {
    celestial_req_close_window_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_close_window_t),
        .type = CELESTIAL_REQ_CLOSE_WINDOW,
        .wid = win->wid,
    };

    celestial_sendRequest(&req, req.size);

    // Close the window shm
    close(win->shmfd);
    
    win->state = CELESTIAL_STATE_CLOSED;
}

/**
 * @brief Resize a window
 * @param win The window to resize
 * @param width The new width of the window
 * @param height The new height of the window
 */
int celestial_resizeWindow(window_t *win, size_t width, size_t height) {
    celestial_req_resize_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_resize_t),
        .type = CELESTIAL_REQ_RESIZE,
        .wid = win->wid,
        .width = width,
        .height = height,
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1;

    // Wait for a resonse
    celestial_resp_resize_t *resp = celestial_getResponse(CELESTIAL_REQ_RESIZE);
    if (!resp) return -1;

    // Handle error in resp
    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    free(resp);
    return 0;
}

/**
 * @brief Complete resize of window (internal method)
 * @param win The window to use
 * @param resize_event The resize event that was received
 */
void celestial_completeWindowResize(window_t *win, celestial_event_resize_t *resize_event) {
    int old_shm_fd = win->shmfd;

    // Update the window data. Depending on whether the window is decorated we need to fake it.
    if (win->flags & CELESTIAL_WINDOW_FLAG_DECORATED) {
        // Ah, this will be fun. We have to reinitialize decorations basically.
        // First, update the shared memory object key
        win->shmfd = -1; // celestial_getFramebuffer opens it
        win->key = resize_event->buffer_key;

        // Almost nothing that isn't in the decorations can be trusted
        munmap(win->decor->ctx->buffer, win->width * win->height * 4);
        win->buffer = NULL; // So that celestial_getFramebuffer returns a proper framebuffer from our buffer key

        // Update the window width + height
        win->width = win->ctx->width;
        win->height = win->ctx->height;

        // Now let's start updating decorations. First, using the decor context, update the real window height
        win->decor->ctx->width = resize_event->new_width;
        win->decor->ctx->height = resize_event->new_height;
        if (win->decor->ctx->backbuffer) win->decor->ctx->backbuffer = realloc(win->decor->ctx->backbuffer, resize_event->new_width * resize_event->new_height * 4);
        win->decor->ctx->buffer = celestial_getFramebuffer(win);
        win->decor->ctx->pitch = resize_event->new_width * 4;

        // Move window graphics context
        win->ctx->buffer = &GFX_PIXEL_REAL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
        if (win->decor->ctx->backbuffer) win->ctx->backbuffer = &GFX_PIXEL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
        win->ctx->width -= win->decor->borders.right_width + win->decor->borders.left_width;
        win->ctx->height -= win->decor->borders.bottom_height + win->decor->borders.top_height;
        win->ctx->pitch = win->decor->ctx->pitch;
    } else {
        // Easy, just update some minor stuff
        // First, update the shared memory object key
        win->shmfd = -1; // celestial_getFramebuffer opens it
        win->key = resize_event->buffer_key;
    
        // Unmap the old buffer
        if (win->buffer) {
            munmap(win->buffer, win->width * win->height * 4);
            win->buffer = NULL; // So that celestial_getFramebuffer returns a proper framebuffer from our buffer key
        }

        // Update width and height
        win->width = resize_event->new_width;
        win->height = resize_event->new_height;

        // Modify the graphics context
        if (!win->ctx) {
            win->ctx = celestial_getGraphicsContext(win);
        } else {
            // Existing graphical context, update it.
            win->ctx->width = resize_event->new_width;
            win->ctx->height = resize_event->new_height;
            win->ctx->pitch = resize_event->new_width * 4;
            if (win->ctx->backbuffer) win->ctx->backbuffer = realloc(win->ctx->backbuffer, GFX_SIZE(win->ctx));
            win->ctx->buffer = celestial_getFramebuffer(win);
        }
    }

    // The resize event is complete
    close(old_shm_fd);
} 