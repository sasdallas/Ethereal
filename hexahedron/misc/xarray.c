/**
 * @file hexahedron/misc/xarray.c
 * @brief XArray
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/misc/xarray.h>
#include <kernel/processor_data.h>
#include <kernel/mm/slab.h>
#include <kernel/init.h>

/* XArray node cache */
static slab_cache_t *xarray_node_cache = NULL;

/* ugh i should have some sort of API for this since spinlocks are bad */
#define XA_LOCK_READ(xa) PREEMPT_DISABLE()
#define XA_UNLOCK_READ(xa) PREEMPT_ENABLE()
#define XA_LOCK_WRITE(xa) while (__atomic_test_and_set(&(xa)->xa_lck, __ATOMIC_ACQUIRE)) { arch_pause_single(); }
#define XA_UNLOCK_WRITE(xa) __atomic_clear(&(xa)->xa_lck, __ATOMIC_RELEASE)

/* RCU */
#define XA_DEREF(ptr) __atomic_load_n(&(ptr), __ATOMIC_CONSUME)
#define XA_STORE_PTR(ptr,val) __atomic_store_n(&(ptr), val, __ATOMIC_RELEASE)
#define XA_GET_VALUE(ptr) ((void*)((uintptr_t)(ptr) & ~0x1))
#define XA_IS_VALUE(ptr) (((uintptr_t)(ptr) & 0x1))
#define XA_AS_VALUE(ptr) ((ptr) ? (void*)((uintptr_t)ptr | 0x1) : NULL)


/**
 * @brief Allocate a new XArray node
 */
static xarray_node_t *xa_allocnode() {
    assert(xarray_node_cache);
    xarray_node_t *n = slab_allocate(xarray_node_cache);
    memset(n, 0, sizeof(xarray_node_t));
    assert(!XA_IS_VALUE(n));
    return n;
}

/**
 * @brief Free node
 */
static void xa_freenode(xarray_node_t *n) {
    slab_free(xarray_node_cache, n);
}

/**
 * @brief Load an entry from an XArray
 * @param xa The XArray to load from
 * @param index The index to load
 * @returns The value at that index
 */
