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
#include <stdlib.h>
#include <errno.h>

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
 * @brief Get a window object from an ID
 * @param wid The window ID to get the window for
 * @returns A window object or -1 (errno set)
 */
window_t *celestial_getWindow(wid_t wid) {
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
    win->wid = wid;
    win->key = resp->buffer_key;
    win->x = resp->x;
    win->y = resp->y;
    win->width = resp->width;
    win->height = resp->height;

    free(resp);
    return win;
}