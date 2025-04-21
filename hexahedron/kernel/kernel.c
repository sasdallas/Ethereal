/**
 * @file hexahedron/kernel/kernel.c
 * @brief Start of the generic parts of Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

// libpolyhedron
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

// Kernel includes
#include <kernel/kernel.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

// Loaders
#include <kernel/loader/driver.h>

// Memory
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>

// VFS
#include <kernel/fs/vfs.h>
#include <kernel/fs/tarfs.h>
#include <kernel/fs/ramdev.h>
#include <kernel/fs/null.h>
#include <kernel/fs/periphfs.h>

// Drivers
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <kernel/drivers/net/loopback.h>
#include <kernel/drivers/net/arp.h>
#include <kernel/drivers/net/ipv4.h>
#include <kernel/drivers/net/icmp.h>

// Graphics
#include <kernel/gfx/term.h>
#include <kernel/gfx/gfx.h>

// Misc.
#include <kernel/misc/ksym.h>
#include <kernel/misc/args.h>

// Tasking
#include <kernel/task/process.h>

/* Log method of generic */
#define LOG(status, ...) dprintf_module(status, "GENERIC", __VA_ARGS__)

/**
 * @brief Mount the initial ramdisk to /device/initrd/
 */
void kernel_mountRamdisk(generic_parameters_t *parameters) {
    // Find the initial ramdisk and mount it to RAM.
    fs_node_t *initrd_ram = NULL;
    generic_module_desc_t *mod = parameters->module_start;

    while (mod) {
        if (mod->cmdline && !strncmp(mod->cmdline, "type=initrd", 9)) {
            // Found it, mount the ramdev.
            initrd_ram = ramdev_mount(mod->mod_start, mod->mod_end - mod->mod_start);
            break;
        }

        mod = mod->next;
    }

    if (!initrd_ram) {
        // We didn't find it. Panic.
        LOG(ERR, "Module with type=initrd not found\n");

        kernel_panic(INITIAL_RAMDISK_CORRUPTED, "kernel");
        __builtin_unreachable();
    }

    // Now we have to mount tarfs to it.
    char devpath[64];
    snprintf(devpath, 64, "/device/%s", initrd_ram->name);
    if (vfs_mountFilesystemType("tarfs", devpath, "/device/initrd") == NULL) {
        // Oops, we couldn't mount it.
        LOG(ERR, "Failed to mount initial ramdisk (tarfs)\n");
        kernel_panic(INITIAL_RAMDISK_CORRUPTED, "kernel");

        __builtin_unreachable();
    }

    LOG(INFO, "Mounted initial ramdisk to /device/initrd\n");
    printf("Mounted initial ramdisk successfully\n");
}

/**
 * @brief Load kernel drivers
 */
void kernel_loadDrivers() {
    driver_initialize(); // Initialize the driver system

    fs_node_t *conf_file = kopen(DRIVER_DEFAULT_CONFIG_LOCATION, O_RDONLY);
    if (!conf_file) {
        kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Missing driver configuration file (%s)\n", DRIVER_DEFAULT_CONFIG_LOCATION);
        __builtin_unreachable();
    }
    
    driver_loadConfiguration(conf_file); // Load the configuration
    fs_close(conf_file);
}


struct timeval tv_start;

void kthread() {
    int iterations = 0;
    for (;;) {
        if (current_cpu->current_process->pid == 1) {
            terminal_clear(TERMINAL_DEFAULT_FG, TERMINAL_DEFAULT_BG);

            #include <kernel/drivers/clock.h>
            
            
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (!tv_start.tv_sec) tv_start = tv;

            terminal_setXY(100, 100);
            printf("==== CURRENT PROCESSES\n");
            for (unsigned i = 0; i < arch_get_generic_parameters()->cpu_count; i++) {
                if (processor_data[i].current_process && processor_data[i].current_thread) printf("\tCPU%d: Process \"%s\" (%ld ticks so far)\n", i, processor_data[i].current_process->name, processor_data[i].current_thread->total_ticks);
                else printf("\tCPU%d: No thread/no process\n", i);
            }

            extern volatile int task_switches;
            if ((tv.tv_sec - tv_start.tv_sec)) {
                printf("==== TASK SWITCHES: %d (%d switches per second)\n", task_switches, task_switches / (tv.tv_sec - tv_start.tv_sec));
            } else {
                printf("==== TASK SWITCHES: %d (give it a second, scheduler is waking up)\n", task_switches);
            }
            printf("==== WE HAVE NOT CRASHED FOR %d SECONDS\n", tv.tv_sec - tv_start.tv_sec);
        }

        arch_pause();
    }
}

