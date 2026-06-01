/**
 * @file hexahedron/include/kernel/subsystems/irq.h
 * @brief Hexahedron IRQ manager
 * 
 * Provides the generic stuff for IRQs including allocation, registering, starting, ending, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_IRQ_H
#define KERNEL_SUBSYSTEMS_IRQ_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/procmask.h>
#include <kernel/misc/spinlock.h>
#include <kernel/arch/arch.h>

/**** DEFINITIONS ****/

#define IRQ_MAX_VECTORS 256

// irq_handler return values
#define IRQ_NOT_SOURCE      0   // If this interrupt wasn't the source, then continue looking through list. Only applies to IRQ_FLAG_SHARED handlers
#define IRQ_HANDLED         1   // Handled successfully!
#define IRQ_ERROR           2   // Uh oh, panic!

// flags for IRQ register
#define IRQ_FLAG_DEFAULT        0x0
#define IRQ_FLAG_SHARED         0x1
#define IRQ_FLAG_UNMAPPED       0x2 // hacky: This IRQ was allocated but hasnt been registered yet.
#define IRQ_FLAG_REGISTERS      0x4 // Instead of passing the context pointer pass architecture registers

/**** TYPES ****/

struct irq;
struct irq_domain;
typedef unsigned int irq_number_t;
typedef int (*irq_handler_t)(struct irq *irq, void *context);

/* IRQ handler: One small little IRQ */
typedef struct irq {
    struct irq *next; // for IRQ_FLAG_SHARED
    struct irq_domain *domain;

    spinlock_t lck;
    irq_number_t num;
    int hwirq;
    irq_handler_t handler;
    void *context;
    uint32_t flags;
    procmask_t affinity;
} irq_t;

/* IRQ chip: Architecture-specific */
typedef struct irq_chip {
    char *name;
    struct {
        void (*irq_mask)(irq_t*);
        void (*irq_unmask)(irq_t*);
        void (*irq_eoi)(irq_t*);
        void (*irq_set_affinity)(irq_t *, procmask_t);
    } ops;
    void *priv;
} irq_chip_t;

/* IRQ domain: Provides a singular domain of IRQs (MSI, I/O APIC, etc.) */
typedef struct irq_domain {
    char *domain_name;
    irq_chip_t *chip;

    struct {
        int (*domain_alloc)(struct irq_domain *domain, int hwirq, void *dev, irq_number_t *ret);
        int (*domain_map)(struct irq_domain *domain, irq_t *irq, int hwirq, void *dev);
    } ops;

    void *priv;
} irq_domain_t;

typedef struct irq_table {
    irq_t *irqs[IRQ_MAX_VECTORS];
} irq_table_t;

// !!! Hack! These are the expected domains.
extern irq_domain_t *percpu_domain;
extern irq_domain_t *global_domain;
extern irq_domain_t *msi_domain;
extern irq_domain_t *msix_domain;

/**** FUNCTIONS ****/

/**
 * @brief Initialize IRQ handling subsystem
 */
void irq_init();

/**
 * @brief Initialize IRQ handling on this CPU
 */
void irq_initCPU();

/**
 * @brief Register a new IRQ
 * @param vector The vector to register
 * @param handler The handler to register
 * @param flags The flags for the IRQ
 * @param ctx Context for the IRQ
 * @param irq_out Output for the IRQ
 * @returns 0 on success
 * 
 * @note If this IRQ was not already mapped it will use the default domain
 */
int irq_register(irq_number_t vector, irq_handler_t handler, uint32_t flags, void *ctx, irq_t **irq_out);

/**
 * @brief Map to a domain
 * 
 * Effectively, when you do @c irq_register you have registered an entry
 * in the CPU's vector table. The actual IRQ chip has not mapped it yet.
 * You can call this to map your IRQ in with the domain
 * 
 * Allocates an empty irq_t which will later be filled by a call to @c irq_register
 * 
 * @param domain The domain to map in the IRQ vector
 * @param vector The vector to map to the domain
 * @param hwirq The hardware IRQ number to map in (domain-specific)
 * @param context Domain context (domain-specific)
 * @returns 0 on success
 */
int irq_map(irq_domain_t *domain, irq_number_t vector, int hwirq, void *context);

/**
 * @brief Allocate an IRQ vector
 * @warning DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING!
 * Just allocates an IRQ vector but doesnt map it to anything
 */
irq_number_t irq_allocateVector();

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
int irq_allocate(irq_domain_t *domain, int hwirq, void *dev, irq_number_t *output);

/**
 * @brief Reserve an IRQ vector
 * @param vector The vector to reserve
 * 
 * Mainly this is only used during boot process to reserve exception vectors
 * Note that exceptions are never passed to @c irq_handler!!!
 */
void irq_reserveVector(irq_number_t num);

/**
 * @brief IRQ generic handler
 * @param vector The vector that caused the interrupt
 * @param regs Registers
 */
void irq_handler(irq_number_t vector, registers_t *regs);

/**
 * @brief Set the affinity of an IRQ
 * @param irq The IRQ to set the affinity of
 * @param affinity Processor mask for affinity
 */
int irq_setAffinity(irq_t *irq, procmask_t affinity);

/**
 * @brief Get an IRQ handler for a vector
 * @param vector The vector to get
 * Will return the first IRQ in a shared list (use ->next to iterate) if present
 */
irq_t *irq_get(irq_number_t num);

#endif
