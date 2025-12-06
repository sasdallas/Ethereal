/**
 * @file drivers/fs/ext2/ext2.c
 * @brief Main components of EXT2 filesystem driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "ext2.h"
#include <kernel/loader/driver.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <kernel/task/process.h>
#include <sys/time.h>
#include <assert.h>

/**
 * @brief Read method for EXT2
 * @param node The node
 * @param off The offset
 * @param size The size
 * @param buffer The buffer
 */
ssize_t ext2_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!size) return 0;
    ext2_t *ext2 = (ext2_t*)node->dev;
    if (off > (off_t)node->length) return 0;
    if ((size_t)off + size > node->length) size = node->length - off;

    // Read the inode (TODO: Cache this in impl_struct?)
    ext2_inode_t *ino;
    int r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return r;

    // Now, we need to do some weird math to calculate blocks
    uint32_t start_inode_block = off / ext2->block_size;
    uint32_t end_inode_block = (off + size) / ext2->block_size;
    uint32_t end_buffer_size = ((off + size) % ext2->block_size);
    uint32_t start_buffer_offset = off % ext2->block_size;

    LOG(DEBUG, "Read blocks %d - %d with end buffer size %d start buffer offset %d\n", start_inode_block, end_inode_block, end_buffer_size, start_buffer_offset);

    size_t buffer_offset = 0;
    for (uint32_t i = start_inode_block; i < end_inode_block; i++) {
        // Read into temporary block buffer
        uint8_t *blk;
        int r = ext2_readInodeBlock(ext2, ino, i, &blk);
        if (r) return r;

        // Is this the first block?
        if (i == start_inode_block) {
            // We should copy with start_buffer_copy
            memcpy(buffer, blk + start_buffer_offset, ext2->block_size - start_buffer_offset);
            buffer_offset += ext2->block_size - start_buffer_offset;
        } else {
            // Nope, we can just copy normally
            memcpy(buffer + buffer_offset, blk, ext2->block_size);
            buffer_offset += ext2->block_size;
        }

        kfree(blk);
    }

    if (end_buffer_size) {
        // Read the final block
        uint8_t *blk;
        int r = ext2_readInodeBlock(ext2, ino, end_inode_block, &blk);
        if (r) return r;

        memcpy(buffer + buffer_offset, blk, end_buffer_size);
        kfree(blk);
    }

    return size;
}

/**
 * @brief Write method for EXT2
 * @param node The node
 * @param off The offset
 * @param size The size
 * @param buffer The buffer
 */
ssize_t ext2_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!size) return 0;
    ext2_t *ext2 = (ext2_t*)node->dev;
    if (off > (off_t)node->length) return 0;


    // Read the inode (TODO: Cache this in impl_struct?)
    ext2_inode_t *ino;
    int r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return r;


    if (size + off > node->length) {
        // We will need to possibly allocate more blocks
        // Allocate additional blocks to extend the file
        uint32_t additional_blocks = ((size + off) - node->length + ext2->block_size - 1) / ext2->block_size;
        for (uint32_t i = 0; i < additional_blocks; i++) {
            uint32_t new_block = ext2_allocateBlock(ext2);
            if (!new_block) {
            LOG(ERR, "Failed to allocate new block\n");
            return -ENOSPC;
            }
            ext2_setInodeBlock(ext2, ino, (node->length / ext2->block_size) + i, new_block);
        }

        // Update the inode size
        node->length = size + off;
        ino->size_low = node->length;
        ext2_writeInode(ext2, ino, node->inode);
    }

    // Now, we need to do some weird math to calculate blocks
    uint32_t start_inode_block = off / ext2->block_size;
    uint32_t end_inode_block = (off + size) / ext2->block_size;
    uint32_t end_buffer_size = ((off + size) % ext2->block_size);
    uint32_t start_buffer_offset = off % ext2->block_size;

    // LOG(DEBUG, "Write blocks %d - %d with end buffer size %d start buffer offset %d\n", start_inode_block, end_inode_block, end_buffer_size, start_buffer_offset);

    size_t buffer_offset = 0;
    for (uint32_t i = start_inode_block; i < end_inode_block; i++) {
        // Read into temporary block buffer
        uint8_t *blk;
        int r = ext2_readInodeBlock(ext2, ino, i, &blk);
        if (r) return r;

        // Is this the first block?
        if (i == start_inode_block) {
            // We should copy with start_buffer_copy
            memcpy(blk + start_buffer_offset, buffer, ext2->block_size - start_buffer_offset);
            buffer_offset += ext2->block_size - start_buffer_offset;
        } else {
            // Nope, we can just copy normally
            memcpy(blk, buffer + buffer_offset , ext2->block_size);
            buffer_offset += ext2->block_size;
        }

        ext2_writeInodeBlock(ext2, ino, i, blk);

        kfree(blk);
    }

    if (end_buffer_size) {
        // Read the final block
        uint8_t *blk = NULL;
        int r = ext2_readInodeBlock(ext2, ino, end_inode_block, &blk);
        if (r) return r;

        memcpy(blk, buffer + buffer_offset, end_buffer_size);
        ext2_writeInodeBlock(ext2, ino, end_inode_block, blk);
        kfree(blk);
    }

    return size;
}



