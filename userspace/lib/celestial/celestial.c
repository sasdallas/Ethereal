/**
 * @file userspace/lib/celestial/celestial.c
 * @brief Main celestial functions
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
 * @brief Main loop for a Celestial window
 */
void celestial_mainLoop() {
    // For now, just collect events
    while (1) {
        void *resp = celestial_getResponse(-1);
        free(resp);
    }
}

/**
 * @brief Get Celestial window server information
 * @returns Celestial server information
 */
celestial_info_t *celestial_getServerInformation() {
    // Send request
    celestial_req_get_server_info_t req = {
        .magic = CELESTIAL_MAGIC,
        .size = sizeof(celestial_req_get_server_info_t),
        .type = CELESTIAL_REQ_GET_SERVER_INFO
    };

    if (celestial_sendRequest(&req, req.size) < 0) return NULL;

    // Wait for a response
    celestial_resp_get_server_info_t *resp = celestial_getResponse(CELESTIAL_REQ_GET_SERVER_INFO);
    if (!resp) return NULL;

    CELESTIAL_HANDLE_RESP_ERROR(resp, NULL);

    // ???
    celestial_info_t *info = malloc(sizeof(celestial_info_t));
    info->screen_width = resp->screen_width;
    info->screen_height = resp->screen_height;

    free(resp);
    return info;
}