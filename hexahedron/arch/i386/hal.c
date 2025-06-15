/**
 * @file hexahedron/arch/i386/hal.c
 * @brief Hardware abstraction layer for I386
 * 
 * This implements a hardware abstraction system for the architecture.
 * No specific calls should be made from generic. This includes interrupt handlers, port I/O, etc.
 * 
 * Generic HAL calls are implemented in kernel/hal.h
 * Architecture-specific HAL calls are implemented in kernel/arch/<architecture>/hal.h
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


/* General includes */
#include <time.h>
#include <string.h>

/* Kernel includes */
#include <kernel/arch/i386/arch.h>
#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/smp.h>
#include <kernel/arch/i386/cpu.h>
#include <kernel/hal.h>
#include <kernel/debug.h>
#include <kernel/config.h>
#include <kernel/debugger.h>
#include <kernel/mem/alloc.h>
#include <kernel/gfx/term.h>
#include <kernel/misc/args.h>
#include <kernel/arch/arch.h>

/* Generic drivers */
#include <kernel/drivers/serial.h>
#include <kernel/drivers/grubvid.h>
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <kernel/drivers/usb/usb.h>

/* Architecture drivers */
#include <kernel/drivers/x86/serial.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/drivers/x86/pit.h>
#include <kernel/drivers/x86/acpica.h> // #ifdef ACPICA_ENABLED in this file
#include <kernel/drivers/x86/minacpi.h>

/* Root system descriptor pointer */
static uint64_t hal_rsdp = 0x0; 


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

    // Get SMP information
    smp_info_t *smp = ACPICA_GetSMPInfo();
    if (!smp) {
        dprintf(WARN, "SMP is not supported on this computer\n");
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
 * @brief Stage 1 startup - initializes logging, interrupts, clock, etc.
 */
static void hal_init_stage1() {
    // Initialize serial logging.
    if (serial_initialize() == 0) {
        // Success!
        debug_setOutput(serial_print);
    }

    arch_say_hello(1);

    // Initialize the FPU
    cpu_fpuInitialize();

    // Initialize interrupts
    hal_initializeInterrupts();
    
    // Start the clock system
    clock_initialize();

    // Initialize the PIT
    pit_initialize();
}

/**
 * @brief Stage 2 startup - initializes debugger, ACPI, etc.
 */
static void hal_init_stage2() {

    /* DEBUGGER INITIALIZATION */

    // We need to reconfigure the serial ports and initialize the debugger.
    // Configure debug output port
    serial_setPort(serial_createPortData(__debug_output_com_port, __debug_output_baud_rate), 1);

    // Now start preparing for debugger
    if (!__debugger_enabled) goto _no_debug;

    serial_port_t *port = serial_initializePort(__debugger_com_port, __debugger_baud_rate);
    if (!port) {
        dprintf(WARN, "Failed to initialize COM%i for debugging\n", __debugger_com_port);
        goto _no_debug;
    }

    serial_setPort(port, 0);
    if (debugger_initialize(port) != 1) {
        dprintf(WARN, "Debugger failed to initialize or connect.\n");
    }

_no_debug: ;

    /* ACPI INITIALIZATION */

    smp_info_t *smp = hal_initACPI();
    if (!smp) goto _no_smp;

    /* SMP INITIALIZATION */

    // Now that we have our SMP information one way or another, we can initialize SMP.
    smp_init(smp);

_no_smp: ;
    
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

    /* USB INITIALIZATION */
    usb_init();

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
