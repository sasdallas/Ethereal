/**
 * @file hexahedron/drivers/video.c
 * @brief Generic video driver
 * 
 * This video driver system handles abstracting the video layer away.
 * It supports text-only video drivers (but may cause gfx display issues) and it
 * supports pixel-based video drivers.
 * 
 * The video system works by drawing in a linear framebuffer and then passing it to the video driver
 * to update the screen based off this linear framebuffer.
 * 
 * Video acceleration (for font drawers) is allowed, they are allowed to manually modify the pixels of the system
 * without bothering to mess with anything else.
 * 
 * Currently, implementation is rough because we are at the beginnings of a new kernel,
 * but here is what is planned:
 * - Implement a list system to allow choosing
 * - Allow arguments to modify this
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/video.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <string.h>
#include <errno.h>

#include <kernel/mm/vmm.h>
#include <kernel/fs/vfs.h>
#include <kernel/task/process.h>
#include <kernel/task/syscall.h>
#include <kernel/gfx/video.h>
#include <kernel/misc/args.h>

/* List of available drivers */
static list_t *video_driver_list = NULL;

/* Current driver */
static video_driver_t *current_driver = NULL;

/* Video framebuffer. This will be passed to the driver */
uint8_t *video_framebuffer = NULL;

/* Video VFS node (TEMPORARY) */
fs_node_t *video_node = NULL;

/* Kill switch (this disables kernel writes to video memory and is temporary) */
int video_ks = 0;

/**
 * @brief ioctl method for video node
 */
int video_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    size_t bufsz = (current_driver->screenWidth * 4) + (current_driver->screenHeight * current_driver->screenPitch);
    switch (request) {
        case IO_VIDEO_GET_INFO:
            // TODO: check range, possible bad behavior
            SYSCALL_VALIDATE_PTR(argp);
            video_info_t info = {
                .screen_width = current_driver->screenWidth,
                .screen_height = current_driver->screenHeight,
                .screen_bpp = current_driver->screenBPP,
                .screen_pitch = current_driver->screenPitch
            };

            memcpy(argp, (void*)&info, sizeof(video_info_t));
            return 0;
            

            return -EINVAL;

        case IO_VIDEO_SET_INFO:
            dprintf(ERR, "IO_VIDEO_SET_INFO is unimplemented\n");
            return -EINVAL;

        default:
            return -EINVAL;
    }
}

/**
 * @brief mmap method for video driver
 * This just calls into the actual driver's video map method.
 */
int video_mmap(fs_node_t *node, void *addr, size_t len, off_t offset) {
    if (current_driver->map) {
        // Map it
        int r = current_driver->map(current_driver, len, offset, addr);
        if (r != 0) return r;

        // Enable device memory
        vmm_memory_range_t *range = vmm_getRange(vmm_getSpaceForAddress(addr), (uintptr_t)addr, len);
        range->vmm_flags |= VM_FLAG_DEVICE;

        // Disable kernel video writes
        video_ks = 1;
        return 0;
    } else {
        return -ENOTSUP;
    }
}

/**
 * @brief munmap method for video driver
 * This just calls into the actual driver's video unmap method.
 */
int video_munmap(fs_node_t *node, void *addr, size_t len, off_t offset) {
    size_t bufsz = (current_driver->screenHeight * current_driver->screenPitch);
    len = (len > bufsz) ? bufsz : len;

    for (uintptr_t i = (uintptr_t)addr; i < (uintptr_t)addr + len; i += PAGE_SIZE) {
        arch_mmu_unmap(NULL, i);
    }

    // Enable kernel video writes
    video_ks = 0;
    return 0;
}

/**
 * @brief Mount video node
 */
void video_mount() {
    // Create /device/fb0
    // TODO: scuffed
    video_node = kmalloc(sizeof(fs_node_t));
    memset(video_node, 0, sizeof(fs_node_t));
    strcpy(video_node->name, "fb0");
    video_node->ioctl = video_ioctl;
    video_node->flags = VFS_BLOCKDEVICE;
    video_node->mask = 0660;
    video_node->mmap = video_mmap;
    video_node->munmap = video_munmap;
    vfs_mount(video_node, "/device/fb0");
}

/**
 * @brief Initialize and prepare the video system.
 * 
 * This doesn't actually initialize any drivers, just starts the system.
 */
void video_init() {
    video_driver_list = list_create("video drivers");
}

/**
 * @brief Add a new driver
 * @param driver The driver to add
 */
void video_addDriver(video_driver_t *driver) {
    if (!driver) return;

    list_append(video_driver_list, driver);
}

/**
 * @brief Switch to a specific driver
 * @param driver The driver to switch to. If not found in the list it will be added.
 */
void video_switchDriver(video_driver_t *driver) {
    if (!driver) return;

    if (list_find(video_driver_list, driver) == NULL) {
        video_addDriver(driver);
    }

    // Framebuffer
    video_framebuffer = driver->videoBuffer;

    // Set driver
    if (current_driver && current_driver->unload) current_driver->unload(current_driver);
    current_driver = driver;
    if (current_driver->load) current_driver->load(current_driver);
}

/**
 * @brief Find a driver by name
 * @param name The name to look for
 * @returns NULL on not found, else it returns a driver
 */
video_driver_t *video_findDriver(char *name) {
    foreach(driver_node, video_driver_list) {
        if (!strcmp(((video_driver_t*)driver_node->value)->name, name)) {
            return (video_driver_t*)driver_node->value;
        }
    }

    return NULL;
}

/**
 * @brief Get the current driver
 */
video_driver_t *video_getDriver() {
    return current_driver;
}


/**** INTERFACING FUNCTIONS ****/

/**
 * @brief Plot a pixel on the screen
 * @param x The x coordinate of where to plot the pixel
 * @param y The y coordinate of where to plot the pixel
 * @param color The color to plot
 */
void video_plotPixel(int x, int y, color_t color) {
    if ((unsigned)x > current_driver->screenWidth || (unsigned)y > current_driver->screenHeight) return;
    if (video_framebuffer) {
        uintptr_t location = (uintptr_t)video_framebuffer + (x * 4 + y * current_driver->screenPitch);
        *(uint32_t*)location = color.rgb;
    }
}

/**
 * @brief Clear the screen with colors
 * @param bg The background of the screen
 */
void video_clearScreen(color_t bg) {
    uint32_t *buffer = (uint32_t*)video_framebuffer;
    for (uint32_t y = 0; y < current_driver->screenHeight; y++) {
        for (uint32_t x = 0; x < current_driver->screenWidth; x++) {
            buffer[x] = bg.rgb;
        }

        buffer += (current_driver->screenPitch/4);
    }

    // Update screen
    video_updateScreen();
}

/**
 * @brief Update the screen
 */
void video_updateScreen() {
    // This used to be for double buffered implementations
    if (video_ks) return;
    if (video_framebuffer != current_driver->videoBuffer && current_driver && current_driver->update) {
        current_driver->update(current_driver, video_framebuffer);
    }
}

/**
 * @brief Returns the current video framebuffer
 * @returns The framebuffer or NULL
 * 
 * You are allowed to draw in this just like you would a normal linear framebuffer,
 * just call @c video_updateScreen when finished
 */
uint8_t *video_getFramebuffer() {
    return video_framebuffer;
}
