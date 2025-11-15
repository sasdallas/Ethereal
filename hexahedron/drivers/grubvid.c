/**
 * @file hexahedron/drivers/grubvid.c
 * @brief Generic LFB/GRUB video driver
 * 
 * This isn't a driver model, it's just a video driver that can work with a framebuffer
 * passed by GRUB (unless its EGA, blegh.)
 * Also, this is better known as an LFB video driver, since that's what GRUB passes, but whatever.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
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
#include <kernel/mem/vas.h>
#include <kernel/processor_data.h>
#include <kernel/misc/args.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>

/* Log method */
#define LOG(status, message, ...) dprintf_module(status, "GRUBVID", message, ## __VA_ARGS__)

/**
 * @brief Update screen function
 */
void grubvid_updateScreen(struct _video_driver *driver, uint8_t *buffer) {
    memcpy(driver->videoBuffer, buffer, (driver->screenHeight * driver->screenPitch));
}

/**
 * @brief Unload function
 */
int grubvid_unload(video_driver_t *driver) {
    // Unmap
    for (uintptr_t i = (uintptr_t)driver->videoBuffer; i < (uintptr_t)driver->videoBuffer + (driver->screenHeight * driver->screenPitch);
        i += PAGE_SIZE) 
    {
        mem_allocatePage(mem_getPage(NULL, i, MEM_DEFAULT), MEM_PAGE_NOALLOC | MEM_PAGE_NOT_PRESENT);
    }

    return 0;
}

/**
 * @brief Map into memory function
 * @param driver The driver object
 * @param size How much of the framebuffer was requested to be mapped into memory
 * @param off The offset to start mapping at
 * @param addr The address to map at
 * @returns Error code
 */
int grubvid_map(video_driver_t *driver, size_t size, off_t off, void *addr) {
    size_t bufsz = (driver->screenHeight * driver->screenPitch);
    if ((size_t)off > bufsz) return -EINVAL;
    if (off + size > bufsz) size = bufsz-off;

    // Start mapping
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        arch_mmu_map(NULL,(uintptr_t)addr + i, (uintptr_t)driver->videoBufferPhys + i + off, MMU_FLAG_RW | MMU_FLAG_PRESENT | MMU_FLAG_USER | MMU_FLAG_WC);
    }

    return 0;
}

/**
 * @brief Unmap the raw framebuffer from memory
 * @param driver The driver object
 * @param size How much of the framebuffer was mapped in memory
 * @param off The offset to start mapping at
 * @param addr The address of the framebuffer memory
 * @returns 0 on success
 */
int grubvid_unmap(struct _video_driver *driver, size_t size, off_t off, void *addr) {
    size_t bufsz = (driver->screenHeight * driver->screenPitch);
    if (off + size > bufsz) size = bufsz-off;

    for (uintptr_t i = (uintptr_t)addr; i < (uintptr_t)addr + size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
        if (pg) {
            pg->bits.present = 0;
        }
    }

    return 0;
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
    driver->videoBufferPhys = (uint8_t*)(uintptr_t)parameters->framebuffer->framebuffer_addr;
    driver->allowsGraphics = 1;

    driver->map = grubvid_map;
    driver->update = grubvid_updateScreen;
    driver->unload = grubvid_unload;

    int wc = !kargs_has("--no-write-combine");

    // BEFORE WE DO ANYTHING, WE HAVE TO REMAP THE FRAMEBUFFER TO SPECIFIED ADDRESS
    size_t fbsize = (driver->screenHeight * driver->screenPitch);
    uintptr_t region = (uintptr_t)vmm_map(NULL, fbsize, VM_FLAG_DEFAULT, MMU_FLAG_WC | MMU_FLAG_RW | MMU_FLAG_PRESENT);
    for (uintptr_t phys = parameters->framebuffer->framebuffer_addr, virt = region;
            phys < parameters->framebuffer->framebuffer_addr + fbsize;
            phys += PAGE_SIZE, virt += PAGE_SIZE) 
    {
        arch_mmu_map(NULL, virt, phys, MMU_FLAG_WC | MMU_FLAG_RW | MMU_FLAG_PRESENT);
    }

    driver->videoBuffer = (uint8_t*)region;

    return driver;
}
