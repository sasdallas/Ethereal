/**
 * @file hexahedron/arch/x86_64/smp.c
 * @brief Symmetric multiprocessing/processor data handler
 * 
 * 
 * @copyright
 * This file is part of Ethereal Operating System, which is created by Samuel.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel S.
 * 
 * @copyright
 * Portions of this file are sourced from the Astral Operating System. License text is included below.
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

#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/interrupt.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/arch.h>
#include <kernel/task/process.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/x86/local_apic.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/misc/spinlock.h>
#include <kernel/misc/args.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

/* SMP data */
static smp_info_t *smp_data = NULL;

/* Local APIC mmio address */
uintptr_t lapic_remapped = 0;

/* Remapped page for the bootstrap code */
static uintptr_t bootstrap_page_remap = 0;

/* Core stack - this is used after paging is setup */
uintptr_t _ap_stack_base = 0;

/* Trampoline variables */
extern uintptr_t _ap_bootstrap_start, _ap_bootstrap_end;

/* Mask of APs done stopping */
procmask_t ap_stopped_mask = PROCMASK_INITIALIZER;

/* TLB shootdown */
struct tlb_shootdown_request {
    spinlock_t shootdown_lck;
    uintptr_t addr;
    size_t size;
    atomic_int *pending_completion;
};

static struct tlb_shootdown_request tlb_shootdown_req[MAX_CPUS];

/* Last CPU number */
static int last_cpu_number = 1;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SMP", __VA_ARGS__)

/**
 * @brief Invalidate pages
 */
static void smp_invalidate(uintptr_t addr, size_t size) {
    if (size > PAGE_SIZE*16) {
        // reload cr3 instead, its faster
        asm volatile ("movq %%cr3, %%rax\n"
                      "movq %%rax, %%cr3" ::: "rax", "memory");
    } else {
        for (uintptr_t i = addr; i < addr + size; i += PAGE_SIZE) {
            asm volatile ("invlpg (%0)" :: "r"(i) : "memory");
        }
    }
}

/**
 * @brief Handle a TLB shootdown
 */
int smp_handleTLBShootdown(irq_t *irq, void *context) {
    // Acknowledge the request
    struct tlb_shootdown_request *r = &tlb_shootdown_req[arch_current_cpu()];

    smp_invalidate(r->addr, r->size);
    __atomic_add_fetch(r->pending_completion, 1, __ATOMIC_SEQ_CST);
    spinlock_releaseRaw(&r->shootdown_lck); // Astral

    return IRQ_HANDLED;
}

/**
 * @brief Get the current local APIC ID
 */
static int smp_getLocalAPICID() {
	uint32_t ebx, unused;
    __cpuid(0x1, unused, ebx, unused, unused);
    return (int)(ebx >> 24);
}

/**
 * @brief Collect AP information to store in processor_data
 * @param ap The core to store information on
 */
void smp_collectAPInfo(int ap) {
    strncpy(processor_data[ap].info.vendor, cpu_getVendorName(), CPU_MAX_VENDOR);
    strncpy(processor_data[ap].info.model, cpu_getBrandString(), CPU_MAX_MODEL);
    processor_data[ap].info.model_number = cpu_getModelNumber();
    processor_data[ap].info.family = cpu_getFamily();
    processor_data[ap].lapic_id = smp_getLocalAPICID();
    processor_data[ap].cpu_id = ap;
}

/**
 * @brief Finish an AP's setup. This is done right after the trampoline code gets to 32-bit mode and sets up a stack
 * @param params The AP parameters set up by @c smp_prepareAP
 */
__attribute__((noreturn)) void smp_finalizeAP() {
    // Load new stack
    asm volatile ("movq %0, %%rsp" :: "m"(_ap_stack_base));

    // Get the CPU ID
    int id = last_cpu_number++;

    // Set GSbase
    arch_set_gsbase((uintptr_t)&processor_data[id]);
    arch_initialize_syscall_handler();

    current_cpu->cpu_id = id;

    // Enter AP
    arch_mmu_ap();

    // We want all cores to have a consistent GDT
    hal_gdtInitCore(arch_current_cpu(), _ap_stack_base);
    
    // Install the IDT
extern void hal_installIDT();
    hal_installIDT();

    // Initialize SSE
    arch_enable_sse();

    // Set current core's directory
    vmm_switch(vmm_kernel_context);

    // Configure IRQ subsystem
    irq_initCPU();

    // Map the SMP TLB shootdown event
    irq_map(percpu_domain, 124, 124, NULL);
    irq_register(124, smp_handleTLBShootdown, 0, NULL, NULL);

    // Enable interrupts
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);

    // Before the local APIC timer is initialized we must have tasklets
    tasklet_init();

    // Reinitialize the APIC
    lapic_initialize(lapic_remapped);

    // Now collect information
    smp_collectAPInfo(smp_currentCPU());

    // Jump to generic
    smp_apEntry();
    for (;;);
}


