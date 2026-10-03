/**
 * @file hexahedron/include/kernel/subsystems/auth.h
 * @brief Authorization manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_AUTH_H
#define KERNEL_SUBSYSTEMS_AUTH_H

/**** INCLUDES ****/
#include <sys/types.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define AUTH_FILE_READ          0x1
#define AUTH_FILE_WRITE         0x2
#define AUTH_FILE_EXECUTE       0x4
#define AUTH_FILE_SETATTR       0x8
#define AUTH_FILE_UNLINK        0x10
#define AUTH_FILE_RENAME        0x20

/**** TYPES ****/

typedef enum auth_class {
    AUTH_CLASS_FILE,
    AUTH_CLASS_PROCESS,
    AUTH_CLASS_SOCKET,
    AUTH_CLASS_SYSTEM
} auth_class_t;

typedef struct auth_cred {
    uid_t uid;
    gid_t gid;
    uid_t euid;
    gid_t egid;
    uid_t suid;
    gid_t sgid;
    gid_t *groups;
} auth_cred_t;

struct vfs_inode;

typedef struct auth_request {
    auth_class_t auth_class;
    uint32_t auth_subclass;
    auth_cred_t *cred;

    union {
        struct {
            struct vfs_inode *inode;
            struct vfs_inode *directory; // parental directory, if applicable
        } file;
    };
} auth_request_t;

/**** MACROS ****/

#define CRED_ROOT(cred) ((cred)->euid == 0)

/**** FUNCTIONS ****/

/**
 * @brief Perform an authorization request
 * @param request The request to perform
 * @returns 0 on granted, anything else is an error code
 */
int auth_validate(auth_request_t *req);

/**
 * @brief Check filesystem access
 * @param cred The credentials to validate
 * @param inode The inode to check
 * @param dir Parental directory if applicable 
 * @param request The requested permissions
 * @returns 0 on success
 */
static inline int auth_filesystem(auth_cred_t *cred, struct vfs_inode *inode, struct vfs_inode *dir, uint32_t request) {
    auth_request_t req = {
        .auth_class = AUTH_CLASS_FILE,
        .auth_subclass = request,
        .cred = cred,
        .file = {
            .inode = inode,
            .directory = dir,
        }
    };

    return auth_validate(&req);
}


#endif
