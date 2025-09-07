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
 */

#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/interrupt.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/arch.h>
#include <kernel/processor_data.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/x86/local_apic.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/misc/spinlock.h>
#include <kernel/misc/args.h>

#include <kernel/mem/pmm.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>

#include <string.h>
#include <stddef.h>
#include <errno.h>

/* SMP data */
static smp_info_t *smp_data = NULL;

/* CPU data */
processor_t processor_data[MAX_CPUS] = {0};

/* CPU count */
int processor_count = 1;

/* Local APIC mmio address */
uintptr_t lapic_remapped = 0;

/* Remapped page for the bootstrap code */
static uintptr_t bootstrap_page_remap = 0;

/* Core stack - this is used after paging is setup */
uintptr_t _ap_stack_base = 0;

/* Trampoline variables */
extern uintptr_t _ap_bootstrap_start, _ap_bootstrap_end;

/* AP startup flag. This will change when the AP finishes starting */
static int ap_startup_finished = 0;

/* AP shutdown flag. This will change when the AP finishes shutting down. */
static int ap_shutdown_finished = 0;

/* TLB shootdown */
static uintptr_t tlb_shootdown_address = 0x0;
static spinlock_t tlb_shootdown_lock = { 0 };

/* Last CPU number */
static int last_cpu_number = 1;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SMP", __VA_ARGS__)

/**
 * @brief Handle a TLB shootdown
 */
int smp_handleTLBShootdown(uintptr_t exception_index, uintptr_t interrupt_number, registers_t *regs, extended_registers_t *extended) {
    // LOG(DEBUG, "TLB shootdown acknowledged for %p\n", tlb_shootdown_address);

    asm ("invlpg (%0)" :: "r"(tlb_shootdown_address));
    return 0;
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
    processor_data[ap].cpu_manufacturer = cpu_getVendorName();
    strncpy(processor_data[ap].cpu_model, cpu_getBrandString(), 48);
    processor_data[ap].cpu_model_number = cpu_getModelNumber();
    processor_data[ap].cpu_family = cpu_getFamily();
    current_cpu->lapic_id = smp_getLocalAPICID();
}

/**
 * @brief Finish an AP's setup. This is done right after the trampoline code gets to 32-bit mode and sets up a stack
 * @param params The AP parameters set up by @c smp_prepareAP
 */
__attribute__((noreturn)) void smp_finalizeAP() {
    // Load new stack
    asm volatile ("movq %0, %%rsp" :: "m"(_ap_stack_base));

    int id = last_cpu_number++;

    // Set GSbase
    arch_set_gsbase((uintptr_t)&processor_data[id]);
    arch_initialize_syscall_handler();

    current_cpu->cpu_id = id;

    // Setup the PAT
    asm volatile(
    	"movl $0x277, %%ecx\n"
    	"rdmsr\n"
    	"movw $0x0401, %%dx\n"
    	"wrmsr\n"
    	::: "eax", "ecx", "edx", "memory"
    );

    // We want all cores to have a consistent GDT
    hal_gdtInitCore(smp_getCurrentCPU(), _ap_stack_base);
    
    // Install the IDT
    extern void hal_installIDT();
    hal_installIDT();

    // Initialize SSE
    arch_enable_sse();

    // Set current core's directory
    current_cpu->current_dir = mem_getKernelDirectory();

    // Reinitialize the APIC
    lapic_initialize(lapic_remapped);

    // Now collect information
    smp_collectAPInfo(smp_getCurrentCPU());

    // Spawn idle task
    current_cpu->idle_process = process_spawnIdleTask();

    // Allow BSP to continue
    LOG(DEBUG, "CPU%i online and ready\n", smp_getCurrentCPU());
    ap_startup_finished = 1;

    // NULL
    page_t *pg = mem_getPage(NULL, 0x0, MEM_CREATE);
    pg->bits.present = 0;

    // Switch
    process_switchNextThread();
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
    ap_startup_finished = 0;

    // Copy the bootstrap code. The AP might've messed with it.
    memcpy((void*)SMP_AP_BOOTSTRAP_PAGE, (void*)&_ap_bootstrap_start, (uintptr_t)&_ap_bootstrap_end - (uintptr_t)&_ap_bootstrap_start);

    // Allocate a stack for the AP
    if (alloc_canHasValloc()) {
        _ap_stack_base = (uintptr_t)kvalloc(PAGE_SIZE) + PAGE_SIZE;
    } else {
        _ap_stack_base = (uintptr_t)mem_sbrk(PAGE_SIZE * 2) + (PAGE_SIZE);       // !!!: Giving two pages when we're only using one 
                                                                                // !!!: Stack alignment issues - you can also use kvalloc but some allocators don't support it in Hexahedron
    }

    memset((void*)(_ap_stack_base - 4096), 0, PAGE_SIZE);

    // Send the INIT signal
    lapic_sendInit(lapic_id);
    smp_delay(5000UL);

    // Send SIPI
    lapic_sendStartup(lapic_id, SMP_AP_BOOTSTRAP_PAGE);

    // Wait for AP to finish and set the startup flag
    LOG(DEBUG, "Waiting for CPU%d to finish startup\n", lapic_id);
    do { asm volatile ("pause" ::: "memory"); } while (!ap_startup_finished);
}

