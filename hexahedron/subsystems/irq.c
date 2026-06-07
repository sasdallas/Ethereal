/**
 * @file hexahedron/subsystems/irq.c
 * @brief IRQ subsystem
 * 
 * There are a few flaws in this IRQ subsystem.
 * One is the annoyingness for reliability and fallback.
 * 
 * The IRQ subsystem is built around redirection and transparency, with the global
 * pattern being to allocate/map/register a vector. However, for certain devices
 * like the 8259 PIC this does not work.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/subsystems/irq.h>
#include <kernel/processor_data.h>
#include <kernel/mm/slab.h>
#include <kernel/arch/arch.h>
#include <kernel/task/process.h>
#include <kernel/tasklet.h>
#include <kernel/panic.h>
#include <kernel/debug.h>
#include <string.h> 

/* Global list of (volatile) interrupt vectors */
/* Modifications to this are atomic */
static irq_table_t irq_table = { 0 };

/* Interrupt cache */
slab_cache_t *irq_cache = NULL;

/* IRQ bitmap */
BITMAP_DEFINE(irq_bitmap, IRQ_MAX_VECTORS);
spinlock_t irq_bitmap_lock = SPINLOCK_INITIALIZER;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "IRQ", __VA_ARGS__)

/**
 * @brief Initialize IRQ handling subsystem
 */
void irq_init() {
    memset(&irq_table, 0, sizeof(irq_table));
    bitmap_fill(irq_bitmap, 0, IRQ_MAX_VECTORS);
    irq_cache = slab_createCache("irq cache", SLAB_CACHE_DEFAULT, sizeof(irq_t), 0, NULL, NULL);
    LOG(INFO, "Interrupt subsystem initialized\n");
}

/**
 * @brief Initialize IRQ handling on this CPU
 */
void irq_initCPU() {
    current_cpu->irq_table = kzalloc(sizeof(irq_table_t));
}

/**
 * @brief Allocate a new interrupt entry
 */
static irq_t *irq_allocateEntry() {
    return slab_allocate(irq_cache);
}

/**
 * @brief Free an interrupt entry
 */
static void irq_freeEntry(irq_t *irq) {
    slab_free(irq_cache, irq);
}

/**
 * @brief Register a new IRQ
 * @param vector The vector to register
 * @param handler The handler to register
 * @param flags The flags for the IRQ
 * @param ctx Context for the IRQ
 * @param irq_out Output for the IRQ
 * @returns 0 on success
 */
int irq_register(irq_number_t vector, irq_handler_t handler, uint32_t flags, void *ctx, irq_t **irq_out) {
    irq_t *i = irq_get(vector);
    if (!i) {
        assert(0 && "you must call irq_mapDomain to map an IRQ before calling irq_register");
    }

    // We need to search for a valid IRQ.
    // A valid IRQ is one that isnt locked and has IRQ_FLAG_UNMAPPED set on it
    // !!!: Makes the assumption any locked IRQ is already being setup by irq_register. Only bad for shared IRQs
    while (i) {
        int r = spinlock_tryAcquire(&i->lck);
        if (r == 1) {
            if (i->flags & IRQ_FLAG_UNMAPPED) {
                break;
            }

            spinlock_release(&i->lck);
        }

        assert((i->flags & IRQ_FLAG_SHARED) && (flags & IRQ_FLAG_SHARED));
        i = i->next;
    }

    assert(i && "IRQ must be registered with irq_mapDomain (or this is kernel bug)");

    // Prepare all the IRQ-specific data, assuming the rest has already been setup
    i->handler = handler;
    i->flags = flags;
    i->context = ctx;

    // Right now, its easier to make it such that this IRQ can only occur on this CPU. irq_setAffinity can be used to change this
    // TODO: A way to balance IRQs across CPUs
    procmask_clear(&i->affinity);
    procmask_set(&i->affinity, current_cpu->cpu_id);
    
    // Unmask the IRQ now
    i->domain->chip->ops.irq_unmask(i);

    // Done
    spinlock_release(&i->lck);
    if (irq_out) *irq_out = i;
    return 0;
}

