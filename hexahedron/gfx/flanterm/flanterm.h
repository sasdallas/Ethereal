/* SPDX-License-Identifier: BSD-2-Clause */

/* Copyright (C) 2022-2026 Mintsuki and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FLANTERM_H
#define FLANTERM_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLANTERM_CB_DEC 10
#define FLANTERM_CB_BELL 20
#define FLANTERM_CB_PRIVATE_ID 30
#define FLANTERM_CB_STATUS_REPORT 40
#define FLANTERM_CB_POS_REPORT 50
#define FLANTERM_CB_KBD_LEDS 60
#define FLANTERM_CB_MODE 70
#define FLANTERM_CB_LINUX 80
#define FLANTERM_CB_OSC 90

#ifdef FLANTERM_IN_FLANTERM

#include "flanterm_private.h"

#else

struct flanterm_context;

#endif

void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count);
void flanterm_flush(struct flanterm_context *ctx);
void flanterm_full_refresh(struct flanterm_context *ctx);
void flanterm_deinit(struct flanterm_context *ctx, void (*_free)(void *ptr, size_t size));

void flanterm_get_dimensions(struct flanterm_context *ctx, size_t *cols, size_t *rows);
void flanterm_set_autoflush(struct flanterm_context *ctx, bool state);
void flanterm_set_callback(struct flanterm_context *ctx, void (*callback)(struct flanterm_context *, uint64_t, uint64_t, uint64_t, uint64_t));

/**
 * Get the current cursor position.
 *
 * The returned coordinates are zero-based.
 *
 * @param ctx Flanterm context to query.
 * @param x Receives the cursor X position.
 * @param y Receives the cursor Y position.
 */
void flanterm_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y);

/**
 * Set the current cursor position.
 *
 * Coordinates are zero-based.
 *
 * @param ctx Flanterm context to update.
 * @param x New cursor X position.
 * @param y New cursor Y position.
 */
void flanterm_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y);

/**
 * Set the current text foreground colour.
 *
 * The colour index uses the terminal's base 0-7 colour range. Brightness is
 * controlled separately by the bright flag.
 *
 * @param ctx Flanterm context to update.
 * @param colour Base foreground colour index.
 * @param bright True for bright/intense output, false for normal intensity.
 */
void flanterm_set_text_fg(struct flanterm_context *ctx, size_t colour, bool bright);

/**
 * Set the current text background colour.
 *
 * The colour index uses the terminal's base 0-7 colour range. Brightness is
 * controlled separately by the bright flag.
 *
 * @param ctx Flanterm context to update.
 * @param colour Base background colour index.
 * @param bright True for bright/intense output, false for normal intensity.
 */
void flanterm_set_text_bg(struct flanterm_context *ctx, size_t colour, bool bright);

/**
 * Reset the foreground colour to the current default.
 *
 * @param ctx Flanterm context to update.
 */
void flanterm_reset_text_fg(struct flanterm_context *ctx);

/**
 * Reset the background colour to the current default.
 *
 * @param ctx Flanterm context to update.
 */
void flanterm_reset_text_bg(struct flanterm_context *ctx);

/**
 * Clear the terminal contents.
 *
 * @param ctx Flanterm context to clear.
 * @param move True to reposition the cursor as part of the clear.
 */
void flanterm_clear(struct flanterm_context *ctx, bool move);


#ifdef __cplusplus
}
#endif

#endif
