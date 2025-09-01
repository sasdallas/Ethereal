/**
 * @file userspace/tray/clock/clock.c
 * @brief Clock tray widget
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

gfx_font_t *font = NULL;

int clock_init(desktop_tray_widget_t *widget) {
    fprintf(stderr, "Clock widget coming up\n");
    font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");

    widget->width = 85;
    widget->padded_top = 0;
    widget->height = 40;
    widget->padded_left = 20;

    return 0;
}

int clock_drawIcon(desktop_tray_widget_t *widget) {
    // Update time string
    time_t current_time;
    current_time = time(NULL);
    struct tm *tm = localtime(&current_time);

    char time_str[9];
    strftime(time_str, 9, "%I:%M %p", tm);

    char date_str[15];
    strftime(date_str, 15, "%m/%d/%Y", tm);
    
    // Render the string
    gfx_renderString(widget->ctx, font, time_str, 13, 16, GFX_RGB(255, 255, 255));
    gfx_renderString(widget->ctx, font, date_str, 5, 31, GFX_RGB(255, 255, 255));
    gfx_render(widget->ctx);
    

    return 0;
}

desktop_tray_widget_data_t this_widget = {
    .name = "Clock Widget",
    .init = clock_init,
    .deinit = NULL,
    .icon = clock_drawIcon,
};