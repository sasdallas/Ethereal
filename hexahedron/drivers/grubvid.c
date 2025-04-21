/**
 * @file hexahedron/drivers/grubvid.c
 * @brief Generic LFB/GRUB video driver
 * 
 * This isn't a driver model, it's just a video driver that can work with a framebuffer
 * passed by GRUB (unless its EGA, blegh.)
 * Also, this is better known as an LFB video driver, since that's what GRUB passes, but whatever.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/grubvid.h>
#include <kernel/drivers/video.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, message, ...) dprintf_module(status, "GRUBVID", message, ## __VA_ARGS__)

/**
 * @brief Update screen function
 */
void grubvid_updateScreen(struct _video_driver *driver, uint8_t *buffer) {
    memcpy(driver->videoBuffer, buffer, (driver->screenWidth * 4) + (driver->screenHeight * driver->screenPitch));
}

/**
 * @brief Unload function
 */
void grubvid_unload(video_driver_t *driver) {
    // Unmap
    for (uintptr_t i = (uintptr_t)driver->videoBuffer; i < (uintptr_t)driver->videoBuffer + (driver->screenWidth * 4) + (driver->screenHeight * driver->screenPitch);
        i += PAGE_SIZE) 
    {
        mem_allocatePage(mem_getPage(NULL, i, MEM_DEFAULT), MEM_PAGE_NOALLOC | MEM_PAGE_NOT_PRESENT);
    }
}

/**
 * @brief Initialize the GRUB video driver
 * @param parameters Generic parameters containing the LFB driver.
 * @returns NULL on failure to initialize, else a video driver structure
 */
video_driver_t *grubvid_initialize(generic_parameters_t *parameters) {
    if (!parameters || !parameters->framebuffer) return NULL;
    if (!parameters->framebuffer->framebuffer_addr) return NULL;

    video_driver_t *driver = kmalloc(sizeof(video_driver_t));
    memset(driver, 0, sizeof(video_driver_t));

    strcpy((char*)driver->name, "GRUB Video Driver");

    driver->screenWidth = parameters->framebuffer->framebuffer_width;
    driver->screenHeight = parameters->framebuffer->framebuffer_height;
    driver->screenPitch = parameters->framebuffer->framebuffer_pitch;
    driver->screenBPP = parameters->framebuffer->framebuffer_bpp;
    driver->allowsGraphics = 1;

    driver->update = grubvid_updateScreen;
    driver->unload = grubvid_unload;

    // BEFORE WE DO ANYTHING, WE HAVE TO REMAP THE FRAMEBUFFER TO SPECIFIED ADDRESS
    size_t fbsize = (driver->screenWidth * 4) + (driver->screenHeight * driver->screenPitch);
    uintptr_t region = mem_allocate(0, fbsize, MEM_ALLOC_HEAP, MEM_PAGE_NOALLOC | MEM_PAGE_KERNEL | MEM_PAGE_WRITE_COMBINE);
    for (uintptr_t phys = parameters->framebuffer->framebuffer_addr, virt = region;
            phys < parameters->framebuffer->framebuffer_addr + fbsize;
            phys += PAGE_SIZE, virt += PAGE_SIZE) 
    {
        mem_mapAddress(NULL, phys, virt, MEM_PAGE_KERNEL | MEM_PAGE_WRITE_COMBINE); // !!!: usermode access?
    }

    driver->videoBuffer = (uint8_t*)region;

    return driver;
}
