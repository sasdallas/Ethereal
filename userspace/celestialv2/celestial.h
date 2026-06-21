/**
 * @file userspace/celestialv2/celestial.h
 * @brief Celestial
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_H
#define _CELESTIAL_WM_H

/**** INCLUDES ****/
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>
#include <ethereal/shared.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <graphics/gfx.h>
#include <sys/socket.h>
#include <assert.h>
#include <ethereal/celestial.h>

#define APP_NAME "celestial"
#include <ethereal/log.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef int wid_t;

typedef enum {
    Z_BACKGROUND,
    Z_DEFAULT,
    Z_COUNT
} z_array_t;

typedef enum {
    WINDOW_STATE_OPENING,
    WINDOW_STATE_NORMAL,
    WINDOW_STATE_CLOSING,
    WINDOW_STATE_CLOSED,
    WINDOW_STATE_DRAGGING,
    WINDOW_STATE_RESIZING,
} window_state_t;

/* Socket client */
typedef struct wm_client {
    struct wm_client *next;
    struct wm_window *window_head;
    int client_fd;
    int window_count;
    bool dead;
} wm_client_t;

/* Window animation state */
struct wm_window;
typedef struct wm_window_anim {
    struct wm_window_anim *next;
    struct wm_window *win;
    int anim_time;
    uint64_t anim_last_paint;
    bool running;
} wm_window_anim_t;

typedef struct wm_window_announce_data {
    char *name;
    char *icon;
} wm_window_announce_data_t;

/* Per-window object */
typedef struct wm_window {
    struct wm_window *next;
    struct wm_window *prev;

    struct wm_window *next_client;
    wm_client_t *client;
    wid_t id;
    int x;
    int y;
    int width;
    int height;
    int flags;
    z_array_t z_array;
    bool visible;
    window_state_t state;

    volatile int refcount;
    wm_window_anim_t anim;

    wm_window_announce_data_t announce;

    uint8_t *buffer;            // Buffer allocated to the window
    key_t bufkey;               // Buffer shared memory key
    int shmfd;                  // Buffer shared memory fd

} wm_window_t;

typedef struct damage_node {
    struct damage_node *next;
    gfx_rect_t rect;
} damage_node_t;

typedef struct render_request {
    struct render_request *next;
    gfx_rect_t rect;
    wm_window_t *win;
    int x;
    int y;
    window_state_t state;
    int anim_time;
} render_request_t;


#ifdef BUILDING_LINUX
#define MOUSE_BUTTON_LEFT 0x1
#define MOUSE_BUTTON_RIGHT 0x2
#define MOUSE_BUTTON_MIDDLE 0x4
#endif

#ifdef BUILDING_LINUX
#include <X11/Xlib.h>
typedef struct renderer {
    XImage image;
    gfx_context_t *ctx;
} renderer_t;
#else
typedef struct renderer {
    render_request_t *frame;
    gfx_context_t *ctx;
} renderer_t;
#endif

typedef struct input_bind {
    struct input_bind *next;
    wm_window_t *win;
    key_scancode_t sc;
    key_modifiers_t mods;
    bool capture;
} input_bind_t;

/* Primary server state object */
typedef struct celestial {
    int sock_fd;

    // IPC
    int client_count;
    pthread_mutex_t client_lock;
    wm_client_t *client_list;
    int ipc_wake_pipe[2];

    // WINDOWS
    pthread_mutex_t window_lock;
    wm_window_t *window_list[Z_COUNT];
    wm_window_t *focused;
    uint32_t next_window_id;
    int window_count;
    wm_window_t *root;

    // RENDERER/DAMAGE
    renderer_t renderer;
    pthread_mutex_t dmg_lock;
    pthread_cond_t dmg_cond;
    damage_node_t *dmg_head;

    // INPUT
    int mouse_x;
    int mouse_y;
    uint32_t last_buttons;
    wm_window_t *mouse_window; // the topmost window that the mouse is currently in
    input_bind_t *binds;
    pthread_mutex_t bind_lck;
    wm_window_t *mouse_capture;

    // THREADS
    pthread_t ipc_acceptor_thread;
    pthread_t ipc_thread;
    pthread_t render_thread;
    pthread_t mouse_thread;
    pthread_t keyboard_thread;
} celestial_t;

/**** MACROS ****/