/**
 * @brief Allocate an IRQ vector
 * 
 * @warning DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING! 
 * Just allocates an IRQ vector but doesnt map it to anything
 */
irq_number_t irq_allocateVector() {
    spinlock_acquire(&irq_bitmap_lock);
    int v = bitmap_find_first(irq_bitmap, IRQ_MAX_VECTORS);
    if (v != -1) {
        assert(bitmap_test(irq_bitmap, v) == false);
        bitmap_set(irq_bitmap, v);
    }
    spinlock_release(&irq_bitmap_lock);
    
    if (v == -1) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "irq", "*** Out of usable IRQ numbers.\n");
        __builtin_unreachable();
    }

    return v;
}

/**
 * @brief Allocate an IRQ vector for a specific hwirq
 * @param domain The domain to allocate the IRQ vector in
 * @param hwirq The hardware IRQ to allocate (domain-specific)
 * @param dev Domain mapping context (domain-specific)
 * @param output The output IRQ number
 * @returns 0 on success 
 * 
 * Allocates it and maps it for you. You still need to call @c irq_register
 */
int irq_allocate(irq_domain_t *domain, int hwirq, void *dev, irq_number_t *output) {
    // First we will call the domain's alloc method
    // !!!: I dont like this code either
    irq_number_t allocated;
    if (domain->ops.domain_alloc) {
        // Domain does care, do the thing.
        int r = domain->ops.domain_alloc(domain, hwirq, dev, &allocated);
        if (r != 0) {
            LOG(WARN, "irq_allocate failed to allocate for hwirq %d on domain \"%s\" (error %d)\n", hwirq, domain->domain_name, r);
            return r;
        }

        // TODO: racey and unnecessary
        spinlock_acquire(&irq_bitmap_lock);
        bitmap_set(irq_bitmap, allocated);
        spinlock_release(&irq_bitmap_lock);
    
        assert(irq_get(allocated) == NULL && "domain_alloc returned already allocated and IRQ_FLAG_SHARED not ready yet");
    } else {
        // Domain doesn't care, grab our own IRQ.
        allocated = irq_allocateVector();
    }

    // Okay we allocated an IRQ number. Now we can use the domain and map it.
    int ret = irq_map(domain, allocated, hwirq, dev);
    if (ret != 0) {
        LOG(ERR, "Failed to map virq%d -> hwirq%d in domain \"%s\" (error %d)\n", allocated, hwirq, domain->domain_name, ret);
        LOG(ERR, "IRQ FREEING NOT IMPLEMENTED!\n");
        return ret;
    }

    LOG(DEBUG, "Mapped virq%d -> hwirq%d in domain \"%s\" successfully.\n", allocated, hwirq, domain->domain_name);

    if (output) *output = allocated;
    return 0;
}

/**
 * @brief Reserve an IRQ vector
 * @param vector The vector to reserve
 * 
 * Mainly this is only used during boot process to reserve exception vectors
 * Note that exceptions are never passed to @c irq_handler!!!
 */
void irq_reserveVector(irq_number_t num) {
    spinlock_acquire(&irq_bitmap_lock);
    assert(bitmap_test(irq_bitmap, num)==false);
    bitmap_set(irq_bitmap, num);
    spinlock_release(&irq_bitmap_lock);
}

/**
 * @brief Map to a domain
 * 
 * Effectively, when you do @c irq_register you have registered an entry
 * in the CPU's vector table. The actual IRQ chip has not mapped it yet.
 * You can call this to map your IRQ in with the domain
 * 
 * Allocates an empty irq_t which will later be filled by a call to irq_register
 * 
 * @param domain The domain to map in the IRQ vector
 * @param vector The vector to map to the domain
 * @param hwirq The hardware IRQ number to map in (domain-specific)
 * @param context Domain context (domain-specific)
 * @returns 0 on success
 */
