/**
 * @file hexahedron/kernel/kernel.c
 * @brief Start of the generic parts of Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
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
#include <kernel/fs/pty.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/fs/log.h>
#include <kernel/fs/shared.h>
#include <kernel/fs/console.h>

// Drivers
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/net/loopback.h>
#include <kernel/drivers/net/arp.h>
#include <kernel/drivers/net/ipv4.h>
#include <kernel/drivers/net/icmp.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/drivers/net/udp.h>
#include <kernel/drivers/net/tcp.h>
#include <kernel/drivers/net/unix.h>
#include <kernel/drivers/sound/mixer.h>

// Graphics
#include <kernel/gfx/term.h>
#include <kernel/gfx/gfx.h>

// Misc.
#include <kernel/misc/ksym.h>
#include <kernel/misc/args.h>

// Tasking
#include <kernel/task/process.h>
#include <structs/ini.h>

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
    pty_init();
    kernelfs_init();
    tmpfs_init();
    driverfs_init();
    nic_init(); // This initializes the network kernelfs directory
    socket_init();
    video_mount();
    shared_init();
    pci_mount();
    arch_mount_kernelfs();
    console_mount();
    log_mount();

    // TEMPORARY
    vfs_mountFilesystemType("tmpfs", "tmpfs", "/tmp");
    vfs_mountFilesystemType("tmpfs", "tmpfs", "/comm");
    vfs_dump();

    // Networking
    arp_init();
    ipv4_init();
    icmp_init();
    udp_init();
    tcp_init();
    unix_init();

    // Audio
    mixer_init();

    // Setup loopback interface
    loopback_install();

    // Now we need to mount the initial ramdisk
    kernel_mountRamdisk(parameters);
    vfs_mountFilesystemType("tarfs", "/device/ram0", "/"); // And mount to root
    
    // Load the INI file
    ini_t *ini = ini_load("/device/initrd/boot/conf.ini");
    if (!ini) kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "initrd", "*** Missing /boot/conf.ini\n");

    // Try to load new font file
    if (!kargs_has("--no-psf-font")) {
        char *font_file = ini_get(ini, "boot", "kernel_font");
        if (!font_file) {
            LOG(ERR, "No entry for \"kernel_font\" in /boot/conf.ini, cannot load new font\n");
        } else {
            fs_node_t *new_font = kopen("/device/initrd/usr/share/ter-112n.psf", O_RDONLY);
            if (new_font) {
                // Load PSF
                if (!font_loadPSF(new_font)) {
                    // Say hello
                    gfx_drawLogo(TERMINAL_DEFAULT_FG);
                    arch_say_hello(0);
                    printf("Loaded font from initial ramdisk successfully\n");
                } else {
                    fs_close(new_font);
                    LOG(ERR, "Failed to load font file \"/device/initrd/usr/share/ter-112n.psf\".\n");
                }
            } else {
                LOG(ERR, "Could not find new font file \"/device/initrd/usr/share/ter-112n.psf\", using old font\n");
            }
        }
    }


    // At this point in time if the user wants to view debugging output not on the serial console, they
    // can. Look for kernel boot argument "--debug=console"
    if (kargs_has("--debug")) {
        if (!strcmp(kargs_get("--debug"), "console")) {
            debug_setOutput(terminal_print);
        } else if (!strcmp(kargs_get("--debug"), "none")) {
            debug_setOutput(NULL);
        }
    }

    // Load symbols
    char *symmap_path = ini_get(ini, "boot", "symmap");
    if (!symmap_path) {
        LOG(WARN, "Boot config file (/boot/conf.ini) does not specify symbol map, assuming default path");
        symmap_path = "/device/initrd/boot/hexahedron-kernel-symmap.map";
    }

    fs_node_t *symfile = kopen(symmap_path, O_RDONLY);
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
    sleep_init();
    process_init();

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
    const char *path = "/device/initrd/usr/bin/init";

    fs_node_t *file;
    char *argv[] = { "/device/initrd/usr/bin/init", NULL, NULL };
    int argc = 1;

    if (kargs_has("exec")) {
        char *name = kargs_get("exec");
        file = kopen(name, O_RDONLY);
        argv[0] = name;
    } else {
        LOG(INFO, "Running %s as init process\n", path);
        file = kopen(path, O_RDONLY);
    }

    if (kargs_has("initarg")) {
        argv[1] = strdup(kargs_get("initarg"));
        argc++;
    }

    if (file) {
        char *envp[] = { "TEST_ENV=test1", "FOO=bar", NULL };
        process_execute(argv[0], file, argc, argv, envp);
    } else {
        LOG(WARN, "Init process not found, destroying init and switching\n");
        current_cpu->current_process = NULL;
        process_switchNextThread();
    }

}