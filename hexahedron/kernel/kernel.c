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
#include <kernel/mm/vmm.h>

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
#include <kernel/fs/random.h>

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
#include <kernel/drivers/usb/usb.h>

// Graphics
#include <kernel/gfx/term.h>
#include <kernel/gfx/gfx.h>

// Misc.
#include <kernel/misc/ksym.h>
#include <kernel/misc/args.h>

// Tasking
#include <kernel/task/process.h>
#include <structs/ini.h>
#include <structs/tinf.h>

/* Log method of generic */
#define LOG(status, ...) dprintf_module(status, "GENERIC", __VA_ARGS__)

/* Set when the kernel is beginning to shutdown */
int kernel_shutdown = 0;

/* Taken from tgunzip example of tinf inflate library */
static unsigned int read_le32(const unsigned char *p)
{
	return ((unsigned int) p[0])
	     | ((unsigned int) p[1] << 8)
	     | ((unsigned int) p[2] << 16)
	     | ((unsigned int) p[3] << 24);
}

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

    // Check if this is a compressed archive
    uint8_t *gz = (uint8_t*)mod->mod_start;
    if (gz[0] == 0x1f && gz[1] == 0x8b) {
        LOG(INFO, "Initial ramdisk is packed into a .GZ file - begin decompression!\n");

        uint32_t extracted_size = read_le32(&gz[mod->mod_end - mod->mod_start - 4]);
        LOG(INFO, "Extracted file size in memory: %d\n", extracted_size);

        void *mem = kzalloc(MEM_ALIGN_PAGE(extracted_size));

        LOG(INFO, "Decompressing ramdisk...\n");
        printf("Please wait, decompressing ramdisk...\n");

        uint32_t outlen = extracted_size;
        int res = tinf_gzip_uncompress((void*)mem, &outlen, (void*)mod->mod_start, mod->mod_end - mod->mod_start);
        if (res != TINF_OK || outlen != extracted_size) {
            kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Failed to decompress the initial ramdisk (error code %d, extracted %ld bytes total)\n", res, outlen);
        }

        LOG(INFO, "Decompression finished\n");
        

        initrd_ram = ramdev_mount((uintptr_t)mem, MEM_ALIGN_PAGE(extracted_size));
    } else {
        LOG(INFO, "Ramdisk is not packed, magic is %x %x\n", gz[0], gz[1]);
    }

    // Now we have to mount tarfs to it.
    char devpath[64];
    snprintf(devpath, 64, "/device/%s", initrd_ram->name);
    if (vfs_mountFilesystemType("tarfs", devpath, "/device/initrd", NULL) != 0 || vfs_mountFilesystemType("tarfs", devpath, "/", NULL) != 0) {
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
 * @brief Dump kernel statistics to console
 */
void kernel_statistics() {
    LOG(INFO, "===== KERNEL STATISTICS\n");
    LOG(INFO, "Using %d kB of physical memory\n", pmm_getUsedBlocks() * PAGE_SIZE / 1000);
    LOG(INFO, "Kernel allocator has %d bytes in use\n", alloc_used());
}


/**
 * @brief Kernel main function
 */
void kmain() {
    LOG(INFO, "Reached kernel main, starting Hexahedron...\n");
    generic_parameters_t *parameters = arch_get_generic_parameters();

    // Now, initialize the VFS.
    vfs_init();

    // Startup the builtin filesystem drivers    
    kernelfs_init();
    tarfs_init();
    nulldev_init();
    zerodev_init();
    debug_mountNode();
    periphfs_init();
    pty_init();
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
    random_mount();
    usb_mount();

    // TEMPORARY
    vfs_mountFilesystemType("tmpfs", "tmpfs", "/tmp", NULL);
    vfs_mountFilesystemType("tmpfs", "tmpfs", "/comm", NULL);
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

    kernel_statistics();

    // All architecture-specific stuff is done now. We need to get ready to initialize the whole system.
    // Do some sanity checks first.
    if (!parameters->module_start) {
        LOG(ERR, "No modules detected - cannot continue\n");
        kernel_panic(INITIAL_RAMDISK_CORRUPTED, "kernel");
        __builtin_unreachable();
    }

    // Now we need to mount the initial ramdisk
    kernel_mountRamdisk(parameters);

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


    // Check debug arguments
extern void debug_check();
    debug_check();

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

    // Before we load drivers, initialize the process system. This will let drivers create their own kernel threads
    current_cpu->current_thread = NULL;
    current_cpu->current_process = NULL;
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

    // Alright, we are done booting, print post-boot stats
    kernel_statistics();

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
        char *envp[] = { "FOO=bar", NULL };
        process_execute(argv[0], file, argc, argv, envp);
    } else {
        LOG(WARN, "Init process not found, destroying init and switching\n");
        current_cpu->current_process = NULL;
        process_switchNextThread();
    }

}

/**
 * @brief Kernel prepare for new power state
 */
void kernel_prepareForPowerState(int state) {
    if (!(state == HAL_POWER_SHUTDOWN || state == HAL_POWER_REBOOT)) {
        return;
    }

    hal_prepareForPowerState(state);

    // Enter shutdown state
    kernel_shutdown = 1;

extern int video_ks;
    video_ks = 0;

    video_clearScreen(RGB(0,0,0));
    terminal_clear(TERMINAL_DEFAULT_FG, TERMINAL_DEFAULT_BG);

    printf(COLOR_CODE_YELLOW "System is preparing to enter power state: %s\n" COLOR_CODE_RESET, (state == HAL_POWER_SHUTDOWN) ? "SHUTDOWN" : "REBOOT");
    printf("Waiting for all processes to exit\t\t\t\t\t\t\t\t\t\t\t\t\t");

    // Exit all other processes
extern list_t *process_list;
    foreach(process, process_list) {
        if (process->value != current_cpu->current_process && !(((process_t*)process->value)->flags & PROCESS_KERNEL) && (((process_t*)process->value)->pid != 0)) {
            dprintf(INFO, "Exiting process: %s (%d)\n", ((process_t*)process->value)->name, ((process_t*)process->value)->pid);
            process_exit((process_t*)process->value, 0);
        }
    }

    printf("[" COLOR_CODE_GREEN "OK  " COLOR_CODE_RESET "]\n");

    // Deinitialize all drivers
    foreach(d, driver_list) {
        loaded_driver_t *driver = (loaded_driver_t*)d->value;

        // Print status
        char *filename = driver->filename;
        char spaces[63] = { 0 };
        for (unsigned i = 0; i < 63 - strlen(filename); i++) spaces[i] = ' ';            

        // Unload the driver
        printf("Unloading driver: %s%s", filename, spaces);
        if (driver->metadata->deinit() != DRIVER_STATUS_SUCCESS) {
            printf("   [" COLOR_CODE_RED "FAIL" COLOR_CODE_RESET "]\n");
        } else {
            printf("   [" COLOR_CODE_GREEN "OK  " COLOR_CODE_RESET "]\n");
        }
    }

    printf(COLOR_CODE_GREEN "System is ready to exit Ethereal. Bye!\n");
}