/**
 * @brief Find directory method for EXT2
 * @param node Node
 * @param name The name to search for
 */
fs_node_t *ext2_finddir(fs_node_t *node, char *name) {
    ext2_t *ext2 = (ext2_t*)node->dev;

    // Read the inode (TODO: Cache this in impl_struct?)
    ext2_inode_t *ino;
    int r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return NULL;

    // Begin at inode block 0 
    uint8_t blk = 0;
    uint8_t *block_buffer = NULL;
    size_t offset = 0;
    size_t offset_total = 0;

    while (offset_total < ino->size_low) {
        if (!block_buffer) {
            // We have to read this block in
            int r = ext2_readInodeBlock(ext2, ino, blk, &block_buffer);
            if (r != 0) return NULL;
        }

        ext2_dirent_t *dent = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);
        
        if (dent->name_length == strlen(name)) {       
            char dname_tmp[strlen(dent->name)];
            memcpy(dname_tmp, dent->name, strlen(dent->name));
            dname_tmp[dent->name_length] = 0;

            if (dent->inode && !strcmp(dname_tmp, name)) {
                uint8_t *inode_buf;
                if (ext2_readInode(ext2, dent->inode, &inode_buf)) {
                    LOG(ERR, "Error reading inode\n");
                    return NULL;
                }

                fs_node_t *n = ext2_inodeToNode(ext2, (ext2_inode_t*)inode_buf, dent->inode, dent->name);
                kfree(inode_buf);
                return n;
            }
        }

        // Next!
        offset += dent->entry_size;
        offset_total += dent->entry_size;
        if (offset >= ext2->block_size) {
            blk++;
            offset -= ext2->block_size;
            
            kfree(block_buffer);
            block_buffer = NULL;
        }
    }

    if (block_buffer) kfree(block_buffer);
    return NULL;
}

/**
 * @brief Read directory method for EXT2
 * @param node Node
 * @param index The index to use
 */
struct dirent *ext2_readdir(fs_node_t *node, unsigned long index) {
    ext2_t *ext2 = (ext2_t*)node->dev;

    // Read the inode (TODO: Cache this in impl_struct?)
    ext2_inode_t *ino;
    int r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return NULL;

    // Begin at inode block 0 
    uint8_t blk = 0;
    uint8_t *block_buffer = NULL;
    uint32_t idx = 0;
    size_t offset = 0;
    size_t offset_total = 0;

    while (offset_total < ino->size_low) {
        if (!block_buffer) {
            // We have to read this block in
            int r = ext2_readInodeBlock(ext2, ino, blk, &block_buffer);
            if (r != 0) return NULL;
        }

        ext2_dirent_t *dent = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);
        
        if (dent->inode && index == idx) {
            struct dirent *ent = kmalloc(sizeof(struct dirent));
            memcpy(ent->d_name, dent->name, dent->name_length);
            ent->d_name[dent->name_length] = 0;
            ent->d_off = 0;
            
            // TODO: d_type and other fields
            return ent;
        }

