/**
 * @file hexahedron/drivers/font.c
 * @brief Font driver (supports backup font/PSF)
 * 
 * @warning This driver is a little bit hacky and maybe too overcomplicated.
 * 
 * @see https://wiki.osdev.org/PC_Screen_Font for unicode translation
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/font.h>
#include <kernel/debug.h>
#include <kernel/misc/backup-font.h>
#include <kernel/mem/alloc.h>
#include <kernel/gfx/term.h>
#include <kernel/fs/vfs.h>
#include <string.h>
#include <limits.h>

/* Font data */
font_data_t *current_font = NULL;

/* Log */
#define LOG(status, ...) dprintf_module(status, "FONT", __VA_ARGS__)

/**
 * @brief Initializes the font driver with the backup font
 */
void font_init() {

#ifndef KERNEL_NO_BACKUP_FONT
    current_font = kmalloc(sizeof(font_data_t));
    current_font->data = NULL;
    current_font->width = BACKUP_LARGE_FONT_CELL_WIDTH;
    current_font->height = BACKUP_LARGE_FONT_CELL_HEIGHT;
    current_font->type = FONT_TYPE_BACKUP;
#else
    LOG(WARN, "No backup font compiled into kernel, font system will initialize when PSF font is loaded");
#endif
}


/**
 * @brief Put character function (backup font)
 * 
 * Implementation of a small backup font stored within the kernel. This backup font
 * doesn't have to be compiled.
 */
static void font_putCharacterBackup(int c, int _x, int _y, color_t fg, color_t bg) {
    uint8_t* fc = backup_large_font[c];

    int x = _x * current_font->width;
    int y = _y * current_font->height;

    // Is there a framebuffer?
    if (video_getFramebuffer() == NULL) {
        return;
    }

    // Is there a driver?
    video_driver_t *driver = video_getDriver();
    if (driver == NULL) {
        return;
    }

    // Accelerate!
    uint8_t *buffer = video_getFramebuffer() + (driver->screenPitch * y) + (x * 4); 

    for (uint8_t h = 0; h < font_getHeight(); h++) {
        for (uint8_t w = 0; w < font_getWidth(); w++) {
            if (fc[h] & (1 << (BACKUP_LARGE_FONT_MASK - w))) {
                // Foreground pixel
                *(uint32_t*)(buffer + (w*4)) = fg.rgb; 
            } else {
                // Background pixel
                *(uint32_t*)(buffer + (w*4)) = bg.rgb; 
            }
        }

        buffer += driver->screenPitch;
    }
}

/**
 * @brief Put character function (PC screen font)
 */
static void font_putCharacterPSF(int c, int _x, int _y, color_t fg, color_t bg) {
    // Get the header
    font_psf_t *psf = ((font_psf_t*)current_font->data);
    font_psf2_header_t *header = (font_psf2_header_t*)psf->psf_data;
    
    // Fix X and Y
    int x = _x * (current_font->width + 1);
    int y = _y * current_font->height;

    // Unicode
    // if (psf->unicode) c_actual = psf->unicode[0x0021];

    // Check how many bytes encode one row
    int bytesPerLine = (current_font->width + 7) / 8; // TODO: Calculate this outside?
    
    // Get a glyph for the character
    unsigned char *glyph = (unsigned char*)psf->psf_data + header->headersize + ((c > 0 && c < (int)header->glyphs ? c : 0) * header->glyph_bytes);

    // Get framebuffer
    video_driver_t *driver = video_getDriver();
    uint8_t *fb  = video_getFramebuffer() + (y * driver->screenPitch) + (x * sizeof(uint32_t));

    // Display the pixels according to the bitmap
    for (uint32_t h = 0; h < current_font->height; h++) {
        // Calculate a mask
        uint32_t mask = 1 << (current_font->width + 1);
        
        // Display a row
        for (uint32_t w = 0; w < current_font->width+1; w++) {
            if (*((unsigned int*)glyph) & mask) {
                // video_plotPixel(x + w, y + h, fg);
                *((uint32_t*)(fb + (w*4))) = fg.rgb;
            } else {
                // video_plotPixel(x + w, y + h, bg);
                *((uint32_t*)(fb + (w*4))) = bg.rgb;
            }
            
            // Shift mask over
            mask >>= 1;
        }

        // Update framebuffer
        fb += driver->screenPitch;

        // Adjust to next line
        glyph += bytesPerLine;
    }
}


/**
 * @brief Put a character to the screen
 * @param c The character
 * @param x X coordinate - this is expected as a coordinate relative to the terminal
 * @param y Y coordinate - this is expected as a coordinate relative to the terminal
 * @param fg The foreground color to use
 * @param bg The background color to use
 */
