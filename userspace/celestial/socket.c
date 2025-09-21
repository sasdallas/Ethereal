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
#include <sys/mman.h>

/* Celestial socket file descriptor */
int __celestial_socket = -1;

/* Validate request size matches */
#define CELESTIAL_VALIDATE(req_struct, type) if (hdr->size < sizeof(req_struct)) return socket_error(sock, type, EINVAL)

/**
 * @brief Create and bind the window server socket
 */
void socket_init() {
    WM_SOCK = socket(AF_UNIX, SOCK_SEQPACKET, 0);

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
        CELESTIAL_PERROR("accept");
        celestial_fatal();
    }

    // Mark this socket as nonblocking
    int i = 1;
    if (ioctl(fd, FIONBIO, &i) < 0) {
        CELESTIAL_PERROR("FIONBIO");
        CELESTIAL_ERR("socket: Could not set socket as nonblocking\n");
        celestial_fatal();
    }

    CELESTIAL_DEBUG("socket: New connection on fd %d\n", fd);
    celestial_addClient(fd, -1);
}

/**
 * @brief Send a packet to a socket
 * @param sock The socket to send the packet to
 * @param size The size of the packet
 * @param packet The packet to send
 * @returns 0 on success
 */
int socket_send(int sock, size_t size, void *packet) {
    ssize_t r = send(sock, packet, size, 0);
    if (r < 0) return -1;

    return 0;
}

/**
 * @brief Send a response to a socket
 * @param sock The socket to send to
 * @param resp The response to send
 * @returns 0 on success
 */
int socket_sendResponse(int sock, void *resp) {
    CELESTIAL_DEBUG("socket: Send response %d\n", ((celestial_req_header_t*)resp)->type);
    int r = socket_send(sock, ((celestial_req_header_t*)resp)->size, resp);
    return r;
}

/**
 * @brief Send error for a socket
 * @param sock The socket to error on
 * @param type The type of request to respond with an error to
 * @param error The error number
 */
void socket_error(int sock, int type, int error) {
    celestial_resp_error_t err = {
        .magic = CELESTIAL_MAGIC_ERROR,
        .type = type,
        .size = sizeof(celestial_resp_error_t),
        .error = error
    };

    CELESTIAL_LOG("Warning: Sending error %s to socket\n", strerror(error));

    socket_sendResponse(sock, &err);
}

/**
 * @brief Send OK on a socket
 * @param sock The socket to error on
 * @param type The type of request to respond with an ok to
 */
void socket_ok(int sock, int type) {
    celestial_resp_ok_t ok = {
        .magic = CELESTIAL_MAGIC_OK,
        .type = type,
        .size = sizeof(celestial_resp_ok_t),
    };

    socket_sendResponse(sock, &ok);
}

/**
 * @brief New data available on a socket
 * @param sock The socket with data available
 */