        // Next index
        if (dent->inode) idx++;

        // Next!
        offset += dent->entry_size;
        offset_total += dent->entry_size;
        if (offset >= ext2->block_size) {
            blk++;
            offset -= ext2->block_size;
            
            kfree(block_buffer);
            block_buffer = NULL;
        }
    }

    if (block_buffer) kfree(block_buffer);
    return NULL;
}

/**
 * @brief Create entry method
 * @param node Node
 * @param name The name of the entry
 * @param mode Mode to create width
 */
int ext2_create(fs_node_t *node, char *name, mode_t mode, fs_node_t **node_out) {
    ext2_t *ext2 = (ext2_t*)node->dev;
    
    LOG(DEBUG, "Creating entry: %s\n", name);

    // Get a new inode
    uint32_t new_ino = ext2_allocateInode(ext2);

    LOG(DEBUG, "Allocated new inode: %d\n", new_ino);

    // Read the inode
    ext2_inode_t *new_inode = NULL;
    int r = ext2_readInode(ext2, new_ino, (uint8_t**)&new_inode);
    if (r) return r;

    // Setup fields
    memset(new_inode, 0, sizeof(ext2_inode_t));

    new_inode->atime = now();
    new_inode->mtime = new_inode->atime;
    new_inode->ctime = new_inode->atime;
    new_inode->uid = current_cpu->current_process->euid;
    new_inode->gid = current_cpu->current_process->egid;
    new_inode->block_address = 0x0;
    new_inode->disk_sectors_used = 0;
    
    new_inode->type = EXT2_INODE_TYPE_FILE;
#define CONVERT_MASK_BIT(b) if (mode & S_##b) new_inode->type |= EXT2_INODE_##b;
    CONVERT_MASK_BIT(IRUSR);
    CONVERT_MASK_BIT(IWUSR);
    CONVERT_MASK_BIT(IXUSR);
    CONVERT_MASK_BIT(IRGRP);
    CONVERT_MASK_BIT(IWGRP);
    CONVERT_MASK_BIT(IXGRP);
    CONVERT_MASK_BIT(IROTH);
    CONVERT_MASK_BIT(IWOTH);
    CONVERT_MASK_BIT(IXOTH);
#undef CONVERT_MASK_BIT

    new_inode->nlink = 1;
    ext2_writeInode(ext2, new_inode, new_ino);

    // Now create the directory entry

    ext2_inode_t *ino;
    r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return -EIO;

    // Begin at inode block 0 
    uint8_t blk = 0;
    uint8_t *block_buffer = NULL;
    uint32_t idx = 0;
    size_t offset = 0;
    size_t offset_total = 0;

    ext2_dirent_t *dir_to_modify = NULL;

    // Calculate needed inode length
    unsigned int needed_length = strlen(name) + sizeof(ext2_dirent_t);
    needed_length += (needed_length % 4) ? (4 - (needed_length % 4)) : 0;

    while (offset_total < ino->size_low) {
        if (!block_buffer) {
            // We have to read this block in
            int r = ext2_readInodeBlock(ext2, ino, blk, &block_buffer);
            if (r != 0) return -EIO;
        }

        ext2_dirent_t *dent = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);

        if (!dent->inode) {
            // This is a fake inode, let's fuck it up.
            dir_to_modify = dent;
            goto _modify;
        }

        // Next index
        if (dent->inode) idx++;


        // Next!
        offset += dent->entry_size;
        offset_total += dent->entry_size;

        // Check if we're at the end
        if (offset_total >= ino->size_low) {
            assert(offset_total == ino->size_low);
            
            // Validte length (taken from ToaruOS)
            unsigned int expected = dent->name_length + sizeof(ext2_dirent_t);
            expected += (expected % 4) ? (4 - (expected % 4)) : 0;

            if (dent->entry_size != expected) {
                // Let's see, we might have to expand this inode if it will be greater than a block
                if (offset + needed_length > ext2->block_size) {
                    // Yes, it was.
                    // Allocate a new inode block
                    blk++;
                    kfree(block_buffer);
                    uint32_t new_blk_num = ext2_allocateBlock(ext2);
                    ino->size_low += ext2->block_size;
                    ext2_readBlock(ext2, new_blk_num, &block_buffer);
                    memset(block_buffer, 0, ext2->block_size);
                    ext2_setInodeBlock(ext2, ino, blk, new_blk_num);
                    ext2_writeInode(ext2, ino, node->inode);
                    offset = 0;
                } else {
                    // (also taken from ToaruOS) Adjust previous node reclen
                    unsigned int sreclen = dent->name_length + sizeof(ext2_dirent_t);
                    sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;
                    dent->entry_size = sreclen;
                }
                
                dir_to_modify = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);
                goto _modify;
            }
        }

        if (offset >= ext2->block_size) {
            blk++;
            offset -= ext2->block_size;
            
            kfree(block_buffer);
            block_buffer = NULL;
        }
    }



