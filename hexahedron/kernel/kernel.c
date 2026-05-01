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

// klib
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

// Kernel includes
#include <kernel/kernel.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/init.h>

// Loaders
#include <kernel/loader/driver.h>

// Memory
#include <kernel/mm/vmm.h>

// VFS
#include <kernel/fs/vfs_new.h>
#include <kernel/fs/periphfs.h>
#include <kernel/fs/shared.h>

#include <kernel/fs/vfs_new.h>
#include <kernel/fs/initrd.h>

// Drivers
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <kernel/drivers/pci.h>
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
    void* ram_start = 0;
    size_t ram_size = 0;
    int found_initrd = 0;
    generic_module_desc_t *mod = parameters->module_start;

    while (mod) {
        if (mod->cmdline && !strncmp(mod->cmdline, "type=initrd", 9)) {
            // Found it
            ram_start = (void*)mod->mod_start;
            ram_size = mod->mod_end - mod->mod_start;
            found_initrd = 1;
            break;
        }

        mod = mod->next;
    }

    if (!found_initrd) {
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

        void *mem = kzalloc(PAGE_ALIGN_UP(extracted_size));

        LOG(INFO, "Decompressing ramdisk...\n");
        printf("Please wait, decompressing ramdisk...\n");

        uint32_t outlen = extracted_size;
        int res = tinf_gzip_uncompress((void*)mem, &outlen, (void*)mod->mod_start, mod->mod_end - mod->mod_start);
        if (res != TINF_OK || outlen != extracted_size) {
            kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Failed to decompress the initial ramdisk (error code %d, extracted %ld bytes total)\n", res, outlen);
        }

        LOG(INFO, "Decompression finished\n");
        
        ram_start = mem;
        ram_size = extracted_size;

        printf("\nUnpacking ramdisk...\n");
        int r = initrd_unpack(mem, extracted_size);
        if (r) {
            kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** initrd_unpack failed with %d\n", r);
        }
        
        kfree(mem);
    } else {
        LOG(INFO, "Ramdisk is not packed\n");
        printf("\nUnpacking ramdisk...\n");
        int r = initrd_unpack(ram_start, ram_size);
        if (r) {
            kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** initrd_unpack failed with %d\n", r);
        }
    
        // pmm reclaiming system will free it at KERN_LATE
    }


    LOG(INFO, "Mounted initial ramdisk to /device/initrd\n");
    printf("Mounted initial ramdisk successfully\n");
}

/**
 * @brief Load kernel drivers
 */
void kernel_loadDrivers() {
    driver_initialize(); // Initialize the driver system

    vfs_file_t *conf_file;
    int r = vfs_open(DRIVER_DEFAULT_CONFIG_LOCATION, O_RDONLY, &conf_file);

    if (r) {
        kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Error opening driver configuration file %s - code %d\n", DRIVER_DEFAULT_CONFIG_LOCATION, r);
        __builtin_unreachable();
    }
    
    driver_loadConfiguration(conf_file); // Load the configuration
    vfs_close(conf_file);
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
 * @brief Load symbols
 */
void kernel_loadSymbols(ini_t *ini) {
    char *symmap_path = ini_get(ini, "boot", "symmap");
    if (!symmap_path) {
        LOG(WARN, "Boot config file (/boot/conf.ini) does not specify symbol map, assuming default path");
        symmap_path = "/boot/hexahedron-kernel-symmap.map";
    }

    vfs_file_t *f;
    int r = vfs_open(symmap_path, O_RDONLY, &f);
    if (r != 0) {
        kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "kernel", "*** Error opening %s: %d\n", symmap_path, r);
        __builtin_unreachable();
    }

    int symbols = ksym_load(f);
    vfs_close(f);

    LOG(INFO, "Loaded %i symbols from symbol map\n", symbols);
    printf("Loaded kernel symbol map from initial ramdisk successfully\n");
}

/**
 * @brief Load font file
 */
void kernel_loadFont(ini_t *ini) {
    // Try to load new font file
    if (!kargs_has("--no-psf-font")) {
        char *font_file = ini_get(ini, "boot", "kernel_font");
        if (!font_file) {
            LOG(ERR, "No entry for \"kernel_font\" in /boot/conf.ini, cannot load new font\n");
        } else {

            vfs_file_t *new_font;
            int r = vfs_open(font_file, O_RDONLY, &new_font);

            if (new_font) {
                // Load PSF
                if (!font_loadPSF(new_font)) {
                    // Say hello
                    gfx_drawLogo(TERMINAL_DEFAULT_FG);
                    arch_say_hello(0);
                    printf("Loaded font from initial ramdisk successfully\n");
                } else {
                    LOG(ERR, "Failed to load font file \"%s\".\n", font_file);
                }
                
                vfs_close(new_font);
            } else {
                LOG(ERR, "Could not find new font file \"%s\" (error %d), using old font\n", font_file, r);
            }
        }
    }
}