/**
 * @brief Sleep for a short period of time
 * @param delay How long to sleep for
 */
static void smp_delay(unsigned int delay) {
    uint64_t clock = clock_readTSC();
    while (clock_readTSC() < clock + delay * clock_getTSCSpeed());
}

/**
 * @brief Start an AP
 * @param lapic_id The ID of the local APIC to start
 */
void smp_startAP(uint8_t lapic_id) {
    int new_cpu = last_cpu_number;
    LOG(DEBUG, "Starting CPU%d with local APIC ID 0x%x\n", new_cpu, lapic_id);

    // Copy the bootstrap code. The AP might've messed with it.
    memcpy((void*)SMP_AP_BOOTSTRAP_PAGE, (void*)&_ap_bootstrap_start, (uintptr_t)&_ap_bootstrap_end - (uintptr_t)&_ap_bootstrap_start);

    // Allocate a stack for the AP
    _ap_stack_base = (uintptr_t)vmm_map(NULL, PAGE_SIZE * 2, VM_FLAG_ALLOC, MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    memset((void*)(_ap_stack_base), 0, PAGE_SIZE);
    _ap_stack_base += (PAGE_SIZE);

    // Send the INIT signal
    lapic_sendInit(lapic_id);
    smp_delay(5000UL);

    // Send SIPI
    lapic_sendStartup(lapic_id, SMP_AP_BOOTSTRAP_PAGE);

    // Wait for AP to finish and set the startup flag
    LOG(DEBUG, "Waiting for CPU%d to finish startup\n", new_cpu);
    
    do {
        arch_pause_single();
    } while (!procmask_test(&smp_online_cpus, new_cpu));
}

/**
 * @brief Initialize the SMP system
 * @param info Collected SMP information
 * @returns 0 on success, non-zero is failure
 */
int smp_init(smp_info_t *info) {
    // Store this information for later
    smp_data = info;

    // Map local APIC
    lapic_remapped = arch_mmu_remap_physical((uintptr_t)info->lapic_address, PAGE_SIZE, REMAP_PERMANENT);

    // Initialize the local APIC
    assert(lapic_initialize(lapic_remapped) == 0);

    // Now initialize I/O APIC
    assert(pic_init(PIC_TYPE_IOAPIC, (void*)info) == 0);

    // Initialize the generic SMP layer
    smp_genericInit();

    // Don't use SMP?
    if (info->processor_count == 1 || kargs_has("--disable-smp")) {
        processor_count = 1;
        arch_get_generic_parameters()->cpu_count = 1;
        goto _finish_collection;
    }

    // The AP expects its code to be bootstrapped to a page-aligned address (SIPI expects a starting page number)
    // The remapped page for SMP is stored in the variable SMP_AP_BOOTSTRAP_PAGE
    // Assuming that page has some content in it, copy and store it.
    uintptr_t temp_frame = pmm_allocatePage(ZONE_DEFAULT);
    uintptr_t temp_frame_remap = arch_mmu_remap_physical(temp_frame, PAGE_SIZE, REMAP_TEMPORARY);

    // Map in the bootstrap page
    arch_mmu_map(NULL, SMP_AP_BOOTSTRAP_PAGE, SMP_AP_BOOTSTRAP_PAGE, MMU_FLAG_WRITE | MMU_FLAG_PRESENT);

    // Copy the prior contents
    memcpy((void*)temp_frame_remap, (void*)SMP_AP_BOOTSTRAP_PAGE, PAGE_SIZE);

    // Start APs
    // WARNING: Starting CPU0/BSP will triple fault (bad)
    for (int i = 0; i < smp_data->processor_count; i++) {
        if (i != 0) {
            smp_startAP(smp_data->lapic_ids[i]);
        }
    }

    // Finished! Unmap bootstrap code
    memcpy((void*)SMP_AP_BOOTSTRAP_PAGE, (void*)temp_frame_remap, PAGE_SIZE);
    arch_mmu_unmap_physical(temp_frame_remap, PAGE_SIZE);

    // Unmap bootstrap page
    arch_mmu_unmap(NULL, SMP_AP_BOOTSTRAP_PAGE);
    pmm_freePage(temp_frame);

    // Register TLB shootdown IRQ for BSP
    irq_map(percpu_domain, 124, 124, NULL);
    irq_register(124, smp_handleTLBShootdown, 0, NULL, NULL);

    processor_count = smp_data->processor_count;
    arch_get_generic_parameters()->cpu_count = processor_count;

_finish_collection:
    LOG(INFO, "SMP initialization completed successfully - %i CPUs available to system\n", processor_count);
    
    if (processor_count > 1) {
        vmm_postSMP();
    }

    return 0;
}

/**
 * @brief Get the current CPU ID (not APIC ID)
 */
int smp_getCurrentCPU() {
    return current_cpu->cpu_id;
}

/**
 * @brief Acknowledge core shutdown (called by ISR)
 */
void smp_acknowledgeCoreShutdown() {
    procmask_set(&ap_stopped_mask, arch_current_cpu());
}

/**
 * @brief Shutdown all cores in a system
 * 
 * This causes ISR2 (NMI) to be thrown, disabling the core's interrupts and 
 * looping it on a halt instruction.
 */
void smp_disableCores() {
    if (smp_data == NULL || processor_count == 1) return;

    for (int i = 0; i < smp_data->processor_count; i++) {
        if (smp_data->lapic_ids[i] != current_cpu->lapic_id) {
            lapic_sendNMI(smp_data->lapic_ids[i], 0); // The interrupt vector here doesnt matter as an NMI is sent regardless
            
            do {
                arch_pause_single();
            } while (!procmask_test(&ap_stopped_mask, i));
        }
    }

extern spinlock_t debug_lock;
    spinlock_release(&debug_lock);
}


/**
 * @brief Perform a TLB shootdown on a specific range
 * @param address The address to perform the TLB shootdown on
 * @param size The size of the TLB shootdown
 * 
 * @ref https://github.com/Mathewnd/Astral/blob/rewrite/kernel-src/arch/x86-64/mmu.c#L222
 * @todo Fix all of this junk
 */
void smp_tlbShootdown(uintptr_t address, size_t size) {
    if (!size || !smp_data) return; // no.
    if (processor_count < 2) return; // No CPUs
    if (size & 0xfff) size = PAGE_ALIGN_UP(size);

    int is_user_shootdown = (address < MMU_USERSPACE_END);
    int state = hal_getInterruptState();

    // Ensure non-interruptable
    __PREEMPT_DISABLE();
    
    atomic_int waiting = 0;
    int expected = 0;

    for (int i = 0; i < processor_count; i++) {
        if (i != arch_current_cpu() && (!is_user_shootdown || processor_data[i].current_context == current_cpu->current_context)) {
            // This CPU needs to be shotdown
            spinlock_acquireRaw(&tlb_shootdown_req[i].shootdown_lck); // astral's idea
            tlb_shootdown_req[i].addr = address;
            tlb_shootdown_req[i].size = size;
            tlb_shootdown_req[i].pending_completion = &waiting;
            lapic_sendIPI(processor_data[i].lapic_id, 124, LAPIC_ICR_DESTINATION_PHYSICAL | LAPIC_ICR_INITDEASSERT | LAPIC_ICR_EDGE);
            expected++;
        }
    }

    // dirty TLB hack
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
    while (__atomic_load_n(&waiting, __ATOMIC_RELAXED) != expected) __builtin_ia32_pause();
    hal_setInterruptState(state);
    __PREEMPT_ENABLE();
}

/**
 * @brief Send IPI to core(s)
 * @param core The core to send the IPI to if sending to a specific core
 * @param ipi The IPI vector to send
 * @param destination IPI destination
 * @param flags IPI flags
 */
int arch_send_ipi(int core, unsigned int ipi, unsigned int destination, unsigned int flags) {
    unsigned int apic_flags = LAPIC_ICR_EDGE;
    if (flags & IPI_FLAG_NMI) apic_flags |= LAPIC_ICR_NMI;

    if (destination == IPI_DEST_CORE) {
        apic_flags |= LAPIC_ICR_DESTINATION_PHYSICAL | (1 << 14);
    } else if (destination == IPI_DEST_SELF) {
        apic_flags |= LAPIC_ICR_DESTINATION_SELF;
    } else if (destination == IPI_DEST_ALL) {
        apic_flags |= LAPIC_ICR_DESTINATION_ALL;
    } else {
        apic_flags |= LAPIC_ICR_DESTINATION_EXCLUDE_SELF | LAPIC_ICR_INITDEASSERT;
    }

    lapic_sendIPI((uint8_t)processor_data[core].lapic_id, (uint8_t)ipi, apic_flags);
    return 0;
}
