/**
 * @file hexahedron/arch/x86_64/hal.c
 * @brief x86_64 hardware abstraction layer
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/hal.h>

// General
#include <stdint.h>

// Kernel includes
#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/config.h>
#include <kernel/hal.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/debugger.h>
#include <kernel/gfx/term.h>
#include <kernel/misc/args.h>
#include <kernel/mm/vmm.h>

// Drivers (generic)
#include <kernel/drivers/serial.h>
#include <kernel/drivers/grubvid.h>
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/usb/usb.h>

// Drivers (x86)
#include <kernel/drivers/x86/serial.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/drivers/x86/pit.h>
#include <kernel/drivers/x86/acpica.h> // #ifdef ACPICA_ENABLED in this file
#include <kernel/drivers/x86/minacpi.h>
#include <kernel/drivers/x86/early_log.h>

static uintptr_t hal_rsdp = 0x0;
static int hal_acpica_in_use = 0;


/**
 * @brief Sets an RSDP if one was set
 */
void hal_setRSDP(uint64_t rsdp) {
    hal_rsdp = rsdp;
}

/**
 * @brief Returns a RSDP if one was found
 * 
 * You can call this method also to search for one (in EBDA/range)
 */
uint64_t hal_getRSDP() {
    if (hal_rsdp != 0x0) return hal_rsdp;
    
    // TODO: We can check EBDA/BDA but ACPICA (ACPI components) provides a method to check those for us.
    return 0x0;
}


/**
 * @brief Stage 1 startup - initializes logging, interrupts, clock, etc.
 */
static void hal_init_stage1() {
    // Initialize serial driver
    if (serial_initialize() == 0) {
        // Setup debug output
        // debug_setOutput(serial_print);
    }    

    // Fire up the early logging subsystem
    earlylog_init();

    // Say hi!
    arch_say_hello(1);

    // Initialize SSE
    arch_enable_sse();

    // Initialize clock driver
    clock_initialize();

    // Initialize the PIT
    pit_initialize();

    // Initialize interrupts
    hal_initializeInterrupts();
    dprintf(INFO, "HAL stage 1 initialization completed\n");
}


/**
 * @brief Initialize the ACPI subsystem
 */
smp_info_t *hal_initACPI() {
    #ifdef ACPICA_ENABLED
    // Initialize ACPICA
    // There are still a few bugs in ACPICA implementation that I have yet to track down.
    
    // Make sure they actually want to initialize ACPICA
    if (kargs_has("--no-acpica")) {
        dprintf(INFO, "Skipping ACPICA as --no-acpica was present\n");
        goto _minacpi; // Use minified ACPI
    }

    if (kargs_has("--no-acpi")) {
        dprintf(INFO, "Skipping ACPI initialization as --no-acpi was present\n");
        return NULL;
    }

    // Initialize ACPICA
    int init_status = ACPICA_Initialize();
    if (init_status != 0) {
        dprintf(ERR, "ACPICA failed to initialize correctly - please see log messages.\n");
        return NULL;
    }

    hal_acpica_in_use = 1;

    // Get SMP information
    smp_info_t *smp = ACPICA_GetSMPInfo();
    if (!smp) {
        dprintf(WARN, "SMP is not supported on this computer");
        return NULL;
    }

    return smp;

_minacpi: ; // Jumped here if "--no-acpica" was present

#else
    // No ACPICA, fall through 
    if (kargs_has("--no-acpi")) {
        dprintf(INFO, "Skipping ACPI initialization as --no-acpi was present\n");
        return NULL;
    }
#endif

    // Initialize the minified ACPI driver
    int minacpi = minacpi_initialize();
    if (minacpi != 0) {
        dprintf(ERR, "MINACPI failed to initialize correctly - please see log messages.\n");
        return NULL;
    }

    // Get SMP information
    smp_info_t *info = minacpi_parseMADT();
    if (info == NULL) {
        dprintf(WARN, "SMP is not supported on this computer\n");
        return NULL;
    }

    return info;
}


/**
 * @brief Stage 2 startup - initializes debugger, ACPI, etc.
 */
static void hal_init_stage2() {
    /* VIDEO INITIALIZATION */

    if (!kargs_has("--no-video")) {
        // Next, initialize video subsystem.
        video_init();

        video_driver_t *driver = grubvid_initialize(arch_get_generic_parameters());
        if (driver) {
            video_switchDriver(driver);
        }

        // Now, fonts - just do the backup one for now.
        font_init();

        // Terminal!
        int term = terminal_init(TERMINAL_DEFAULT_FG, TERMINAL_DEFAULT_BG);
        if (term != 0) {
            dprintf(WARN, "Terminal failed to initialize (return code %i)\n", term);
        }

        // Say hi again!
        arch_say_hello(0);
    } else {
        dprintf(INFO, "Argument \"--no_video\" found, disabling video.\n");
    }

    /* ACPI INITIALIZATION */

    smp_info_t *smp = hal_initACPI();

    /* SMP INITIALIZATION */

    // Collect AP info for CPU0
    smp_collectAPInfo(0);

    if (smp) {
        if (smp_init(smp)) {
            dprintf(ERR, "Failed to initialize SMP\n");
        }
    } else {
        arch_get_generic_parameters()->cpu_count = 1;
    }

    /* PCI INITIALIZATION */
    pci_init();
}


