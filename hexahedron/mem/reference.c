/**
 * @file hexahedron/mem/reference.c
 * @brief Reference count manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <kernel/misc/spinlock.h>
#include <string.h>

/* Refcount bitmap */
uint8_t *ref_bitmap = NULL;

/* Available frames */
size_t ref_frame_count = 0;

/* Reference lock */
static spinlock_t ref_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MEM:REF", __VA_ARGS__)

/**
 * @brief Initialize the reference system
 * @param bytes How many bytes we need to allocate.
 */
void ref_init(size_t bytes) {
    ref_bitmap = (uint8_t*)mem_sbrk(bytes);
    memset(ref_bitmap, 0, bytes);
    ref_frame_count = bytes;
}

/**
 * @brief Get how many references a frame has
 * @param frame The frame to get references of
 * @returns The current count of references a frame has
 */
int ref_get(uintptr_t frame) {
    if (frame >= ref_frame_count) {
        LOG(ERR, "Attempt to access reference count for frame %p which is outside of allocated frames %d\n", frame, ref_frame_count);
        return -1;
    } 

    spinlock_acquire(&ref_lock);
    int refs = ref_bitmap[frame];
    spinlock_release(&ref_lock);
    return refs;
}

/**
 * @brief Set the frame reference count
 * @param frame The frame to set references of
 * @param refs The references to set
 * @returns The previous references of the frame
 */
int ref_set(uintptr_t frame, uint8_t refs) {
    if (frame >= ref_frame_count) {
        LOG(ERR, "Attempt to access reference count for frame %p which is outside of allocated frames %d\n", frame, ref_frame_count);
        return -1;
    } 

    spinlock_acquire(&ref_lock);
    int refs_old = ref_bitmap[frame];
    ref_bitmap[frame] = refs;
    spinlock_release(&ref_lock);
    return refs_old;
}

/**
 * @brief Increment the current references of a frame
 * @param frame The frame to increment references of
 * @returns The new amount of references or -1 if a new reference cannot be allocated
 */
int ref_increment(uintptr_t frame) {
    if (frame >= ref_frame_count) {
        LOG(ERR, "Attempt to access reference count for frame %p which is outside of allocated frames %d\n", frame, ref_frame_count);
        return -1;
    } 

    spinlock_acquire(&ref_lock);
    
    // Check to see if we overflow
    if (ref_bitmap[frame] + 1 > UINT8_MAX) {
        return -1;
    }
    
    ref_bitmap[frame]++;

    int refs = ref_bitmap[frame];
    spinlock_release(&ref_lock);
    return refs;
}

/**
 * @brief Decrement the current references of a page
 * @param frame The frame to decrement references of
 * @returns The new amount of references or -1 if a reference cannot be decreased
 */
int ref_decrement(uintptr_t frame) {
    if (frame >= ref_frame_count) {
        LOG(ERR, "Attempt to access reference count for frame %p which is outside of allocated frames %d\n", frame, ref_frame_count);
        return -1;
    } 

    spinlock_acquire(&ref_lock);
    
    // Check to see if we underflow
    if (ref_bitmap[frame] == 0) {
        return -1;
    }
    
    ref_bitmap[frame]--;

    int refs = ref_bitmap[frame];
    spinlock_release(&ref_lock);
    return refs;
}