_modify:
    if (!dir_to_modify) {
        LOG(WARN, "BAD!\n"); // ???
        return -EFAULT;
    }

    memset(dir_to_modify, 0, sizeof(ext2_dirent_t));
    dir_to_modify->inode = new_ino;
    strcpy(dir_to_modify->name, name);
    dir_to_modify->name_length = strlen(name);
    dir_to_modify->entry_size = ext2->block_size - offset;

    ext2_writeInodeBlock(ext2, ino, blk, block_buffer);
    if (block_buffer) kfree(block_buffer);

    *node_out = ext2_inodeToNode(ext2, new_inode, new_ino, name);
    kfree(ino);
    kfree(new_inode);

    return 0;
}

/**
 * @brief Make directory EXT2
 * @param node The node to make the directory in
 * @param name The name of the directory
 * @param mode The mode of the directory
 */
int ext2_mkdir(fs_node_t *node, char *name, mode_t mode) {
    ext2_t *ext2 = (ext2_t*)node->dev;
    
    LOG(DEBUG, "Creating entry: %s\n", name);

    // Get a new inode
    uint32_t new_ino = ext2_allocateInode(ext2);

    LOG(DEBUG, "Allocated new inode: %d\n", new_ino);

    // Read the inode
    ext2_inode_t *new_inode = NULL;
    int r = ext2_readInode(ext2, new_ino, (uint8_t**)&new_inode);
    if (r) return r;

    // Setup fields
    memset(new_inode, 0, sizeof(ext2_inode_t));

    new_inode->atime = now();
    new_inode->mtime = new_inode->atime;
    new_inode->ctime = new_inode->atime;
    new_inode->uid = current_cpu->current_process->euid;
    new_inode->gid = current_cpu->current_process->egid;
    new_inode->block_address = 0x0;
    new_inode->disk_sectors_used = 0;
    
    new_inode->type = EXT2_INODE_TYPE_DIRECTORY;
#define CONVERT_MASK_BIT(b) if (mode & S_##b) new_inode->type |= EXT2_INODE_##b;
    CONVERT_MASK_BIT(IRUSR);
    CONVERT_MASK_BIT(IWUSR);
    CONVERT_MASK_BIT(IXUSR);
    CONVERT_MASK_BIT(IRGRP);
    CONVERT_MASK_BIT(IWGRP);
    CONVERT_MASK_BIT(IXGRP);
    CONVERT_MASK_BIT(IROTH);
    CONVERT_MASK_BIT(IWOTH);
    CONVERT_MASK_BIT(IXOTH);
#undef CONVERT_MASK_BIT

    new_inode->nlink = 2; // . and the parent
    ext2_writeInode(ext2, new_inode, new_ino);

    // Now create the directory entry
    ext2_inode_t *ino;
    r = ext2_readInode(ext2, node->inode, (uint8_t**)&ino);
    if (r) return -EIO;

    // Begin at inode block 0 
    uint8_t blk = 0;
    uint8_t *block_buffer = NULL;
    uint32_t idx = 0;
    size_t offset = 0;
    size_t offset_total = 0;

    ext2_dirent_t *dir_to_modify = NULL;

    // Calculate needed inode length
    unsigned int needed_length = strlen(name) + sizeof(ext2_dirent_t);
    needed_length += (needed_length % 4) ? (4 - (needed_length % 4)) : 0;

    while (offset_total < ino->size_low) {
        if (!block_buffer) {
            // We have to read this block in
            int r = ext2_readInodeBlock(ext2, ino, blk, &block_buffer);
            if (r != 0) return -EIO;
        }

        ext2_dirent_t *dent = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);

        if (!dent->inode) {
            // This is a fake inode, let's fuck it up.
            dir_to_modify = dent;
            goto _modify;
        }

        // Next index
        if (dent->inode) idx++;


        // Next!
        offset += dent->entry_size;
        offset_total += dent->entry_size;

        // Check if we're at the end
        if (offset_total >= ino->size_low) {
            assert(offset_total == ino->size_low);
            
            // Validte length (taken from ToaruOS)
            unsigned int expected = dent->name_length + sizeof(ext2_dirent_t);
            expected += (expected % 4) ? (4 - (expected % 4)) : 0;

            if (dent->entry_size != expected) {
                // Let's see, we might have to expand this inode if it will be greater than a block
                if (offset + needed_length > ext2->block_size) {
                    // Yes, it was.
                    // Allocate a new inode block
                    blk++;
                    kfree(block_buffer);
                    uint32_t new_blk_num = ext2_allocateBlock(ext2);
                    ino->size_low += ext2->block_size;
                    ext2_readBlock(ext2, new_blk_num, &block_buffer);
                    memset(block_buffer, 0, ext2->block_size);
                    ext2_setInodeBlock(ext2, ino, blk, new_blk_num);
                    ext2_writeInode(ext2, ino, node->inode);
                    offset = 0;
                } else {
                    // (also taken from ToaruOS) Adjust previous node reclen
                    unsigned int sreclen = dent->name_length + sizeof(ext2_dirent_t);
                    sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;
                    dent->entry_size = sreclen;
                }
                
                dir_to_modify = (ext2_dirent_t*)((uintptr_t)block_buffer + offset);
                goto _modify;
            }
        }

        if (offset >= ext2->block_size) {
            blk++;
            offset -= ext2->block_size;
            
            kfree(block_buffer);
            block_buffer = NULL;
        }
    }