void socket_handle(int sock) {
    // Receive a request from the socket
    char data[4096];

    if (recv(sock, data, 4096, 0) < 0) {
        if (errno == EWOULDBLOCK) return;
        
        // Was the error ECONNRESET? We should catch that
        if (errno == ECONNRESET) {
            CELESTIAL_LOG("socket: Connection with socket %d was closed by remote peer.\n", sock);
            
            // Free all resources allocated by the socket
            celestial_removeClient(sock);
            close(sock);

            // Find any windows used by the socket
            // TODO: (really TODO) WM_SW_MAP should also have a list of windows..
            foreach(winn, WM_WINDOW_LIST) {
                wm_window_t *win = (wm_window_t*)winn->value;
                if (win && win->sock == sock) {
                    window_close(win);
                }
            }

            foreach(winn, WM_WINDOW_LIST_BG) {
                wm_window_t *win = (wm_window_t*)winn->value;
                if (win && win->sock == sock) {
                    window_close(win);
                }
            }

            foreach(winn, WM_WINDOW_LIST_OVERLAY) {
                wm_window_t *win = (wm_window_t*)winn->value;
                if (win && win->sock == sock) {
                    window_close(win);
                }
            }

            return;
        }

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

        if (req->width > GFX_WIDTH(WM_GFX)) req->width = GFX_WIDTH(WM_GFX);
        if (req->height > GFX_HEIGHT(WM_GFX)) req->height = GFX_HEIGHT(WM_GFX); 

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
    } else if (hdr->type == CELESTIAL_REQ_SET_WINDOW_POS) {
        // Get window info request
        CELESTIAL_VALIDATE(celestial_req_set_window_pos_t, CELESTIAL_REQ_SET_WINDOW_POS);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_SET_WINDOW_POS\n");
        celestial_req_set_window_pos_t *req = (celestial_req_set_window_pos_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_POS, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_POS, EPERM);

        // Adjust bounds
        wm_window_t *win = WID(req->wid);
        if (req->x < 0) req->x = 0;
        if (req->x + win->width > GFX_WIDTH(WM_GFX)) req->x = GFX_WIDTH(WM_GFX) - win->width;
        if (req->y < 0) req->y = 0;
        if (req->y + win->height > GFX_HEIGHT(WM_GFX)) req->y = GFX_HEIGHT(WM_GFX) - win->height;

        gfx_rect_t old_rect = GFX_RECT(win->x, win->y, win->width, win->height);

        win->x = req->x;
        win->y = req->y;

        window_updateRegion(old_rect);

        celestial_resp_set_window_pos_t resp = {
            .magic = CELESTIAL_MAGIC,
            .size = sizeof(celestial_resp_set_window_pos_t),
            .type = CELESTIAL_REQ_SET_WINDOW_POS,
            .x = win->x,
            .y = win->y
        };

        socket_sendResponse(sock, &resp);
        return;
    } else if (hdr->type == CELESTIAL_REQ_SUBSCRIBE) {
        // Subscribe request
        CELESTIAL_VALIDATE(celestial_req_subscribe_t, CELESTIAL_REQ_SUBSCRIBE);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_SUBSCRIBE\n");
        celestial_req_subscribe_t *req = (celestial_req_subscribe_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_SUBSCRIBE, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_SUBSCRIBE, EPERM);

        wm_window_t *win = WID(req->wid);
        win->events |= req->events;

        return socket_ok(sock, CELESTIAL_REQ_SUBSCRIBE);
    } else if (hdr->type == CELESTIAL_REQ_UNSUBSCRIBE) {
        CELESTIAL_VALIDATE(celestial_req_unsubscribe_t, CELESTIAL_REQ_UNSUBSCRIBE);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_UNSUBSCRIBE\n");
        celestial_req_unsubscribe_t *req = (celestial_req_unsubscribe_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_UNSUBSCRIBE, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_UNSUBSCRIBE, EPERM);

        wm_window_t *win = WID(req->wid);
        win->events &= ~(req->events);

        return socket_ok(sock, CELESTIAL_REQ_UNSUBSCRIBE);
    } else if (hdr->type == CELESTIAL_REQ_DRAG_START) {
        // Start drag request
        CELESTIAL_VALIDATE(celestial_req_drag_start_t, CELESTIAL_REQ_DRAG_START);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_DRAG_START\n");
        celestial_req_drag_start_t *req = (celestial_req_drag_start_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_DRAG_START, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_DRAG_START, EPERM);

        wm_window_t *win = WID(req->wid);
        win->state = WINDOW_STATE_DRAGGING;

        return socket_ok(sock, CELESTIAL_REQ_DRAG_START);
    } else if (hdr->type == CELESTIAL_REQ_DRAG_STOP) {
        // Start drag request
        CELESTIAL_VALIDATE(celestial_req_drag_stop_t, CELESTIAL_REQ_DRAG_STOP);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_DRAG_STOP\n");
        celestial_req_drag_stop_t *req = (celestial_req_drag_stop_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_DRAG_STOP, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_DRAG_STOP, EPERM);

        wm_window_t *win = WID(req->wid);
        win->state = WINDOW_STATE_NORMAL;

        return socket_ok(sock, CELESTIAL_REQ_DRAG_STOP);
    } else if (hdr->type == CELESTIAL_REQ_GET_SERVER_INFO) {
        // Get server info
        CELESTIAL_VALIDATE(celestial_req_get_server_info_t, CELESTIAL_REQ_GET_SERVER_INFO);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_GET_SERVER_INFO\n");
        
        celestial_resp_get_server_info_t resp = {
            .magic = CELESTIAL_MAGIC,
            .size = sizeof(celestial_resp_get_server_info_t),
            .type = CELESTIAL_REQ_GET_SERVER_INFO,
            .screen_width = GFX_WIDTH(WM_GFX),
            .screen_height = GFX_HEIGHT(WM_GFX)
        };

        socket_sendResponse(sock, &resp);
        return;
    } else if (hdr->type == CELESTIAL_REQ_SET_Z_ARRAY) {
        // Set Z array
        CELESTIAL_VALIDATE(celestial_req_set_z_array_t, CELESTIAL_REQ_SET_Z_ARRAY);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_SET_Z_ARRAY\n");
        celestial_req_set_z_array_t *req = (celestial_req_set_z_array_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_SET_Z_ARRAY, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_SET_Z_ARRAY, EPERM);
        if (req->array > CELESTIAL_Z_OVERLAY) return socket_error(sock, CELESTIAL_REQ_SET_Z_ARRAY, EINVAL);

        // Drop the window from its previous queue
        wm_window_t *win = WID(req->wid);

        switch (win->z_array) {
            case CELESTIAL_Z_BACKGROUND:
                list_delete(WM_WINDOW_LIST_BG, list_find(WM_WINDOW_LIST_BG, win));
                break;
            case CELESTIAL_Z_DEFAULT:
                list_delete(WM_WINDOW_LIST, list_find(WM_WINDOW_LIST, win));
                break;
            case CELESTIAL_Z_OVERLAY:
                list_delete(WM_WINDOW_LIST_OVERLAY, list_find(WM_WINDOW_LIST, win));
                break;
        }

        // Now add it to the new queue
        switch (req->array) {
            case CELESTIAL_Z_BACKGROUND:
                list_append(WM_WINDOW_LIST_BG, win);
                break;
            case CELESTIAL_Z_DEFAULT:
                list_append(WM_WINDOW_LIST, win);
                break;
            case CELESTIAL_Z_OVERLAY:
                list_append(WM_WINDOW_LIST_OVERLAY, win);
                break;
        }
        
        win->z_array = req->array;
        return socket_ok(sock, CELESTIAL_REQ_SET_Z_ARRAY);
    } else if (hdr->type == CELESTIAL_REQ_FLIP) {
        // Flip window
        CELESTIAL_VALIDATE(celestial_req_flip_t, CELESTIAL_REQ_FLIP);
        celestial_req_flip_t *req = (celestial_req_flip_t*)hdr;
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_FLIP for WID %d\n", req->wid);
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_FLIP, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_FLIP, EPERM);
        
        // Mark the window as requiring a flip
        wm_window_t *win = WID(req->wid);
        
        // If this is our first flip request..
        if (win->state == WINDOW_STATE_OPENING) {
            window_beginAnimation(win, WINDOW_ANIM_OPENING);
            return;
        }


        if (req->x < 0) req->x = 0;
        if (req->y < 0) req->y = 0;
        if (req->x + req->width > win->width) req->width = win->width - req->x;
        if (req->y + req->height > win->height) req->height = win->height - req->y;

        gfx_rect_t upd_rect = { .x = req->x, .y = req->y, .width = req->width, .height = req->height };
        window_update(win, upd_rect);

        // TODO: This will repeatedly draw (can be slow)
        upd_rect.x += win->x;
        upd_rect.y += win->y;
        window_updateRegion(upd_rect);
        return; // NO RESPONSE FOR FLIP REQUEST
    } else if (hdr->type == CELESTIAL_REQ_CLOSE_WINDOW) {
        // Close window
        CELESTIAL_VALIDATE(celestial_req_close_window_t, CELESTIAL_REQ_CLOSE_WINDOW);
        CELESTIAL_DEBUG("socket: Received CELESTIAL_REQ_CLOSE_WINDOW\n");
        celestial_req_close_window_t *req = (celestial_req_close_window_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_CLOSE_WINDOW, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_CLOSE_WINDOW, EPERM);
        
        window_close(WID(req->wid));

        return; // NO RESPONSE FOR CLOSE REQUEST
    } else if (hdr->type == CELESTIAL_REQ_MINIMIZE_WINDOW) {
        // TODO: This needs to interop with panel
        CELESTIAL_ERR("CELESTIAL_REQ_MINIMIZE_WINDOW is not implemented\n");
        return socket_error(sock, CELESTIAL_REQ_MINIMIZE_WINDOW, ENOTSUP);
    } else if (hdr->type == CELESTIAL_REQ_MAXIMIZE_WINDOW) {
        // Maximize window
        // Maximize is not the same as fullscreen. This needs to interop with taskbar.
        CELESTIAL_ERR("CELESTIAL_REQ_MAXIMIZE_WINDOW is not implemented\n");
        return socket_error(sock, CELESTIAL_REQ_MAXIMIZE_WINDOW, ENOTSUP);
    } else if (hdr->type == CELESTIAL_REQ_RESIZE) {
        // Resize request
        CELESTIAL_VALIDATE(celestial_req_resize_t, CELESTIAL_REQ_RESIZE);
        CELESTIAL_DEBUG("socket: Receieved CELESTIAL_REQ_RESIZE\n");
        celestial_req_resize_t *req = (celestial_req_resize_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_CLOSE_WINDOW, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_CLOSE_WINDOW, EPERM);

        // Documentation of how the resize process works can be found in the lib function for resizing
        // First, do bounds checks to ensure that we don't explode.
        wm_window_t *win = WID(req->wid);
        
        int resize = window_resize(win, req->width, req->height);
        if (resize != 0) return socket_error(sock, CELESTIAL_REQ_RESIZE, -(resize));

        // Build response
        celestial_resp_resize_t resp = {
            .magic = CELESTIAL_MAGIC,
            .type = CELESTIAL_REQ_RESIZE,
            .size = sizeof(celestial_resp_resize_t),
            .width = win->width,
            .height = win->height
        };

        socket_sendResponse(sock, &resp);
        return;
    } else if (hdr->type == CELESTIAL_REQ_SET_WINDOW_VISIBLE) {
        // Set window visible
        CELESTIAL_VALIDATE(celestial_req_set_window_visible_t, CELESTIAL_REQ_SET_WINDOW_VISIBLE);
        CELESTIAL_DEBUG("socket: Receieved CELESTIAL_REQ_SET_WINDOW_VISIBLE\n");
        celestial_req_set_window_visible_t *req = (celestial_req_set_window_visible_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_VISIBLE, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_VISIBLE, EPERM);
        
        wm_window_t *win = WID(req->wid);
        if (!req->visible) {
            win->state = WINDOW_STATE_HIDDEN; // !!!: Test for bugs
        } else if (req->visible) {
            win->state = WINDOW_STATE_NORMAL;
        }

        window_updateRegion(GFX_RECT(win->x, win->y, win->width, win->height));
        return socket_ok(sock, CELESTIAL_REQ_SET_WINDOW_VISIBLE);
    } else if (hdr->type == CELESTIAL_REQ_SET_MOUSE_CAPTURE) {
        // Set mouse capture
        CELESTIAL_VALIDATE(celestial_req_set_mouse_capture_t, CELESTIAL_REQ_SET_MOUSE_CAPTURE);
        CELESTIAL_DEBUG("socket: Receieved CELESTIAL_REQ_SET_MOUSE_CAPTURE\n");
        celestial_req_set_mouse_capture_t *req = (celestial_req_set_mouse_capture_t*)hdr;
        if (!WID_EXISTS(req->wid)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_VISIBLE, EINVAL);
        if (!WID_BELONGS_TO_SOCKET(req->wid, sock)) return socket_error(sock, CELESTIAL_REQ_SET_WINDOW_VISIBLE, EPERM);
        
        // Focus up this window
        wm_window_t *win = WID(req->wid);
        window_changeFocused(win);

        WM_MOUSE_WINDOW = win;

        WM_MOUSE_RELATIVE = req->capture;

        return socket_ok(sock, CELESTIAL_REQ_SET_MOUSE_CAPTURE);
    } else if (hdr->type == CELESTIAL_REQ_SET_MOUSE_CURSOR) {
        // Set mouse cursor
        CELESTIAL_VALIDATE(celestial_req_set_mouse_cursor_t, CELESTIAL_REQ_SET_MOUSE_CURSOR);
        CELESTIAL_DEBUG("socket: Receieved CELESTIAL_REQ_SET_MOUSE_CURSOR\n");
        celestial_req_set_mouse_cursor_t *req = (celestial_req_set_mouse_cursor_t*)hdr;

        mouse_change(req->cursor);
        mouse_render();
        return socket_ok(sock, CELESTIAL_REQ_SET_MOUSE_CURSOR);
    } else {
        CELESTIAL_ERR("socket: Unknown request type %d\n", hdr->type);
        return socket_error(sock, hdr->type, ENOTSUP);
    }
}