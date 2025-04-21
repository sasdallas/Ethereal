/**
 * @file hexahedron/include/kernel/drivers/font.h
 * @brief Font driver header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_FONT_H
#define DRIVERS_FONT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/video.h>
#include <kernel/fs/vfs.h>

/**** TYPES ****/

/**
 * @brief Font data
*/
typedef struct font_data {
    int type;           // The type of the font
    size_t width;       // Width of the font
    size_t height;      // Height of the font
    uint8_t *data;      // Data pointer to the font (font-specific)
} font_data_t;

/**
 * @brief Stored PSF data in @c font_data->data
 */
typedef struct font_psf {
    uint8_t *psf_data;      // PSF data
    uint16_t *unicode;      // Unicode translation table
} font_psf_t;

/**
 * @brief PSF2 header
 */
typedef struct font_psf2_header {
    uint32_t magic;             // Magic bytes
    uint32_t version;           // Version
    uint32_t headersize;        // Offset of bitmaps in file
    uint32_t flags;             // 0 if no unicode table
    uint32_t glyphs;            // Amount of glyphs
    uint32_t glyph_bytes;       // Bytes per glyph
    uint32_t height;            // Height in pixels
    uint32_t width;             // Width in pixels
} font_psf2_header_t;


/**** DEFINITIONS ****/

#define FONT_TYPE_BACKUP        0       // Backup font
#define FONT_TYPE_PSF           1       // PC screen font

// PSF variables
#define FONT_PSF1_MAGIC         0x0436      // PSF1 is unsupported
#define FONT_PSF2_MAGIC         0x864ab572     

/**** FUNCTIONS ****/

/**
 * @brief Initializes the font driver with the backup font
 */
void font_init();

/**
 * @brief Put a character to the screen
 * @param c The character
 * @param x X coordinate - this is expected as a coordinate relative to the terminal
 * @param y Y coordinate - this is expected as a coordinate relative to the terminal
 * @param fg The foreground color to use
 * @param bg The background color to use
 */
void font_putCharacter(int c, int x, int y, color_t fg, color_t bg);

/**
 * @brief Get font width
 */
int font_getWidth();

/**
 * @brief Get font height
 */
int font_getHeight();

/**
 * @brief Load a PC screen font file
 * @param file The PSF file to load
 * @returns 0 on success
 */
int font_loadPSF(fs_node_t *file);

#endif