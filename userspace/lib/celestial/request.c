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
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

/* Celestial socket */
int __celestial_socket = -1;

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
    __celestial_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
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

    int i = 1;
    if (ioctl(__celestial_socket, FIONBIO, &i) < 0) {
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
        if (c < 0) {
            fprintf(stderr, "celestial_lib: Connection failed with Celestial\n");
            return NULL;
        }
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

        int p = poll(fds, 1, -1);
        if (p <= 0 || !(fds[0].revents & POLLIN)) {
            fprintf(stderr, "celestial_lib: Poll failed (%d): %s\n", p, strerror(errno));
            return NULL;
        }
        
        // New data available
        // !!! THIS COULD LOSE DATA
        char data[4096];
        ssize_t r = recv(__celestial_socket, data, 4096, 0);
        if (r < 0 && errno == EAGAIN) {
            continue;
        }

        if (r < 0 || r < (ssize_t)sizeof(celestial_req_header_t)) {
            return NULL;
        }
        
        // Malloc and move it
        void *m = malloc(((celestial_req_header_t*)data)->size);
        memcpy(m, data, ((celestial_req_header_t*)data)->size);

        // Is it an event? Process those immediately
        if (((celestial_req_header_t*)m)->magic == CELESTIAL_MAGIC_EVENT) {
            celestial_handleEvent(m);
            continue;
        }

        if (((celestial_req_header_t*)data)->type == type || type == -1) {
            return m;
        }

        // Put it in the queue
        if (!celestial_resp_queue) {
            celestial_resp_queue = list_create("celestial resp queue");
        }
        
        list_append(celestial_resp_queue, m);
    }
}

/**
 * @brief Poll for events with timeout
 * @param timeout The timeout to wait for
 */
void celestial_pollTimeout(int timeout) {
    // Anything in queue?
    if (celestial_resp_queue && celestial_resp_queue->length) {
        node_t *n = list_popleft(celestial_resp_queue);
        while (n) {
            celestial_req_header_t *h = (celestial_req_header_t*)n->value;
            free(n);

            if (h->magic == CELESTIAL_MAGIC_EVENT) {
                celestial_handleEvent(h);
                return;
            }

            n = list_popleft(celestial_resp_queue);
        }
    }

    // Nope, check the sockets
    struct pollfd fds[1];
    fds[0].fd = __celestial_socket;
    fds[0].events = POLLIN;

    int p = poll(fds, 1, timeout);
    if (p <= 0 || !(fds[0].revents & POLLIN)) return;

    while (1) {
        // New data available
        char data[4096];
        ssize_t r = recv(__celestial_socket, data, 4096, 0);
        
        // TODO bail out on this
        if (r < 0 || r < (ssize_t)sizeof(celestial_req_header_t)) return;

        // Malloc and move it
        void *m = malloc(((celestial_req_header_t*)data)->size);
        memcpy(m, data, ((celestial_req_header_t*)data)->size);

        // Is it an event? Process those immediately
        if (((celestial_req_header_t*)m)->magic == CELESTIAL_MAGIC_EVENT) {
            celestial_handleEvent(m);
        } else {
            free(m);
        }
    }
}

/**
 * @brief Poll for events
 * If events are available, the corresponding event handler will be called.
 */
void celestial_poll() {
    celestial_pollTimeout(0);
}

/**
 * @brief Wait for an event on any windows
 * Returns immediately on event
 */
void celestial_pollIndefinite() {
    celestial_pollTimeout(-1);
}

/**
 * @brief Query to see if anything is available on the socket (or in queued)
 * @returns 1 if content is available
 */
int celestial_query() {
    if (celestial_resp_queue->length) return 1;

    struct pollfd fds[1];
    fds[0].fd = __celestial_socket;
    fds[0].events = POLLIN;

    int p = poll(fds, 1, 0);
    if (p <= 0 || !(fds[0].revents & POLLIN)) return 0;
    return 1;
}

/**
 * @brief Get the Celestial socket file descriptor
 * @returns The socket file descriptor if it exists or -1 if it is not ready
 */
int celestial_getSocketFile() {
    return __celestial_socket;
}

/**
 * @brief Change the Celestial mouse cursor
 * @param cursor_id The ID of the target cursor (CELESTIAL_MOUSE_xxx)
 */
int celestial_setMouseCursor(int cursor_id) {
    celestial_req_set_mouse_cursor_t req = {
        .type = CELESTIAL_REQ_SET_MOUSE_CURSOR,
        .size = sizeof(celestial_req_set_mouse_cursor_t),
        .magic = CELESTIAL_MAGIC,
        .cursor = cursor_id
    };

    celestial_sendRequest(&req, req.size);

    celestial_resp_error_t *e = celestial_getResponse(CELESTIAL_REQ_SET_MOUSE_CURSOR);
    if (!e) return -1;

    CELESTIAL_HANDLE_RESP_ERROR(e, -1);
    free(e);
    return 0;
}

/**
 * @brief Query a specific window
 * @param wid The window to query
 * @param info The output information buffer
 */
int celestial_queryWindow(wid_t wid, window_info_t *output) {
    celestial_req_query_window_t req = {
        .type = CELESTIAL_REQ_QUERY_WINDOW,
        .size = sizeof(celestial_req_query_window_t),
        .magic = CELESTIAL_MAGIC,
        .query = wid
    };

    celestial_sendRequest(&req, req.size);

    celestial_resp_query_window_t *resp = celestial_getResponse(CELESTIAL_REQ_QUERY_WINDOW);
    if (!resp) return -1;

    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    output->width = resp->width;
    output->height = resp->height;
    strncpy(output->name, resp->name, 128);
    strncpy(output->icon, resp->icon, 128);

    free(resp);
    return 0;
}

/**
 * @brief Query window IDs
 * @param nids Pointer to store number of window ids
 * @param wids A pointer to an allocated list of window IDs along with the number of them
 * @returns 0 on success
 */
int celestial_queryWindowIDs(size_t *nids, wid_t **wids) {
    celestial_req_query_window_ids_t req = {
        .type = CELESTIAL_REQ_QUERY_WINDOW_IDS,
        .size = sizeof(celestial_req_query_window_ids_t),
        .magic = CELESTIAL_MAGIC
    };

    if (celestial_sendRequest(&req, req.size) < 0) return -1;

    celestial_resp_query_window_ids_t *resp = celestial_getResponse(CELESTIAL_REQ_QUERY_WINDOW_IDS);
    if (!resp) return -1;

    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    *(nids) = resp->nwids;
    *(wids) = malloc(sizeof(wid_t) * resp->nwids);
    memcpy(*(wids), resp->wids, sizeof(wid_t) * resp->nwids);

    free(resp);
    return 0;
} 


/**
 * @brief Query mouse
 * @param out_x Output X
 * @param out_y Output Y
 */
int celestial_queryMouse(int *x, int *y) {
    celestial_req_query_mouse_t req = {
        .type = CELESTIAL_REQ_QUERY_MOUSE,
        .size = sizeof(celestial_req_query_mouse_t),
        .magic = CELESTIAL_MAGIC,
    };

    if (celestial_sendRequest(&req, req.size)) return -1;

    celestial_resp_query_mouse_t *resp = celestial_getResponse(CELESTIAL_REQ_QUERY_MOUSE);
    if (!resp) return -1;

    CELESTIAL_HANDLE_RESP_ERROR(resp, -1);

    *x = resp->x;
    *y = resp->y;
    
    free(resp);
    return 0;
}
