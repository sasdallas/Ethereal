/**
 * @file hexahedron/include/kernel/event.h
 * @brief 
 * 
 * 
 * @copyright
 *  
 * MIT License
 * Copyright (c) 2024 Mathewnd
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef KERNEL_EVENT_H
#define KERNEL_EVENT_H

#include <kernel/fs/poll.h>

typedef poll_event_t event_t;

typedef poll_waiter_t *event_listener_t;

#define EVENT_MAX 12

#define EVENT_INIT_LISTENER(l) { (l) = poll_createWaiter(current_cpu->current_thread, 12); }
#define EVENT_INIT(e) POLL_EVENT_INIT(e)
#define EVENT_SIGNAL(e) poll_signal((e), POLLPRI)
#define EVENT_WAIT(e, t) poll_wait((e), (t))
#define EVENT_ATTACH(l, e) { poll_add((l), (e), POLLPRI); }
#define EVENT_DETACH(l) { poll_exit((l)); }
#define EVENT_DESTROY_LISTENER(l) poll_destroyWaiter(l);

#endif