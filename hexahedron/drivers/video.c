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
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <string.h>
#include <errno.h>

#include <kernel/mem/mem.h>
#include <kernel/fs/vfs.h>
#include <kernel/task/process.h>
#include <kernel/task/syscall.h>
#include <kernel/gfx/video.h>

/* List of available drivers */
static list_t *video_driver_list = NULL;

/* Current driver */
static video_driver_t *current_driver = NULL;

/* Video framebuffer. This will be passed to the driver */
uint8_t *video_framebuffer = NULL;

/* User fb */
uint8_t *user_fb = NULL;

/* Video VFS node (TEMPORARY) */
fs_node_t *video_node = NULL;

/**
 * @brief ioctl method for video node
 */
int video_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    
    size_t bufsz = (current_driver->screenWidth * 4) + (current_driver->screenHeight * current_driver->screenPitch);
    switch (request) {
        case IO_VIDEO_GET_INFO:
            // TODO: check range, possible bad behavior
            if (mem_validate((void*)argp, PTR_USER | PTR_STRICT)) {
                video_info_t info = {
                    .screen_width = current_driver->screenWidth,
                    .screen_height = current_driver->screenHeight,
                    .screen_bpp = current_driver->screenBPP,
                    .screen_pitch = current_driver->screenPitch
                };

                memcpy(argp, (void*)&info, sizeof(video_info_t));
                return 0;
            }

            return -EINVAL;

        case IO_VIDEO_SET_INFO:
            dprintf(ERR, "IO_VIDEO_SET_INFO is unimplemented\n");
            return -EINVAL;

        case IO_VIDEO_GET_FRAMEBUFFER:
            void **fbout = (void**)argp;
            if (!mem_validate(argp, PTR_USER | PTR_STRICT)) {
                dprintf(ERR, "Evil bad process provided a bad argp %p\n", argp);
                process_exit(current_cpu->current_process, 1);
            }

            for (uintptr_t i = MEM_USERMODE_DEVICE_REGION; i < MEM_USERMODE_DEVICE_REGION + bufsz; i += PAGE_SIZE) {
                page_t *pg = mem_getPage(NULL, i, MEM_CREATE);
                mem_allocatePage(pg, MEM_PAGE_WRITE_COMBINE);
            }

            user_fb = (uint8_t*)MEM_USERMODE_DEVICE_REGION;
            *fbout = (void*)MEM_USERMODE_DEVICE_REGION;
            return 0;

        case IO_VIDEO_SET_FRAMEBUFFER:
            dprintf(ERR, "IO_VIDEO_SET_FRAMEBUFFER is unimplemented\n");
            return -EINVAL;

        case IO_VIDEO_FLUSH_FRAMEBUFFER:
            if (user_fb) {
                size_t bufsz = (current_driver->screenWidth * 4) + (current_driver->screenHeight * current_driver->screenPitch);
                memcpy(video_framebuffer, user_fb, bufsz);
                video_updateScreen();
                return 0;
            }
            return 0;

        default:
            return -EINVAL;
    }
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

    // Allocate framebuffer
    if (video_framebuffer) {
        // Reallocate
        video_framebuffer = krealloc(video_framebuffer, (driver->screenWidth * 4) + (driver->screenHeight * driver->screenPitch));
    } else {
        // Allocate
        video_framebuffer = kmalloc((driver->screenWidth * 4) + (driver->screenHeight * driver->screenPitch));
    }

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
    uint8_t *buffer = video_framebuffer;
    for (uint32_t y = 0; y < current_driver->screenHeight; y++) {
        for (uint32_t x = 0; x < current_driver->screenWidth; x++) {
            buffer[x*4] = RGB_B(bg);
            buffer[x*4+1] = RGB_G(bg);
            buffer[x*4+2] = RGB_R(bg);  
        }

        buffer += current_driver->screenPitch;
    }

    // Update screen
    video_updateScreen();
}

/**
 * @brief Update the screen
 */
void video_updateScreen() {
    if (current_driver != NULL && current_driver->update) {
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
