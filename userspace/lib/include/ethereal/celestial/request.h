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
#include <ethereal/keyboard.h>
#include <stdbool.h>

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
#define CELESTIAL_REQ_RESIZE                0x100F
#define CELESTIAL_REQ_SET_WINDOW_VISIBLE    0x1010
#define CELESTIAL_REQ_SET_MOUSE_CAPTURE     0x1011
#define CELESTIAL_REQ_SET_MOUSE_CURSOR      0x1012
#define CELESTIAL_REQ_ANNOUNCE_WINDOW       0x1013
#define CELESTIAL_REQ_QUERY_WINDOW          0x1014
#define CELESTIAL_REQ_QUERY_WINDOW_IDS      0x1015
#define CELESTIAL_REQ_BIND_KEY              0x1016
#define CELESTIAL_REQ_SET_ROOT_WINDOW       0x1017
#define CELESTIAL_REQ_QUERY_MOUSE           0x1018
#define CELESTIAL_REQ_SET_RESIZE_PARAMS     0x1019
#define CELESTIAL_REQ_START_RESIZE          0x101A
#define CELESTIAL_REQ_STOP_RESIZE           0x101B
#define CELESTIAL_REQ_ACK_RESIZE            0x101C

#define CELESTIAL_REQ_COMMON                uint32_t magic;\
                                            uint16_t type; \
                                            size_t size; \

#define CELESTIAL_DEFAULT_SOCKET_NAME       "/comm/wndsrv"

/* Mouse types */
#define CELESTIAL_MOUSE_DEFAULT             0   // Default pointer
#define CELESTIAL_MOUSE_TEXT                1   // I-Beam for text
#define CELESTIAL_MOUSE_HORIZONTAL          2   // Horizontal resize
#define CELESTIAL_MOUSE_VERTICAL            3   // Vertical resize
#define CELESTIAL_MOUSE_DIAG_ASCEND         4   // Diagonal ascending resize
#define CELESTIAL_MOUSE_DIAG_DESCEND        5   // Diagonal descending resize

/* Resize direction */
#define CELESTIAL_RESIZE_TOP                    0
#define CELESTIAL_RESIZE_RIGHT                  1
#define CELESTIAL_RESIZE_LEFT                   2
#define CELESTIAL_RESIZE_BOTTOM                 3
#define CELESTIAL_RESIZE_TOP_LEFT               4
#define CELESTIAL_RESIZE_TOP_RIGHT              5
#define CELESTIAL_RESIZE_BOTTOM_RIGHT           6
#define CELESTIAL_RESIZE_BOTTOM_LEFT            7

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

typedef struct celestial_req_resize {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    size_t width;                       // Width
    size_t height;                      // Height
} celestial_req_resize_t;

typedef struct celestial_req_set_window_visible {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint8_t visible;                    // Visible
} celestial_req_set_window_visible_t;

typedef struct celestial_req_set_mouse_capture {
    CELESTIAL_REQ_COMMON                // Common
    wid_t wid;                          // Window ID
    uint8_t capture;                    // Capture state
} celestial_req_set_mouse_capture_t;

typedef struct celestial_req_set_mouse_cursor {
    CELESTIAL_REQ_COMMON                // Common
    uint8_t cursor;                     // Cursor ID
} celestial_req_set_mouse_cursor_t;

typedef struct celestial_req_announce_window {
    CELESTIAL_REQ_COMMON
    wid_t wid;                          // Window ID to announce
    char name[128];                     // Name of window
    char icon[128];                     // Name of icon (/usr/share/icons/32/(icon))
                                        // TODO: allow full icon images to be passed over this
} celestial_req_announce_window_t;

typedef struct celestial_req_query_window {
    CELESTIAL_REQ_COMMON
    wid_t query;                        // Window ID to query
} celestial_req_query_window_t;

typedef struct celestial_req_query_window_ids {
    CELESTIAL_REQ_COMMON
} celestial_req_query_window_ids_t;