void font_putCharacter(int c, int x, int y, color_t fg, color_t bg) {
    if (!current_font) return;

    switch (current_font->type) {
        case FONT_TYPE_BACKUP:
            return font_putCharacterBackup(c, x, y, fg, bg);
        case FONT_TYPE_PSF:
            return font_putCharacterPSF(c, x, y, fg, bg);
        default:
            return;
    }
}

/**
 * @brief Get font width
 */
int font_getWidth() {
    if (!current_font) return 0;
    return current_font->width;
}

/**
 * @brief Get font height
 */
int font_getHeight() {
    if (!current_font) return 0;
    return current_font->height;
}


/**
 * @brief Load a PC screen font file
 * @param file The PSF file to load
 * @returns 0 on success
 */
int font_loadPSF(fs_node_t *file) {
    if (!file) return 1;

    // Look for PSF magic bytes
    uint32_t magic;
    if (fs_read(file, 0, 4, (uint8_t*)&magic) != 4) {
        // Error reading magic bytes
        return 1;
    }
    // Is it a valid PSF2 magic?
    if (magic != FONT_PSF2_MAGIC) {
        // TODO: We don't support PSF1 yet 
        return 1;
    }

    // Read the whole file into buffer
    uint8_t *buffer = kmalloc(file->length);

    // TODO: Don't use all of file length? Only read needed glyphs?
    if (fs_read(file, 0, file->length, (uint8_t*)buffer) != (ssize_t)file->length) {
        // Error reading
        kfree(buffer);
        return 1;
    }

    // Get PSF2 header
    font_psf2_header_t *psf_header = (font_psf2_header_t*)buffer;
    LOG(INFO, "Loading PSF2 font file: version %d flags 0x%x glyphs %d (%d bytes per glyph) height %08x width %08x\n", psf_header->version, psf_header->flags, psf_header->glyphs, psf_header->glyph_bytes, psf_header->height, psf_header->width);

    // Allocate font PSF structure
    font_psf_t *psf = kmalloc(sizeof(font_psf_t));
    memset(psf, 0, sizeof(font_psf_t));
    psf->psf_data = buffer;

    // // Do we need to do unicode translation?
    // if (psf_header->flags) {
    //     // Yes, translate
    //     psf->unicode = kcalloc(USHRT_MAX, 2);
        
    //     // Get the table offset
    //     uint8_t *table = (uint8_t*)(buffer +  psf_header->headersize + (psf_header->glyphs * psf_header->glyph_bytes));
    //     uint16_t glyph = 0;

    //     LOG(DEBUG, "Processing unicode table at offset 0x%x\n", table - buffer);

    //     while (table < buffer + file->length) {
    //         uint8_t sequence = *(uint8_t*)table;

    //         // Translate
    //         if (sequence == 0xFF) {
    //             glyph++;
    //             table++;
    //             continue;
    //         } else if (sequence & 128) {
    //             wchar_t uc = 0;

    //             // Convert from UTF-8 to unicode
    //             if ((sequence & 0xE0) == 0xC0) {
    //                 uc = ((table[0] & 0x1F) << 6);
    //                 uc |= (table[1] & 0x3F);
    //                 table += 2;
    //             } else if ((sequence & 0xF0) == 0xE0) {
    //                 uc = (table[0] & 0xF) << 12;
    //                 uc |= (table[1] & 0x3F) << 6;
    //                 uc |= (table[2] & 0x3F);
    //                 table += 3;
    //             } else if ((sequence & 0xF8) == 0xF0) {
    //                 uc = (table[0] & 0x7) << 18;
    //                 uc |= (table[1] & 0x3F) << 12;
    //                 uc |= (table[2] & 0x3F) << 6;
    //                 uc |= (table[3] & 0x3F);
    //                 table += 4;
    //             } else if ((sequence & 0xFC) == 0xF8) {
    //                 uc = (table[0] & 0x3) << 24;
    //                 uc |= (table[1] & 0x3F) << 18;
    //                 uc |= (table[2] & 0x3F) << 12;
    //                 uc |= (table[3] & 0x3F) << 6;
    //                 uc |= (table[4] & 0x3F);
    //                 table += 5;
    //             } else {

    //                 table++;
    //             }

    //             // Store translation
    //             psf->unicode[uc] = glyph;
    //         } else {
    //             table++;
    //         }
    //     }
    // }

    // Unload current font
    if (current_font && current_font->data) kfree(current_font->data); // NOTE: Unless we plan on supporting custom fonts in the future this should be fine.
    if (current_font) kfree(current_font);


    // Load new PSF font
    font_data_t *font_data = kmalloc(sizeof(font_data_t));
    font_data->width = psf_header->width;
    font_data->height = psf_header->height;
    font_data->data = (uint8_t*)psf;
    font_data->type = FONT_TYPE_PSF;
    current_font = font_data;

    // As we are switching fonts, we need to reinitialize the terminal
    terminal_init(TERMINAL_DEFAULT_FG, TERMINAL_DEFAULT_BG);

    // Done!
    return 0;
}