/**
 * @file hexahedron/task/sched/sched_ule.c
 * @brief ULE-style scheduler for Hexahedron
 * 
 * You'll notice this breaks the naming convention a lot. This is fine because all the symbols are private.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/sched.h>
#include <kernel/task/process.h>
#include <kernel/task/sched/sched_ule.h>
#include <kernel/debug.h>
#include <kernel/processor_data.h>
#include <kernel/init.h>
#include <kernel/smp.h>

#define SCHED_THIS() ((sched_ule_cpu_t*)(current_cpu->sched_data))
#define SCHED_THR(t) ((sched_ule_thread_t*)((t)->sched))

/* runqueue and timeshare queue prototypes */
static void sched_ule_rq_insert(sched_ule_rq_t *rq, thread_t *thread);
static thread_t *sched_ule_rq_pop(sched_ule_rq_t *rq);
static void sched_ule_ts_insert(sched_ule_ts_t *ts, thread_t *thread);
static thread_t *sched_ule_ts_pop(sched_ule_ts_t *ts);
static thread_t *sched_ule_rq_steal(sched_ule_rq_t *rq, int cpu_id);
static thread_t *sched_ule_ts_steal(sched_ule_ts_t *ts, int cpu_id);

/* ULE CPU helpers */
static int sched_ule_cpu_load(int cpu_id, sched_ule_cpu_t *ule);
static int sched_ule_pick_cpu(thread_t *thread);
static void sched_ule_insert_cpu(int cpu_id, thread_t *thread);

/* other ULE stuff */
static void sched_ule_calendar(void *context);
static void sched_ule_reschedule(void *context);
static void sched_ule_balance(void *context);

/* ULE scheduler */
static void sched_ule_init();
static void sched_ule_ap();
static void sched_ule_start();
static struct thread *sched_ule_get();
static void sched_ule_insert(struct thread *thread);
static void sched_ule_yield(struct thread *thread);
static void sched_ule_thread(thread_t *thread);
static void sched_ule_free(thread_t *thread);
static void sched_ule_event(thread_t *thread, sched_event_t event);
sched_t ule_scheduler = {
    .name = "ule",
    .ops = {
        .sched_init = sched_ule_init,
        .sched_ap = sched_ule_ap,
        .sched_start = sched_ule_start,
        .sched_thread = sched_ule_thread,
        .sched_free = sched_ule_free,
        .sched_get = sched_ule_get,
        .sched_insert = sched_ule_insert,
        .sched_yield = sched_ule_yield,
        .sched_event = sched_ule_event,
    }
};

/* Reschedule IPI */
smp_ipi_t *reschedule_ipi;

/* RQ initializer */
#define ULE_RQ_INIT(rq) ({ memset((rq)->queues, 0, sizeof((rq)->queues)); bitmap_fill((rq)->status, 0, SCHED_NUM_QUEUES); })
#define ULE_TS_INIT(ts) ({ memset((ts)->queues, 0, sizeof((ts)->queues)); bitmap_fill((ts)->status, 0, SCHED_NUM_QUEUES); (ts)->idx = 0; (ts)->ridx = 0; })

/* Lock/unlock */
#define SCHED_LOCK(x) spinlock_acquire(&(x)->lock)
#define SCHED_UNLOCK(x) spinlock_release(&(x)->lock)

/* Quantum */
#define ULE_QUANTUM 10000000
#define ULE_BALANCE_QUANTUM 100000000
#define ULE_BALANCE_MAX_MOVES 8

/* Log method for ULE */
#define LOG(status, ...) dprintf_module(status, "SCHED:ULE", __VA_ARGS__)

/* Load balancer */
timer_event_t ule_load_balance;

/**
 * @brief Insert into ULE runqueue
 */
static void sched_ule_rq_insert(sched_ule_rq_t *rq, thread_t *thread) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);
    int index = 0;
    if (uthr->class == SCHED_CLASS_KERNEL || uthr->class == SCHED_CLASS_REALTIME) {
        // Kernel and realtime threads both share a queue with different ranges
        // Kernel threads, however, outrank realtime
        index = ((uthr->prio * SCHED_NUM_QUEUES) / (SCHED_PRIO_RT_MAX+1)); 
    } else if (uthr->class == SCHED_CLASS_IDLE) {
        index = (uthr->prio - SCHED_PRIO_IDLE_MIN) % SCHED_NUM_QUEUES;
    } else if (uthr->class == SCHED_CLASS_TIMESHARE) {
        index = (uthr->prio - SCHED_PRIO_TS_MIN) % SCHED_NUM_QUEUES;
    }

    // rq insertion is ugly
    struct sched_ule_rq_entry *queue = &rq->queues[index];
    if (queue->tail == NULL) {
        queue->head = thread;
        queue->tail = thread;
        uthr->next = uthr->prev = NULL;
    } else {
        sched_ule_thread_t *tail = SCHED_THR(queue->tail);
        tail->next = thread;
        uthr->prev = queue->tail;
        uthr->next = NULL;
        queue->tail = thread;
    }

    bitmap_set(rq->status, index);
}