_modify:
    if (!dir_to_modify) {
        LOG(WARN, "BAD!\n"); // ???
        return -EFAULT;
    }

    memset(dir_to_modify, 0, sizeof(ext2_dirent_t));
    dir_to_modify->inode = new_ino;
    strcpy(dir_to_modify->name, name);
    dir_to_modify->name_length = strlen(name);
    dir_to_modify->entry_size = ext2->block_size - offset;

    ext2_writeInodeBlock(ext2, ino, blk, block_buffer);
    if (block_buffer) { kfree(block_buffer); block_buffer = NULL; }


    // Update parent link count!
    ino->nlink++;
    ext2_writeInode(ext2, ino, node->inode);
    kfree(ino);


    // Create the inode's two entries, '.' and '..'
    new_inode->size_low = ext2->block_size;
    uint32_t first_blk = ext2_allocateBlock(ext2);
    ext2_setInodeBlock(ext2, new_inode, 0, first_blk);

    ext2_writeInode(ext2, new_inode, new_ino);

    // Create a block for the inode
    ext2_readBlock(ext2, first_blk, &block_buffer);

    memset(block_buffer, 0, ext2->block_size);

    ext2_dirent_t *d = (ext2_dirent_t*)(block_buffer);
    memset(d, 0, 12);
    d->inode = new_ino;
    d->name[0] = '.';
    d->name_length = 1;
    d->entry_size = 12;

    d = (ext2_dirent_t*)(block_buffer + 12);
    memset(d, 0, 12);
    d->inode = node->inode;
    d->name[0] = '.';
    d->name[1] = '.';
    d->name_length = 2;
    d->entry_size = ext2->block_size - 12;

    ext2_writeBlock(ext2, first_blk, block_buffer);
    kfree(block_buffer);
    kfree(new_inode);
    
    // Update and flush BGDs
    uint32_t grp = new_ino / ext2->inodes_per_group;
    EXT2_BGD(grp)->directory_count++;
    ext2_flushBGDs(ext2);
    return 0;
}

