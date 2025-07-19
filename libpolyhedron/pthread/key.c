/**
 * @file libpolyhedron/pthread/key.c
 * @brief pthread keys
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

// !!!: SERIOUS BUILD HACK FOR GH ACTIONS
#ifndef PTHREAD_KEYS_MAX
#define PTHREAD_KEYS_MAX            128
#endif

// Yet another pthread idea from banan-os :D

typedef struct __pthread_key_obj {
    void *value;
    void (*destructor)(void*);
} __pthread_key_obj_t;

/* pthread key list */
static __thread __pthread_key_obj_t __pthread_keys[PTHREAD_KEYS_MAX] = {};

/* pthread key bitmap */
static __thread uint8_t __pthread_key_bitmap[(PTHREAD_KEYS_MAX+7)/8];

#define PTHREAD_ALLOCATE_KEY(i) { __pthread_key_bitmap[i / 8] |= (1 << (i % 8)); }
#define PTHREAD_FREE_KEY(i) { __pthread_key_bitmap[i / 8] &= ~(1 << (i % 8)); }
#define PTHREAD_KEY_IN_USE(i) ((__pthread_key_bitmap[i / 8] & (1 << (i % 8))))


int pthread_key_create(pthread_key_t *key, void (*destructor)(void*)) {
    // Find a free key
    for (int i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (!PTHREAD_KEY_IN_USE(i)) {
            // A key isn't in use!
            __pthread_keys[i].value = NULL;
            __pthread_keys[i].destructor = destructor;
            PTHREAD_ALLOCATE_KEY(i);
            *key = (pthread_key_t)i;
            return 0;
        }
    }

    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX) return EINVAL;
    if (!PTHREAD_KEY_IN_USE(key)) return EINVAL;

    __pthread_keys[key].value = NULL;
    __pthread_keys[key].destructor = NULL;
    PTHREAD_FREE_KEY(key);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX) return NULL;
    if (!PTHREAD_KEY_IN_USE(key)) return NULL;

    return __pthread_keys[key].value;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= PTHREAD_KEYS_MAX) return EINVAL;
    if (!PTHREAD_KEY_IN_USE(key)) return EINVAL;

    __pthread_keys[key].value = (void*)value;
    return 0;
}