/**
 * @brief Pop from runqueue
 */
static thread_t *sched_ule_rq_pop(sched_ule_rq_t *rq) {
    int target_queue = bitmap_find_first_set(rq->status, SCHED_NUM_QUEUES);
    if (target_queue == -1) {
        return NULL;
    }

    struct sched_ule_rq_entry *ent = &rq->queues[target_queue];
    thread_t *thr = ent->head;
    assert(thr != NULL);

    // Pop from list
    sched_ule_thread_t *uthr = SCHED_THR(thr);
    if (uthr->next) {
        thread_t *next_thr = uthr->next;
        sched_ule_thread_t *next = SCHED_THR(next_thr);
        next->prev = NULL;
        ent->head = uthr->next;
    } else {
        ent->head = NULL;
        ent->tail = NULL;
        bitmap_clear(rq->status, target_queue);
    }

    uthr->next = NULL;
    uthr->prev = NULL;
    return thr;
}

/**
 * @brief Timeshare queue insert
 */
static void sched_ule_ts_insert(sched_ule_ts_t *ts, thread_t *thread) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    // TODO apply 
    int prio_offset = SCHED_TS_OFFSET(uthr->prio);
    int cal_idx = SCHED_TS_NORMALIZE(ts->idx + prio_offset);
    if (ts->ridx != ts->idx && SCHED_TS_DISTANCE(ts->ridx, cal_idx) < SCHED_TS_DISTANCE(ts->ridx, ts->idx)) {
        // Don't place new work in a region that still must be drained
        cal_idx = SCHED_TS_NORMALIZE(ts->ridx - 1);
    }

    bitmap_set(ts->status, cal_idx);

    struct sched_ule_rq_entry *ent = &ts->queues[cal_idx];
    if (ent->tail != NULL) {
        thread_t *tail = ent->tail;
        sched_ule_thread_t *utail = SCHED_THR(tail);
        utail->next = thread;
        uthr->next = NULL;
        uthr->prev = tail;
        ent->tail = thread;
    } else {
        ent->head = ent->tail = thread;
        uthr->next = uthr->prev = NULL;
    }
}

/**
 * @brief Timeshare queue pop
 */
static thread_t *sched_ule_ts_pop(sched_ule_ts_t *ts) {
    int index = bitmap_find_first_set_from(ts->status, ts->ridx, SCHED_NUM_QUEUES);
    if (index == -1) {
        index = bitmap_find_first_set(ts->status, SCHED_NUM_QUEUES);
        if (index == -1) {
            return NULL;
        }
    }

    struct sched_ule_rq_entry *ent = &ts->queues[index];
    thread_t *thread = ent->head;
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    ent->head = uthr->next;

    if (ent->head != NULL) {
        SCHED_THR(ent->head)->prev = NULL;
    } else {
        // The queue is now empty
        ent->tail = NULL;
        bitmap_clear(ts->status, index);
    }

    uthr->next = NULL;
    uthr->prev = NULL;

    if (ent->head == NULL) {
        bool found = false;
        ts->ridx = SCHED_TS_NORMALIZE(index + 1);

        for (int j = 0; j < SCHED_NUM_QUEUES; j++) {
            if (ts->queues[ts->ridx].head != NULL) {
                found = true;
                break;
            }

            ts->ridx = SCHED_TS_NORMALIZE(ts->ridx + 1);
        }

        if (!found) {
            ts->ridx = ts->idx;
        }
    } else {
        ts->ridx = index;
    }

    return thread;
}

/**
 * @brief Steal a runqueue thread
 */