/**
 * @brief Convert inode to VFS node
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert
 * @param inode_number The number of the inode
 * @param name The name of the file
 */
fs_node_t *ext2_inodeToNode(ext2_t *ext2, ext2_inode_t *inode, uint32_t inode_number, char *name) {
    fs_node_t *r = fs_node();
    strncpy(r->name, name, 256);

    // Determine inode type
    if ((inode->type & EXT2_INODE_TYPE_SOCKET) == EXT2_INODE_TYPE_SOCKET) {
        r->flags = VFS_SOCKET;
    } else if ((inode->type & EXT2_INODE_TYPE_SYMLINK) == EXT2_INODE_TYPE_SYMLINK) {
        r->flags = VFS_SYMLINK;
    } else if ((inode->type & EXT2_INODE_TYPE_FILE) == EXT2_INODE_TYPE_FILE) {
        r->flags = VFS_FILE;
    } else if ((inode->type & EXT2_INODE_TYPE_BLKDEV) == EXT2_INODE_TYPE_BLKDEV) {
        r->flags = VFS_BLOCKDEVICE;
    } else if ((inode->type & EXT2_INODE_TYPE_DIRECTORY) == EXT2_INODE_TYPE_DIRECTORY) {
        r->flags = VFS_DIRECTORY;
    } else if ((inode->type & EXT2_INODE_TYPE_CHARDEV) == EXT2_INODE_TYPE_CHARDEV) {
        r->flags = VFS_CHARDEVICE;
    } else if ((inode->type & EXT2_INODE_TYPE_FIFO) == EXT2_INODE_TYPE_FIFO) {
        r->flags = VFS_PIPE;
    }

    if (r->flags & VFS_DIRECTORY) {
        r->readdir = ext2_readdir;
        r->finddir = ext2_finddir;
        r->create = ext2_create;
        r->mkdir = ext2_mkdir;
    } else {
        r->read = ext2_read;
        r->write = ext2_write;
    }

    r->atime = inode->atime;
    r->mtime = inode->mtime;
    r->ctime = inode->ctime;
    r->inode = inode_number;
    r->length = inode->size_low; // TODO: size_upper

    // Convert EXT2 mask
#define CONVERT_MASK_BIT(b) if (inode->type & EXT2_INODE_##b) r->mask |= S_##b;
    CONVERT_MASK_BIT(IRUSR);
    CONVERT_MASK_BIT(IWUSR);
    CONVERT_MASK_BIT(IXUSR);
    CONVERT_MASK_BIT(IRGRP);
    CONVERT_MASK_BIT(IWGRP);
    CONVERT_MASK_BIT(IXGRP);
    CONVERT_MASK_BIT(IROTH);
    CONVERT_MASK_BIT(IWOTH);
    CONVERT_MASK_BIT(IXOTH);
    CONVERT_MASK_BIT(ISUID);
    CONVERT_MASK_BIT(ISGID);
#undef CONVERT_MASK_BIT

    r->uid = inode->uid;
    r->gid = inode->gid;

    r->dev = (void*)ext2;

    return r;
}

/**
 * @brief EXT2 mount callback 
 * @param argument The argument to use
 * @param mountpoint The destination mountpoint
 */
