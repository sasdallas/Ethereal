/**
 * @file userspace/celestial/socket.c
 * @brief Celestial socket server handler
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <sys/un.h>
#include <sys/ioctl.h>

/* Celestial socket file descriptor */
int __celestial_socket = -1;

/* Validate request size matches */
#define CELESTIAL_VALIDATE(req_struct, type) if (hdr->size < sizeof(req_struct)) return socket_error(sock, type, EINVAL)

/**
 * @brief Create and bind the window server socket
 */
void socket_init() {
    WM_SOCK = socket(AF_UNIX, SOCK_STREAM, 0);

    if (WM_SOCK < 0) {
        CELESTIAL_PERROR("socket");
        celestial_fatal();
    }

    // Bind the socket to the default address
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = CELESTIAL_SOCKET_NAME
    };

    if (bind(WM_SOCK, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        CELESTIAL_PERROR("bind");
        CELESTIAL_ERR("socket: Could not bind to %s\n", CELESTIAL_SOCKET_NAME);
        celestial_fatal();
    }

    // Start accepting connections
    if (listen(WM_SOCK, 5) < 0) {
        CELESTIAL_PERROR("listen");
        CELESTIAL_ERR("socket: Could not listen for connections\n");
        celestial_fatal();
    }

    // Set as nonblocking
    int i = 1;
    if (ioctl(WM_SOCK, FIONBIO, &i) < 0) {
        CELESTIAL_PERROR("FIONBIO");
        CELESTIAL_ERR("socket: Could not set socket as nonblocking\n");
        celestial_fatal();
    }
}

/**
 * @brief Handle any new socket accepts
 * @note This is non-blocking, call and just do whatever afterwards
 */
void socket_accept() {
    // Can we accept any new connections?
    int fd;
    if ((fd = accept(WM_SOCK, NULL, NULL)) < 0) {
        if (errno == EWOULDBLOCK) return;
    }

    CELESTIAL_DEBUG("socket: New connection on fd %d\n", fd);
    celestial_addClient(fd, -1);
}

/**
 * @brief Send a response to a socket
 * @param sock The socket to send to
 * @param resp The response to send
 * @returns 0 on success
 */
int socket_sendResponse(int sock, void *resp) {
    CELESTIAL_DEBUG("socket: Send response %d\n", ((celestial_req_header_t*)resp)->type);
    if (send(sock, resp, ((celestial_req_header_t*)resp)->size, 0) < 0) return -1;
    return 0;
}

/**
 * @brief Send error for a socket
 * @param sock The socket to error on
 * @param type The type of request to respond with an error to
 * @param errno The error number
 */
void socket_error(int sock, int type, int errno) {
    celestial_resp_error_t err = {
        .magic = CELESTIAL_MAGIC_ERROR,
        .type = type,
        .size = sizeof(celestial_resp_error_t),
        .errno = errno
    };

    socket_sendResponse(sock, &err);
}

/**
 * @brief New data available on a socket
 * @param sock The socket with data available
 */
void socket_handle(int sock) {
    // Receive a request from the socket
    char data[4096];

    if (recv(sock, data, 4096, 0) < 0) {
        CELESTIAL_PERROR("recv");
        celestial_fatal();
    }

    // Is this a valid request?
    celestial_req_header_t *hdr = (celestial_req_header_t*)data;
    if (hdr->magic != CELESTIAL_MAGIC) {
        return socket_error(sock, hdr->type, EINVAL);
    }

    // Check the type of request
    if (hdr->type == CELESTIAL_REQ_CREATE_WINDOW) {
        // Create window request
        CELESTIAL_VALIDATE(celestial_req_create_window_t, CELESTIAL_REQ_CREATE_WINDOW);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_CREATE_WINDOW\n");
        celestial_req_create_window_t *req = (celestial_req_create_window_t*)hdr;
        wm_window_t *new_win = window_new(sock, req->flags, req->width, req->height);
        
        // Construct response
        celestial_resp_create_window_t resp = {
            .magic = CELESTIAL_MAGIC,
            .type = CELESTIAL_REQ_CREATE_WINDOW,
            .size = sizeof(celestial_resp_create_window_t),
            .id = new_win->id,
        };

        socket_sendResponse(sock, &resp);
        return;
    } else if (hdr->type == CELESTIAL_REQ_GET_WINDOW_INFO) {
        // Get window info request
        CELESTIAL_VALIDATE(celestial_req_get_window_info_t, CELESTIAL_REQ_GET_WINDOW_INFO);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_GET_WINDOW_INFO\n");
        celestial_req_get_window_info_t *req = (celestial_req_get_window_info_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_GET_WINDOW_INFO, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_GET_WINDOW_INFO, EPERM);

        // Construct response
        wm_window_t *win = WID(req->wid);
        
        celestial_resp_get_window_info_t resp = {
            .magic = CELESTIAL_MAGIC,
            .type = CELESTIAL_REQ_GET_WINDOW_INFO,
            .size = sizeof(celestial_resp_get_window_info_t),
            .width = win->width,
            .height = win->height,
            .x = win->x,
            .y = win->y,
            .buffer_key = win->bufkey
        };

        socket_sendResponse(sock, &resp);
        return;
    } else {
        return socket_error(sock, hdr->type, ENOTSUP);
    }
}