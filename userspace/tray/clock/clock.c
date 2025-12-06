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
#include <ethereal/widget.h>

gfx_font_t *font = NULL;
window_t *win = NULL;
gfx_context_t *ctx = NULL;

/* Days */
static int days[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  
};

/* Abbreviated days */
static char *days_abbrev[] = {
    "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"  
};

/* Is month leap */
#define IS_LEAP(yr) ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0)

void clock_set(desktop_tray_widget_t *widget, int visible) {
    fprintf(stderr, "Set visible: %d\n", visible);
    celestial_setWindowVisible(win, visible);
    celestial_flip(win);
}

int get_days_in_month(struct tm *tm) {
    if (tm->tm_mon == 1) {
        // February, are we in a leap year?
        if (IS_LEAP(tm->tm_year)) {
            return 29;
        }

        return 28;
    }

    return days[tm->tm_mon];
}

int get_start_dow(struct tm *tm) {
    // https://stackoverflow.com/questions/70118705/finding-the-right-algorithm-to-determine-the-first-day-of-a-given-month

    int dow = 3;
    int yr = tm->tm_year;
    while (yr-- > 1800) {
        dow += 365 + IS_LEAP(yr);
    }

    int mon = tm->tm_mon;
    while (mon--) {
        dow += days[mon];
    }
    
    return dow % 7;
}


static void cal_init(struct tm *tm) {
    // Render the top heading
    char date_str[32] = { 0 };
    strftime(date_str, 32, "%B %Y", tm);
    gfx_string_size_t s; gfx_getStringSize(font, date_str, &s);
    int strx = ((200 - s.width) / 2) + 10;
    gfx_renderString(ctx, font, date_str, strx, 40, GFX_RGB(0,0,0));

    // Calculate our target cell size.
    int cell_size = 200 / 7; // 7 days a week
    int offset = (200 - cell_size * 7) / 2 + 10; // Get the starting X offset for the separator

    int x = offset;

    // Use 10-pt font
    gfx_setFontSize(font, 11);

    for (int i = 0; i < 7; i++) {
        gfx_getStringSize(font, days_abbrev[i], &s);
        gfx_renderString(ctx, font, days_abbrev[i], x + (cell_size - s.width) / 2, 50 + 15, GFX_RGB(0, 0, 0));
        x += cell_size;
    }

    gfx_drawRectangleFilled(ctx, &GFX_RECT(offset + 5, 45 + 25, 200 - offset, 2), GFX_RGB(221, 221, 221));


    // Get the starting day of the week
    int start = get_start_dow(tm);
    int end = start +  days[tm->tm_mon];


    int day = 0;

    for (int i = 0; i < 5; i++) {
        x = offset;
        for (int j = 0; j < 7; j++) {
            int out_of_month = (day < start || day >= end);

            char d[3] = { 0 };
            if (!out_of_month) {
                snprintf(d, 3, "%2d", day-start+1);
            } else {
                d[0] = ' ';
                d[1] = ' ';
                d[2] = 0;
            }

            gfx_getStringSize(font, d, &s);

            if (tm->tm_mday == day-start+1) {
                gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + (cell_size - s.width) / 2  - 1, (55 + 25) + cell_size*i - 1, s.width + 2, s.height + 2), 4, GFX_RGB(93, 163, 236));
                gfx_drawRoundedRectangle(ctx, &GFX_RECT(x + (cell_size - s.width) / 2 , (55 + 25) + cell_size*i, s.width, s.height), 4, GFX_RGB(0xd2, 0xe6, 0xff));
            }

            gfx_renderString(ctx, font, d, x + (cell_size - s.width) / 2, (55 + 35) + cell_size*i, GFX_RGB(0, 0, 0));
            x += cell_size;
            day++;
        }
    }


    gfx_setFontSize(font, 12);
}

static void time_init(struct tm *tm) {
    // I don't feel like drawing a clock..

    char *greeting = "morning";

    if (tm->tm_hour >= 17) {
        greeting = "evening";
    } else if (tm->tm_hour >= 12) {
        greeting = "afternoon";
    }


    char text[128] = { 0 };
    snprintf(text, 128, "Good %s.", greeting);

    gfx_setFontSize(font, 16);

    gfx_renderString(ctx, font, text, 256 + 20 / 2, 40, GFX_RGB(0, 0, 0));
    gfx_setFontSize(font, 12);
    strftime(text, 128, "It's a lovely %A this %B.", tm);
    gfx_renderString(ctx, font, text, 256 + 20 / 2, 60, GFX_RGB(0, 0, 0));
}

int clock_init(desktop_tray_widget_t *widget) {
    font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");

    widget->width = 85;
    widget->padded_top = 0;
    widget->height = 40;
    widget->padded_left = 20;
    widget->data->set = clock_set;

    wid_t w = celestial_createWindowUndecorated(CELESTIAL_WINDOW_FLAG_NO_ANIMATIONS, 512, 256);
    win = celestial_getWindow(w);
    celestial_setWindowVisible(win, CELESTIAL_WINDOW_INVISIBLE);

    celestial_info_t *info = celestial_getServerInformation();

    celestial_setWindowPosition(win, info->screen_width - 512 - 10, info->screen_height - 40 - 10 - 256 );

    ctx = celestial_getGraphicsContext(win);
    
    gfx_clear(ctx, GFX_RGB(0xfb, 0xfb, 0xfb));
    gfx_drawRectangle(ctx, &GFX_RECT(0, 0, GFX_WIDTH(ctx), GFX_HEIGHT(ctx)-1), GFX_RGB(0, 0, 0));

    // Get the current time
    time_t current_time;
    current_time = time(NULL);
    struct tm *tm = localtime(&current_time);
    
    // Create the calendar
    cal_init(tm);

    gfx_drawRectangleFilled(ctx, &GFX_RECT(255, 10, 3, 256 - 20), GFX_RGB(221, 221, 221));

    // Create the clock
    time_init(tm);

    gfx_render(ctx);
    celestial_flip(win);

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