static thread_t *sched_ule_rq_steal(sched_ule_rq_t *rq, int cpu_id) {
    for (int i = 0; i < SCHED_NUM_QUEUES; i++) {
        struct sched_ule_rq_entry *ent = &rq->queues[i];
        thread_t *thread = ent->tail;

        while (thread) {
            sched_ule_thread_t *uthr = SCHED_THR(thread);

            if (procmask_test(&thread->affinity, cpu_id)) {
                if (uthr->prev) SCHED_THR(uthr->prev)->next = uthr->next;
                else ent->head = uthr->next;

                if (uthr->next) SCHED_THR(uthr->next)->prev = uthr->prev;
                else ent->tail = uthr->prev;

                if (ent->head == NULL) {
                    bitmap_clear(rq->status, i);
                }

                uthr->next = NULL;
                uthr->prev = NULL;
                return thread;
            }

            thread = uthr->prev;
        }
    }

    return NULL;
}

/**
 * @brief Steal a timeshare thread
 */
static thread_t *sched_ule_ts_steal(sched_ule_ts_t *ts, int cpu_id) {
    int i = -1;
    while (1) {
        i = bitmap_find_first_set_from(ts->status, i+1, SCHED_NUM_QUEUES);
        if (i == -1) break;

        struct sched_ule_rq_entry *ent = &ts->queues[i];
        thread_t *thread = ent->tail;

        while (thread) {
            sched_ule_thread_t *uthr = SCHED_THR(thread);

            if (procmask_test(&thread->affinity, cpu_id)) {
                if (uthr->prev) SCHED_THR(uthr->prev)->next = uthr->next;
                else ent->head = uthr->next;

                if (uthr->next) SCHED_THR(uthr->next)->prev = uthr->prev;
                else ent->tail = uthr->prev;

                uthr->next = NULL;
                uthr->prev = NULL;
                
                if (ent->head == NULL) {
                    bitmap_clear(ts->status, i);
                }

                return thread;
            }

            thread = uthr->prev;
        }
    }

    return NULL;
}

/**
 * @brief Initialize ULE scheduler
 */
static void sched_ule_init() {
    timer_init(&ule_load_balance, sched_ule_balance, NULL, ULE_BALANCE_QUANTUM, true, "ule load balance");
    reschedule_ipi = smp_createIPI("reschedule ipi", sched_ule_reschedule, NULL);

    sched_ule_ap();
}

/**
 * @brief Initialize ULE scheduler on AP
 */
static void sched_ule_ap() {
    sched_ule_cpu_t *ule = kmalloc(sizeof(sched_ule_cpu_t));
    SPINLOCK_INIT(&ule->lock);
    ule->threads = 0;
    ULE_RQ_INIT(&ule->rt_queue);
    ULE_RQ_INIT(&ule->idle_queue);
    ULE_RQ_INIT(&ule->ts_interact_queue);
    ULE_TS_INIT(&ule->ts_queue);
    timer_init(&ule->calendar, sched_ule_calendar, ule, 10000000, true, "ule calendar");
    timer_init(&ule->reschedule, sched_ule_reschedule, ule, ULE_QUANTUM, true, "ule reschedule");

    current_cpu->sched_data = ule;
}

/**
 * @brief Start ULE scheduler
 */
static void sched_ule_start() {
    sched_ule_cpu_t *ule = SCHED_THIS();

    // Insert the CPU idle thread into the idle queue
    SCHED_LOCK(ule);
    sched_ule_rq_insert(&ule->idle_queue, current_cpu->idle_process->main_thread);
    SCHED_UNLOCK(ule);

    timer_insert(&ule->calendar);
    timer_insert(&ule->reschedule);

    if (IS_BSP() && processor_count > 1) {
        timer_insert(&ule_load_balance);
    }
}

/**
 * @brief Init thread
 */
static void sched_ule_thread(thread_t *thread) {
    sched_ule_thread_t *uthr = kmalloc(sizeof(sched_ule_thread_t));
    uthr->next = NULL;
    uthr->prev = NULL;

    // timing: run time is 0, so this thread will start fully interactive
    uthr->sleep_time = ULE_QUANTUM;
    uthr->run_time = 0;
    uthr->run_start = timemonitor_getNanoseconds();
    uthr->sleep_start = 0;
    uthr->sleep_boost = 0;

    // Assign the thread a class and priority
    if (thread->flags & THREAD_FLAG_IDLE) {
        uthr->class = SCHED_CLASS_IDLE;
        uthr->prio = SCHED_PRIO_IDLE_MIN;
    } else if (thread->flags & THREAD_FLAG_KERNEL) {
        uthr->class = SCHED_CLASS_KERNEL;
        uthr->prio = SCHED_PRIO_KERN_MIN;
    } else {
        uthr->class = SCHED_CLASS_TIMESHARE;
        uthr->prio = SCHED_PRIO_TS_MIN;

        // threads that are being created get a small bonus
        uthr->sleep_boost = SCHED_WAKEUP_BONUS;
    }

    thread->sched = uthr;
}