/**
 * @brief Initialize the SMP system
 * @param info Collected SMP information
 * @returns 0 on success, non-zero is failure
 */
int smp_init(smp_info_t *info) {
    if (!info) return -EINVAL;
    smp_data = info;

    // Local APIC region is finite size - at least I hope.
    lapic_remapped = mem_mapMMIO((uintptr_t)smp_data->lapic_address, PAGE_SIZE);

    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    // Initialize I/O APIC first
    pic_init(PIC_TYPE_IOAPIC, (void*)info);

    // Initialize the local APIC
    int lapic = lapic_initialize(lapic_remapped);
    if (lapic != 0) {
        LOG(ERR, "Failed to initialize local APIC");
        return -EIO;
    }

    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);

    if (info->processor_count == 1 || kargs_has("--disable-smp")) goto _finish_collection;

    // The AP expects its code to be bootstrapped to a page-aligned address (SIPI expects a starting page number)
    // The remapped page for SMP is stored in the variable SMP_AP_BOOTSTRAP_PAGE
    // Assuming that page has some content in it, copy and store it.
    // !!!: This is a bit hacky as we're playing with fire here. What if PMM_BLOCK_SIZE != PAGE_SIZE?
    uintptr_t temp_frame = pmm_allocateBlock();
    uintptr_t temp_frame_remap = mem_remapPhys(temp_frame, PAGE_SIZE);

    // Temporary map back in 0x1000
    mem_mapAddress(NULL, SMP_AP_BOOTSTRAP_PAGE, SMP_AP_BOOTSTRAP_PAGE, MEM_PAGE_KERNEL);

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
    mem_unmapPhys(temp_frame_remap, PAGE_SIZE);

    // Unmap bootstrap page
    page_t *bootstrap_page = mem_getPage(NULL, SMP_AP_BOOTSTRAP_PAGE, MEM_DEFAULT);
    if (bootstrap_page) bootstrap_page->bits.present = 0;
    
    pmm_freeBlock(temp_frame);

_finish_collection:
    // Register TLB shootdown IRQ
    hal_registerInterruptHandlerRegs(124 - 32, smp_handleTLBShootdown);

    processor_count = smp_data->processor_count;
    arch_get_generic_parameters()->cpu_count = smp_getCPUCount();
    LOG(INFO, "SMP initialization completed successfully - %i CPUs available to system\n", processor_count);

    return 0;
}

/**
 * @brief Get the amount of CPUs present in the system
 */
int smp_getCPUCount() {
    return processor_count;
}

/**
 * @brief Get the current CPU's APIC ID
 */
int smp_getCurrentCPU() {
    return current_cpu->cpu_id;
}

/**
 * @brief Acknowledge core shutdown (called by ISR)
 * 
 * @bug On an NMI, we just assume it's a core shutdown - is this okay?
 */
void smp_acknowledgeCoreShutdown() {
    LOG(INFO, "CPU%i finished shutting down\n", smp_getCurrentCPU());
    ap_shutdown_finished = 1;
}

/**
 * @brief Shutdown all cores in a system
 * 
 * This causes ISR2 (NMI) to be thrown, disabling the core's interrupts and 
 * looping it on a halt instruction.
 */
void smp_disableCores() {
    if (smp_data == NULL) return;
    LOG(INFO, "Disabling cores - please wait...\n");

    for (int i = 0; i < smp_data->processor_count; i++) {
        if (smp_data->lapic_ids[i] != current_cpu->lapic_id) {
            lapic_sendNMI(smp_data->lapic_ids[i], 0); // The interrupt vector here doesnt matter as an NMI is sent regardless

            // do { asm volatile ("pause"); } while (!ap_shutdown_finished);


            ap_shutdown_finished = 0;
        }
    }
}

/**
 * @brief Perform a TLB shootdown on a specific page
 * @param address The address to perform the TLB shootdown on
 */
void smp_tlbShootdown(uintptr_t address) {
    if (!address || !smp_data) return; // no.
    if (processor_count < 2) return; // No CPUs

    spinlock_acquire(&tlb_shootdown_lock);

    // Send an IPI for the TLB shootdown vector
    // TODO: Make this vector changeable
    tlb_shootdown_address    = address;
    lapic_sendIPI(0, 124, LAPIC_ICR_DESTINATION_PHYSICAL | LAPIC_ICR_INITDEASSERT | LAPIC_ICR_EDGE | LAPIC_ICR_DESTINATION_EXCLUDE_SELF);

    spinlock_release(&tlb_shootdown_lock);
}

