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
        CELESTIAL_ERR("Could not bind to %s\n", CELESTIAL_SOCKET_NAME);
        celestial_fatal();
    }

    // Start accepting connections
    if (listen(WM_SOCK, 5) < 0) {
        CELESTIAL_PERROR("listen");
        CELESTIAL_ERR("Could not listen for connections\n");
        celestial_fatal();
    }

    // Set as nonblocking
    int i = 1;
    if (ioctl(WM_SOCK, FIONBIO, &i) < 0) {
        CELESTIAL_PERROR("FIONBIO");
        CELESTIAL_ERR("Could not set socket as nonblocking\n");
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

    CELESTIAL_DEBUG("New connection on fd %d\n", fd);
    celestial_addClient(fd, -1);
}

/**
 * @brief Send a response to a socket
 * @param sock The socket to send to
 * @param resp The response to send
 * @returns 0 on success
 */
int socket_sendResponse(int sock, void *resp) {
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
    switch (hdr->type) {
        case CELESTIAL_REQ_CREATE_WINDOW:
            CELESTIAL_DEBUG("CELESTIAL_REQ_CREATE_WINDOW\n");
            return socket_error(sock, hdr->type, ENOTSUP);
            
        default:
            return socket_error(sock, hdr->type, ENOTSUP);
    }

}