/**
 * @brief Kernel main function
 */
void kmain() {
    LOG(INFO, "Reached kernel main, starting Hexahedron...\n");
    generic_parameters_t *parameters = arch_get_generic_parameters();

    // All architecture-specific stuff is done now. We need to get ready to initialize the whole system.
    // Do some sanity checks first.
    if (!parameters->module_start) {
        LOG(ERR, "No modules detected - cannot continue\n");
        kernel_panic(INITIAL_RAMDISK_CORRUPTED, "kernel");
        __builtin_unreachable();
    }

    // Now, initialize the VFS.
    vfs_init();

    // Startup the builtin filesystem drivers    
    tarfs_init();
    nulldev_init();
    zerodev_init();
    debug_mountNode();
    periphfs_init();
    video_mount();
    vfs_dump();

    // Networking
    arp_init();
    ipv4_init();
    icmp_init();

    // Setup loopback interface
    loopback_install();

    // Now we need to mount the initial ramdisk
    kernel_mountRamdisk(parameters);

    // Try to load new font file
    if (!kargs_has("--no-psf-font")) {
        fs_node_t *new_font = kopen("/device/initrd/ter-112n.psf", O_RDONLY);
        if (new_font) {
            // Load PSF
            if (!font_loadPSF(new_font)) {
                // Say hello
                gfx_drawLogo(TERMINAL_DEFAULT_FG);
                arch_say_hello(0);
            } else {
                fs_close(new_font);
                LOG(ERR, "Failed to load font file \"/device/initrd/ter-112n.psf\".\n");
            }
        } else {
            LOG(ERR, "Could not find new font file \"/device/initrd/ter-112n.psf\", using old font\n");
        }
        printf("Loaded font from initial ramdisk successfully\n");
    }

    // At this point in time if the user wants to view debugging output not on the serial console, they
    // can. Look for kernel boot argument "--debug=console"
    if (kargs_has("--debug") && !strcmp(kargs_get("--debug"), "console")) {
        debug_setOutput(terminal_print);
    }

    // Load symbols
    fs_node_t *symfile = kopen("/device/initrd/hexahedron-kernel-symmap.map", O_RDONLY);
    if (!symfile) {
        kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Missing hexahedron-kernel-symmap.map\n");
        __builtin_unreachable();
    }

    int symbols = ksym_load(symfile);
    fs_close(symfile);

    LOG(INFO, "Loaded %i symbols from symbol map\n", symbols);
    printf("Loaded kernel symbol map from initial ramdisk successfully\n");


    // Unmap 0x0 (fault detector, temporary)
    page_t *pg = mem_getPage(NULL, 0, MEM_CREATE);
    mem_allocatePage(pg, MEM_PAGE_NOT_PRESENT | MEM_PAGE_NOALLOC | MEM_PAGE_READONLY);

    // Before we load drivers, initialize the process system. This will let drivers create their own kernel threads
    process_init();
    sleep_init();

    // Load drivers
    if (!kargs_has("--no-load-drivers")) {
        kernel_loadDrivers();
        printf(COLOR_CODE_GREEN     "Successfully loaded all drivers from ramdisk\n" COLOR_CODE_RESET);
    } else {
        LOG(WARN, "Not loading any drivers, found argument \"--no-load-drivers\".\n");
        printf(COLOR_CODE_YELLOW    "Refusing to load drivers because of kernel argument \"--no-load-drivers\" - careful!\n" COLOR_CODE_RESET);
    }
    
    // Spawn idle task for this CPU
    current_cpu->idle_process = process_spawnIdleTask();

    // Spawn init task for this CPU
    current_cpu->current_process = process_spawnInit();

    // !!!: TEMPORARY

    #ifdef __ARCH_I386__
    const char *path = "/device/initrd/test_app";
    #else
    const char *path = "/device/initrd/bin/init";
    #endif

    fs_node_t *file;
    if (kargs_has("exec")) {
        file = kopen(kargs_get("exec"), O_RDONLY);
    } else {
        LOG(INFO, "Running %s as init process\n", path);
        file = kopen(path, O_RDONLY);
    }

    if (file) {
        char *argv[] = { "test_argv1", "test_argv2", NULL };
        char *envp[] = { "TEST_ENV=test1", "FOO=bar", NULL };
        process_execute(file, 2, argv, envp);
    } else {
        LOG(WARN, "Init process not found, destroying init and switching\n");
        current_cpu->current_process = NULL;
        process_switchNextThread();
    }

}