/**
 * @file userspace/lib/include/graphics/text.h
 * @brief Ethereal text renderer
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_TEXT_H
#define _GRAPHICS_TEXT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <ft2build.h>
#include <stdio.h>
#include FT_FREETYPE_H

/**** DEFINITIONS ****/

#define GFX_TEXT_DEFAULT_FONT_SIZE      12

#define GFX_FONT_TYPE_TTF               1
#define GFX_FONT_TYPE_PSF               2

/**** TYPES ****/

typedef struct gfx_font {
    int type;                   // Type of font
    FILE *f;                    // Font file
    uint8_t *font_data;         // Buffer of file data

    FT_Face face;               // FreeType font face
    uint8_t *psf;               // PSF data
} gfx_font_t;

typedef struct gfx_psf2_header {
    uint32_t magic;             // Magic bytes
    uint32_t version;           // Version
    uint32_t headersize;        // Offset of bitmaps in file
    uint32_t flags;             // 0 if no unicode table
    uint32_t glyphs;            // Amount of glyphs
    uint32_t glyph_bytes;       // Bytes per glyph
    uint32_t height;            // Height in pixels
    uint32_t width;             // Width in pixels
} gfx_psf2_header_t;

typedef struct gfx_string_size {
    size_t width;               // Width in pixels
    size_t height;              // Height in pixels
} gfx_string_size_t;


/**** FUNCTIONS ****/

/**
 * @brief Load a font from a file into the graphics render
 * @param ctx The context to use
 * @param filename The file to load as a font
 * @returns A handle to the new font or NULL
 */
gfx_font_t *gfx_loadFont(struct gfx_context *ctx, char *filename);

/**
 * @brief Set the font size for a specific font
 * @param font The font size to use
 * @param size The size of the font to use
 * @returns 0 on success
 */
int gfx_setFontSize(gfx_font_t *font, size_t size);

/**
 * @brief Render a character at specific coordinates
 * @param ctx The context to render with
 * @param font The font character to render
 * @param ch The character to render
 * @param x The X coordinate to render the char at
 * @param y The Y coordinate to render the char at
 * @param color The color to render them
 */
int gfx_renderCharacter(struct gfx_context *ctx, gfx_font_t *font, char ch, int _x, int _y, gfx_color_t color);

/**
 * @brief Render a string of text at specific coordinates
 * @param ctx The context to render with
 * @param font The font to render
 * @param str The string to render
 * @param x The X coordinate to render the str at
 * @param y The Y coordinate to render the str at
 * @param color The color to render with
 */
int gfx_renderString(struct gfx_context *ctx, gfx_font_t *font, char *str, int x, int y, gfx_color_t color);

/**
 * @brief Get X advance of a specific glyph
 * @param ctx The context to use
 * @param font The font to use
 * @param ch The character
 */
int gfx_getAdvanceX(struct gfx_context *ctx, gfx_font_t *font, char ch);

/**
 * @brief Destroy a font
 * @param font The font to destroy
 */
int gfx_destroyFont(gfx_font_t *font);

/**
 * @brief Get bounds for a string
 * @param font The font to use
 * @param string The string to get the bounds of
 * @returns Allocated @c gfx_string_size_t object
 */
int gfx_getStringSize(gfx_font_t *font, char *string, gfx_string_size_t *s);

#endif