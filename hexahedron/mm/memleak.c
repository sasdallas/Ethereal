/**
 * @file hexahedron/mm/memleak.c
 * @brief Kernel memory leak detector
 * 
 * Modeled after the Linux memory leak detector, the kernel memory leak detector scans
 * regions of memory for valid pointers, adding those to a list.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/memleak.h>
#include <kernel/mm/slab.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>
#include <kernel/misc/util.h>
#include <kernel/init.h>
#include <structs/rbtree.h>
#include <structs/hashmap.h>
#include <kernel/misc/ksym.h>
#include <string.h>

#include <kernel/config.h>


#define LOG(status, ...) dprintf_module(status, "MM:MEMLEAK", __VA_ARGS__)

#ifdef KERNEL_ENABLE_MEMORY_LEAK_SCANNER

/* Cache */
slab_cache_t *memleak_cache = NULL; 

/* Object tree and list */
rb_tree_t *obj_tree = NULL;
list_t *obj_list = NULL;

/* Lock */
spinlock_t memleak_lock = { 0 };

/* Memory leak process */
process_t *memleak_proc;

/**
 * @brief Memory leak checker init
 */
void memleak_init() {
    obj_tree  = rbtree_create();
    obj_list = list_create("memory leak object list");
    memleak_cache = slab_createCache("memory leak object cache", SLAB_CACHE_NOLEAKTRACE, sizeof(memleak_object_t), 0, NULL, NULL);
}

/**
 * @brief Get object from tree
 */
static memleak_object_t *memleak_get(void *ptr) {
    uintptr_t val = (uintptr_t)ptr;

    rb_tree_node_t *n = obj_tree->root;
    while (n) {

        memleak_object_t *obj = (memleak_object_t *)n->value;
        if (ptr >= obj->ptr && (void*)((uintptr_t)obj->ptr + obj->size) > ptr) {
            return obj;
        }

        if ((uintptr_t)n->key > val) {
            n = n->left;
        } else if ((uintptr_t)n->key < val) {
            n = n->right;
        } else if ((uintptr_t)n->key == val) {
            return (memleak_object_t*)n->value;
        }
    }

    foreach(ln, obj_list) {
        memleak_object_t *obj = (memleak_object_t*)ln->value;
        if (obj->ptr == ptr) {
            return obj;
        }
    }

    // LOG(WARN, "Object %p missing\n", ptr);
    return NULL;
}

/**
 * @brief Add object from allocation
 * @param ptr The allocation pointer
 * @param size The allocation size
 */
void memleak_alloc(void *ptr, size_t size) {
    if (!memleak_cache) return;

    memleak_object_t *obj = slab_allocate(memleak_cache);
    memset(obj, 0, sizeof(memleak_object_t));
    SPINLOCK_INIT(&obj->lck);
    obj->paint = MEMLEAK_GREY; // will be reset anyways
    obj->ptr = ptr;
    obj->size  = size;
    RB_TREE_INIT_NODE(&obj->node, ptr, obj);

    // Fill object backtrace frames
    struct frame { struct frame *next; uintptr_t ip; };
    struct frame *stk = __builtin_frame_address(0);
    uintptr_t ip = (uintptr_t)&memleak_alloc;
    for (int frame = 0; stk && frame < 10; frame++) {
        obj->frames[frame] = (void*)ip;
        ip = stk->ip;
        stk = stk->next;
        
        if (!(arch_mmu_read_flags(NULL, (uintptr_t)stk) & MMU_FLAG_PRESENT)) {
            break;
        }
    }


    spinlock_acquire(&memleak_lock);
    rbtree_insert(obj_tree, &obj->node);
    obj->lnode.value = obj;
    list_append_node(obj_list, &obj->lnode);
    spinlock_release(&memleak_lock);
}

/**
 * @brief Remove object from allocation
 * @param ptr Pointer to the object
 */
void memleak_free(void *ptr) {
    if (!memleak_cache) return;

    spinlock_acquire(&memleak_lock);

    memleak_object_t *obj = memleak_get(ptr);
    if (!obj) { LOG(WARN, "Object %p missing\n", ptr); spinlock_release(&memleak_lock);  return; }

    rbtree_delete(obj_tree, &obj->node);
    list_delete(obj_list, &obj->lnode);
    assert(list_find(obj_list, obj) == NULL);
    slab_free(memleak_cache, obj);
    spinlock_release(&memleak_lock);
}

