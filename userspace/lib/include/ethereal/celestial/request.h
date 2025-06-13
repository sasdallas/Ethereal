/**
 * @file userspace/lib/include/ethereal/celestial/request.h
 * @brief Celestial request framework
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_REQUEST_H
#define _CELESTIAL_REQUEST_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <ethereal/celestial/types.h>

/**** DEFINITIONS ****/

#define CELESTIAL_MAGIC                     0x45485445
#define CELESTIAL_MAGIC_ERROR               0x00525245

#define CELESTIAL_REQ_CREATE_WINDOW         0x1000
#define CELESTIAL_REQ_DESTROY_WINDOW        0x1001
#define CELESTIAL_REQ_GET_WINDOW_INFO       0x1002
#define CELESTIAL_REQ_SET_WINDOW_POS        0x1003
#define CELESTIAL_REQ_SUBSCRIBE             0x1004
#define CELESTIAL_REQ_UNSUBSCRIBE           0x1005

#define CELESTIAL_REQ_COMMON                uint32_t magic;\
                                            uint16_t type; \
                                            size_t size; \

#define CELESTIAL_DEFAULT_SOCKET_NAME       "/comm/wndsrv"

/**** TYPES ****/

typedef struct celestial_req_header {
    CELESTIAL_REQ_COMMON
} celestial_req_header_t;

typedef struct celestial_req_create_window {
    CELESTIAL_REQ_COMMON                // Common
    int flags;                          // Window flags
} celestial_req_create_window_t;

typedef struct celestial_resp_error {
    CELESTIAL_REQ_COMMON                // Common
    int errno;                          // Errno
} celestial_resp_error_t;

typedef struct celestial_resp_create_window {
    CELESTIAL_REQ_COMMON                // Common
    wid_t id;                           // ID of the new window
} celestial_resp_create_window_t;

/**** FUNCTIONS ****/

/**
 * @brief Connect to a Celestial window server
 * @param sockname The socket of the window server
 * @returns 0 on success
 */
int celestial_connect(char *sockname);

/**
 * @brief Send a request to the Celestial window sever
 * @param req The request to send
 * @param size The sizeof the request
 * @returns 0 on success
 */
int celestial_sendRequest(void *req, size_t size);

/**
 * @brief Wait for a specific response type
 * @param type The type of response to wait for (or -1 if you don't care)
 * @returns The response pointer or NULL
 */
void *celestial_getResponse(int type);

#endif