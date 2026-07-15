/**
 * @file userspace/celestialv2/damage.c
 * @brief Damage component
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

#define DAMAGE_COLLAPSE_THRESHOLD 64

static inline bool damage_intersectsWindow(gfx_rect_t damage, wm_window_t *win) {
    return !((int)(damage.x + damage.width) <= win->x ||
             (int)(damage.y + damage.height) <= win->y ||
             (int)damage.x >= win->x + win->width ||
             (int)damage.y >= win->y + win->height);
}

static inline bool damage_overlapOrAdjacent(gfx_rect_t a, gfx_rect_t b) {
    return !(a.x + (int)a.width < b.x ||
             a.y + (int)a.height < b.y ||
             b.x + (int)b.width < a.x ||
             b.y + (int)b.height < a.y);
}

static inline void damage_mergeRects(gfx_rect_t *a, gfx_rect_t b) {
    int a_x2 = a->x + (int)a->width;
    int a_y2 = a->y + (int)a->height;
    int b_x2 = b.x + (int)b.width;
    int b_y2 = b.y + (int)b.height;

    int new_x = (a->x < b.x) ? a->x : b.x;
    int new_y = (a->y < b.y) ? a->y : b.y;
    int new_x2 = (a_x2 > b_x2) ? a_x2 : b_x2;
    int new_y2 = (a_y2 > b_y2) ? a_y2 : b_y2;

    a->x = new_x;
    a->y = new_y;
    a->width = new_x2 - new_x;
    a->height = new_y2 - new_y;
}

static bool damage_clipToWindow(gfx_rect_t damage, wm_window_t *win, gfx_rect_t *out_local_rect) {
    int d_x1 = damage.x;
    int d_y1 = damage.y;
    int d_x2 = damage.x + (int)damage.width;
    int d_y2 = damage.y + (int)damage.height;

    int w_x1 = win->x;
    int w_y1 = win->y;
    int w_x2 = win->x + win->width;
    int w_y2 = win->y + win->height;

    int i_x1 = (d_x1 > w_x1) ? d_x1 : w_x1;
    int i_y1 = (d_y1 > w_y1) ? d_y1 : w_y1;
    int i_x2 = (d_x2 < w_x2) ? d_x2 : w_x2;
    int i_y2 = (d_y2 < w_y2) ? d_y2 : w_y2;

    if (i_x2 <= i_x1 || i_y2 <= i_y1) return false;

    out_local_rect->x = i_x1 - win->x;
    out_local_rect->y = i_y1 - win->y;
    out_local_rect->width = i_x2 - i_x1;
    out_local_rect->height = i_y2 - i_y1;
    return true;
}

static void damage_appendRequest(render_request_t **head, render_request_t **tail, wm_window_t *win, gfx_rect_t rect) {
    render_request_t *rq = malloc(sizeof(render_request_t));
    if (!rq) {
        FATAL("malloc(render_request_t) failed: %s\n", strerror(errno));
    }

    rq->next = NULL;
    rq->win = win;
    rq->rect = rect;
    rq->x = 0;
    rq->y = 0;
    rq->state = WINDOW_STATE_CLOSED;
    rq->anim_time = 0;

    if (win) {
        rq->x = win->x;
        rq->y = win->y;
        rq->state = win->state;
        rq->anim_time = win->anim.anim_time;
    }

    if (!*head) {
        *head = rq;
        *tail = rq;
        return;
    }

    (*tail)->next = rq;
    *tail = rq;
}

render_request_t *damage_build() {
    pthread_mutex_lock(&SERVER->dmg_lock);
    
    if (SERVER->dmg_head == NULL) {
        pthread_mutex_unlock(&SERVER->dmg_lock);
        usleep(10000);
        return NULL;
    }

    damage_node_t *dmg = SERVER->dmg_head;
    SERVER->dmg_head = NULL;
    pthread_mutex_unlock(&SERVER->dmg_lock);

    gfx_rect_t rects[DAMAGE_COLLAPSE_THRESHOLD];
    size_t rect_count = 0;
    int bbox_x1 = 0;
    int bbox_y1 = 0;
    int bbox_x2 = 0;
    int bbox_y2 = 0;
    bool bbox_init = false;

    for (damage_node_t *node = dmg; node; node = node->next) {
        gfx_rect_t rect = node->rect;
        if (rect.width <= 0 || rect.height <= 0) continue;

        int x1 = (int)rect.x;
        int y1 = (int)rect.y;
        int x2 = (int)rect.x + (int)rect.width;
        int y2 = (int)rect.y + (int)rect.height;

        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > (int)renderer_getWidth()) x2 = (int)renderer_getWidth();
        if (y2 > (int)renderer_getHeight()) y2 = (int)renderer_getHeight();
        if (x2 <= x1 || y2 <= y1) continue;

        rect = GFX_RECT(x1, y1, x2 - x1, y2 - y1);

        if (rect_count < DAMAGE_COLLAPSE_THRESHOLD) {
            rects[rect_count++] = rect;
        }

        if (!bbox_init) {
            bbox_x1 = x1;
            bbox_y1 = y1;
            bbox_x2 = x2;
            bbox_y2 = y2;
            bbox_init = true;
        } else {
            if (x1 < bbox_x1) bbox_x1 = x1;
            if (y1 < bbox_y1) bbox_y1 = y1;
            if (x2 > bbox_x2) bbox_x2 = x2;
            if (y2 > bbox_y2) bbox_y2 = y2;
        }
    }

    while (dmg) {
        damage_node_t *next = dmg->next;
        free(dmg);
        dmg = next;
    }

    if (!bbox_init) return NULL;

    if (rect_count >= DAMAGE_COLLAPSE_THRESHOLD) {
        rect_count = 1;
        rects[0] = GFX_RECT(bbox_x1, bbox_y1, bbox_x2 - bbox_x1, bbox_y2 - bbox_y1);
    } else {
        bool changed;
        do {
            changed = false;

            for (size_t i = 0; i < rect_count; i++) {
                for (size_t j = i + 1; j < rect_count; ) {
                    if (damage_overlapOrAdjacent(rects[i], rects[j])) {
                        damage_mergeRects(&rects[i], rects[j]);
                        rects[j] = rects[rect_count - 1];
                        rect_count--;
                        changed = true;
                        continue;
                    }

                    j++;
                }
            }
        } while (changed);
    }

    render_request_t *head = NULL;
    render_request_t *tail = NULL;

    pthread_mutex_lock(&SERVER->window_lock);

    for (size_t r = 0; r < rect_count; r++) {
        gfx_rect_t damage = rects[r];

        damage_appendRequest(&head, &tail, NULL, damage);

        for (int z = 0; z < Z_COUNT; z++) {

            // Walk to bottommost window in this z layer.
            wm_window_t *tail_win = SERVER->window_list[z];
            while (tail_win && tail_win->next) {
                tail_win = tail_win->next;
            }

            // Traverse bottom -> top so alpha and overlaps compose correctly.
            for (wm_window_t *win = tail_win; win; win = win->prev) {

                if (win->state == WINDOW_STATE_CLOSED || !win->visible) {
                    continue;
                }

                if (!damage_intersectsWindow(damage, win)) {
                    continue;
                }

                gfx_rect_t local_rect;

                if (!damage_clipToWindow(damage, win, &local_rect)) {
                    continue;
                }

                window_hold(win);
                damage_appendRequest(&head, &tail, win, local_rect);
            }
        }
    }

    pthread_mutex_unlock(&SERVER->window_lock);

    return head;
}

void damage_lock() {
    pthread_mutex_lock(&SERVER->dmg_lock);
}

void damage_unlock() {
    pthread_mutex_unlock(&SERVER->dmg_lock);
}

// Add a damage rectangle
void damage_add(gfx_rect_t rect) {
    damage_node_t *n = malloc(sizeof(damage_node_t));
    if (!n) {
        FATAL("malloc(damage_node_t) failed: %s\n", strerror(errno));
    }

    n->rect = rect;
    n->next = SERVER->dmg_head;
    SERVER->dmg_head = n;
}

// Fully damage an entire window
inline void damage_window(wm_window_t *window) {
    damage_add(GFX_RECT(window->x, window->y, window->width, window->height));
}

// Damage a window when it moves
void damage_move(wm_window_t *window, int old_x, int old_y) {
    damage_add(GFX_RECT(old_x, old_y, window->width, window->height));
    damage_window(window);
}
