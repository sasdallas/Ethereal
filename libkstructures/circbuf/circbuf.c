/**
 * @file libkstructures/circbuf/circbuf.c
 * @brief Circular buffer implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <structs/circbuf.h>
#include <kernel/mm/alloc.h>
#include <string.h>
#include <errno.h>
#include <kernel/debug.h>

/**
 * @brief Create a new circular buffer
 * @param name Optional name of the circular buffer
 * @param size Size of the circular buffer
 * @returns A pointer to the circular buffer
 */
circbuf_t *circbuf_create(char *name, size_t size) {
    circbuf_t *circbuf = kmalloc(sizeof(circbuf_t));
    memset(circbuf, 0, sizeof(circbuf_t));

    circbuf->name = name;
    circbuf->buffer_size = size;
    circbuf->buffer = kmalloc(size);
    memset(circbuf->buffer, 0, size);
    circbuf->lock = spinlock_create("circular buffer lock");
    circbuf->head = 0;
    circbuf->tail = 0;
    circbuf->readers = sleep_createQueue("circbuf readers");
    circbuf->writers = sleep_createQueue("circbuf writers");

    return circbuf;
}

/**
 * @brief Get some data from a circular buffer
 * @param circbuf The buffer to get from
 * @param size The MAXIMUM amount of data to get
 * @param buffer The output buffer
 * @returns Amount of characters read or error code
 */
ssize_t circbuf_read(circbuf_t *circbuf, size_t size, uint8_t *buffer) {
    if (!circbuf || !buffer) return 1;


    // Start readin' data
    size_t got = 0;
    while (!got) {
        if (circbuf->tail == circbuf->head) {
            // Did we collect anything?
            if (!got) {
                // Wakeup the writers
                sleep_wakeupQueue(circbuf->writers, 1);

                // No, drop lock and sleep in reader queue
                sleep_inQueue(circbuf->readers);
                int w = sleep_enter();
                if (w == WAKEUP_SIGNAL) return -EINTR;

                // Were we stopped?
                if (circbuf->stop) return 0;

                // Keep going
                continue;
            } else {
                // How bad did they really want it?
                break;
            }
        }


        spinlock_acquire(circbuf->lock);
        while (got < size && circbuf->tail != circbuf->head) {
            buffer[got] = circbuf->buffer[circbuf->tail];
            circbuf->tail++;
            if ((size_t)circbuf->tail >= circbuf->buffer_size) circbuf->tail = 0;
            got++;
        }
        spinlock_release(circbuf->lock);

        // Wakeup writers if there are any
        sleep_wakeupQueue(circbuf->writers, 1);
    }
    return got;
}

/**
 * @brief Write some data to a circular buffer
 * @param circbuf The buffer to write to
 * @param size The amount of data to write
 * @param buffer The input buffer
 * @returns Amount of characters written or error code
 */
ssize_t circbuf_write(circbuf_t *circbuf, size_t size, uint8_t *buffer) {
    if (!circbuf || !buffer) return 1;


    // Start copyin' data
    size_t copied = 0;
    while (copied == 0) {
        // Check, do we have any space?
        if (circbuf->tail - circbuf->head - 1 == 0) {
            // We have no space
            if (copied) break; // It doesn't matter

            // Sleep in writers queue
            sleep_inQueue(circbuf->writers);
            int w = sleep_enter();
            if (w == WAKEUP_SIGNAL) return -EINTR;

            // Were we stopped?
            if (circbuf->stop) return 0;

            continue;
        }
        
        // Acquire lock and write
        spinlock_acquire(circbuf->lock);
        while (circbuf->tail - circbuf->head - 1 != 0 && copied < size) {
            circbuf->buffer[circbuf->head] = buffer[copied];
            circbuf->head++;
            if ((size_t)circbuf->head >= circbuf->buffer_size) circbuf->head = 0;
            copied++;
        }
        spinlock_release(circbuf->lock);

        // Wakeup any readers
        sleep_wakeupQueue(circbuf->readers, 1);
    }

    return copied;
}

/**
 * @brief Returns whether a circular buffer has any content available
 * @param circbuf The circular buffer
 */
int circbuf_available(circbuf_t *circbuf) {
    return !(circbuf->tail == circbuf->head);
}

/**
 * @brief Destroy a circular buffer
 * @param circbuf The circular buffer to destroy
 */
void circbuf_destroy(circbuf_t *circbuf) {
    kfree(circbuf->readers);
    kfree(circbuf->writers);
    spinlock_destroy(circbuf->lock);
    kfree(circbuf->buffer);
    kfree(circbuf);
}

/**
 * @brief Returns how much content a circbuf has left to read
 * @param circbuf The circbuf to check
 */
ssize_t circbuf_remaining_read(circbuf_t *circbuf) {
    // Three conditions:
    // Head is equal to tail, i.e. nothing is left to read
    // Head is behind tail, in which case it looped back around and there's stuff left
    // Head is in front of tail, in which case there's stuff left to read

    if (circbuf->tail == circbuf->head) return 0;
    else if (circbuf->head < circbuf->tail) return (circbuf->buffer_size - circbuf->tail) + circbuf->head;
    else return circbuf->head - circbuf->tail;
}

/**
 * @brief Returns how much content a circbuf has left to write
 * @param circbuf The circbuf to check
 */
ssize_t circbuf_remaining_write(circbuf_t *circbuf) {
    // Three conditions:
    // Head is one behind tail, nothing left to write
    // Head is equal to tail, we have the buffer's size - 1 left to write
    // Head is in front of tail, we have loop around + offset - 1

    if (circbuf->head == circbuf->tail - 1) return 0;
    else if (circbuf->head == circbuf->tail) return circbuf->buffer_size - 1;
    else return ((circbuf->buffer_size - circbuf->head) + circbuf->tail) - 1;
}

/**
 * @brief Wakes up all threads sleeping in the ringbuffer and makes them return an error code of -EINTR
 * @param circbuf The circbuf to stop 
 * @returns 0 on success
 */
int circbuf_stop(circbuf_t *circbuf) {
    spinlock_acquire(circbuf->lock);
    circbuf->stop = 1;
    sleep_wakeupQueue(circbuf->readers, 0);
    sleep_wakeupQueue(circbuf->writers, 0);
    spinlock_release(circbuf->lock);

    return 0;
}