typedef struct celestial_req_bind_key {
    CELESTIAL_REQ_COMMON
    wid_t wid;                          // Window to bind to
    key_scancode_t scancode;
    key_modifiers_t modifiers;
    bool capture;                       // Whether to stop the key event from going to its rightful window
} celestial_req_bind_key_t;

typedef struct celestial_req_set_root_window {
    CELESTIAL_REQ_COMMON
    wid_t wid;
} celestial_req_set_root_window_t;


typedef struct celestial_req_query_mouse {
    CELESTIAL_REQ_COMMON
} celestial_req_query_mouse_t;

typedef struct celestial_req_set_resize_params {
    CELESTIAL_REQ_COMMON
    wid_t wid;
    size_t min_width;
    size_t min_height;
    size_t max_width;
    size_t max_height;
} celestial_req_set_resize_params_t;

typedef struct celestial_req_start_resize {
    CELESTIAL_REQ_COMMON
    wid_t wid;
    unsigned char direction;
} celestial_req_start_resize_t;

typedef struct celestial_req_stop_resize {
    CELESTIAL_REQ_COMMON
    wid_t wid;
} celestial_req_stop_resize_t;

typedef struct celestial_req_ack_resize {
    CELESTIAL_REQ_COMMON
    wid_t wid;
} celestial_req_ack_resize_t;

/* RESPONSES */

/* Generic error response */
typedef struct celestial_resp_error {
    CELESTIAL_REQ_COMMON                // Common
    int error;                          // Errno
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

typedef struct celestial_resp_resize {
    CELESTIAL_REQ_COMMON                // Common
    size_t width;                       // New width
    size_t height;                      // New height
} celestial_resp_resize_t;

typedef struct celestial_resp_query_window {
    CELESTIAL_REQ_COMMON                // Common
    char name[128];
    char icon[128];
    size_t width;
    size_t height;
} celestial_resp_query_window_t;

typedef struct celestial_resp_query_window_ids {
    CELESTIAL_REQ_COMMON                // Common
    size_t nwids;
    wid_t wids[];
} celestial_resp_query_window_ids_t;

typedef struct celestial_resp_query_mouse {
    CELESTIAL_REQ_COMMON
    int x;
    int y;
} celestial_resp_query_mouse_t;

/**** MACROS ****/

/* Internal macro */
#define CELESTIAL_HANDLE_RESP_ERROR(resp, errval)   if (resp->magic == CELESTIAL_MAGIC_ERROR) { \
    celestial_resp_error_t *err = (celestial_resp_error_t*)resp; \
    errno = err->error; \
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

/**
 * @brief Poll for events with timeout
 * @param timeout The timeout to wait for
 */
void celestial_pollTimeout(int timeout);

/**
 * @brief Poll for events
 * If events are available, the corresponding event handler will be called.
 */
void celestial_poll();

/**
 * @brief Wait for an event on any windows
 * Returns immediately on event
 */
void celestial_pollIndefinite();

/**
 * @brief Query to see if anything is available on the socket (or in queued)
 * @returns 1 if content is available
 */
int celestial_query();

/**
 * @brief Get the Celestial socket file descriptor
 * @returns The socket file descriptor if it exists or -1 if it is not ready
 */
int celestial_getSocketFile();

/**
 * @brief Change the Celestial mouse cursor
 * @param cursor_id The ID of the target cursor (CELESTIAL_MOUSE_xxx)
 */
int celestial_setMouseCursor(int cursor_id);

/**
 * @brief Query mouse
 * @param out_x Output X
 * @param out_y Output Y
 */
int celestial_queryMouse(int *x, int *y);

struct _window_info;

/**
 * @brief Query a specific window
 * @param wid The window to query
 * @param info The output information buffer
 */
int celestial_queryWindow(wid_t wid, struct _window_info *output);

/**
 * @brief Query window IDs
 * @param nids Pointer to store number of window ids
 * @param wids A pointer to an allocated list of window IDs along with the number of them
 * @returns 0 on success
 */
int celestial_queryWindowIDs(size_t *nids, wid_t **wids);

#endif