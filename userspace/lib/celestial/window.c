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
 * @returns A window ID or -1
 */
wid_t celestial_createWindowUndecorated(int flags) {
    // Build a new request
    celestial_req_create_window_t req = {
        .magic = CELESTIAL_MAGIC,
        .type = CELESTIAL_REQ_CREATE_WINDOW,
        .size = sizeof(celestial_req_create_window_t),
        .flags = flags
    };

    // Send the request
    if (celestial_sendRequest(&req, req.size) < 0) return -1; 

    // Wait for a response
    celestial_resp_create_window_t *resp = celestial_getResponse(CELESTIAL_REQ_CREATE_WINDOW);
    if (!resp) return -1;

    // Handle error in resp
    if (resp->magic == CELESTIAL_MAGIC_ERROR) {
        celestial_resp_error_t *err = (celestial_resp_error_t*)resp;
        errno = err->errno;
        free(err);
        return -1;
    }

    wid_t wid = resp->id;
    free(resp);
    return wid;
}