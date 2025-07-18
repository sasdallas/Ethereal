/**
 * @file libpolyhedron/pthread/attr.c
 * @brief pthread attr
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

int pthread_attr_init(pthread_attr_t *attr) {
    attr->stack_size = PTHREAD_STACK_SIZE;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *__restrict attr, struct sched_param *__restrict param) {
    param->sched_priority = attr->sched;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *__restrict attr, const struct sched_param *__restrict param) {
    attr->sched = param->sched_priority;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *__restrict attr, size_t *__restrict stacksize) {
    *stacksize = attr->stack_size;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *__restrict attr, int *__restrict detach_state) {
    *detach_state = attr->detach_state;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *__restrict attr, int detach_state) {
    attr->detach_state = detach_state;
    return 0;
}