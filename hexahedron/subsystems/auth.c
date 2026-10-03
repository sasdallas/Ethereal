/**
 * @file hexahedron/subsystems/auth.c
 * @brief Authorization subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/subsystems/auth.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "AUTH", __VA_ARGS__)

typedef int (*auth_validate_t)(auth_request_t *req);
static int auth_validateFile(auth_request_t *req);

/* Authentication subsystems */
auth_validate_t auth_classes[] = {
    [AUTH_CLASS_FILE] = auth_validateFile,
    [AUTH_CLASS_PROCESS] = NULL,
    [AUTH_CLASS_SOCKET] = NULL,
    [AUTH_CLASS_SYSTEM] = NULL,
};

#define FILE_VALIDATE_ATTR(cred, attr, rtype) ({ \
    bool decision = false; \
    if (CRED_ROOT(cred)) decision = true; \
    else if (attr.uid == cred->euid && (attr.mode & S_I##rtype##USR)) decision = true; \
    else if (attr.gid == cred->egid && (attr.mode & S_I##rtype##GRP)) decision = true; \
    else if ((attr.mode & S_I##rtype##OTH)) decision = true; \
    decision; \
})

// what can i say, i am lazy
#define FILE_VALIDATE_SUBCLASS(attr, subclass, type) ({ \
    if (req->auth_subclass & (subclass)) {\
        if (FILE_VALIDATE_ATTR(cred, attr, type) == false) { \
            return -EPERM; \
        }\
    }\
})

/**
 * @brief Validate file access
 */
static int auth_validateFile(auth_request_t *req) {
    vfs_inode_t *inode = req->file.inode;
    vfs_inode_t *directory = req->file.directory;
    auth_cred_t *cred = req->cred;

    // get the inode attr
    vfs_inode_attr_t attr;
    vfs_getattr(inode, &attr);

    if (attr.type == VFS_PIPE || attr.type == VFS_SOCKET) {
        return 0;
    }

    vfs_inode_attr_t dattr;
    if (directory) vfs_getattr(directory, &dattr); 

    // Knock out the first 3 right away
    FILE_VALIDATE_SUBCLASS(attr, AUTH_FILE_READ, R);
    FILE_VALIDATE_SUBCLASS(attr, AUTH_FILE_WRITE, W);
    FILE_VALIDATE_SUBCLASS(attr, AUTH_FILE_EXECUTE, X);

    // setattr is easy, you can only do it if you are the owner of the file
    if (req->auth_subclass & AUTH_FILE_SETATTR) {
        if (attr.uid != cred->euid && !CRED_ROOT(cred)) {
            return -EPERM;
        }
    }

    if (req->auth_subclass & AUTH_FILE_RENAME) {
        // If sticky bit stuff gets freaky
        assert(directory);
        FILE_VALIDATE_SUBCLASS(dattr, AUTH_FILE_WRITE, W);
    
        if (!CRED_ROOT(cred) && (dattr.mode & S_ISVTX)) {
            // sticky bit is present, one can only make changes to files they own
            if (dattr.uid != cred->euid && attr.uid != cred->euid) {
                return -EPERM;
            }
        }

    }

    if (req->auth_subclass & AUTH_FILE_UNLINK) {
        // If sticky bit stuff gets freaky
        assert(directory);
        FILE_VALIDATE_SUBCLASS(dattr, AUTH_FILE_WRITE, W);
    
        if (!CRED_ROOT(cred) && (dattr.mode & S_ISVTX)) {
            // sticky bit is present, one can only make changes to files they own
            if (dattr.uid != cred->euid && attr.uid != cred->euid) {
                return -EPERM;
            }
        }
    }

    return 0;
}

/**
 * @brief Perform an authorization request
 * @param request The request to perform
 * @returns 0 on granted, anything else is an error code
 */
int auth_validate(auth_request_t *req) {
    return auth_classes[req->auth_class](req);
}
