/**
 * @file hexahedron/drivers/grubvidv2.c
 * @brief Generic LFB/GRUB framebuffer driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/grubvid.h>
#include <kernel/mm/vmm.h>
#include <string.h>

/**
 * @brief grubvid destroy
 */
static void grubvid_destroy(fb_device_t *driver) {
    uintptr_t offset = (uintptr_t)driver->info.mmio_start & (PAGE_SIZE - 1);
    vmm_unmap(driver->screen_base - offset, driver->info.mmio_length + offset);
    kfree(driver);
}

/**
 * @brief Initialize the GRUB framebuffer driver
 * @param parameters Generic parameters
 * @returns 0 on success
 */
int grubvid_initialize(generic_parameters_t *parameters) {
    if (!parameters->framebuffer || !parameters->framebuffer->framebuffer_addr) {
        return -1;
    }

    generic_fb_desc_t *framebuffer = parameters->framebuffer;
    size_t fbsize = (size_t)framebuffer->framebuffer_height * framebuffer->framebuffer_pitch;

    // Allocate a new driver
    fb_device_t *driver = kmalloc(sizeof(fb_device_t));
    memset(driver, 0, sizeof(fb_device_t));
    SPINLOCK_INIT(&driver->fb_lock);

    // Prepare driver information
    strcpy(driver->info.name, "GRUB Video");
    driver->info.xres = framebuffer->framebuffer_width;
    driver->info.yres = framebuffer->framebuffer_height;
    driver->info.xres_virtual = framebuffer->framebuffer_width;
    driver->info.yres_virtual = framebuffer->framebuffer_height;
    driver->info.bpp = framebuffer->framebuffer_bpp;
    driver->info.pitch = framebuffer->framebuffer_pitch;
    driver->info.mmio_length = fbsize;
    driver->info.mmio_start = (char*)(uintptr_t)framebuffer->framebuffer_addr;

    driver->ops.fb_mmap = NULL;
    driver->ops.fb_munmap = NULL;
    driver->ops.fb_set = NULL;
    driver->ops.fb_validate = NULL;
    driver->ops.fb_destroy = grubvid_destroy;

    // Map the physical framebuffer into kernel memory
    uintptr_t offset = framebuffer->framebuffer_addr & (PAGE_SIZE - 1);
    uintptr_t phys_base = framebuffer->framebuffer_addr - offset;
    uintptr_t region = (uintptr_t)vmm_map(NULL, fbsize + offset, VM_FLAG_DEVICE, MMU_FLAG_WC | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    for (size_t i = 0; i < fbsize + offset; i += PAGE_SIZE) {
        arch_mmu_map(NULL, region + i, phys_base + i, MMU_FLAG_WC | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
    }

    driver->screen_base = (char*)(region + offset);
    fb_register(driver);

    return 0;
}
