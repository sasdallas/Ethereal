/**
 * @file drivers/fs/ext2/ext2.h
 * @brief EXT2
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _EXT2_H
#define _EXT2_H

/**** INCLUDES ****/
#include "ext2_structures.h"
#include <kernel/fs/vfs_new.h>
#include <structs/list.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define EXT2_BCACHE_NONE            ((uint64_t)-1)
#define EXT2_BCACHE_MAX_HASH        512                 // Total amount of hash buckets
#define EXT2_BCACHE_POOL_SIZE       256                 // Size of the initial buffer pool

#define EXT2_MAX_BIO_PAGES          128                 // Stored on the stack, each is uintptr_t size


/**** TYPES ****/

/* metadata buffer */
/* used by the metadata cache to cache quickly used blocks */
typedef struct ext2_buffer {
    struct ext2_buffer *next;
    struct ext2_buffer *prev;
    DLIST_ENTRY(struct ext2_buffer) lru;
    
    uint64_t device_page;           // device offset in page form
    uintptr_t phys_backing;         // phys page behind vpage
    char *data;                     // the vpage that actually backs this
    refcount_t refs;
    bool valid;
    bool dirty;
    mutex_t lock;
} ext2_buffer_t;

/* buffer cache */
typedef struct ext2_bcache {
    mutex_t lock;
    ext2_buffer_t pool[EXT2_BCACHE_POOL_SIZE];
    ext2_buffer_t *hash[EXT2_BCACHE_MAX_HASH];
    DLIST_HEAD(lru, ext2_buffer_t);
} ext2_bcache_t;

/* block group */
typedef struct ext2_group {
    rwsem_t sem;
    ext2_bgd_t bg;
    uint32_t *blkbm;    // NULL if not loaded
    uint32_t *inobm;    // NULL if not loaded
    uint32_t hint;      // last allocation in the group
    bool bg_dirty;
    bool blkbm_dirty;
    bool inobm_dirty;
} ext2_group_t;

typedef struct ext2 {
    vfs_mount_t *mount;
    vfs_file_t *backing;

    ext2_sb_t sb;
    mutex_t sb_lock;
    bool sb_dirty;

    size_t blksize;
    size_t blkshift;
    size_t inosize;
    size_t ptrs_per_block;  // blksize / sizeof(uint32_t)
    size_t blocks_per_page; // if 0, blksize > PAGE_SIZE

    ext2_group_t *groups;
    size_t ngroups;
    uint32_t alloc_hint;

    ext2_bcache_t bcache;
} ext2_t;

typedef struct ext2_extent {
    uint32_t lblk;  // logical block
    uint32_t pblk;  // physical block
    uint32_t len;   // amount of blocks
} ext2_extent_t;

typedef struct ext2_node {
    rwsem_t sem;
    ext2_t *fs;
    ino_t num;
    ext2_inode_t ino;
    bool dirty;
    uint32_t last_alloc;

    // TODO: move this to vfs
    struct {
        spinlock_t lock;
        bool valid;
        unsigned int pos;
        uint32_t lblk;
        uint32_t off;

        uint32_t ins_hint; // not protected by lock
    } dcursor;
} ext2_node_t;

/**** MACROS ****/

#define EXT2_BLOCK_TO_DISK(fs, blk) (((loff_t)(blk)) << (fs)->blkshift)
#define EXT2_TO_BLOCKS(fs, sz) (((sz) + (fs)->blksize - 1) >> (fs)->blkshift)

#define EXT2_INO_GROUP(fs, ino) (((ino) - 1) / (fs)->sb.bg_ino_count)
#define EXT2_INO_INDEX(fs, ino) (((ino) - 1) % (fs)->sb.bg_ino_count)
#define EXT2_BLK_GROUP(fs, blk) (((blk) - (fs)->sb.start_block_number) / (fs)->sb.bg_block_count)
#define EXT2_BLK_INDEX(fs, blk) (((blk) - (fs)->sb.start_block_number) % (fs)->sb.bg_block_count)
#define EXT2_BGD_READ_OFFSET(fs, bgd) ((EXT2_BLOCK_TO_DISK(fs, (fs)->sb.start_block_number+1) + (sizeof(ext2_bgd_t) * (bgd))))

#define EXT2_GROUP(fs, g) (&((fs)->groups[(g)]))

#define EXT2_EMBEDDED_LINK_SIZE (15 * sizeof(uint32_t))

#define NODE_START_READ(n)              rwsem_startRead(&(n)->sem)
#define NODE_FINISH_READ(n)             rwsem_finishRead(&(n)->sem)
#define NODE_START_WRITE(n)             rwsem_startWrite(&(n)->sem)
#define NODE_FINISH_WRITE(n)            rwsem_finishWrite(&(n)->sem)

#define GROUP_START_READ(fs, g)         rwsem_startRead(&(fs)->groups[(g)].sem)
#define GROUP_FINISH_READ(fs, g)        rwsem_finishRead(&(fs)->groups[(g)].sem)
#define GROUP_START_WRITE(fs, g)        rwsem_startWrite(&(fs)->groups[(g)].sem)
#define GROUP_FINISH_WRITE(fs, g)       rwsem_finishWrite(&(fs)->groups[(g)].sem)