/**
 * @brief Scan block of memory
 */
int memleak_scanMemory(void *ptr, size_t size) {
    unsigned long *p = (unsigned long*)ALIGN_UP((uintptr_t)ptr, sizeof(void*));
    unsigned long *end = (unsigned long*)((uint8_t*)p + size);

    int found = 0;

    for (; p < end; p++) {
        void *candidate = (void*)(*(uintptr_t*)p);
        memleak_object_t *obj = memleak_get(candidate);
        if (obj && obj->paint == MEMLEAK_WHITE) {
            obj->paint = MEMLEAK_GREY;
            found = 1;
        }
    }

    return found;
}

/**
 * @brief Scan object
 */
void memleak_scanObject(memleak_object_t *obj) {
    int found = memleak_scanMemory(obj->ptr, obj->size);
    if (found) obj->paint = MEMLEAK_BLACK;
}

/**
 * @brief Scan function
 */
void memleak_scan() {
    if (!memleak_cache) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    LOG(DEBUG, "Beginning memory leak scan.\n");

    
    spinlock_acquire(&memleak_lock);
    foreach(ln, obj_list) {
        memleak_object_t *obj = (memleak_object_t*)ln->value;
        obj->paint = MEMLEAK_WHITE;
    }

    foreach(ln, obj_list) {
        memleak_object_t *obj = (memleak_object_t*)ln->value;
        memleak_scanObject(obj);
    }

extern uintptr_t __bss_start, __bss_end;
extern uintptr_t __data_start, __data_end;
extern uintptr_t __lbss_start, __lbss_end;

extern uintptr_t __kernel_start, __kernel_end;

    memleak_scanMemory((void*)&__bss_start, (size_t)((uintptr_t)&__bss_end - (uintptr_t)&__bss_start));
    memleak_scanMemory((void*)&__data_start, (size_t)((uintptr_t)&__data_end - (uintptr_t)&__data_start));
    memleak_scanMemory((void*)&__lbss_start, (size_t)((uintptr_t)&__lbss_end - (uintptr_t)&__lbss_start));
    spinlock_release(&memleak_lock);
    
    size_t leak_possible = 0;

    foreach(ln, obj_list) {
        memleak_object_t *obj = (memleak_object_t*)ln->value;
        if (obj->paint == MEMLEAK_WHITE) {
            leak_possible += obj->size;
            LOG(WARN, "Possible memory leak at %p size %zu\n", obj->ptr, obj->size);
            LOG(WARN, "Backtrace:\n");
            for (int i = 0; i < 10; i++) {
                if (obj->frames[i] == 0x0) break;
                LOG(WARN, "%016llX ", obj->frames[i]);
                if ((uintptr_t)obj->frames[i] >=  (uintptr_t)&__kernel_start && (uintptr_t)obj->frames[i] <= (uintptr_t)&__kernel_end) {
                    char *name;
                    uintptr_t addr = ksym_find_best_symbol((uintptr_t)obj->frames[i], &name);
                    if (addr) {
                        dprintf(NOHEADER, " (%s+0x%llX)\n", name, (uintptr_t)obj->frames[i] - addr);
                    } else {
                        dprintf(NOHEADER, " (symbol not found)\n");
                    }
                } else {
                    dprintf(NOHEADER, " (not in kernel)\n");
                }
            }
        }
    }

    LOG(WARN, "Leak summary: %d\n", leak_possible);

}

/**
 * @brief Memory leak thread
 */
void memleak_thread(void *context) {
    for (;;) {
        memleak_scan();

        sleep_time(60, 0);
        sleep_enter();
    }
}

/**
 * @brief Spawn memory leak thread
 */
int memleak_initThread() {
    memleak_proc = process_createKernel("kmemleak scanner", PROCESS_KERNEL, PRIORITY_LOW, memleak_thread, NULL);
    scheduler_insertThread(memleak_proc->main_thread);
    return 0;
}

SCHED_INIT_ROUTINE(memleak, INIT_FLAG_DEFAULT, memleak_initThread);

#else

void memleak_init() {

}

#endif