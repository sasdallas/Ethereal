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
#include FT_FREETYPE_H

/**** DEFINITIONS ****/

#define GFX_TEXT_DEFAULT_FONT_SIZE      12

/**** TYPES ****/

typedef struct gfx_font {
    FT_Face face;
} gfx_font_t;

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

#endif