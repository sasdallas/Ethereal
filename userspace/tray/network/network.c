/**
 * @file userspace/tray/network/network.c
 * @brief Network tray icon
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/desktop.h>
#include <string.h>
#include <ethereal/celestial.h>
#include <graphics/gfx.h>

sprite_t *icon_sprite = NULL;

int network_init(desktop_tray_widget_t *widget) {
    fprintf(stderr, "Network widget coming up\n");

    icon_sprite = gfx_createSprite(0,0);
    gfx_loadSprite(icon_sprite, fopen("/usr/share/icons/24/Ethereal.bmp", "r"));
    widget->width = icon_sprite->width + 2;
    widget->height = icon_sprite->height;

    return 0;
}

int network_drawIcon(desktop_tray_widget_t *widget) {
    gfx_renderSprite(widget->ctx, icon_sprite, 1, 0);
    gfx_render(widget->ctx);

    return 0;
}

desktop_tray_widget_data_t this_widget = {
    .name = "Network Widget",
    .init = network_init,
    .icon = network_drawIcon,
};