extern FILE *celestial_log_device;
extern celestial_t celestial_server;
#define SERVER (&celestial_server)
#define RENDERER (&SERVER->renderer)

#define FATAL(msg, ...) ({ TRACE_ERROR("FATAL: " msg, ## __VA_ARGS__); celestial_fatal(); })

/**** FUNCTIONS ****/

/* I break my own naming convention here, too lazy */

/* Celestial functions */
void celestial_fatal(); // Kill it all
uint64_t celestial_now();

/* IPC functions */
int ipc_init();
void ipc_shutdown();
void ipc_removeClient(int fd);
void ipc_releaseClient(wm_client_t *client);

/* Event functions */
inline int event_send_int(wm_client_t *cli, void *buffer, size_t size) {
    if (!cli || __atomic_load_n(&cli->dead, __ATOMIC_SEQ_CST) || cli->client_fd < 0) {
        return 0;
    }

    ssize_t ret = send(cli->client_fd, buffer, size, 0);
    if (ret < 0) {
        TRACE_ERROR("Failed to send event: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

#define EVENT_SEND(win, evtype, t, ...) ({ wm_window_t *_ev_win = (win); wm_client_t *_ev_cli = _ev_win->client; if (_ev_cli && !__atomic_load_n(&_ev_cli->dead, __ATOMIC_SEQ_CST)) { int _ev_fd = _ev_cli->client_fd; evtype ev = { .magic = CELESTIAL_MAGIC_EVENT, .size = sizeof(evtype), .type = (t), .wid = _ev_win->id, ## __VA_ARGS__ }; if (event_send_int(_ev_cli, &ev, sizeof(evtype))) { TRACE_WARN("Connection error with client %d: %s\n", _ev_fd, strerror(errno)); ipc_removeClient(_ev_fd); } } })

#define WINDOW_CHANGE_STATE(window, new_state) { (window)->state = (new_state); TRACE_DEBUG("Window %d state changed to " #new_state "\n", window->id); }

#define window_hold(w) __atomic_add_fetch(&(w)->refcount, 1, __ATOMIC_SEQ_CST)
#define window_release(w) { __atomic_sub_fetch(&(w)->refcount, 1, __ATOMIC_SEQ_CST); if ((w)->refcount == 0) { window_destroy(w); } }

/* Damage functions */

// Build damage render
render_request_t *damage_build();

// Add a damage rectangle
void damage_add(gfx_rect_t rect);

// Fully damage an entire window
void damage_window(wm_window_t *window);

// Damage a window when it moves
void damage_move(wm_window_t *window, int old_x, int old_y);

// Lock and unlock. Required
void damage_lock();
void damage_unlock();

#define damage_add_locked(rect) ({ damage_lock(); damage_add(rect); damage_unlock(); })
#define damage_window_locked(win) ({ damage_lock(); damage_window(win); damage_unlock(); })
#define damage_move_locked(win, ox, oy) ({ damage_lock(); damage_move(win, ox, oy); damage_unlock(); })

/* Window functions */
wm_window_t *window_create(wm_client_t *client, int flags, size_t width, size_t height);
wm_window_t *window_top(int x, int y);
wm_window_t *window_get(wm_client_t *client, wid_t id);
void window_focus(wm_window_t *win);
void window_moveZ(wm_window_t *win, z_array_t new_z);
void window_close(wm_window_t *win);
void window_destroy(wm_window_t *win);
void window_beginAnimation(wm_window_t *win);

inline bool window_contains(wm_window_t *win, int x, int y) {
    return (x >= win->x && y >= win->y && x < win->x + win->width && y < win->y + win->height);
}

/* Renderer functions */
int renderer_init();
void renderer_shutdown();
size_t renderer_getWidth();
size_t renderer_getHeight();
void render_request(render_request_t *upd);
void renderer_buildFrame();

/* Request functions */
void request_handle(wm_client_t *client, void *buffer, size_t size);

/* Input functions */
int input_init();
int input_init_backend();
void input_draw();
void input_draw_at(int x, int y);
bool input_frameCursor(render_request_t *frame, int *x, int *y);
void mouse_check_events(int new_x, int new_y, int scroll, uint32_t new_btns);
void input_get_mouse_pos(int *x, int *y);
void input_set_mouse(int mouse);
void input_set_mouse_capture(wm_window_t *win);

#endif