/**
 * @brief Free thread
 */
static void sched_ule_free(thread_t *thread) {
    kfree(thread->sched);
}

/**
 * @brief Calendar callback
 */
static void sched_ule_calendar(void *context) {
    sched_ule_cpu_t *ule = context;
    SCHED_LOCK(ule);
    
    sched_ule_ts_t *ts = &ule->ts_queue;

    if (ts->idx == ts->ridx) {
        ts->idx = SCHED_TS_NORMALIZE(ts->idx + 1);

        while (ts->ridx != ts->idx) {
            if (ts->queues[ts->ridx].head != NULL) break;
            ts->ridx = SCHED_TS_NORMALIZE(ts->ridx + 1);
        }
    }

    SCHED_UNLOCK(ule);
}


/**
 * @brief Reschedule callback
 */
static void sched_ule_reschedule(void *context) {
    sched_ule_cpu_t *ule = SCHED_THIS();

    // this thread has reached the end of its quantum by now
    SCHED_LOCK(ule);
    if (current_cpu->current_thread && (current_cpu->current_thread->status & THREAD_STATUS_STOPPED) == 0) {
        current_cpu->current_thread->flags |= THREAD_FLAG_NEEDS_RESCHED;
    }
    SCHED_UNLOCK(ule);
}


/**
 * @brief Get CPU load
 */
static int sched_ule_cpu_load(int cpu_id, sched_ule_cpu_t *ule) {
    // TODO: threads != load
    thread_t *current_thread = NULL;
    int load = ule->threads;

    if (cpu_id == current_cpu->cpu_id) {
        current_thread = current_cpu->current_thread;
    } else {
        current_thread = processor_data[cpu_id].current_thread;
    }

    if (current_thread && (current_thread->flags & THREAD_FLAG_IDLE) == 0 && (current_thread->status & THREAD_STATUS_STOPPED) == 0) {
        load++;
    }

    return load;
}

/**
 * @brief Pick a CPU for a thread
 */
static int sched_ule_pick_cpu(thread_t *thread) {
    // TODO: get a better system for this    
    int best_cpu = current_cpu->cpu_id;
    int best_load = -1;
    int local_load = -1;

    for (int i = 0; i < processor_count; i++) {
        sched_ule_cpu_t *ule = processor_data[i].sched_data;
        int load;

        if (ule == NULL || procmask_test(&thread->affinity, i) == 0) {
            continue;
        }

        SCHED_LOCK(ule);
        load = sched_ule_cpu_load(i, ule);
        SCHED_UNLOCK(ule);

        if (i == current_cpu->cpu_id) {
            local_load = load;
        }

        if (best_load == -1 || load < best_load) {
            best_cpu = i;
            best_load = load;
        }
    }

    if (local_load != -1 && local_load <= best_load + 1) {
        return current_cpu->cpu_id;
    }

    assert(processor_data[best_cpu].sched_data != NULL);
    return best_cpu;
}

/**
 * @brief Load balancer
 */
