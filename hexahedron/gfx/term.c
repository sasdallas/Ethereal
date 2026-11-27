/**
 * @file hexahedron/gfx/term.c
 * @brief Flanterm terminal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/gfx/term.h>
#include "flanterm/flanterm.h"
#include "flanterm/fb.h"
#include <kernel/drivers/fb.h>
#include <kernel/panic.h>

struct flanterm_context *ft_ctx = NULL;

/**
 * @brief Initialize the terminal
 */
void terminal_init() {
    if (ft_ctx != NULL) return;

    fb_device_t *driver = fb_getDefaultDevice();
    if (!driver || !driver->screen_base || driver->info.bpp != 32) {
        // todo we can totally support bpp != 32
        dprintf(ERR, "Terminal is not supported here!\n");
        return;
    }

    ft_ctx = flanterm_fb_init(
        NULL,NULL,
        (uint32_t*)driver->screen_base,
        driver->info.xres,
        driver->info.yres,
        driver->info.pitch,
        8,16,8,8,8,0,
        NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, 8, 16, 1, 0, 0, 0,
        0
    );
}

/**
 * @brief Clear terminal screen
 * @param fg The foreground of the terminal
 * @param bg The background of the terminal
 */
void terminal_clear(color_t fg, color_t bg) {
    return;
}

/**
 * @brief Put character method
 */
int terminal_putchar(int ch) {
    return terminal_print(NULL, (char)ch);
}

/**
 * @brief Put character method (printf-conforming)
 */
int terminal_print(void *user, char c) {
    if (!ft_ctx || (fb_is_disabled && !kernel_in_panic_state)) {
        return 0;
    }

    flanterm_write(ft_ctx, &c, 1);
    if (c == '\n') {
        c = '\r';
        flanterm_write(ft_ctx, &c, 1);
    }
    return 0;
}