#define EXT2_HAS_DIRENT_TYPE(fs) (((fs)->sb.ver_major > 0) && !!((fs)->sb.extended.features_required & EXT2_REQUIRES_DIRECTORY_TYPE))

/**** FUNCTIONS ****/

/* bcache */
void ext2_bcacheInit(ext2_t *fs);
void ext2_bcacheDestroy(ext2_t *fs);
ext2_buffer_t *ext2_bcacheGet(ext2_t *fs, uint64_t page);
void ext2_bcacheFinish(ext2_t *fs, ext2_buffer_t *buffer);
ext2_buffer_t *ext2_bcacheGetZero(ext2_t *fs, uint64_t page);
int ext2_bcacheSync(ext2_t *fs);

/* inode */
int ext2_inodeRead(ext2_t *fs, ino_t ino, ext2_inode_t *out);
int ext2_inodeWrite(ext2_t *fs, ino_t ino, ext2_inode_t *out);
int ext2f_inodeClear(ext2_t *fs, ino_t ino);

static inline int ext2_flushNode(ext2_node_t *n) {
    if (n->dirty == false) {
        return 0;
    }

    int r = ext2_inodeWrite(n->fs, n->num, &n->ino);
    if (r != 0) return r;
    n->dirty = false;
    return 0;
}

static inline void ext2_markErrorInternal(ext2_t *fs, char *file, int line, const char *function) {
    // no need to lock sb here, its fucked anyways :-)
    fs->sb.state = EXT2_STATE_ERROR;
    fs->sb_dirty = true;
    dprintf_module(WARN, "DRIVER:EXT2", "(%s:%d in %s) Filesystem has errors\n", file, line, function);
}

#define ext2_markError(fs) ext2_markErrorInternal(fs, __FILE__, __LINE__, __FUNCTION__)

static inline uint64_t ext2_size(ext2_t *fs, ext2_inode_t *ino) {
    if ((ino->type_permissions & 0xF000) == EXT2_INODE_TYPE_DIR) return (uint64_t)ino->size_lower;
    if (fs->sb.ver_major > 0 && (fs->sb.extended.features_write & EXT2_WRITE_64BIT)) {
        return (((uint64_t)ino->size_upper) << 32) | ino->size_lower;
    }

    return (uint64_t)ino->size_lower;
}

static inline void ext2_setSize(ext2_t *fs, ext2_inode_t *ino, uint64_t sz) {
    if ((ino->type_permissions & 0xF000) == EXT2_INODE_TYPE_DIR) {
        ino->size_lower = (uint32_t)sz;
        return;
    }

    if (fs->sb.ver_major > 0 && (fs->sb.extended.features_write & EXT2_WRITE_64BIT)) {
        ino->size_lower = (uint32_t)(sz & 0xFFFFFFFF);
        ino->size_upper = (uint32_t)(sz >> 32);
        return;
    }

    ino->size_lower = (uint32_t)sz;
}


/* bio */
int ext2_bio(ext2_t *fs, loff_t off, uintptr_t *pages, size_t npages, bool write);
char *ext2_blockGet(ext2_t *fs, uint32_t blk, ext2_buffer_t **out);
int ext2_blockRead(ext2_t *fs, uint32_t blk, void *dst);
int ext2_blockZero(ext2_t *fs, uint32_t blk);
int ext2_blockWrite(ext2_t *fs, uint32_t blk, void *src);

// !!! DOES NOT ZERO THE BLOCK, DO THAT YOURSELF
char *ext2_blockGetZero(ext2_t *fs, uint32_t blk, ext2_buffer_t **out);

/* node */
int ext2_rootInit(vfs_mount_t *mount, ext2_t *fs);
void ext2_nodeInit();

/* dir */
int ext2_dirIterate(ext2_node_t *n, vfs_dir_context_t *ctx);
int ext2_dirFind(ext2_node_t *n, char *name, ino_t *out);
int ext2_dirInsert(ext2_node_t *n, char *name, ino_t ino, vfs_ino_type_t type);
int ext2_dirRemove(ext2_node_t *n, char *name, ino_t ino);
int ext2_dirReplace(ext2_node_t *n, char *name, ino_t old_ino, ino_t new_ino, vfs_ino_type_t type);
int ext2_dirEmpty(ext2_node_t *n, bool *empty);
int ext2_dirSetParent(ext2_node_t *n, ino_t parent);

/* map */
int ext2_map(ext2_node_t *n, uint32_t lblk, uint32_t want, bool alloc, ext2_extent_t *out);
ssize_t ext2_nodeReadBytes(ext2_node_t *n, loff_t off, size_t size, char *buffer);
ssize_t ext2_nodeWriteBytes(ext2_node_t *n, loff_t off, size_t size, char *buffer);
int ext2_truncateInternal(ext2_node_t *n, uint64_t new_size);

/* alloc */
// preferred is where you want the allocation to start (or 0 if no pref.)
// NOTE: will not gurantee that *got == want, just gurantees you get the largest run near preferred.
int ext2_alloc(ext2_t *fs, uint32_t preferred, size_t want, uint32_t *start, size_t *got);
int ext2_allocInode(ext2_t *fs, uint32_t b_group, bool dir, ino_t *out);
int ext2_freeBlocks(ext2_t *fs, uint32_t start, size_t count);
int ext2_freeInode(ext2_t *fs, ino_t ino, bool dir);

#endif

