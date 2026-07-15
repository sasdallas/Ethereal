/**
 * @file userspace/celestialv2/request.c
 * @brief Celestial request framework
 * 
 * Provides the actual code behind CREATE_WINDOW and whatnot
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <ethereal/celestial/request.h>
#include <ethereal/celestial/event.h>
#include <sys/socket.h>

#define DECLARE_RESP(resp, name, t, ...) celestial_resp_ ## resp ## _t name = {\
                                    .magic = CELESTIAL_MAGIC,\
                                    .size = sizeof(celestial_resp_ ## resp ## _t),\
                                    .type = t, ## __VA_ARGS__ \
                                }

#define SEND_RESP(resp) request_send(client, &(resp), sizeof(resp));

#define REQ_OK(req) ({ celestial_resp_ok_t ok = { .magic = CELESTIAL_MAGIC_OK, .size = sizeof(celestial_resp_ok_t), .type = req->type }; SEND_RESP(ok); })

#define REQUEST_HANDLER(name, type) void request_ ## name (wm_client_t *client, celestial_req_ ## name ## _t *req)

#define EXECUTE_REQUEST(name, type) celestial_req_ ## name ## _t *request = (typeof(request))buffer; \
                                    if (size != sizeof(celestial_req_ ## name ## _t)) {\
                                        TRACE_ERROR("Client %d sent request " #type " with invalid size %d\n", client->client_fd, request->size);\
                                        return request_send_error(client, type, -EINVAL);\
                                    }\
                                    TRACE_DEBUG("Received " #type "\n");\
                                    return request_ ## name (client, request);
                                    
#define EXECUTE_REQUEST_SILENT(name, type) celestial_req_ ## name ## _t *request = (typeof(request))buffer; \
                                    if (size != sizeof(celestial_req_ ## name ## _t)) {\
                                        TRACE_ERROR("Client %d sent request " #type " with invalid size %d\n", client->client_fd, request->size);\
                                        return request_send_error(client, type, -EINVAL);\
                                    }\
                                    return request_ ## name (client, request);


void request_send(wm_client_t *client, void *buffer, size_t size) {
    if (!client || __atomic_load_n(&client->dead, __ATOMIC_SEQ_CST) || client->client_fd < 0) {
        return;
    }

    int fd = client->client_fd;
    ssize_t ret = send(fd, (const void*)buffer, size, 0);
    if (ret < 0) {
        if (errno == ECONNRESET || errno == EPIPE || errno == EBADF) {
            // Peer disconnected
            TRACE_INFO("Client %d disconnected\n", fd);
            ipc_removeClient(fd);
        } else {
            FATAL("send failed: %s\n", strerror(errno));
        }
    }
}


void __request_send_error(wm_client_t *client, int type, int error) {
    celestial_resp_error_t error_resp = {
        .magic = CELESTIAL_MAGIC_ERROR,
        .size = sizeof(celestial_resp_error_t),
        .type = type,
        .error = error,
    };

    SEND_RESP(error_resp);
}

#define request_send_error(client, type, error) ({ TRACE_ERROR("Sending error " #error " to client in request %d\n", type); __request_send_error(client, type, error); })

REQUEST_HANDLER(create_window, CELESTIAL_REQ_CREATE_WINDOW) {
    if (req->width > renderer_getWidth()) { req->width = renderer_getWidth(); }
    if (req->height > renderer_getHeight()) { req->height = renderer_getHeight(); }

    wm_window_t *window = window_create(client, req->flags, req->width, req->height);
    assert(window);
    
    DECLARE_RESP(create_window, resp, CELESTIAL_REQ_CREATE_WINDOW,
        .id = window->id
    );

    SEND_RESP(resp);
}

REQUEST_HANDLER(get_window_info, CELESTIAL_REQ_GET_WINDOW_INFO) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_GET_WINDOW_INFO, -ESRCH);

    DECLARE_RESP(get_window_info, resp, CELESTIAL_REQ_GET_WINDOW_INFO,
        .x = win->x,
        .y = win->y,
        .width = win->width,
        .height = win->height,
        .buffer_key = win->bufkey
    );

    SEND_RESP(resp);
}

REQUEST_HANDLER(set_window_pos, CELESTIAL_REQ_SET_WINDOW_POS) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_WINDOW_POS, -ESRCH);

    if (req->x < 0) req->x = 0;
    if (req->x + win->width > (int)renderer_getWidth()) req->x = renderer_getWidth() - win->width;
    if (req->y < 0) req->y = 0;
    if (req->y + win->height > (int)renderer_getHeight()) req->y = renderer_getHeight() - win->height;

    
    pthread_mutex_lock(&SERVER->window_lock);
    int ox = win->x;
    int oy = win->y;
    win->x = req->x;
    win->y = req->y;
    pthread_mutex_unlock(&SERVER->window_lock);

    damage_move_locked(win, ox, oy);

    DECLARE_RESP(set_window_pos, resp, CELESTIAL_REQ_SET_WINDOW_POS,
        .x = win->x,
        .y = win->y,
    );

    SEND_RESP(resp);
} 

REQUEST_HANDLER(get_server_info, CELESTIAL_REQ_GET_SERVER_INFO) {
    DECLARE_RESP(get_server_info, resp, CELESTIAL_REQ_GET_SERVER_INFO,
        .screen_width = renderer_getWidth(),
        .screen_height = renderer_getHeight()
    );
    SEND_RESP(resp);
}

REQUEST_HANDLER(flip, CELESTIAL_REQ_FLIP) {
    // No response for flip
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return;

    damage_window_locked(win);
}

REQUEST_HANDLER(set_z_array, CELESTIAL_REQ_SET_Z_ARRAY) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_Z_ARRAY, -ESRCH);

    window_moveZ(win, req->array);

    REQ_OK(req);
}

REQUEST_HANDLER(subscribe, CELESTIAL_REQ_SUBSCRIBE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SUBSCRIBE, -ESRCH);
    TRACE_WARN("CELESTIAL_REQ_SUBSCRIBE is deprecated\n");
    REQ_OK(req);
}

REQUEST_HANDLER(unsubscribe, CELESTIAL_REQ_UNSUBSCRIBE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_UNSUBSCRIBE, -ESRCH);
    TRACE_WARN("CELESTIAL_REQ_UNSUBSCRIBE is deprecated\n");
    REQ_OK(req);
}

REQUEST_HANDLER(drag_start, CELESTIAL_REQ_DRAG_START) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_DRAG_START, -ESRCH);

    WINDOW_CHANGE_STATE(win, WINDOW_STATE_DRAGGING);
    
    REQ_OK(req);
}

REQUEST_HANDLER(drag_stop, CELESTIAL_REQ_DRAG_STOP) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_DRAG_STOP, -ESRCH);

    WINDOW_CHANGE_STATE(win, WINDOW_STATE_NORMAL);
    
    REQ_OK(req);
}

REQUEST_HANDLER(close_window, CELESTIAL_REQ_CLOSE_WINDOW) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_CLOSE_WINDOW, -ESRCH);
    window_close(win);
    REQ_OK(req);
}

REQUEST_HANDLER(set_window_visible, CELESTIAL_REQ_SET_WINDOW_VISIBLE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_WINDOW_VISIBLE, -ESRCH);
    
    win->visible = req->visible;
    damage_window_locked(win);
    REQ_OK(req);
}

REQUEST_HANDLER(set_mouse_cursor, CELESTIAL_REQ_SET_MOUSE_CURSOR) {
    input_set_mouse(req->cursor);
    REQ_OK(req);
}

REQUEST_HANDLER(announce_window, CELESTIAL_REQ_ANNOUNCE_WINDOW) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_ANNOUNCE_WINDOW, -ESRCH);

    win->announce.name = strdup(req->name);
    win->announce.icon = strdup(req->icon);

    if (SERVER->root != NULL) {
        wm_window_t *root = SERVER->root;
        EVENT_SEND(root, celestial_event_window_changed_t, CELESTIAL_EVENT_WINDOW_CHANGED, .changed_window = req->wid, .changed_event = CELESTIAL_WINDOW_CHANGE_ADVERTISED);
    }

    REQ_OK(req);
}

REQUEST_HANDLER(bind_key, CELESTIAL_REQ_BIND_KEY) {
    // Binding keys is hard
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_BIND_KEY, -ESRCH);

    // Allocate a new bind
    input_bind_t *bind = malloc(sizeof(input_bind_t));
    bind->sc = req->scancode;
    bind->mods = req->modifiers;
    bind->capture = req->capture;
    bind->win = win;

    pthread_mutex_lock(&SERVER->bind_lck);
    bind->next = SERVER->binds;
    SERVER->binds = bind;
    pthread_mutex_unlock(&SERVER->bind_lck);

    REQ_OK(req);
}

REQUEST_HANDLER(query_window, CELESTIAL_REQ_QUERY_WINDOW) {
    wm_window_t *win = window_get(client, req->query);
    if (!win) return request_send_error(client, CELESTIAL_REQ_QUERY_WINDOW, -ESRCH);

    DECLARE_RESP(query_window, resp, CELESTIAL_REQ_QUERY_WINDOW,
        .width = win->width,
        .height = win->height,
    );

    if (win->announce.name) {
        strncpy(resp.name, win->announce.name, 128);
    } else {
        resp.name[0] = 0;
    }

    if (win->announce.icon) {
        strncpy(resp.icon, win->announce.icon, 128);
    } else {
        resp.icon[0] = 0;
    }

    SEND_RESP(resp);
}

REQUEST_HANDLER(query_window_ids, CELESTIAL_REQ_QUERY_WINDOW_IDS) {
    pthread_mutex_lock(&SERVER->window_lock);
    int nwids = __atomic_load_n(&SERVER->window_count, __ATOMIC_SEQ_CST);
    char buffer[sizeof(celestial_resp_query_window_ids_t) + (sizeof(wid_t) * nwids)];
    celestial_resp_query_window_ids_t *resp = (celestial_resp_query_window_ids_t *)buffer;
    
    resp->magic = CELESTIAL_MAGIC;
    resp->size = sizeof(buffer);
    resp->type = CELESTIAL_REQ_QUERY_WINDOW_IDS;

    int idx = 0;
    for (int i = 0; i < Z_COUNT; i++) {
        wm_window_t *iter = SERVER->window_list[i];
        while (iter) {
            if (iter->announce.name) {
                resp->wids[idx++] = iter->id;
            }

            iter = iter->next;
        }
    }

    resp->size = sizeof(celestial_resp_query_window_ids_t) + (sizeof(wid_t) * idx);
    resp->nwids = idx;

    pthread_mutex_unlock(&SERVER->window_lock);

    SEND_RESP(resp);
}

REQUEST_HANDLER(set_root_window, CELESTIAL_REQ_SET_ROOT_WINDOW) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_ROOT_WINDOW, -ESRCH);

    if (SERVER->root != NULL) {
        return request_send_error(client, CELESTIAL_REQ_SET_ROOT_WINDOW, -EPERM);
    }
    
    // TODO: atomic
    SERVER->root = win;
    
    REQ_OK(req);
}

REQUEST_HANDLER(set_mouse_capture, CELESTIAL_REQ_SET_MOUSE_CAPTURE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_MOUSE_CAPTURE, -ESRCH);
    
    if (req->capture) {
        input_set_mouse_capture(win);    
    } else {
        input_set_mouse_capture(NULL);
    }

    REQ_OK(req);
}

REQUEST_HANDLER(query_mouse, CELESTIAL_REQ_QUERY_MOUSE) {
    int x, y;
    input_get_mouse_pos(&x, &y);

    DECLARE_RESP(query_mouse, resp, CELESTIAL_REQ_QUERY_MOUSE,
        .x = x,
        .y = y,
    );

    SEND_RESP(resp);
}

REQUEST_HANDLER(set_resize_params, CELESTIAL_REQ_SET_RESIZE_PARAMS) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_SET_RESIZE_PARAMS, -ESRCH);

    if ((size_t)win->width > req->max_width || (size_t)win->width < req->min_width ||
        (size_t)win->height > req->max_height || (size_t)win->height < req->min_height) {
        return request_send_error(client, CELESTIAL_REQ_SET_RESIZE_PARAMS, -EINVAL);
    }

    win->resize.bounds.min_width = req->min_width;
    win->resize.bounds.min_height = req->min_height;
    win->resize.bounds.max_width = req->max_width;
    win->resize.bounds.max_height = req->max_height;

    REQ_OK(req);
}

REQUEST_HANDLER(start_resize, CELESTIAL_REQ_START_RESIZE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_START_RESIZE, -ESRCH);

    win->resize.direction = req->direction;
    WINDOW_CHANGE_STATE(win, WINDOW_STATE_RESIZING);
    REQ_OK(req);
}

REQUEST_HANDLER(stop_resize, CELESTIAL_REQ_STOP_RESIZE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return request_send_error(client, CELESTIAL_REQ_STOP_RESIZE, -ESRCH);

    WINDOW_CHANGE_STATE(win, WINDOW_STATE_NORMAL);
    REQ_OK(req);
}

REQUEST_HANDLER(ack_resize, CELESTIAL_REQ_ACK_RESIZE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return; // no response

    window_resize_finish(win);

    // no response
}

REQUEST_HANDLER(resize, CELESTIAL_REQ_RESIZE) {
    wm_window_t *win = window_get(client, req->wid);
    if (!win) return; // no response

    win->resize.oneoff = true;
    TRACE_DEBUG("celestial_req_resize\n");
    window_resize(win, win->x, win->y, req->width, req->height);
    REQ_OK(req);
}

void request_handle(wm_client_t *client, void *buffer, size_t size) {
    if (size < sizeof(celestial_req_header_t)) {
        TRACE_ERROR("Client (fd = %d) sent request with invalid size %d\n", client->client_fd, size);
    
        // we can't trust the contents of hdr, ignore it.
        return;
    }

    celestial_req_header_t *hdr = (celestial_req_header_t*)buffer;

    if (hdr->magic != CELESTIAL_MAGIC) {
        TRACE_ERROR("Client (fd = %d) sent request with invalid magic 0x%x\n", client->client_fd, hdr->magic);
        return request_send_error(client, hdr->type, -EINVAL);
    }


    if (size != hdr->size) {
        TRACE_ERROR("Client %d sent request with invalid size %d (received %d bytes)\n", client->client_fd, hdr->size, size);
        return request_send_error(client, hdr->type, -EINVAL);
    }

    if (hdr->type == CELESTIAL_REQ_CREATE_WINDOW) {
        EXECUTE_REQUEST(create_window, CELESTIAL_REQ_CREATE_WINDOW);
    } else if (hdr->type == CELESTIAL_REQ_GET_WINDOW_INFO) {
        EXECUTE_REQUEST(get_window_info, CELESTIAL_REQ_GET_WINDOW_INFO);
    } else if (hdr->type == CELESTIAL_REQ_SET_WINDOW_POS) {
        EXECUTE_REQUEST(set_window_pos, CELESTIAL_REQ_SET_WINDOW_POS);
    } else if (hdr->type == CELESTIAL_REQ_SUBSCRIBE) {
        EXECUTE_REQUEST(subscribe, CELESTIAL_REQ_SUBSCRIBE);
    } else if (hdr->type == CELESTIAL_REQ_UNSUBSCRIBE) {
        EXECUTE_REQUEST(unsubscribe, CELESTIAL_REQ_UNSUBSCRIBE);
    } else if (hdr->type == CELESTIAL_REQ_GET_SERVER_INFO) {
        EXECUTE_REQUEST(get_server_info, CELESTIAL_REQ_GET_SERVER_INFO);
    } else if (hdr->type == CELESTIAL_REQ_FLIP) {
        EXECUTE_REQUEST_SILENT(flip, CELESTIAL_REQ_FLIP);
    } else if (hdr->type == CELESTIAL_REQ_SET_Z_ARRAY) {
        EXECUTE_REQUEST(set_z_array, CELESTIAL_REQ_SET_Z_ARRAY);
    } else if (hdr->type == CELESTIAL_REQ_DRAG_START) {
        EXECUTE_REQUEST(drag_start, CELESTIAL_REQ_DRAG_START);
    } else if (hdr->type == CELESTIAL_REQ_DRAG_STOP) {
        EXECUTE_REQUEST(drag_stop, CELESTIAL_REQ_DRAG_STOP);
    } else if (hdr->type == CELESTIAL_REQ_CLOSE_WINDOW) {
        EXECUTE_REQUEST(close_window, CELESTIAL_REQ_CLOSE_WINDOW);
    } else if (hdr->type == CELESTIAL_REQ_SET_WINDOW_VISIBLE) {
        EXECUTE_REQUEST(set_window_visible, CELESTIAL_REQ_SET_WINDOW_VISIBLE);
    } else if (hdr->type == CELESTIAL_REQ_SET_MOUSE_CURSOR) {
        EXECUTE_REQUEST(set_mouse_cursor, CELESTIAL_REQ_SET_MOUSE_CURSOR);
    } else if (hdr->type == CELESTIAL_REQ_ANNOUNCE_WINDOW) {
        EXECUTE_REQUEST(announce_window, CELESTIAL_REQ_ANNOUNCE_WINDOW);
    } else if (hdr->type == CELESTIAL_REQ_QUERY_WINDOW) {
        EXECUTE_REQUEST(query_window, CELESTIAL_REQ_QUERY_WINDOW);
    } else if (hdr->type == CELESTIAL_REQ_QUERY_WINDOW_IDS) {
        EXECUTE_REQUEST(query_window_ids, CELESTIAL_REQ_QUERY_WINDOW_IDS);
    } else if (hdr->type == CELESTIAL_REQ_BIND_KEY) {
        EXECUTE_REQUEST(bind_key, CELESTIAL_REQ_BIND_KEY);
    } else if (hdr->type == CELESTIAL_REQ_SET_ROOT_WINDOW) {
        EXECUTE_REQUEST(set_root_window, CELESTIAL_REQ_SET_ROOT_WINDOW);
    } else if (hdr->type == CELESTIAL_REQ_SET_MOUSE_CAPTURE) {
        EXECUTE_REQUEST(set_mouse_capture, CELESTIAL_REQ_SET_MOUSE_CAPTURE);
    } else if (hdr->type == CELESTIAL_REQ_QUERY_MOUSE) {
        EXECUTE_REQUEST(query_mouse, CELESTIAL_REQ_QUERY_MOUSE);
    } else if (hdr->type == CELESTIAL_REQ_SET_RESIZE_PARAMS) {
        EXECUTE_REQUEST(set_resize_params, CELESTIAL_REQ_SET_RESIZE_PARAMS);
    } else if (hdr->type == CELESTIAL_REQ_START_RESIZE) {
        EXECUTE_REQUEST(start_resize, CELESTIAL_REQ_START_RESIZE);
    } else if (hdr->type == CELESTIAL_REQ_STOP_RESIZE) {
        EXECUTE_REQUEST(stop_resize, CELESTIAL_REQ_STOP_RESIZE);
    } else if (hdr->type == CELESTIAL_REQ_ACK_RESIZE) {
        EXECUTE_REQUEST(ack_resize, CELESTIAL_REQ_ACK_RESIZE);
    } else if (hdr->type == CELESTIAL_REQ_RESIZE) {
        EXECUTE_REQUEST(resize, CELESTIAL_REQ_RESIZE);
    } else {
        TRACE_ERROR("Client %d sent unknown/unhandled request %d\n", client->client_fd, hdr->type);
        return request_send_error(client, hdr->type, -ENOSYS);
    }
}