/**
 * @brief Initialize the hardware abstraction layer
 * 
 * Initializes serial output, memory systems, and much more for I386.
 *
 * @param stage What stage of HAL initialization should be performed.
 *              Specify HAL_STAGE_1 for initial startup, and specify HAL_STAGE_2 for post-memory initialization startup.
 * @todo A better driver interface is needed.
 */
void hal_init(int stage) {
    if (stage == HAL_STAGE_1) {
        hal_init_stage1();
    } else if (stage == HAL_STAGE_2) {
        hal_init_stage2();
    }
}


#ifdef ACPICA_ENABLED

/**
 * @brief HAL do ACPICA shutdown
 */
int hal_acpicaShutdown() {
    if (!hal_getACPICA()) return 1;

    AcpiEnterSleepStatePrep(ACPI_STATE_S5);
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    AcpiEnterSleepState(ACPI_STATE_S5);

    return 1;
}

/**
 * @brief HAL do ACPICA reboot
 */
int hal_acpicaReboot() {
    dprintf(ERR, "ACPICA reboot not supported\n");
    return 1;
}

/**
 * @brief HAL do ACPICA hibernate
 */
int hal_acpicaHibernate() {
    if (!hal_getACPICA()) return 1;

    AcpiEnterSleepStatePrep(ACPI_STATE_S4);
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    AcpiEnterSleepState(ACPI_STATE_S4);

    dprintf(DEBUG, "Resuming from sleep state!\n");
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);

    return 1;
}

#else

int hal_acpicaShutdown() { return 1; }
int hal_acpicaReboot() { return 1; }
int hal_acpicaHibernate() { return 1; }

#endif

/**
 * @brief Set power state
 * @param state The power state to set
 * @returns Error code
 */
int hal_setPowerState(int state) {
    if (state == HAL_POWER_SHUTDOWN) {
        if (hal_acpicaShutdown()) {
            // TODO: APM driver

            // Try emulator shutdown
            outportw(0xB004, 0x2000);   // Older versions of QEMU
            outportw(0x604, 0x2000);    // Newer versions of QEMU
            outportw(0x4004, 0x3400);   // VirtualBox
            outportw(0x600, 0x34);      // Cloud Hypervisor

            printf(WARN_COLOR_CODE "WARNING: No good way of powering the computer off" COLOR_CODE_RESET);
            asm volatile ("hlt");
        }
    } else if (state == HAL_POWER_REBOOT) {
        if (hal_acpicaReboot()) {
            // Fuck it, we ball with the IDT
            dprintf(WARN, "ACPICA reboot failure: Using backup method\n");
            uintptr_t frame = pmm_allocatePage(ZONE_DEFAULT);
            uintptr_t fr = arch_mmu_remap_physical(frame, 4096, REMAP_PERMANENT);
            memset((void*)fr, 0, 4096);

            asm volatile (
                "lidt (%0)" :: "r"(fr)
            );

            uint8_t out;
            do {
                out = inportb(0x64);
            } while (out & 0x02);

            outportb(0x64, 0xFE);
            
            printf(WARN_COLOR_CODE "WARNING: No good way of rebooting the computer" COLOR_CODE_RESET);
            asm volatile ("hlt");
        } 
    } else if (state == HAL_POWER_HIBERNATE) {
        if (hal_acpicaHibernate()) {
            return -ENOSYS;
        }
    }

    return -ENOTSUP;
}

/**
 * @brief Prepare for power state change
 * @param state The state
 */
void hal_prepareForPowerState(int state) {
    if (state == HAL_POWER_SHUTDOWN || state == HAL_POWER_REBOOT) {
        smp_disableCores();
    } 

    dprintf(ERR, "All cores disabled. Ready to reboot.\n");
}

/**
 * @brief Get whether ACPICA is in use and callable
 */
int hal_getACPICA() {
    return hal_acpica_in_use;
}

/* PORT I/O FUNCTIONS */

void io_wait() {
    outportb(0x80, 0x00);
}

void outportb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %b[Data], %w[Port]" :: [Port] "Nd" (port), [Data] "a" (data));
}

void outportw(unsigned short port, unsigned short data) {
    __asm__ __volatile__("outw %w[Data], %w[Port]" :: [Port] "Nd" (port), [Data] "a" (data));
}

void outportl(unsigned short port, unsigned long data) {
    __asm__ __volatile__("outl %k[Data], %w[Port]" :: [Port] "Nd"(port), [Data] "a" (data));
}

unsigned char inportb(unsigned short port) {
    unsigned char returnValue;
    __asm__ __volatile__("inb %w[Port]" : "=a"(returnValue) : [Port] "Nd" (port));
    return returnValue;
}

unsigned short inportw(unsigned short port) {
    unsigned short returnValue;
    __asm__ __volatile__("inw %w[Port]" : "=a"(returnValue) : [Port] "Nd" (port));
    return returnValue;
}

unsigned long inportl(unsigned short port) {
    unsigned long returnValue;
    __asm__ __volatile__("inl %w[Port]" : "=a"(returnValue) : [Port] "Nd" (port));
    return returnValue;
}