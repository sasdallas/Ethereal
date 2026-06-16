/**
 * @file userspace/celestial/socket.h
 * @brief Celestial socket handler
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_SOCKET_H
#define _CELESTIAL_WM_SOCKET_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/socket.h>
#include <ethereal/celestial/request.h>

/**** DEFINITIONS ****/

#define CELESTIAL_SOCKET_NAME       "/comm/wndsrv"

/**** FUNCTIONS ****/

/**
 * @brief Create and bind the window server socket
 */
void socket_init();

/**
 * @brief Handle any new socket accepts
 * @note This is non-blocking, call and just do whatever afterwards
 */
void socket_accept();

/**
 * @brief New data available on a socket
 * @param sock The socket with data available
 */
void socket_handle(int sock);

/**
 * @brief Send a response to a socket
 * @param sock The socket to send to
 * @param resp The response to send
 * @returns 0 on success
 */
int socket_sendResponse(int sock, void *resp);

/**
 * @brief Send a packet to a socket
 * @param sock The socket to send the packet to
 * @param size The size of the packet
 * @param packet The packet to send
 * @returns 0 on success
 */
int socket_send(int sock, size_t size, void *packet);

#endif