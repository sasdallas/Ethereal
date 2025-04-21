/**
 * @file hexahedron/include/kernel/fs/tarfs.h
 * @brief USTAR archive
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_TARFS_H
#define KERNEL_FS_TARFS_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef struct _ustar_header {
    char name[100];             // File name
    char mode[8];               // File mode
    char uid[8];                // User ID
    char gid[8];                // Group ID
    char size[12];              // File size (in octal)
    char mtime[12];             // Modification time in Unix time format (octal)
    char checksum[8];           // Checksum for header record
    char type[1];               // Type of the file
    char linkname[100];         // Name of linked file
    char ustar[6];              // USTAR + '\0'
    char ustar_version[2];      // USTAR version
    char username[32];          // Username of owner
    char groupname[32];         // Group name of owner

    char devmajor[8];           // Device major number
    char devminor[8];           // Device minor number

    char nameprefix[155];       // Filename prefix
} ustar_header_t;

/**** DEFINITIONS ****/

#define USTAR_FILE          '0'
#define USTAR_HARD_LINK     '1'
#define USTAR_SYMLINK       '2'
#define USTAR_CHARDEV       '3'
#define USTAR_BLOCKDEV      '4'
#define USTAR_DIRECTORY     '5'
#define USTAR_PIPE          '6' 

/**** FUNCTIONS ****/

/**
 * @brief Initialize the tarfs system
 */
void tarfs_init();


#endif
