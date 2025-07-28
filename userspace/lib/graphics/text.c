/**
 * @file userspace/lib/graphics/text.c
 * @brief FreeType text rendering functions
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <graphics/gfx.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

/* FT library */
static FT_Library __gfx_library = NULL;
static int __ft_initialized = 0;

/**
 * @brief Load a font from a file into the graphics render
 * @param ctx The context to use
 * @param filename The file to load as a font
 * @returns A handle to the new font or NULL
 */
gfx_font_t *gfx_loadFont(struct gfx_context *ctx, char *filename) {
    // TODO: Better checks..
    if (!__ft_initialized) {
        if (FT_Init_FreeType(&__gfx_library)) {
            fprintf(stderr, "graphics: FT_Init_FreeType failed\n");
            return NULL;
        }

        __ft_initialized = 1;
        fprintf(stderr, "FT initialized to %p\n", __gfx_library);
    }

    // Create a new FreeType font
    gfx_font_t *font = malloc(sizeof(gfx_font_t));
    memset(font, 0, sizeof(gfx_font_t));

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "graphics: %s: %s\n", filename, strerror(errno));
        free(font);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    font->f = f;

    // Read the file in
    uint8_t *m = malloc(fsize);
    if (fread(m, fsize, 1, f) != 1) {
        fprintf(stderr, "graphics: %s: %s\n", filename, strerror(errno));
        fclose(f);
        free(font);
        free(m);
        return NULL;
    }

    // Create a new memory face
    if (FT_New_Memory_Face(__gfx_library, (const FT_Byte*)m, fsize, 0, &font->face)) {
        fprintf(stderr, "graphics: FT_New_Face error\n");
        free(m);
        fclose(f);
        free(font);
        return NULL;
    }

    font->type = GFX_FONT_TYPE_TTF;
    gfx_setFontSize(font, GFX_TEXT_DEFAULT_FONT_SIZE);
    return font;
}

/**
 * @brief Set the font size for a specific font
 * @param font The font size to use
 * @param size The size of the font to use
 * @returns 0 on success
 */
int gfx_setFontSize(gfx_font_t *font, size_t size) {
    return FT_Set_Pixel_Sizes(font->face, 0, size);
}

/**
 * @brief FreeType alpha blend with mask
 * @param bottom The color on the bottom
 * @param top The color on the top
 * @param mask The mask to use
 */
static gfx_color_t gfx_freetypeBlend(gfx_color_t bottom, gfx_color_t top, uint8_t mask) {
    // Calculate alpha
    float a = mask / 256.0;

    // Use it to mask the rest of the valules
    uint8_t r = GFX_RGB_R(bottom) * (1.0 - a) + GFX_RGB_R(top) * a;
    uint8_t g = GFX_RGB_G(bottom) * (1.0 - a) + GFX_RGB_G(top) * a;
    uint8_t b = GFX_RGB_B(bottom) * (1.0 - a) + GFX_RGB_B(top) * a;
    return GFX_RGB(r, g, b);
}


/**
 * @brief Render a character at specific coordinates
 * @param ctx The context to render with
 * @param font The font character to render
 * @param ch The character to render
 * @param x The X coordinate to render the char at
 * @param y The Y coordinate to render the char at
 * @param color The color to render them
 */
int gfx_renderCharacter(gfx_context_t *ctx, gfx_font_t *font, char ch, int _x, int _y, gfx_color_t color) {
    // Load the glyph
    FT_UInt idx = FT_Get_Char_Index(font->face, ch);

    if (FT_Load_Glyph(font->face, idx, FT_LOAD_DEFAULT | FT_LOAD_FORCE_AUTOHINT)) return 1;
    if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL)) return 1;

    FT_GlyphSlot slot = font->face->glyph;

    int cur_y = _y - slot->bitmap_top;
    int cur_x = _x + slot->bitmap_left;

    for (int y = cur_y; y < cur_y + slot->bitmap.rows; y++) {
        for (int x = cur_x; x < cur_x + slot->bitmap.width; x++) {
            if (ctx->flags & CTX_NO_BACKBUFFER) {
                GFX_PIXEL_REAL(ctx, x, y) = gfx_freetypeBlend(GFX_PIXEL_REAL(ctx, x, y), color, slot->bitmap.buffer[(y-cur_y) * slot->bitmap.width + (x-cur_x)]);
            } else {
                GFX_PIXEL(ctx, x, y) = gfx_freetypeBlend(GFX_PIXEL(ctx, x, y), color, slot->bitmap.buffer[(y-cur_y) * slot->bitmap.width + (x-cur_x)]);
            }
        }
    }

    return 0;
}

/**
 * @brief Render a string of text at specific coordinates
 * @param ctx The context to render with
 * @param font The font to render
 * @param str The string to render
 * @param x The X coordinate to render the str at
 * @param y The Y coordinate to render the str at
 * @param color The color to render with
 */
int gfx_renderString(gfx_context_t *ctx, gfx_font_t *font, char *str, int _x, int _y, gfx_color_t color) {
    int cur_x = _x;
    int cur_y = _y;

    for (size_t i = 0; i < strlen(str); i++) {
        // Load the glyph
        FT_UInt idx = FT_Get_Char_Index(font->face, str[i]);

        if (FT_Load_Glyph(font->face, idx, FT_LOAD_DEFAULT)) return 1;
        if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL)) return 1;

        FT_GlyphSlot slot = font->face->glyph;

        int render_x = cur_x + slot->bitmap_left;
        int render_y = cur_y - slot->bitmap_top;
        for (int y = render_y; y < render_y + slot->bitmap.rows; y++) {
            for (int x = render_x; x < render_x + slot->bitmap.width; x++) {
                if (ctx->flags & CTX_NO_BACKBUFFER) {
                    GFX_PIXEL_REAL(ctx, x, y) = gfx_freetypeBlend(GFX_PIXEL_REAL(ctx, x, y), color, slot->bitmap.buffer[(y-render_y) * slot->bitmap.width + (x-render_x)]);
                } else {
                    GFX_PIXEL(ctx, x, y) = gfx_freetypeBlend(GFX_PIXEL(ctx, x, y), color, slot->bitmap.buffer[(y-render_y) * slot->bitmap.width + (x-render_x)]);
                }
            }
        }

        cur_x += slot->advance.x >> 6;
        cur_y += slot->advance.y >> 6;
    }

    return 0;
}

/**
 * @brief Get X advance of a specific glyph
 * @param ctx The context to use
 * @param font The font to use
 * @param ch The character
 */
int gfx_getAdvanceX(gfx_context_t *ctx, gfx_font_t *font, char ch) {
    FT_UInt idx = FT_Get_Char_Index(font->face, ch);
    
    if (FT_Load_Glyph(font->face, idx, FT_LOAD_DEFAULT)) return 0;
    if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL)) return 0;

    FT_GlyphSlot slot = font->face->glyph;

    return (slot->advance.x >> 6);
}