/**
 * @file userspace/lib/celestial/request.c
 * @brief Celestial request API
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
#include <stdio.h>
#include <structs/list.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

/* Celestial socket */
static int __celestial_socket = -1;

/* Celestial queued responses */
static list_t *celestial_resp_queue = NULL;

/**
 * @brief Connect to a Celestial window server
 * @param sockname The socket of the window server
 * @returns 0 on success
 */
int celestial_connect(char *sockname) {
    if (!(__celestial_socket < 0)) {
        errno = EISCONN;
        return -1;
    }

    // Create a new socket
    __celestial_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (__celestial_socket < 0) {
        return -1;
    }

    // Connect to the server
    struct sockaddr_un un = {
        .sun_family = AF_UNIX
    };

    strncpy(un.sun_path, sockname, 108);

    if (connect(__celestial_socket, (const struct sockaddr*)&un, sizeof(struct sockaddr_un)) < 0) {
        return -1;
    }

    // Connection established
    return 0;
}

/**
 * @brief Send a request to the Celestial window sever
 * @param req The request to send
 * @param size The sizeof the request
 * @returns 0 on success
 */
int celestial_sendRequest(void *req, size_t size) {
    if (__celestial_socket < 0) {
        int c = celestial_connect(CELESTIAL_DEFAULT_SOCKET_NAME);
        if (c < 0) return c;
    } 

    // Send it
    if (send(__celestial_socket, req, size, 0) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Wait for a specific response type
 * @param type The type of response to wait for (or -1 if you don't care)
 * @returns The response pointer or NULL
 */
void *celestial_getResponse(int type) {
    if (__celestial_socket < 0) {
        int c = celestial_connect(CELESTIAL_DEFAULT_SOCKET_NAME);
        if (c < 0) return NULL;
    } 

    // Wait for a response
    if (celestial_resp_queue && celestial_resp_queue->length) {
        foreach(resp_node, celestial_resp_queue) {
            celestial_req_header_t *h = (celestial_req_header_t*)resp_node->value;
            if (h->type == type || type == -1) {
                list_delete(celestial_resp_queue, resp_node);
                return h;
            }
        }
    }

    // Else we have to poll from the socket
    while (1) {
        struct pollfd fds[1];
        fds[0].fd = __celestial_socket;
        fds[0].events = POLLIN;

        fprintf(stderr, "celestial: [lib] Waiting for events from Celestial\n");
        int p = poll(fds, 1, -1);
        if (p <= 0 || !(fds[0].revents & POLLIN)) return NULL;

        // New data available
        char data[4096];
        ssize_t r = recv(__celestial_socket, data, 4096, 0);
        if (r < 0 || r < (ssize_t)sizeof(celestial_req_header_t)) return NULL;

        // Malloc and move it
        void *m = malloc(((celestial_req_header_t*)data)->size);
        memcpy(m, data, ((celestial_req_header_t*)data)->size);

        // Is it an event? Process those immediately
        if (((celestial_req_header_t*)m)->magic == CELESTIAL_MAGIC_EVENT) {
            fprintf(stderr, "Received event from Celestial\n");
            celestial_handleEvent(m);
            continue;
        }

        if (((celestial_req_header_t*)data)->type == type || type == -1) {
            fprintf(stderr, "celestial: [lib] Received response\n");
            return m;
        }

        // Put it in the queue
        if (!celestial_resp_queue) celestial_resp_queue = list_create("celestial resp queue");
        list_append(celestial_resp_queue, m);
    }
}