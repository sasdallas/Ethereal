/**
 * @file userspace/celestial/celestial.h
 * @brief Celestial header file
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
#include <stdint.h>
#include <stdio.h>
#include <graphics/gfx.h>
#include <structs/list.h>
#include <structs/hashmap.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/time.h>

#include "window.h"
#include "socket.h"
#include "mouse.h"
#include "event.h"
#include "kbd.h"

/**** VARIABLES ****/

/* GLOBALS */
extern FILE *__celestial_log_device;
extern int __celestial_debug;

/* GRAPHICS GLOBALS */
extern gfx_context_t *__celestial_gfx;

/* SOCKET GLOBALS */
extern int __celestial_socket;

/* WINDOW GLOBALS */
extern hashmap_t *__celestial_window_map;
extern list_t *__celestial_window_list;
extern list_t *__celestial_window_list_bg;
extern list_t *__celestial_window_list_overlay;
extern list_t *__celestial_window_update_queue;
extern wm_window_t *__celestial_focused_window;

/* CLIENT GLOBALS */
extern hashmap_t *__celestial_map;
extern int __celestial_client_count;

/* MOUSE GLOBALS */
extern int __celestial_mouse_fd;
extern int __celestial_mouse_x;
extern int __celestial_mouse_y;
extern sprite_t *__celestial_mouse_sprite;
extern wm_window_t *__celestial_mouse_window;
extern uint32_t __celestial_mouse_buttons;

/* KEYBOARD GLOBALS */
extern int __celestial_keyboard_fd;

/**** MACROS ****/

#define CELESTIAL_LOG(...) fprintf(__celestial_log_device, "celestial: [log  ] " __VA_ARGS__ )
#define CELESTIAL_DEBUG(...) if (__celestial_debug) fprintf(__celestial_log_device, "celestial: [debug] " __VA_ARGS__)
#define CELESTIAL_ERR(...) fprintf(__celestial_log_device, "celestial: [err  ] " __VA_ARGS__ )
#define CELESTIAL_PERROR(m) CELESTIAL_ERR("%s: %s\n", m, strerror(errno))

#define WM_GFX __celestial_gfx
#define WM_SOCK __celestial_socket
#define WM_WINDOW_MAP __celestial_window_map
#define WM_WINDOW_LIST __celestial_window_list
#define WM_WINDOW_LIST_BG __celestial_window_list_bg
#define WM_WINDOW_LIST_OVERLAY __celestial_window_list_overlay
#define WM_FOCUSED_WINDOW __celestial_focused_window
#define WM_UPDATE_QUEUE __celestial_window_update_queue
#define WM_SW_MAP __celestial_map
#define WM_MOUSE __celestial_mouse_fd
#define WM_MOUSEX __celestial_mouse_x
#define WM_MOUSEY __celestial_mouse_y
#define WM_MOUSE_SPRITE __celestial_mouse_sprite
#define WM_MOUSE_WINDOW __celestial_mouse_window
#define WM_MOUSE_BUTTONS __celestial_mouse_buttons


#define CELESTIAL_PROFILE_START() { struct timeval t; \
                        gettimeofday(&t, NULL);

#define CELESTIAL_PROFILE_END(name) \
                        struct timeval t_now; \
                        gettimeofday(&t_now, NULL); \
                        struct timeval result; \
                        timersub(&t_now, &t, &result); \
                        CELESTIAL_LOG("%s: completed in %ld.%06ld\n", (name), (long int)result.tv_sec, (long int)result.tv_usec); \
                        }


#define CELESTIAL_NOW() ({ struct timeval tv; gettimeofday(&tv, NULL); ((uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec)); })
#define CELESTIAL_SINCE(prev) (CELESTIAL_NOW() - prev)

/**** FUNCTIONS ****/

/**
 * @brief Fatal error method
 */
void celestial_fatal();

/**
 * @brief Add a new client to the poll queue
 * @param fd The file descriptor of the client in the queue
 * @param win The window ID
 * @returns 0 on success
 */
int celestial_addClient(int fd, int win) ;

/**
 * @brief Remove a client from the poll queue
 * @param fd The file descriptor of the client to drop
 * @returns 0 on success
 */
int celestial_removeClient(int fd);


#endif