void *xa_load(xarray_t *xa, unsigned long index) {
    XA_LOCK_READ(xa);

    xarray_ptr_t entry = XA_DEREF(xa->xa_head);
    if (!entry) {
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    if (XA_IS_VALUE(entry)) {
        XA_UNLOCK_READ(xa);

        if (index == 0) {
            return XA_GET_VALUE(entry);
        } else {
            return NULL;
        }
    }

    xarray_node_t *node = (xarray_node_t *)entry;
    
    if ((index >> node->shift) > XA_MASK) {
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    while (node) {
        size_t off = (index >> node->shift) & XA_MASK;
        xarray_ptr_t next = XA_DEREF(node->entries[off]);

        if (!next) {
            XA_UNLOCK_READ(xa);
            return NULL;
        }

        if (XA_IS_VALUE(next)) {
            XA_UNLOCK_READ(xa);
            return XA_GET_VALUE(next);
        }

        node = (xarray_node_t *)next;
    }


    XA_UNLOCK_READ(xa);
    return NULL;
}

/**
 * @brief Store an entry into an XArray
 * @param xa The XArray to store to
 * @param index The index to store at
 * @param entry The entry to store
 * @returns The previous value at index
 * 
 * @note This function allocates memory
 */
void *xa_store(xarray_t *xa, unsigned long index, void *entry) {
    assert(((uintptr_t)entry & 0x1) == 0 && "xarray only accepts values without 1st bit");
    
    XA_LOCK_WRITE(xa);

    // if there is no head
    if (xa->xa_head == NULL) {
        if (entry == NULL) {
            XA_UNLOCK_WRITE(xa);
            return NULL;
        }

        size_t shift = 0;
        if (index > XA_MASK) {
            unsigned bits = sizeof(index) * 8 - __builtin_clzl(index);
            shift = ((bits - 1) / XA_SHIFT) * XA_SHIFT;
        }

        xarray_node_t *root = xa_allocnode();
        root->shift = shift;
        root->parent = NULL;

        size_t offset = (index >> shift) & XA_MASK;
        root->entries[offset] = XA_AS_VALUE(entry);
        root->bitmap = (1ULL << offset);
    
        XA_STORE_PTR(xa->xa_head, root);
        XA_UNLOCK_WRITE(xa);
        return NULL;
    }

    // if there is a head
    xarray_ptr_t h = xa->xa_head;
    if (!XA_IS_VALUE(h)) {
        // We may need to grow the tree upwards
        xarray_node_t *r = xa->xa_head;
        while ((index >> r->shift) > XA_MASK) {
            xarray_node_t *new = xa_allocnode();
            new->shift = r->shift + XA_SHIFT;
            new->entries[0] = (xarray_ptr_t)r;
            new->bitmap = 1;
            new->parent = NULL;
            r->parent = new;

            r = new;
            XA_STORE_PTR(xa->xa_head, r);
        }
    } else {
        // head is currently a value at index 0
        // if (index == 0) {
        //     void *pr = XA_DEREF(xa->xa_head);
        //     XA_STORE_PTR(xa->xa_head, XA_AS_VALUE(entry));
        //     XA_UNLOCK_WRITE(xa);
        //     return XA_GET_VALUE(pr);
        // }

        void *old = XA_GET_VALUE(XA_DEREF(xa->xa_head));
        xarray_node_t *root = xa_allocnode();
        root->shift = 0;
        root->parent = NULL;
        root->entries[0] = XA_AS_VALUE(old);
        root->bitmap = 1ULL;

        XA_STORE_PTR(xa->xa_head, root);

        xarray_node_t *r = xa->xa_head;
        while ((index >> r->shift) > XA_MASK) {
            xarray_node_t *new_node = xa_allocnode();
            new_node->shift = r->shift + XA_SHIFT;
            new_node->entries[0] = (xarray_ptr_t)r;
            new_node->bitmap = 1;
            new_node->parent = NULL;
            r->parent = new_node;

            r = new_node;
            XA_STORE_PTR(xa->xa_head, r);
        } 
    }

    // ok place it
    xarray_node_t *i = (xarray_node_t*)xa->xa_head;
    while (i->shift > 0) {
        size_t off = (index >> i->shift) & XA_MASK;
        xarray_ptr_t next = i->entries[off];

        // todo not use leaf nodes
        if (next == NULL) {
            xarray_node_t *c = xa_allocnode();
            c->shift = i->shift - XA_SHIFT;
            c->parent = i;

            XA_STORE_PTR(i->entries[off], c);
            i->bitmap |= (1ULL << off);
            
            next = c;
        }
        
        assert(!XA_IS_VALUE(next));

        i = next;
    }

    // i should have the node in it
    size_t off = (index >> i->shift) & XA_MASK;
    if (i->entries[off] == NULL && entry) {
        i->bitmap |= (1ULL << off);
    } else if (i->entries[off] && entry == NULL) {
        i->bitmap &= ~(1ULL << off);
    }

    void *pr = XA_DEREF(i->entries[off]);
    XA_STORE_PTR(i->entries[off], entry ? XA_AS_VALUE(entry) : NULL);
    
    if (i->bitmap == 0) {
        // we can free i
        xarray_node_t *clean = i;
        while (clean) {
            xarray_node_t *parent = clean->parent;
            if (parent == NULL) {
                XA_STORE_PTR(xa->xa_head, NULL);
                xa_freenode(clean);
                break;
            }

            size_t poff = (index >> parent->shift) & XA_MASK;
            XA_STORE_PTR(parent->entries[poff], NULL);
            parent->bitmap &= ~(1ULL << poff);

            xa_freenode(clean);
            if (parent->bitmap > 0) break;

            clean = parent;
        }
    }

    XA_UNLOCK_WRITE(xa);
    return XA_GET_VALUE(pr);
}

/**
 * @brief Delete an entry from an XArray
 * @param xa The XArray to delete from
 * @param index The index of the entry
 * @returns The previous value
 */
void *xa_erase(xarray_t *xa, unsigned long index) {
    return xa_store(xa, index, NULL);
}

/**
 * @brief Find the first occupied entry in the XArray
 * @param xa The XArray
 * @param index Output pointer for the index
 * @param max The maximm to go to
 */
void *xa_find(xarray_t *xa, unsigned long *index, unsigned long max) {
    XA_LOCK_READ(xa);

    xarray_ptr_t e = XA_DEREF(xa->xa_head);
    if (!e) {
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    if (XA_IS_VALUE(e)) {
        if (*index == 0) {
            XA_UNLOCK_READ(xa);
            return XA_GET_VALUE(e);
        }

        // nothing else
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    unsigned long idx = *index;
    xarray_node_t *n = (xarray_node_t*)e;

    // descend tree
    while (idx <= max) {
        if ((idx >> n->shift) > XA_MASK) break;

        while (n->shift) {
            size_t off = (idx >> n->shift) & XA_MASK;
            unsigned long active = n->bitmap & (~0UL << off);

            if (!active) {
                unsigned long step = 1UL << n->shift;
                idx = (idx + step) & ~(step-1);
                if (n->parent == NULL) {
                    XA_UNLOCK_READ(xa);
                    return NULL;
                }

                n = n->parent;
                continue;
            }

            size_t next = __builtin_ctzl(active);
            if (next > off) {
                idx &= ~((1UL << (n->shift + XA_SHIFT)) - 1); 
                idx |= (unsigned long)next << n->shift;
            }

            xarray_ptr_t next_ptr = XA_DEREF(n->entries[next]);
            if (XA_IS_VALUE(next_ptr)) {
                *index = idx;
                XA_UNLOCK_READ(xa);
                return XA_GET_VALUE(next_ptr);
            }

            n = (xarray_node_t*)next_ptr;
        }

        size_t off = idx & XA_MASK;
        unsigned long active = n->bitmap & (~0UL << off);
        if (active) {
            size_t next = __builtin_ctzl(active);
            idx = (idx & ~XA_MASK) | next;


            if (idx <= max) {
                *index = idx;
                xarray_ptr_t fin = XA_DEREF(n->entries[next]);
                XA_UNLOCK_READ(xa);
                return XA_GET_VALUE(fin);
            }
        
            break;
        }

        idx = (idx + XA_ENTRIES) & ~XA_MASK;
        if (!n->parent) break;
        n = n->parent;
    }


    XA_UNLOCK_READ(xa);
    return NULL;
}

/**
 * @brief Find the first occupied entry in the XArray, going backwards
 * @param xa The XArray
 * @param index The maximum index
 */
void *xa_findBackwards(xarray_t *xa, unsigned long *index) {
    XA_LOCK_READ(xa);

    xarray_ptr_t e = XA_DEREF(xa->xa_head);
    if (!e) {
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    if (XA_IS_VALUE(e)) {
        if (*index == 0) {
            XA_UNLOCK_READ(xa);
            return XA_GET_VALUE(e);
        }
        XA_UNLOCK_READ(xa);
        return NULL;
    }

    unsigned long idx = *index;
    xarray_node_t *n = (xarray_node_t*)e;

    while (1) {
        if ((idx >> n->shift) > XA_MASK) break;

        while (n->shift) {
            size_t off = (idx >> n->shift) & XA_MASK;
            unsigned long mask = (1UL << (off + 1)) - 1;
            unsigned long active = n->bitmap & mask;

            if (!active) {
                unsigned long step = 1UL << (n->shift + XA_SHIFT);
                if (idx < step) {
                    XA_UNLOCK_READ(xa);
                    return NULL;
                }
                idx = (idx & ~(step - 1)) - 1;
                if (n->parent == NULL) {
                    XA_UNLOCK_READ(xa);
                    return NULL;
                }
                n = n->parent;
                continue;
            }

            size_t next = 63 - __builtin_clzl(active);
            if (next < off) {
                unsigned long slot_mask = (1UL << n->shift) - 1;
                idx = (idx & ~((1UL << (n->shift + XA_SHIFT)) - 1)) | (next << n->shift) | slot_mask;
            }

            xarray_ptr_t next_ptr = XA_DEREF(n->entries[next]);
            if (XA_IS_VALUE(next_ptr)) {
                *index = idx;
                XA_UNLOCK_READ(xa);
                return XA_GET_VALUE(next_ptr);
            }

            n = (xarray_node_t*)next_ptr;
        }

        size_t off = idx & XA_MASK;
        unsigned long mask = (1UL << (off + 1)) - 1;
        unsigned long active = n->bitmap & mask;
        if (active) {
            size_t next = 63 - __builtin_clzl(active);
            idx = (idx & ~XA_MASK) | next;

            *index = idx;
            xarray_ptr_t fin = XA_DEREF(n->entries[next]);
            XA_UNLOCK_READ(xa);
            return XA_GET_VALUE(fin);
        }

        if ((idx & ~XA_MASK) == 0) {
            break;
        }

        idx = (idx & ~XA_MASK) - 1;
        if (!n->parent) break;
        n = n->parent;
    }

    XA_UNLOCK_READ(xa);
    return NULL;
}

/**
 * @brief Get the first entry in the array starting from inex
 * @param xa The XArray
 * @param index Index out
 * @returns The value at the entry or NULL
 */
void *xa_first(xarray_t *xa, unsigned long *index) {
    *index = 0;
    return xa_find(xa, index, ULONG_MAX);
}

/**
 * @brief Get the next entry 
 * @param xa The XArray
 * @param index The index out
 */
void *xa_next(xarray_t *xa, unsigned long *index) {
    *index += 1;
    return xa_find(xa, index, ULONG_MAX);
}

/**
 * @brief Destroy all entries in an XArray
 * @param xa The XArray to destroy
 */
void xa_destroy(xarray_t *xa) {
    unsigned long idx;
    void *ent;
    xa_foreach(xa, idx, ent) {
        xa_erase(xa, idx);
    }
}

/**
 * @brief XArray init
 */
int xa_init() {
    xarray_node_cache = slab_createCache("xarray node", SLAB_CACHE_DEFAULT, sizeof(xarray_node_t), 0, NULL, NULL);
    return 0;
}

KERN_EARLY_INIT_ROUTINE(xarray, INIT_FLAG_DEFAULT, xa_init);