static void sched_ule_balance(void *context) {
    sched_ule_cpu_t *most = NULL;
    sched_ule_cpu_t *least = NULL;
    int most_cpu = -1;
    int least_cpu = -1;
    int most_load = -1;
    int least_load = -1;

    for (int i = 0; i < processor_count; i++) {
        sched_ule_cpu_t *ule = processor_data[i].sched_data;
        int load;

        if (ule == NULL) {
            // rare case where processor is not initialized yet
            continue;
        }

        SCHED_LOCK(ule);
        load = sched_ule_cpu_load(i, ule);
        SCHED_UNLOCK(ule);

        if (most_load == -1 || load > most_load) {
            most = ule;
            most_cpu = i;
            most_load = load;
        }

        if (least_load == -1 || load < least_load) {
            least = ule;
            least_cpu = i;
            least_load = load;
        }
    }

    // if we got nothing, bail
    if (!most || !least || most == least || most_load <= least_load + 1) {
        return;
    }

    LOCK_BOTH(SCHED_LOCK, most, least);

    bool should_send_ipi = false;
    
    // recalculate for race
    most_load = sched_ule_cpu_load(most_cpu, most);
    least_load = sched_ule_cpu_load(least_cpu, least);

    // migrate as many threads as possible to different CPUs
    for (int i = 0; i < ULE_BALANCE_MAX_MOVES && most_load > least_load + SCHED_LOAD_THRESH; i++) {
        thread_t *thread = sched_ule_ts_steal(&most->ts_queue, least_cpu);
        if (thread) {
            sched_ule_ts_insert(&least->ts_queue, thread);
            goto _balance;
        }

        thread = sched_ule_rq_steal(&most->ts_interact_queue, least_cpu);
        if (thread) {
            sched_ule_rq_insert(&least->ts_interact_queue, thread);
            goto _balance;
        }

        thread = sched_ule_rq_steal(&most->rt_queue, least_cpu);
        if (thread) {
            sched_ule_rq_insert(&least->rt_queue, thread);
            goto _balance;
        }

    _balance:
        if (thread) {
            most->threads--;
            least->threads++;
        }

        thread_t *least_thread = processor_data[least_cpu].current_thread;
        if (least_cpu == current_cpu->cpu_id) {
            least_thread = current_cpu->current_thread;
        }

        if (thread && least_thread && SCHED_THR(thread)->prio < SCHED_THR(least_thread)->prio) {
            if (current_cpu->cpu_id != least_cpu) {
                should_send_ipi = true;
            } else {
                least_thread->flags |= THREAD_FLAG_NEEDS_RESCHED;
            }
        }

        if (thread == NULL) {
            break;
        }

        most_load--;
        least_load++;
    }

    UNLOCK_BOTH(SCHED_UNLOCK, most, least);

    if (should_send_ipi) {
        smp_sendCoreIPI(least_cpu, reschedule_ipi);
    }
}

/**
 * @brief Get a thread's interactivity score
 */
static int sched_ule_interactivity(thread_t *thread) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    uint64_t s = uthr->sleep_time;
    uint64_t r = uthr->run_time;

    if (!s) return SCHED_INTERACT_MAX;
    if (!r) return SCHED_INTERACT_MIN;

    // standard ULE scoring
    int score;
    if (s > r) {
        score = (SCHED_INTERACT_HALF * r) / s;
    } else {
        score = SCHED_INTERACT_MAX - ((SCHED_INTERACT_HALF * s) / r);
    }

    if (score > SCHED_INTERACT_MAX) {
        score = SCHED_INTERACT_MAX;
    }

    return score;
}

/**
 * @brief Update priority of a thread
 */
static void sched_ule_priority(thread_t *thread) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);
    int score = sched_ule_interactivity(thread);

    // We score all threads, including interactive and timeshare
    int prio;
    if (score < SCHED_INTERACT_THRESHOLD) {
        // Thread is interactable
        int range = SCHED_INTERACT_PRIO_MAX - SCHED_INTERACT_PRIO_MIN;
        prio = SCHED_INTERACT_PRIO_MIN + (score * range) / (SCHED_INTERACT_THRESHOLD - 1);
        if (prio > SCHED_INTERACT_PRIO_MAX) prio = SCHED_INTERACT_PRIO_MAX;
    } else {
        // Thread is non-interactable, likely a batch job.
        int range = SCHED_INTERACT_BATCH_MAX - SCHED_INTERACT_BATCH_MIN;
        prio = SCHED_INTERACT_BATCH_MIN + ((score - SCHED_INTERACT_THRESHOLD) * range) / (SCHED_INTERACT_MAX - SCHED_INTERACT_THRESHOLD);
        if (prio > SCHED_INTERACT_BATCH_MAX) prio = SCHED_INTERACT_BATCH_MAX;
    }

    // Decay boost
    if (uthr->sleep_boost) {
        prio -= uthr->sleep_boost;
        uthr->sleep_boost >>= 1;
    }

    // Clamp priority to the thread's class
    prio += SCHED_PRIO_TS_MIN;
    if (prio < SCHED_PRIO_TS_MIN) prio = SCHED_PRIO_TS_MIN;
    else if (prio > SCHED_PRIO_TS_MAX) prio = SCHED_PRIO_TS_MAX;

    uthr->prio = prio;
}

/**
 * @brief Decay times of a thread
 */