int ext2_mount(char *argument, char *mountpoint, struct fs_node** node_out) {
    LOG(DEBUG, "Mounting EXT2 filesystem from %s -> %s\n", argument, mountpoint);

    // Get the drive
    fs_node_t *drive = kopen(argument, 0);
    if (!drive) return -ENODEV;

    // Allocate EXT2
    ext2_t *ext2 = kzalloc(sizeof(ext2_t));
    
    // Read into the superblock
    if (fs_read(drive, 1024, sizeof(ext2_superblock_t), (uint8_t*)&ext2->superblock) != sizeof(ext2_superblock_t)) {
        LOG(ERR, "I/O error while reading.\n");
        return -EIO;
    }

    // Check superblock signature
    if (ext2->superblock.signature != EXT2_SIGNATURE) {
        LOG(ERR, "Invalid signature on superblock\n");
        return -EINVAL;
    }

    // Fill fields out in EXT2 filesystem structure
    ext2->drive = drive;
    ext2->block_size = (1024 << ext2->superblock.block_size_unshifted);
    ext2->inode_size = EXT2_DEFAULT_INODE_SIZE;
    ext2->bgd_count = ext2->superblock.block_count / ext2->superblock.bg_block_count;
    ext2->inodes_per_group = ext2->superblock.inode_count / ext2->bgd_count;
    ext2->bgd_offset = (ext2->block_size > 1024 ? 1 : 2);

    if (EXT2_EXTENDED(ext2)) {
        ext2->inode_size = ext2->superblock.extended.inode_size;
    }

    // Valid EXT2 filesystem
    LOG(DEBUG, "EXT2 filesystem detected: block size %d, inode size %d, bgd count %d, inodes per group %d\n", ext2->block_size, ext2->inode_size, ext2->bgd_count, ext2->inodes_per_group);

    // Calculate amount of BGD blocks
    size_t bgd_blocks = (sizeof(ext2_bgd_t) * ext2->bgd_count) / ext2->block_size;
    bgd_blocks += 1;
    ext2->bgd_blocks = bgd_blocks;

    // Allocate BGD memory
    ext2->bgds = kzalloc(ext2->bgd_blocks * ext2->block_size);

    // Read the EXT2 block
    for (uint32_t i = 0; i < ext2->bgd_blocks; i++) {
        uint8_t *bgd_block;
        int r = ext2_readBlock(ext2, ext2->bgd_offset + i, &bgd_block);
        if (r) {
            LOG(ERR, "Error reading EXT2 block\n");
            kfree(ext2->bgds);
            kfree(ext2);
            return r;
        }

        // Copy the block
        memcpy(ext2->bgds + (i * ext2->block_size), bgd_block, ext2->block_size);

        // Now copy into BGD table
        ext2_bgd_t *bgd = (ext2_bgd_t*)bgd_block;
        for (unsigned i = 0; i < ext2->bgd_count; i++) {
            LOG(DEBUG, "BGD %d: blkusage=%d inousage=%d inotable=%d blkunalloc=%d inounaloc=%d dircount=%d\n", i, bgd->block_usage_bitmap, bgd->inode_usage_bitmap, bgd->inode_table, bgd->unallocated_blocks, bgd->unallocated_inodes, bgd->directory_count);
            bgd++;
        }

        kfree(bgd_block);
    }

    // Read the root inode
    uint8_t *b;
    if (ext2_readInode(ext2, 2, &b)) {
        LOG(ERR, "Error reading root inode\n");
        kfree(ext2->bgds);
        kfree(ext2);
        return -EINVAL;
    }
    
    // Convert inode to node
    ext2_inode_t *ino = (ext2_inode_t*)b;
    LOG(DEBUG, "uid=%d gid=%d mode=%x\n", ino->uid, ino->gid, ino->type);
    *node_out = ext2_inodeToNode(ext2, ino, 2, "/");
    return 0;
}

/**
 * @brief Write the superblock after changes were made
 * @param ext2 EXT2 filesystem
 */
void ext2_flushSuperblock(ext2_t *ext2) {
    // Rewrite the superblock
    fs_write(ext2->drive, 1024, sizeof(ext2_superblock_t), (uint8_t*)&ext2->superblock);
}

/**
 * @brief Flush all block group descriptors
 * @param ext2 EXT2 filesystem
 */
void ext2_flushBGDs(ext2_t *ext2) {
    // For each BGD rewrite it
    for (uint32_t i = 0; i < ext2->bgd_blocks; i++) {
        ext2_writeBlock(ext2, i + ext2->bgd_offset, (uint8_t*)ext2->bgds + (i * ext2->block_size));
    }
}

/**
 * @brief Driver initialize method
 */
int driver_init(int argc, char *argv[]) {
    vfs_registerFilesystem("ext2", ext2_mount);
    return 0;
}

/**
 * @brief Driver deinit method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "EXT2 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};