int irq_map(irq_domain_t *domain, irq_number_t vector, int hwirq, void *context) {
    // To map an IRQ to a domain, we need to allocate a blank IRQ entry
    // assert(bitmap_test(irq_bitmap, vector) == true); // vector should already have a bitmap entry set so nothing can modify it
    // TODO: racey
    if (bitmap_test(irq_bitmap, vector) == false) {
        spinlock_acquire(&irq_bitmap_lock);
        bitmap_set(irq_bitmap, vector);
        spinlock_release(&irq_bitmap_lock);
    }

    assert(irq_table.irqs[vector] == NULL && "not supported IRQ_FLAG_SHARED"); 

    // allocate new irq object
    irq_t *irq = irq_allocateEntry();
    irq->domain = domain;
    irq->flags = IRQ_FLAG_UNMAPPED;
    irq->num = vector;
    irq->hwirq = hwirq;
    SPINLOCK_INIT(&irq->lck);
    irq->next = NULL;

    // Map
    if (domain->ops.domain_map) {
        int r = domain->ops.domain_map(domain, irq, hwirq, context);
        if (r != 0) {
            irq_freeEntry(irq);
            return r;
        }
    }

    // Set it in the table
    // !!!: HACK HACK HACK!!!
    if (domain == percpu_domain) {
        current_cpu->irq_table->irqs[vector] = irq;
    } else {
        __atomic_store_n(&irq_table.irqs[vector], irq, __ATOMIC_RELAXED);
    }

    return 0;
}

/**
 * @brief Set the affinity of an IRQ
 * @param irq The IRQ to set the affinity of
 * @param affinity Processor mask for affinity
 */
int irq_setAffinity(irq_t *irq, procmask_t affinity) {
    // TODO
    LOG(WARN, "IRQ set affinity not implemented\n");
    return 0;
}

/**
 * @brief Get an IRQ handler for a vector
 * @param vector The vector to get
 * Will return the first IRQ in a shared list (use ->next to iterate) if present
 */
inline irq_t *irq_get(irq_number_t num) {
    if (current_cpu->irq_table->irqs[num] != NULL) {
        return current_cpu->irq_table->irqs[num];
    }  

    return __atomic_load_n(&irq_table.irqs[num], __ATOMIC_RELAXED);
}

/**
 * @brief IRQ generic handler
 * @param vector The vector that caused the interrupt
 * @param regs Frame regs
 */
void irq_handler(irq_number_t vector, registers_t *regs) {
    IRQ_ENTER();

    irq_t *i = irq_get(vector);
    if (!i) {
        goto _unhandled;
    }

    if (i->flags & IRQ_FLAG_SHARED) {
        while (i) {
            int stat = i->handler(i, (i->flags & IRQ_FLAG_REGISTERS) ? regs : i->context);
            if (stat == IRQ_NOT_SOURCE) {
                i = i->next;
            } else if (stat == IRQ_ERROR) {
                kernel_panic_extended(IRQ_HANDLER_FAILED, "irq", "*** IRQ %d failed\n", vector);
                __builtin_unreachable();
            } else {
                break;
            }
        }

        if (!i) goto _unhandled;
    } else {
        int stat = i->handler(i, (i->flags & IRQ_FLAG_REGISTERS) ? regs : i->context);
        if (stat == IRQ_ERROR) {
            kernel_panic_extended(IRQ_HANDLER_FAILED, "irq", "*** IRQ %d failed\n", vector);
            __builtin_unreachable();
        }
    }

    // EOI now
    i->domain->chip->ops.irq_eoi(i);

    IRQ_EXIT();

    // handle irq exit
    if (current_cpu->tasklet && TASKLET_PENDING()) {
        tasklet_process();
    }
    
    if (current_cpu->irq_bitmap == 0 && current_cpu->current_thread && (current_cpu->current_thread->flags & THREAD_FLAG_NEEDS_RESCHED)) {
        process_yield(1);
    }

    return;

_unhandled:
    kernel_panic_extended(IRQ_HANDLER_FAILED, "irq", "*** Unhandled interrupt 0x%x\n", vector);
    __builtin_unreachable();
}
