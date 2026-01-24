/**
 * @file hexahedron/include/kernel/drivers/net/socket_fs.h
 * @brief Socket FS
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NET_SOCKET_FS_H
#define DRIVERS_NET_SOCKET_FS_H

/**** INCLUDES ****/
#include <kernel/fs/vfs_new.h>

struct sock; // proto

/**** FUNCTIONS ****/

/**
 * @brief Create a new socket inode
 * @param sock The socket to create the inode on
 */
vfs_inode_t *socketfs_create(struct sock *sock);

/**
 * @brief Destroy socket inode
 * @param inode The socket inode to destroy
 */
void socketfs_destroy(vfs_inode_t *inode);

#endif