/**
 * @brief Try running init process
 */
void kernel_runInit(char *path, int argc, char **argv) {
    LOG(DEBUG, "Trying to run \"%s\" as init process...\n", path);

    vfs_file_t *proc;
    int r = vfs_open(path, O_RDONLY, &proc);
    if (r) { LOG(ERR, "Error %d while running process\n", r); return; }

    char *environ[] = { NULL };

    r = process_execute(path, proc, argc, argv, environ);
    LOG(ERR, "Error %d while running process\n", r); 
}

/**
 * @brief Kernel main function
 */
void kmain() {
    LOG(INFO, "Reached kernel main, starting Hexahedron...\n");
    generic_parameters_t *parameters = arch_get_generic_parameters();
    
    // Seed random generator
    srand(time(NULL));

    // Run the early phase of init    
    INIT_RUN_PHASE(PHASE_KERN_EARLY);

    // Now, initialize the VFS.
    vfs_init();
    vfs2_init();


    // Run the system phase
    INIT_RUN_PHASE(PHASE_FS);

    LOG(DEBUG, "Mounting tmpfs on root\n");
    vfs2_filesystem_t *tmpfs = vfs_getFilesystem("tmpfs");
    assert(tmpfs);
    vfs_changeGlobalRoot(tmpfs, NULL, 0, NULL);
    LOG(DEBUG, "tmpfs has been mounted to /\n");

    // Networking
    INIT_RUN_PHASE(PHASE_NET);

    // Initialize scheduler and process system
    current_cpu->current_thread = NULL;
    current_cpu->current_process = NULL;
    process_init();

    // Run scheduler
    INIT_RUN_PHASE(PHASE_SCHED);
    
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
    ini_t *ini = ini_load("/boot/conf.ini");
    if (!ini) kernel_panic_extended(INITIAL_RAMDISK_CORRUPTED, "initrd", "*** Missing /boot/conf.ini\n");

    // Load the font
    kernel_loadFont(ini);

    // Load symbols
    kernel_loadSymbols(ini);
    ini_destroy(ini);

    // TODO: PROPER MOUNTING OF THE ROOT FILESYSTEM.
    // I'm not entirely sure how we could accomplish this.
    INIT_RUN_PHASE(PHASE_ROOTFS);

    // Load drivers
    if (!kargs_has("--no-load-drivers")) {
        kernel_loadDrivers();
        printf(COLOR_CODE_GREEN     "Successfully loaded all drivers from ramdisk\n" COLOR_CODE_RESET);
    } else {
        LOG(WARN, "Not loading any drivers, found argument \"--no-load-drivers\".\n");
        printf(COLOR_CODE_YELLOW    "Refusing to load drivers because of kernel argument \"--no-load-drivers\" - careful!\n" COLOR_CODE_RESET);
    }

    // Run drivers phase
    INIT_RUN_PHASE(PHASE_DRIVER);

    // Spawn idle task for this CPU
    current_cpu->idle_process = process_spawnIdleTask();

    // Spawn init task for this CPU
    current_cpu->current_process = process_spawnInit();

    // Run kernel late
    INIT_RUN_PHASE(PHASE_KERN_LATE);

    // Alright, we are done booting, print post-boot stats
    kernel_statistics();

    char *possible_inits[] = {
        "/sbin/init",
        "/bin/init",
        "/usr/bin/init",
        "/init"
    };

    if (kargs_has("exec")) {
        // use exec as init process
        char *path = kargs_get("exec");
        char *argv[] = { path, NULL };
        kernel_runInit(path, 1, argv);
    }

    for (unsigned i = 0; i < sizeof(possible_inits) / sizeof(const char*); i++) {
        char *argv[] = { possible_inits[i], NULL };
        kernel_runInit(possible_inits[i], 1, argv);
    } 

    LOG(ERR, "Init process not found\n");
    current_cpu->current_process = NULL;
    process_switchNextThread();
    __builtin_unreachable();
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