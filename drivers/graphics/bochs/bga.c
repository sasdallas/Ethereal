/**
 * @file drivers/graphics/bochs/bga.c
 * @brief Driver for the Bochs graphics system (BGA)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/loader/driver.h>
#include <kernel/debug.h>
#include "bga.h"

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#else
#include <kernel/arch/x86_64/hal.h>
#endif

#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/video.h>
#include <kernel/gfx/term.h>
#include <kernel/gfx/gfx.h>
#include <kernel/arch/arch.h>
#include <string.h>


/* BGA, global scope */
uint32_t bga_device = 0x0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:BGA", __VA_ARGS__)

/**
 * @brief Write to a register in the BGA
 * @param index The index to use
 * @param value The value to write
 */
void bga_write(uint16_t index, uint16_t value) {
    outportw(VBE_DISPI_IOPORT_INDEX, index);
    outportw(VBE_DISPI_IOPORT_DATA, value);
}

/**
 * @brief Read a value from the BGA
 * @param index The index to read from
 */
uint16_t bga_read(uint16_t index) {
    outportw(VBE_DISPI_IOPORT_INDEX, index);
    return inportw(VBE_DISPI_IOPORT_DATA);
}

/**
 * @brief Scan method for BGA
 */
int bga_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    if (vendor_id == 0x1234 && device_id == 0x1111) {
        bga_device = PCI_ADDR(bus, slot, function, 0);
        return 1;
    }

    return 0;
}

/**
* @brief Update screen function
 */
void bga_update(struct _video_driver *driver, uint8_t *buffer) {
    memcpy(driver->videoBuffer, buffer, (driver->screenHeight * driver->screenPitch));
}

/**
 * @brief Map into memory function
 * @param driver The driver object
 * @param size How much of the framebuffer was requested to be mapped into memory
 * @param off The offset to start mapping at
 * @param addr The address to map at
 * @returns Error code
 */
int bga_map(struct _video_driver *driver, size_t size, off_t off, void *addr) {
    size_t bufsz = (driver->screenHeight * driver->screenPitch);
    if ((size_t)off > bufsz) return -EINVAL;
    if (off + size > bufsz) size = bufsz-off;

    // Start mapping
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        mem_mapAddress(NULL, (uintptr_t)driver->videoBufferPhys + i + off, (uintptr_t)addr + i, MEM_PAGE_WRITE_COMBINE);
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
int bga_unmap(struct _video_driver *driver, size_t size, off_t off, void *addr) {
    size_t bufsz = (driver->screenHeight * driver->screenPitch);
    if (off + size > bufsz) size = bufsz-off;

    for (uintptr_t i = (uintptr_t)addr; i < (uintptr_t)addr + size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
        if (pg) {
            pg->bits.present = 0;
            pg->bits.rw = 0;
            pg->bits.usermode = 0;
        }
    }

    return 0;
}

/**
 * @brief Driver initialization function
 */
int driver_init(int argc, char **argv) {
    // Scan for BGA device
    pci_scan(bga_scan, NULL, -1);
    if (!bga_device) {
        // No BGA device
        return 0;
    }

    LOG(INFO, "Found a Bochs graphics adapter\n");
    LOG(INFO, "Graphics adapter ID: 0x%x\n", bga_read(VBE_DISPI_INDEX_ID));

    // Get framebuffer region
    pci_bar_t *bar = pci_readBAR(PCI_BUS(bga_device), PCI_SLOT(bga_device), PCI_FUNCTION(bga_device), 0);
    if (!bar || bar->type != PCI_BAR_MEMORY32) {
        LOG(ERR, "Failed to get framebuffer region. Assuming faulty card.\n");
        return 1;   
    }

    uintptr_t fb_physical = bar->address;

    // Get capabilities
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_GETCAPS);
    uint16_t max_xres = bga_read(VBE_DISPI_INDEX_XRES);
    uint16_t max_yres = bga_read(VBE_DISPI_INDEX_YRES);
    uint16_t max_bpp = bga_read(VBE_DISPI_INDEX_BPP);
    LOG(INFO, "Maximum resolution: %dx%d @ %d BPP\n", max_xres, max_yres, max_bpp);

    // Set mode 1600x1200 @ 32 BPP
    // TODO: Potential that card doesn't support 1600x1200? In that case we'd fall back to 1200x768
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, VBE_DISPI_MAX_XRES);
    bga_write(VBE_DISPI_INDEX_YRES, VBE_DISPI_MAX_YRES);
    bga_write(VBE_DISPI_INDEX_BPP, 32);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);

    // Create our video driver object
    video_driver_t *driver = kmalloc(sizeof(video_driver_t));
    memset(driver, 0, sizeof(video_driver_t));
    strncpy(driver->name, "Bochs Graphics Adapter driver", 64);
    driver->allowsGraphics = 1;
    driver->screenBPP = 32;
    driver->screenWidth = VBE_DISPI_MAX_XRES;
    driver->screenHeight = VBE_DISPI_MAX_YRES;
    driver->screenPitch = driver->screenWidth * 4;
    driver->videoBufferPhys = (uint8_t*)fb_physical;
    
    // Register methods
    driver->update = bga_update;
    driver->map = bga_map;
    driver->unmap = bga_unmap;

    // Register it, this should unload anything using MEM_FRAMEBUFFER_REGION
    video_switchDriver(driver);

    // Map it in
    size_t fbsize = (driver->screenHeight * driver->screenPitch);
    uintptr_t region = mem_allocate(0, fbsize, MEM_ALLOC_HEAP, MEM_PAGE_KERNEL | MEM_PAGE_WRITE_COMBINE | MEM_PAGE_NOALLOC);
    for (uintptr_t phys = fb_physical, virt = region;
        phys < fb_physical + fbsize;
        phys += PAGE_SIZE, virt += PAGE_SIZE) 
    {
        mem_mapAddress(NULL, phys, virt, MEM_PAGE_KERNEL | MEM_PAGE_WRITE_COMBINE); // !!!: usermode access?
    }

driver->videoBuffer = (uint8_t*)region;

    // Reinitialize terminal
    terminal_init(TERMINAL_DEFAULT_FG, TERMINAL_DEFAULT_BG);

    // Cleanup and say hi!
    arch_say_hello(0);
    printf(COLOR_CODE_GREEN "Bochs BGA display adapter initialized\n" COLOR_CODE_RESET);
    gfx_drawLogo(RGB(255, 255, 255));

    // All done!
    return 0;
}

int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "Bochs BGA Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};

