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

#include "window.h"
#include "socket.h"
#include "mouse.h"

/**** VARIABLES ****/
extern FILE *__celestial_log_device;
extern int __celestial_debug;
extern gfx_context_t *__celestial_gfx;
extern int __celestial_socket;
extern list_t *__celestial_window_list;
extern hashmap_t *__celestial_map;
extern int __celestial_client_count;
extern int __celestial_mouse_fd;
extern int __celestial_mouse_x;
extern int __celestial_mouse_y;
extern sprite_t *__celestial_mouse_sprite;

/**** MACROS ****/

#define CELESTIAL_LOG(...) fprintf(__celestial_log_device, "celestial: [log  ] " __VA_ARGS__ )
#define CELESTIAL_DEBUG(...) if (__celestial_debug) fprintf(__celestial_log_device, "celestial: [debug] " __VA_ARGS__)
#define CELESTIAL_ERR(...) fprintf(__celestial_log_device, "celestial: [err  ] " __VA_ARGS__ )
#define CELESTIAL_PERROR(m) CELESTIAL_ERR("%s: %s\n", m, strerror(errno))

#define WM_GFX __celestial_gfx
#define WM_SOCK __celestial_socket
#define WM_WINDOW_LIST __celestial_window_list
#define WM_SW_MAP __celestial_map
#define WM_MOUSE __celestial_mouse_fd
#define WM_MOUSEX __celestial_mouse_x
#define WM_MOUSEY __celestial_mouse_y
#define WM_MOUSE_SPRITE __celestial_mouse_sprite

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