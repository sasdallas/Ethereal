/**
 * @file userspace/memvisualize/memvisualize.c
 * @brief Kernel memory visualizer
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */


#include <ethereal/celestial.h>
#include <stdio.h>
#include <graphics/gfx.h>
#include <sys/utsname.h>
#include <ethereal/version.h>
#include <sys/time.h>

#define KB (1000)
#define MB (KB*1000)
#define GB (MB*1000)

gfx_context_t *ctx;
gfx_font_t *font;

void convert_memory(char *string, uintptr_t reading) {
    reading = reading * 1024; // in kB -> bytes

    // assume reading is already in KB
    if (reading >= GB) {
        sprintf(string, "%.2f GB", (double)reading/(double)GB);
    } else if (reading >= MB) {
        sprintf(string, "%.2f MB", (double)reading/(double)MB);
    } else {
        sprintf(string, "%.2f KB", (double)reading/(double)KB);
    }
}

void render_centered(char *str, int y) {
    gfx_string_size_t s; gfx_getStringSize(font, str, &s);
    int strx = (350 - s.width) / 2;
    gfx_renderString(ctx, font, str, strx, y, GFX_RGB(0,0,0));
}

void draw_progress_bar(uintptr_t used, uintptr_t total, int ty) {
    int percent = (int)(((double)used / (double)total) * 100.0);
    int width = (int)(((double)330.0 * (double)percent) / 100.0);

    gfx_drawRoundedRectangle(ctx, &GFX_RECT(9,ty,332,16), 4, GFX_RGB(0x9b,0x9b,0x9b)); // #9b9b9b
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(10,ty+1,330,14), 4, GFX_GRADIENT_VERTICAL, GFX_RGB(0xf3,0xf3,0xf3), GFX_RGB(0xd5, 0xd5, 0xd5)); // #f3f3f3  #d5d5d5
    
    if (percent) {
        gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(10,ty+1,width, 14), 4, GFX_GRADIENT_VERTICAL, GFX_RGB(0, 170, 0), GFX_RGB(0, 255, 0));
        gfx_drawRectangleFilledGradient(ctx, &GFX_RECT(width+10 - ((width > 15) ? 15 : width), ty+1, (width > 15) ? 15 : width, 14), GFX_GRADIENT_VERTICAL, GFX_RGB(0, 170, 0), GFX_RGB(0, 255, 0)); // #00aa00 #00ff00
    }
    
    ty += 13;

    char used_kbs[64];
    convert_memory(used_kbs, used);

    char total_kbs[64];
    convert_memory(total_kbs, total);

    char message[256] = { 0 };
    snprintf(message, 256, "%s / %s", used_kbs, total_kbs);

    gfx_string_size_t s; gfx_getStringSize(font, message, &s);
    int strx = (350 - s.width) / 2;
    gfx_renderString(ctx, font, message, strx, ty, GFX_RGB(0,0,0));
}


int main(int argc, char *argv[]) {
    wid_t w = celestial_createWindow(0, 350, 250);
    window_t *win = celestial_getWindow(w);
    celestial_setTitle(win, "Memory Statistics");

    // Load font we need
    font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");

    // Load sprite we need
    sprite_t *s = gfx_createSprite(0, 0);
    gfx_loadSprite(s, fopen("/usr/share/EtherealLogo.bmp", "r"));

    struct timeval tv_prog_start;
    gettimeofday(&tv_prog_start, NULL);

    ctx = celestial_getGraphicsContext(win);
    while (celestial_running()) {
        gfx_clear(ctx, GFX_RGB(0xFB, 0xFB, 0xFB));

        FILE *f = fopen("/kernel/memory", "r");
        if (!f) return 1;


        uintptr_t total_phys_memory;
        uintptr_t used_phys_memory;
        uintptr_t dma_usage;
        uintptr_t mmio_usage;

        fscanf(f, 
            "TotalPhysBlocks:%*d\n"
            "TotalPhysMemory:%zu kB\n"
            "UsedPhysMemory:%zu kB\n"
            "FreePhysMemory:%*zu kB\n"
            "KHeapPosUsage:%*zu kB\n"
            "DMAUsage:%zu kB\n"
            "MMIOUsage:%zu kB\n"
            "DriverUsage:%*zu kB\n", &total_phys_memory, &used_phys_memory, &dma_usage, &mmio_usage);

        fclose(f);

        int ty = 20;
        render_centered("Used Physical Memory", ty);
        ty += 10;
        draw_progress_bar(used_phys_memory, total_phys_memory, ty);
        ty += 40;
        render_centered("DMA", ty);
        ty += 10;
        draw_progress_bar(dma_usage, total_phys_memory, ty);
        ty += 40;
        render_centered("MMIO", ty);
        ty += 10;
        draw_progress_bar(mmio_usage, total_phys_memory, ty);
        ty += 40;

        struct timeval tvs;
        gettimeofday(&tvs, NULL);

        gfx_render(ctx);
        celestial_flip(win);

        while (1) {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            struct timeval res;
            timersub(&tv, &tvs, &res);
            if (res.tv_sec) break;
            celestial_poll();
        }
    }

    return 0;
}