static void sched_ule_decay(thread_t *thread) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    uint64_t total = uthr->run_time + uthr->sleep_time;
    while (total > SCHED_SLP_RUN_MAX) {
        uthr->run_time = (uthr->run_time * 4) / 5;
        uthr->sleep_time = (uthr->sleep_time * 4) / 5;
        total = uthr->run_time + uthr->sleep_time;
    }
}

/**
 * @brief Get thread from ULE scheduler
 */
static thread_t *sched_ule_get() {
    sched_ule_cpu_t *ule = SCHED_THIS();

    SCHED_LOCK(ule);

    thread_t *thread = sched_ule_rq_pop(&ule->rt_queue);
    if (thread) {
        ule->threads--;
        goto _found;
    }

    thread = sched_ule_rq_pop(&ule->ts_interact_queue);
    if (thread) {
        ule->threads--;
        goto _found;
    }

    thread = sched_ule_ts_pop(&ule->ts_queue);
    if (thread) {
        ule->threads--;
        goto _found;
    }

    thread = sched_ule_rq_pop(&ule->idle_queue);
    if (thread) {
        goto _found;
    }

    // Otherwise, give them the idle thread
    thread = current_cpu->idle_process->main_thread;

_found:
    SCHED_UNLOCK(ule);
    return thread;
}

/**
 * @brief Insert thread on CPU
 */
static void sched_ule_insert_cpu(int cpu_id, thread_t *thread) {
    thread_t *current_thread = NULL;
    sched_ule_cpu_t *ule = processor_data[cpu_id].sched_data;
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    if (ule == NULL) {
        return;
    }

    SCHED_LOCK(ule);

    switch (uthr->class) {
        case SCHED_CLASS_REALTIME:
        case SCHED_CLASS_KERNEL:
            sched_ule_rq_insert(&ule->rt_queue, thread);
            ule->threads++;
            break;
        
        case SCHED_CLASS_TIMESHARE:
            sched_ule_priority(thread);

            if (SCHED_IS_INTERACT(uthr)) {
                sched_ule_rq_insert(&ule->ts_interact_queue, thread);
            } else {
                sched_ule_ts_insert(&ule->ts_queue, thread);
            }

            ule->threads++;
            break;

        case SCHED_CLASS_IDLE:
            sched_ule_rq_insert(&ule->idle_queue, thread);
            break;
    
        default:
            assert(0);
    }

    if (cpu_id == current_cpu->cpu_id) {
        current_thread = current_cpu->current_thread;
    } else {
        current_thread = processor_data[cpu_id].current_thread;
    }

    if (current_thread && SCHED_THR(thread)->prio < SCHED_THR(current_thread)->prio) {
        if (cpu_id == current_cpu->cpu_id) {
            current_cpu->current_thread->flags |= THREAD_FLAG_NEEDS_RESCHED;
        } else {
            smp_sendCoreIPI(cpu_id, reschedule_ipi);
        }
    }

    SCHED_UNLOCK(ule);
}

/**
 * @brief Insert thread on ULE scheduler
 */
static void sched_ule_insert(thread_t *thread) {
    int cpu_id = current_cpu->cpu_id;

    if ((thread->flags & THREAD_FLAG_IDLE) == 0) {
        cpu_id = sched_ule_pick_cpu(thread);
    }

    sched_ule_insert_cpu(cpu_id, thread);
}

/**
 * @brief Yield thread on ULE scheduler
 */
static void sched_ule_yield(thread_t *thread) {
    sched_ule_insert_cpu(current_cpu->cpu_id, thread);
}

/**
 * @brief ULE scheduler event
 */
static void sched_ule_event(thread_t *thread, sched_event_t event) {
    sched_ule_thread_t *uthr = SCHED_THR(thread);

    uint64_t now = timemonitor_getNanoseconds();

    if (event == SCHED_EVENT_SLEEP_ENTER) {
        uthr->sleep_start = now;

        // apply a small bonus to the threads, for when they wakeup
        if (uthr->class == SCHED_CLASS_TIMESHARE) {
            uthr->sleep_boost = SCHED_SLEEP_BONUS;
        }
    } else if (event == SCHED_EVENT_SLEEP_WAKEUP) {
        uthr->sleep_time += now - uthr->sleep_start;
    } else if (event == SCHED_EVENT_DESCHEDULE) {
        uthr->run_time += now - uthr->run_start;
    } else if (event == SCHED_EVENT_DISPATCH) {
        uthr->run_start = now;
    }

    sched_ule_decay(thread);
}
