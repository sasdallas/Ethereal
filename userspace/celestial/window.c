/**
 * @file userspace/celestialv2/window.c
 * @brief Window components
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <stdatomic.h>
#include <ethereal/shared.h>
#include <ethereal/celestial.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

atomic_int last_shm_key = 0;
wm_window_anim_t *anim_list = NULL;
pthread_mutex_t anim_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t anim_cond = PTHREAD_COND_INITIALIZER;

wm_window_t *window_create(wm_client_t *client, int flags, size_t width, size_t height) {
    wm_window_t *window = malloc(sizeof(wm_window_t));
    if (!window) {
        FATAL("malloc(wm_window_t) failed: %s\n", strerror(errno));
    }
    memset(window, 0, sizeof(wm_window_t));
    window->id = atomic_fetch_add(&SERVER->next_window_id, 1);
    window->client = client;
    window->width = width;
    window->height = height;
    window->x = renderer_getWidth() / 2 - (width / 2);
    window->y = renderer_getHeight() / 2 - (height / 2);
    window->flags = flags;
    window->z_array = Z_DEFAULT;
    window->anim.win = window;
    window->visible = true;
    window->resize.bounds.min_width = 0;
    window->resize.bounds.min_height = 0;
    window->resize.bounds.max_width = SIZE_MAX;
    window->resize.bounds.max_height = SIZE_MAX;
    pthread_spin_init(&window->resize.resize_lck, PTHREAD_PROCESS_PRIVATE);
    __atomic_add_fetch(&client->window_count, 1, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&SERVER->window_count, 1, __ATOMIC_SEQ_CST);

    // Start the window with one reference
    __atomic_store_n(&window->refcount, 1, __ATOMIC_SEQ_CST);

#ifdef BUILDING_LINUX
    char buf[512];
    int key = atomic_fetch_add(&last_shm_key, 1);
    snprintf(buf, 512, "/window_%d", key);

    window->shmfd = shm_open(buf, O_RDWR | O_CREAT, 0744);
    if (window->shmfd < 0) {
        FATAL("shm_open: %s\n", strerror(errno));
    }

    window->bufkey = key;

    ftruncate(window->shmfd, window->height * window->width * 4);

    window->buffer = mmap(NULL, window->height * window->width *  4, PROT_READ | PROT_WRITE, MAP_SHARED, window->shmfd, 0);
    if (window->buffer == MAP_FAILED) {
        FATAL("mmap: %s\n", strerror(errno));
    }

#else
    window->shmfd = shared_new(window->height * window->width * 4, SHARED_DEFAULT);
    if (window->shmfd < 0) {
        FATAL("shared_new failed: %s\n", strerror(errno));
    }

    window->bufkey = shared_key(window->shmfd);
    if (window->bufkey < 0) {
        FATAL("shared_key failed: %s\n", strerror(errno));
    }

    window->buffer = mmap(NULL, window->height * window->width * 4, PROT_READ | PROT_WRITE, MAP_SHARED, window->shmfd, 0);
    if (window->buffer == MAP_FAILED) {
        FATAL("mmap failed: %s\n", strerror(errno));
    }
#endif

    // Now that the buffer was created we can add the window
    window->next_client = client->window_head;
    client->window_head = window;

    pthread_mutex_lock(&SERVER->window_lock);
    window->next = SERVER->window_list[Z_DEFAULT];
    window->prev = NULL;
    
    if (SERVER->window_list[Z_DEFAULT]) {
        SERVER->window_list[Z_DEFAULT]->prev = window; 
    }
    
    SERVER->window_list[Z_DEFAULT] = window;
    pthread_mutex_unlock(&SERVER->window_lock);

    // Any new windows automatically become the focused
    // (libcelestial assumes they are by default, so dont bother sending a focused event)
    wm_window_t *old_focused = SERVER->focused;
    SERVER->focused = window;
    if (old_focused) {
        EVENT_SEND(old_focused, celestial_event_unfocused_t, CELESTIAL_EVENT_UNFOCUSED);
    }
    
    // Update window flags
    if (flags & CELESTIAL_WINDOW_FLAG_NO_ANIMATIONS) {
        WINDOW_CHANGE_STATE(window, WINDOW_STATE_NORMAL);
    } else {
        WINDOW_CHANGE_STATE(window, WINDOW_STATE_OPENING);
        window_beginAnimation(window);
    }

    return window;
}


wm_window_t *window_top_exclude(wm_window_t *excl) {
    // Z_DEFAULT
    pthread_mutex_lock(&SERVER->window_lock);
    wm_window_t *win = SERVER->window_list[Z_DEFAULT];
    while (win) {
        if (win != excl) {
            pthread_mutex_unlock(&SERVER->window_lock);
            return win;
        }

        win = win->next;
    }
    pthread_mutex_unlock(&SERVER->window_lock);
    return NULL;
}


void window_close(wm_window_t *win) {
    TRACE_DEBUG("Closing window %p\n", win);
    EVENT_SEND(win, celestial_event_window_close_t, CELESTIAL_EVENT_WINDOW_CLOSE);
    

    // Remove from client list now
    wm_window_t *last = NULL;
    wm_window_t *w = win->client->window_head;
    while (w) {
        if (w == win) {
            if (last) last->next_client = win->next_client;
            else win->client->window_head = win->next_client;
            break;
        }

        last = w;
        w = w->next_client;
    }


    if (win == SERVER->mouse_window) {
        SERVER->mouse_window = NULL; // TODO: RECALCULATE THIS VALUE
    }

    WINDOW_CHANGE_STATE(win, WINDOW_STATE_CLOSING);
    __atomic_sub_fetch(&SERVER->window_count, 1, __ATOMIC_SEQ_CST);
    
    if (win->flags & CELESTIAL_WINDOW_FLAG_NO_ANIMATIONS) {
        WINDOW_CHANGE_STATE(win, WINDOW_STATE_CLOSED);
        damage_window_locked(win);
        window_release(win);
    } else {
        window_beginAnimation(win);
    }
}

void window_destroy(wm_window_t *win) {
    TRACE_DEBUG("window_destroy %p\n", win);

    if (win == SERVER->focused) {
        window_focus(window_top_exclude(win));
    }

    pthread_mutex_lock(&SERVER->window_lock);
    
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == SERVER->window_list[win->z_array]) SERVER->window_list[win->z_array] = win->next;

    pthread_mutex_unlock(&SERVER->window_lock);

    assert(win->state == WINDOW_STATE_CLOSED);

    damage_window_locked(win);

    munmap(win->buffer, win->width * win->height * 4);
    close(win->shmfd);
    wm_client_t *client = win->client;
    __atomic_sub_fetch(&client->window_count, 1, __ATOMIC_SEQ_CST);

    if (win->announce.name) free(win->announce.name);
    if (win->announce.icon) free(win->announce.icon);
    
    free(win);
    ipc_releaseClient(client);
}


wm_window_t *window_get(wm_client_t *client, wid_t id) {
    wm_window_t *win = client->window_head;
    while (win) {
        if (win->id == id) {
            return win;
        }

        win = win->next_client;
    }

    TRACE_WARN("Window %d not found on client %p/%d\n", id, client, client->client_fd);
    return NULL;
}

wm_window_t *window_top(int x, int y) {
    pthread_mutex_lock(&SERVER->window_lock);
    wm_window_t *candidate = NULL;
    for (z_array_t i = Z_BACKGROUND; i < Z_COUNT; i++) {
        wm_window_t *win = SERVER->window_list[i];
        while (win) {
            if (x < win->x + win->width && x >= win->x && y < win->y + win->height && y >= win->y && win->state != WINDOW_STATE_CLOSING && win->state != WINDOW_STATE_CLOSED) {
                if (!candidate || candidate->z_array < i) {
                    candidate = win;   
                    break;
                }
            }
            win = win->next;
        }
    }

    pthread_mutex_unlock(&SERVER->window_lock);
    return candidate;
}


wm_window_t *window_top_nobg(int x, int y) {
    pthread_mutex_lock(&SERVER->window_lock);
    wm_window_t *candidate = NULL;
    for (z_array_t i = Z_DEFAULT; i < Z_COUNT; i++) {
        wm_window_t *win = SERVER->window_list[i];
        while (win) {
            if (x < win->x + win->width && x >= win->x && y < win->y + win->height && y >= win->y) {
                if (!candidate || candidate->z_array < i) {
                    candidate = win;   
                    break;
                }
            }
            win = win->next;
        }
    }

    pthread_mutex_unlock(&SERVER->window_lock);
    return candidate;
}

void window_focus(wm_window_t *win) {
    if (win->z_array == Z_BACKGROUND) {
        return; // can't focus a background window
    }

    if (win == SERVER->focused) return;
    TRACE_DEBUG("window_focus %p/%d\n", win, win->id);

    pthread_mutex_lock(&SERVER->window_lock);
    if (SERVER->focused) {
        // Unfocus the previous window
        EVENT_SEND(SERVER->focused, celestial_event_unfocused_t, CELESTIAL_EVENT_UNFOCUSED);
        SERVER->focused = NULL;
    }

    // Move us to the front of the Z list
    if (win->prev) {
        z_array_t z = win->z_array;
        win->prev->next = win->next;
        if (win->next) win->next->prev = win->prev;
        if (SERVER->window_list[z]) SERVER->window_list[z]->prev = win;
        win->prev = NULL;
        win->next = SERVER->window_list[z];
        SERVER->window_list[z] = win;
    }

    damage_window_locked(win);

    SERVER->focused = win;
    EVENT_SEND(SERVER->focused, celestial_event_focused_t, CELESTIAL_EVENT_FOCUSED);
    pthread_mutex_unlock(&SERVER->window_lock);
}

void window_moveZ(wm_window_t *win, z_array_t new_z) {
    if (win->z_array == new_z) return;

    pthread_mutex_lock(&SERVER->window_lock);
    gfx_rect_t old_rect = GFX_RECT(win->x, win->y, win->width, win->height);

    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == SERVER->window_list[win->z_array]) SERVER->window_list[win->z_array] = win->next;

    // TODO: maybe don't put them at the front of the Z list since that would be easy to exploit
    wm_window_t *prev_head = SERVER->window_list[new_z];
    if (prev_head) prev_head->prev = win;
    win->prev = NULL;
    win->next = prev_head;
    SERVER->window_list[new_z] = win;

    win->z_array = new_z;
    pthread_mutex_unlock(&SERVER->window_lock);

    damage_add_locked(old_rect);
}

void window_processAnimations() {
    uint64_t now = celestial_now();

    wm_window_anim_t *anim = anim_list;
    wm_window_anim_t *prev = NULL;

    while (anim) {
        if (anim->running == false) goto _next_anim;
        
        // Calculate the delta
        uint64_t dt = now - anim->anim_last_paint;
        if (dt < 3) goto _next_anim;

        anim->anim_last_paint = now;
        anim->anim_time += dt;

        if ((double)(anim->anim_time) / (double)125000.0 > 1.0f) {
            TRACE_DEBUG("window_processAnimations: animation finished\n");
            
            wm_window_anim_t *nxt = anim->next;

            // remove from list
            if (prev) {
                prev->next = anim->next;
            } else {
                wm_window_anim_t *old_head;
                do {
                    old_head = __atomic_load_n(&anim_list, __ATOMIC_SEQ_CST);
                } while (!__atomic_compare_exchange_n(&anim_list, &old_head, anim->next, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
            }

            // The animation is finished
            if (anim->win->state == WINDOW_STATE_OPENING) {
                WINDOW_CHANGE_STATE(anim->win, WINDOW_STATE_NORMAL);
                damage_window_locked(anim->win);
            } else if (anim->win->state == WINDOW_STATE_CLOSING) {
                WINDOW_CHANGE_STATE(anim->win, WINDOW_STATE_CLOSED);
                window_release(anim->win);
            }
            
            anim->running = false;

            anim = nxt;
            continue;
        }

        
        damage_window_locked(anim->win);

    _next_anim:
        prev = anim;
        anim = anim->next;
    }
}

void window_beginAnimation(wm_window_t *win) {
    pthread_mutex_lock(&anim_lock);

    win->anim.win = win;
    win->anim.anim_last_paint = celestial_now();
    win->anim.anim_time = 0;
    win->anim.next = NULL;

    if (win->anim.running) {
        // the animation for this window is already running
        // we will just take it over since the window state has already changed
    } else {
        win->anim.next = anim_list;
        anim_list = &win->anim;
        win->anim.running = true;
    }

    pthread_cond_signal(&anim_cond);
    pthread_mutex_unlock(&anim_lock);
}


void window_resize(wm_window_t *win, int nx, int ny, int w, int h) {
    if (__atomic_load_n(&win->resize.pending, __ATOMIC_SEQ_CST)) {
        TRACE_DEBUG("pending resize already\n");
        return;
    }

    // aight, do the thing
    window_hold(win);

    int ox = win->x; int oy = win->y;

    // Clamp the new size to GFX_WIDTH
    if (w + nx >= (int)GFX_WIDTH(SERVER->renderer.ctx)) {
        w = GFX_WIDTH(SERVER->renderer.ctx) - nx;
    }
    
    if (h + ny >= (int)GFX_HEIGHT(SERVER->renderer.ctx)) {
        h = GFX_HEIGHT(SERVER->renderer.ctx) - ny;
    }

    // Clamp it to resize bounds
    if ((unsigned)w < win->resize.bounds.min_width) {
        w = win->resize.bounds.min_width;
    }
    if ((unsigned)h < win->resize.bounds.min_height) {
        h = win->resize.bounds.min_height;
    }
    if ((unsigned)w > win->resize.bounds.max_width) {
        w = win->resize.bounds.max_width;
    }
    if ((unsigned)h > win->resize.bounds.max_height) {
        h = win->resize.bounds.max_height;
    }

    // Configure the new window data
    if (win->width == w && win->height == h) {
        // We don't actually care that much...
        if (win->x != nx || win->y != ny) {
            win->x = nx;
            win->y = ny;
            damage_move_locked(win,ox,oy);
        }

        window_release(win);
        return;
    }


    // Create a new shared buffer
#ifdef BUILDING_LINUX
    #error "Not implemented"
#else
    int new_shm_object = shared_new(w * h * 4, SHARED_DEFAULT);
    win->resize.new_bufkey = shared_key(new_shm_object);
    win->resize.new_shmfd = new_shm_object;
    win->resize.new_buffer = mmap(NULL, w*h*4, PROT_READ | PROT_WRITE, MAP_SHARED, new_shm_object, 0);
#endif

    win->resize.new_rect.width = w;
    win->resize.new_rect.height = h;
    win->resize.new_rect.x = nx;
    win->resize.new_rect.y = ny;

    // Resize is completed
    __atomic_store_n(&win->resize.pending, true, __ATOMIC_SEQ_CST);
    EVENT_SEND(win, celestial_event_resize_t, CELESTIAL_EVENT_RESIZE,
        .new_width = w,
        .new_height = h,
        .buffer_key = win->resize.new_bufkey
    );

    window_release(win);
}

void window_resize_finish(wm_window_t *win) {
    window_hold(win);
    pthread_spin_lock(&win->resize.resize_lck);
    if (__atomic_load_n(&win->resize.pending, __ATOMIC_SEQ_CST) == false) {
        // I guess raced?
        pthread_spin_unlock(&win->resize.resize_lck);
        return;
    }

    // Close the old stuff
    close(win->shmfd);
    munmap(win->buffer, win->width*win->height*4);

    // Swap it out
    // TODO Lock this
    win->buffer = win->resize.new_buffer;
    win->bufkey = win->resize.new_bufkey;
    win->shmfd = win->resize.new_shmfd;
    
    damage_lock();
    damage_window(win);
    int ox = win->x; int oy = win->y;
    win->x = win->resize.new_rect.x;
    win->y = win->resize.new_rect.y;
    win->width = win->resize.new_rect.width;
    win->height = win->resize.new_rect.height;
    damage_window(win);
    damage_unlock();

    if (ox != win->x || oy != win->y) {
        // Send position change
        EVENT_SEND(win, celestial_event_position_change_t, CELESTIAL_EVENT_POSITION_CHANGE,
            .x = win->x,
            .y = win->y
        );
    }

    __atomic_store_n(&win->resize.pending, false, __ATOMIC_SEQ_CST);
    win->resize.oneoff = false;
    pthread_spin_unlock(&win->resize.resize_lck);
    window_release(win);
}
