/**
 * @file userspace/lib/include/ethereal/celestial/request.h
 * @brief Celestial request framework
 * 
 * 
 * The client sends requests to the Celestial server and the celestial server replies with 
 * a reponse. This response, depending on the request, can be an error response, a generic "OK" response, or a specific response
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
#define CELESTIAL_MAGIC_OK                  0x00004B4F
#define CELESTIAL_MAGIC_ERROR               0x00525245

#define CELESTIAL_REQ_CREATE_WINDOW         0x1000
#define CELESTIAL_REQ_DESTROY_WINDOW        0x1001
#define CELESTIAL_REQ_GET_WINDOW_INFO       0x1002
#define CELESTIAL_REQ_SET_WINDOW_POS        0x1003
#define CELESTIAL_REQ_SUBSCRIBE             0x1004
#define CELESTIAL_REQ_UNSUBSCRIBE           0x1005
#define CELESTIAL_REQ_DRAG_START            0x1006
#define CELESTIAL_REQ_DRAG_STOP             0x1007
#define CELESTIAL_REQ_GET_SERVER_INFO       0x1008
#define CELESTIAL_REQ_CLOSE_WINDOW          0x1009
#define CELESTIAL_REQ_MINIMIZE_WINDOW       0x100A
#define CELESTIAL_REQ_MAXIMIZE_WINDOW       0x100B
#define CELESTIAL_REQ_SET_FOCUSED           0x100C
#define CELESTIAL_REQ_SET_Z_ARRAY           0x100D
#define CELESTIAL_REQ_FLIP                  0x100E

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
    size_t width;                       // Width of the window
    size_t height;                      // Height of the window
} celestial_req_create_window_t;

typedef struct celestial_req_get_window_info {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_get_window_info_t;

typedef struct celestial_req_set_window_pos {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    int32_t x;                          // X
    int32_t y;                          // Y
} celestial_req_set_window_pos_t;

typedef struct celestial_req_subscribe {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint32_t events;                    // Events to subscribe to
} celestial_req_subscribe_t;

typedef struct celestial_req_unsubscribe {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint32_t events;                    // Events to unsubscribe from
} celestial_req_unsubscribe_t;

typedef struct celestial_req_drag_start {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_drag_start_t;

typedef struct celestial_req_drag_stop {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_drag_stop_t;

typedef struct celestial_req_get_server_info {
    CELESTIAL_REQ_COMMON                // Common
} celestial_req_get_server_info_t;

typedef struct celestial_req_close_window {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_close_window_t;

typedef struct celestial_req_minimize_window {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_minimize_window_t;

typedef struct celestial_req_maximize_window {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
} celestial_req_close_maximize_t;

typedef struct celestial_req_set_focused {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint8_t focused;                    // State of focused
} celestial_req_set_focused_t;

typedef struct celestial_req_set_z_array {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint8_t array;                      // The Z array to set
} celestial_req_set_z_array_t;

typedef struct celestial_req_flip {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    int32_t x;                          // (window-relative) X coordinate start
    int32_t y;                          // (window-relative) Y coordinate start
    size_t width;                       // Width of region
    size_t height;                      // Height of region
} celestial_req_flip_t;

/* RESPONSES */

/* Generic error response */
typedef struct celestial_resp_error {
    CELESTIAL_REQ_COMMON                // Common
    int errno;                          // Errno
} celestial_resp_error_t;

/* Generic OK response */
typedef struct celestial_resp_ok {
    CELESTIAL_REQ_COMMON                // Common
} celestial_resp_ok_t;

typedef struct celestial_resp_create_window {
    CELESTIAL_REQ_COMMON                // Common
    wid_t id;                           // ID of the new window
} celestial_resp_create_window_t;

typedef struct celestial_resp_get_window_info {
    CELESTIAL_REQ_COMMON                // Common
    int32_t x;                          // X position
    int32_t y;                          // Y position
    size_t width;                       // Width of the window
    size_t height;                      // Height of the window
    key_t buffer_key;                   // Ethereal shared memory key
} celestial_resp_get_window_info_t;

typedef struct celestial_resp_get_server_info {
    CELESTIAL_REQ_COMMON                // Common
    size_t screen_width;                // Width of the screen
    size_t screen_height;               // Height of the screen
} celestial_resp_get_server_info_t;

typedef struct celestial_resp_set_window_pos {
    CELESTIAL_REQ_COMMON                // Common
    int32_t x;                          // Window X position
    int32_t y;                          // Window Y position
} celestial_resp_set_window_pos_t;

/**** MACROS ****/

/* Internal macro */
#define CELESTIAL_HANDLE_RESP_ERROR(resp, errval)   if (resp->magic == CELESTIAL_MAGIC_ERROR) { \
    celestial_resp_error_t *err = (celestial_resp_error_t*)resp; \
    errno = err->errno; \
    free(err); \
    return errval; \
}


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