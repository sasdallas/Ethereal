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

#ifdef __cplusplus
#error "Please do not compile Flanterm as C++ code! Flanterm should be compiled as C99 or newer."
#endif

#ifndef __STDC_VERSION__
#error "Flanterm must be compiled as C99 or newer."
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef FLANTERM_IN_FLANTERM
#define FLANTERM_IN_FLANTERM
#endif

#include "flanterm.h"

// Tries to implement this standard for terminfo
// https://man7.org/linux/man-pages/man4/console_codes.4.html

static const uint32_t col256[] = {
    0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
    0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af,
    0x0087d7, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
    0x00d700, 0x00d75f, 0x00d787, 0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,
    0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
    0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,
    0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
    0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,
    0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,
    0x875f87, 0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af,
    0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff,
    0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
    0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,
    0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
    0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,
    0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
    0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
    0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
    0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,
    0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
    0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,
    0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
    0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,
    0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
    0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787, 0xffd7af,
    0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
    0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,
    0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
    0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee
};

#define CHARSET_DEFAULT 0
#define CHARSET_DEC_SPECIAL 1

void flanterm_context_reinit(struct flanterm_context *ctx) {
    ctx->tab_size = 8;
    ctx->autoflush = true;
    ctx->cursor_enabled = true;
    ctx->scroll_enabled = true;
    ctx->wrap_enabled = true;
    ctx->origin_mode = false;
    ctx->control_sequence = false;
    ctx->escape = false;
    ctx->osc = false;
    ctx->osc_escape = false;
    ctx->rrr = false;
    ctx->discard_next = false;
    ctx->bold = false;
    ctx->bg_bold = false;
    ctx->reverse_video = false;
    ctx->dec_private = false;
    ctx->insert_mode = false;
    ctx->csi_unhandled = false;
    ctx->unicode_remaining = 0;
    ctx->g_select = 0;
    ctx->charsets[0] = CHARSET_DEFAULT;
    ctx->charsets[1] = CHARSET_DEC_SPECIAL;
    ctx->current_charset = 0;
    ctx->escape_offset = 0;
    ctx->esc_values_i = 0;
    ctx->saved_cursor_x = 0;
    ctx->saved_cursor_y = 0;
    ctx->current_primary = (size_t)-1;
    ctx->current_bg = (size_t)-1;
    ctx->saved_state_charsets[0] = CHARSET_DEFAULT;
    ctx->saved_state_charsets[1] = CHARSET_DEC_SPECIAL;
    ctx->saved_state_wrap_enabled = true;
    ctx->saved_state_current_primary = (size_t)-1;
    ctx->saved_state_current_bg = (size_t)-1;
    ctx->saved_state_scroll_top_margin = 0;
    ctx->saved_state_scroll_bottom_margin = ctx->rows;
    ctx->last_printed_char = ' ';
    ctx->last_was_graphic = false;
    ctx->scroll_top_margin = 0;
    ctx->scroll_bottom_margin = ctx->rows;
}

static void flanterm_putchar(struct flanterm_context *ctx, uint8_t c);

void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
        flanterm_putchar(ctx, buf[i]);
    }

    if (ctx->autoflush) {
        ctx->double_buffer_flush(ctx);
    }
}

static void sgr(struct flanterm_context *ctx) {
    size_t i = 0;

    if (!ctx->esc_values_i)
        goto def;

    for (; i < ctx->esc_values_i; i++) {
        size_t offset;

        if (ctx->esc_values[i] == 0) {
def:
            if (ctx->reverse_video) {
                ctx->reverse_video = false;
                ctx->swap_palette(ctx);
            }
            ctx->bold = false;
            ctx->bg_bold = false;
            ctx->current_primary = (size_t)-1;
            ctx->current_bg = (size_t)-1;
            ctx->set_text_bg_default(ctx);
            ctx->set_text_fg_default(ctx);
            continue;
        }

        else if (ctx->esc_values[i] == 1) {
            ctx->bold = true;
            if (ctx->current_primary == (size_t)-2) {
                // RGB/256-color; bold does not alter the colour
            } else if (ctx->current_primary != (size_t)-1) {
                if (!ctx->reverse_video) {
                    ctx->set_text_fg_bright(ctx, ctx->current_primary);
                } else {
                    ctx->set_text_bg_bright(ctx, ctx->current_primary);
                }
            } else {
                if (!ctx->reverse_video) {
                    ctx->set_text_fg_default_bright(ctx);
                } else {
                    ctx->set_text_bg_default_bright(ctx);
                }
            }
            continue;
        }

        else if (ctx->esc_values[i] == 2
              || ctx->esc_values[i] == 3
              || ctx->esc_values[i] == 4
              || ctx->esc_values[i] == 8) {
            continue;
        }

        else if (ctx->esc_values[i] == 5) {
            ctx->bg_bold = true;
            if (ctx->current_bg == (size_t)-2) {
                // RGB/256-color; bold does not alter the colour
            } else if (ctx->current_bg != (size_t)-1) {
                if (!ctx->reverse_video) {
                    ctx->set_text_bg_bright(ctx, ctx->current_bg);
                } else {
                    ctx->set_text_fg_bright(ctx, ctx->current_bg);
                }
            } else {
                if (!ctx->reverse_video) {
                    ctx->set_text_bg_default_bright(ctx);
                } else {
                    ctx->set_text_fg_default_bright(ctx);
                }
            }
            continue;
        }

        else if (ctx->esc_values[i] == 22) {
            ctx->bold = false;
            if (ctx->current_primary == (size_t)-2) {
                // RGB/256-color; unbold does not alter the colour
            } else if (ctx->current_primary != (size_t)-1) {
                if (!ctx->reverse_video) {
                    ctx->set_text_fg(ctx, ctx->current_primary);
                } else {
                    ctx->set_text_bg(ctx, ctx->current_primary);
                }
            } else {
                if (!ctx->reverse_video) {
                    ctx->set_text_fg_default(ctx);
                } else {
                    ctx->set_text_bg_default(ctx);
                }
            }
            continue;
        }

        else if (ctx->esc_values[i] == 23
              || ctx->esc_values[i] == 24
              || ctx->esc_values[i] == 28) {
            continue;
        }

        else if (ctx->esc_values[i] == 25) {
            ctx->bg_bold = false;
            if (ctx->current_bg == (size_t)-2) {
                // RGB/256-color; unbold does not alter the colour
            } else if (ctx->current_bg != (size_t)-1) {
                if (!ctx->reverse_video) {
                    ctx->set_text_bg(ctx, ctx->current_bg);
                } else {
                    ctx->set_text_fg(ctx, ctx->current_bg);
                }
            } else {
                if (!ctx->reverse_video) {
                    ctx->set_text_bg_default(ctx);
                } else {
                    ctx->set_text_fg_default(ctx);
                }
            }
            continue;
        }

        else if (ctx->esc_values[i] >= 30 && ctx->esc_values[i] <= 37) {
            offset = 30;
            ctx->current_primary = ctx->esc_values[i] - offset;

            if (ctx->reverse_video) {
                goto set_bg;
            }

set_fg:
            if ((ctx->bold && !ctx->reverse_video)
             || (ctx->bg_bold && ctx->reverse_video)) {
                ctx->set_text_fg_bright(ctx, ctx->esc_values[i] - offset);
            } else {
                ctx->set_text_fg(ctx, ctx->esc_values[i] - offset);
            }
            continue;
        }

        else if (ctx->esc_values[i] >= 40 && ctx->esc_values[i] <= 47) {
            offset = 40;
            ctx->current_bg = ctx->esc_values[i] - offset;

            if (ctx->reverse_video) {
                goto set_fg;
            }

set_bg:
            if ((ctx->bold && ctx->reverse_video)
             || (ctx->bg_bold && !ctx->reverse_video)) {
                ctx->set_text_bg_bright(ctx, ctx->esc_values[i] - offset);
            } else {
                ctx->set_text_bg(ctx, ctx->esc_values[i] - offset);
            }
            continue;
        }

        else if (ctx->esc_values[i] >= 90 && ctx->esc_values[i] <= 97) {
            offset = 90;
            ctx->current_primary = ctx->esc_values[i] - offset;

            if (ctx->reverse_video) {
                goto set_bg_bright;
            }

set_fg_bright:
            ctx->set_text_fg_bright(ctx, ctx->esc_values[i] - offset);
            continue;
        }

        else if (ctx->esc_values[i] >= 100 && ctx->esc_values[i] <= 107) {
            offset = 100;
            ctx->current_bg = ctx->esc_values[i] - offset;

            if (ctx->reverse_video) {
                goto set_fg_bright;
            }

set_bg_bright:
            ctx->set_text_bg_bright(ctx, ctx->esc_values[i] - offset);
            continue;
        }

        else if (ctx->esc_values[i] == 39) {
            ctx->current_primary = (size_t)-1;

            if (ctx->reverse_video) {
                ctx->swap_palette(ctx);
            }

            if (!ctx->bold) {
                ctx->set_text_fg_default(ctx);
            } else {
                ctx->set_text_fg_default_bright(ctx);
            }

            if (ctx->reverse_video) {
                ctx->swap_palette(ctx);
            }

            continue;
        }

        else if (ctx->esc_values[i] == 49) {
            ctx->current_bg = (size_t)-1;

            if (ctx->reverse_video) {
                ctx->swap_palette(ctx);
            }

            if (!ctx->bg_bold) {
                ctx->set_text_bg_default(ctx);
            } else {
                ctx->set_text_bg_default_bright(ctx);
            }

            if (ctx->reverse_video) {
                ctx->swap_palette(ctx);
            }

            continue;
        }

        else if (ctx->esc_values[i] == 7) {
            if (!ctx->reverse_video) {
                ctx->reverse_video = true;
                ctx->swap_palette(ctx);
            }
            continue;
        }

        else if (ctx->esc_values[i] == 27) {
            if (ctx->reverse_video) {
                ctx->reverse_video = false;
                ctx->swap_palette(ctx);
            }
            continue;
        }

        // 256/RGB
        else if (ctx->esc_values[i] == 38 || ctx->esc_values[i] == 48) {
            bool fg = ctx->esc_values[i] == 38;
            bool render_fg = ctx->reverse_video ? !fg : fg;

            i++;
            if (i >= ctx->esc_values_i) {
                break;
            }

            switch (ctx->esc_values[i]) {
                case 2: { // RGB
                    if (i + 3 >= ctx->esc_values_i) {
                        goto out;
                    }

                    uint32_t rgb_value = 0;

                    rgb_value |= (ctx->esc_values[i + 1] & 0xff) << 16;
                    rgb_value |= (ctx->esc_values[i + 2] & 0xff) << 8;
                    rgb_value |= (ctx->esc_values[i + 3] & 0xff);

                    i += 3;

                    if (fg) {
                        ctx->current_primary = (size_t)-2;
                    } else {
                        ctx->current_bg = (size_t)-2;
                    }

                    (render_fg ? ctx->set_text_fg_rgb : ctx->set_text_bg_rgb)(ctx, rgb_value);

                    break;
                }
                case 5: { // 256 colors
                    if (i + 1 >= ctx->esc_values_i) {
                        goto out;
                    }

                    uint32_t col = ctx->esc_values[i + 1];

                    i++;

                    if (col < 8) {
                        if (fg) {
                            ctx->current_primary = (size_t)-2;
                        } else {
                            ctx->current_bg = (size_t)-2;
                        }
                        (render_fg ? ctx->set_text_fg : ctx->set_text_bg)(ctx, col);
                    } else if (col < 16) {
                        if (fg) {
                            ctx->current_primary = (size_t)-2;
                        } else {
                            ctx->current_bg = (size_t)-2;
                        }
                        (render_fg ? ctx->set_text_fg_bright : ctx->set_text_bg_bright)(ctx, col - 8);
                    } else if (col < 256) {
                        if (fg) {
                            ctx->current_primary = (size_t)-2;
                        } else {
                            ctx->current_bg = (size_t)-2;
                        }
                        uint32_t rgb_value = col256[col - 16];
                        (render_fg ? ctx->set_text_fg_rgb : ctx->set_text_bg_rgb)(ctx, rgb_value);
                    }

                    break;
                }
                default: continue;
            }
        }
    }

out:;
}

static void save_state(struct flanterm_context *ctx);
static void restore_state(struct flanterm_context *ctx);

static void dec_private_parse(struct flanterm_context *ctx, uint8_t c) {
    ctx->dec_private = false;

    if (ctx->esc_values_i == 0) {
        return;
    }

    bool set;

    switch (c) {
        case 'h':
            set = true; break;
        case 'l':
            set = false; break;
        default:
            return;
    }

    for (size_t i = 0; i < ctx->esc_values_i; i++) {
        switch (ctx->esc_values[i]) {
            case 6:
                ctx->origin_mode = set;
                ctx->set_cursor_pos(ctx, 0, set ? ctx->scroll_top_margin : 0);
                break;
            case 7:
                ctx->wrap_enabled = set;
                break;
            case 25:
                ctx->cursor_enabled = set;
                break;
            case 1049:
                if (set) {
                    save_state(ctx);
                    ctx->scroll_top_margin = 0;
                    ctx->scroll_bottom_margin = ctx->rows;
                    ctx->clear(ctx, true);
                } else {
                    ctx->clear(ctx, true);
                    restore_state(ctx);
                }
                break;
        }
    }

    if (ctx->callback != NULL) {
        ctx->callback(ctx, FLANTERM_CB_DEC, ctx->esc_values_i, (uintptr_t)ctx->esc_values, c);
    }
}

static void linux_private_parse(struct flanterm_context *ctx) {
    if (ctx->esc_values_i == 0) {
        return;
    }

    if (ctx->callback != NULL) {
        ctx->callback(ctx, FLANTERM_CB_LINUX, ctx->esc_values_i, (uintptr_t)ctx->esc_values, 0);
    }
}

static void mode_toggle(struct flanterm_context *ctx, uint8_t c) {
    if (ctx->esc_values_i == 0) {
        return;
    }

    bool set;

    switch (c) {
        case 'h':
            set = true; break;
        case 'l':
            set = false; break;
        default:
            return;
    }

    switch (ctx->esc_values[0]) {
        case 4:
            ctx->insert_mode = set; return;
    }

    if (ctx->callback != NULL) {
        ctx->callback(ctx, FLANTERM_CB_MODE, ctx->esc_values_i, (uintptr_t)ctx->esc_values, c);
    }
}

static void osc_finalize(struct flanterm_context *ctx) {
    if (ctx->callback != NULL) {
        // Parse the leading OSC number and skip past the semicolon.
        uint64_t osc_num = 0;
        size_t i = 0;
        while (i < ctx->osc_buf_i && ctx->osc_buf[i] >= '0' && ctx->osc_buf[i] <= '9') {
            osc_num = osc_num * 10 + (ctx->osc_buf[i] - '0');
            i++;
        }
        if (i < ctx->osc_buf_i && ctx->osc_buf[i] == ';') {
            i++;
        }
        ctx->callback(ctx, FLANTERM_CB_OSC, osc_num, ctx->osc_buf_i - i, (uintptr_t)&ctx->osc_buf[i]);
    }
}

static bool osc_parse(struct flanterm_context *ctx, uint8_t c) {
    // ESC \ terminates an OSC sequence cleanly
    // but if ESC is followed by non-\, report failure from osc_parse and
    // try parsing the character as another escape code
    if (ctx->osc_escape) {
        if (c == '\\') {
            osc_finalize(ctx);
            ctx->osc = false;
            ctx->osc_escape = false;
            ctx->escape = false;
            return true;
        } else {
            ctx->osc_escape = false;
            ctx->osc = false;
            // escape stays true here
            return false;
        }
    }
    switch (c) {
        case 0x1b:
            ctx->osc_escape = true;
            break;
        // BEL is the other terminator
        case '\a':
            osc_finalize(ctx);
            ctx->osc_escape = false;
            ctx->osc = false;
            ctx->escape = false;
            break;
        default:
            if (ctx->osc_buf_i < sizeof(ctx->osc_buf)) {
                ctx->osc_buf[ctx->osc_buf_i++] = c;
            }
            break;
    }
    return true;
}

static bool execute_c0(struct flanterm_context *ctx, uint8_t c) {
    size_t x, y;
    switch (c) {
        case '\a':
            if (ctx->callback != NULL) {
                ctx->callback(ctx, FLANTERM_CB_BELL, 0, 0, 0);
            }
            return true;
        case '\b':
            ctx->get_cursor_pos(ctx, &x, &y);
            if (x > 0) {
                ctx->set_cursor_pos(ctx, x - 1, y);
            }
            return true;
        case '\t':
            ctx->get_cursor_pos(ctx, &x, &y);
            x = (x / ctx->tab_size + 1) * ctx->tab_size;
            if (x >= ctx->cols) {
                x = ctx->cols - 1;
            }
            ctx->set_cursor_pos(ctx, x, y);
            return true;
        case 0x0b:
        case 0x0c:
        case '\n':
            ctx->get_cursor_pos(ctx, &x, &y);
            if (y == ctx->scroll_bottom_margin - 1) {
                ctx->scroll(ctx);
                ctx->set_cursor_pos(ctx, x, y);
            } else if (y < ctx->rows - 1) {
                ctx->set_cursor_pos(ctx, x, y + 1);
            }
            return true;
        case '\r':
            ctx->get_cursor_pos(ctx, &x, &y);
            ctx->set_cursor_pos(ctx, 0, y);
            return true;
        case 14:
            ctx->current_charset = 1;
            return true;
        case 15:
            ctx->current_charset = 0;
            return true;
        default:
            return false;
    }
}

static void control_sequence_parse(struct flanterm_context *ctx, uint8_t c) {
    if (ctx->escape_offset == 2) {
        switch (c) {
            case '[':
                ctx->discard_next = true;
                goto cleanup;
            case '?':
                ctx->dec_private = true;
                return;
        }
    }

    if (c < 0x20 && c != 0x1b) {
        execute_c0(ctx, c);
        return;
    }

    if (c >= '0' && c <= '9') {
        if (ctx->esc_values_i == FLANTERM_MAX_ESC_VALUES) {
            return;
        }
        ctx->rrr = true;
        if (ctx->esc_values[ctx->esc_values_i] > UINT32_MAX / 10) {
            return;
        }
        ctx->esc_values[ctx->esc_values_i] *= 10;
        uint32_t digit = c - '0';
        if (ctx->esc_values[ctx->esc_values_i] > UINT32_MAX - digit) {
            ctx->esc_values[ctx->esc_values_i] = UINT32_MAX;
            return;
        }
        ctx->esc_values[ctx->esc_values_i] += digit;
        return;
    }

    if (ctx->rrr == true) {
        ctx->esc_values_i++;
        ctx->rrr = false;
        if (c == ';')
            return;
    } else if (c == ';') {
        if (ctx->esc_values_i == FLANTERM_MAX_ESC_VALUES) {
            return;
        }
        ctx->esc_values[ctx->esc_values_i] = 0;
        ctx->esc_values_i++;
        return;
    }

    size_t esc_default;
    switch (c) {
        case 'J': case 'K': case 'q': case 'm': case 'c': case ']':
            esc_default = 0; break;
        default:
            esc_default = 1; break;
    }

    for (size_t i = ctx->esc_values_i; i < FLANTERM_MAX_ESC_VALUES; i++) {
        ctx->esc_values[i] = esc_default;
    }

    if (esc_default != 0) {
        for (size_t i = 0; i < ctx->esc_values_i; i++) {
            if (ctx->esc_values[i] == 0) {
                ctx->esc_values[i] = esc_default;
            }
        }
    }

    if (ctx->dec_private == true) {
        dec_private_parse(ctx, c);
        goto cleanup;
    }

    // CSI sequences are terminated by a byte in [0x40,0x7E]
    // so skip all bytes until the terminator byte
    if (ctx->csi_unhandled) {
        if (c == 0x1B) {
            ctx->csi_unhandled = false;
            ctx->control_sequence = false;
            ctx->escape_offset = 0;
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            ctx->csi_unhandled = false;
            goto cleanup;
        }
        return;
    }

    bool r = ctx->scroll_enabled;
    ctx->scroll_enabled = false;
    bool saved_wrap = ctx->wrap_enabled;
    ctx->wrap_enabled = true;
    size_t x, y;
    ctx->get_cursor_pos(ctx, &x, &y);

    switch (c) {
        // ESC aborts the current CSI and starts a new escape sequence
        case 0x1B:
            ctx->scroll_enabled = r;
            ctx->wrap_enabled = saved_wrap;
            ctx->control_sequence = false;
            ctx->escape_offset = 0;
            return;
        case 'F':
            x = 0;
            // FALLTHRU
        case 'A': {
            if (ctx->esc_values[0] > y)
                ctx->esc_values[0] = y;
            size_t dest_y = y - ctx->esc_values[0];
            size_t min_y = ctx->origin_mode ? ctx->scroll_top_margin : 0;
            if (dest_y < min_y) {
                dest_y = min_y;
            }
            ctx->set_cursor_pos(ctx, x, dest_y);
            break;
        }
        case 'E':
            x = 0;
            // FALLTHRU
        case 'e':
        case 'B': {
            if (y + ctx->esc_values[0] > ctx->rows - 1)
                ctx->esc_values[0] = (ctx->rows - 1) - y;
            size_t dest_y = y + ctx->esc_values[0];
            size_t max_y = ctx->origin_mode ? ctx->scroll_bottom_margin : ctx->rows;
            if (dest_y >= max_y) {
                dest_y = max_y - 1;
            }
            ctx->set_cursor_pos(ctx, x, dest_y);
            break;
        }
        case 'a':
        case 'C':
            if (x + ctx->esc_values[0] > ctx->cols - 1)
                ctx->esc_values[0] = (ctx->cols - 1) - x;
            ctx->set_cursor_pos(ctx, x + ctx->esc_values[0], y);
            break;
        case 'D':
            if (ctx->esc_values[0] > x)
                ctx->esc_values[0] = x;
            ctx->set_cursor_pos(ctx, x - ctx->esc_values[0], y);
            break;
        case 'c':
            if (ctx->callback != NULL) {
                ctx->callback(ctx, FLANTERM_CB_PRIVATE_ID, 0, 0, 0);
            }
            break;
        case 'd': {
            if (ctx->esc_values[0] != 0) {
                ctx->esc_values[0]--;
            }
            size_t max_row = ctx->rows;
            size_t row_offset = 0;
            if (ctx->origin_mode) {
                max_row = ctx->scroll_bottom_margin - ctx->scroll_top_margin;
                row_offset = ctx->scroll_top_margin;
            }
            if (ctx->esc_values[0] >= max_row)
                ctx->esc_values[0] = max_row - 1;
            ctx->set_cursor_pos(ctx, x, ctx->esc_values[0] + row_offset);
            break;
        }
        case 'G':
        case '`':
            if (ctx->esc_values[0] != 0) {
                ctx->esc_values[0]--;
            }
            if (ctx->esc_values[0] >= ctx->cols)
                ctx->esc_values[0] = ctx->cols - 1;
            ctx->set_cursor_pos(ctx, ctx->esc_values[0], y);
            break;
        case 'H':
        case 'f': {
            if (ctx->esc_values[0] != 0) {
                ctx->esc_values[0]--;
            }
            if (ctx->esc_values[1] != 0) {
                ctx->esc_values[1]--;
            }
            size_t max_row = ctx->rows;
            size_t row_offset = 0;
            if (ctx->origin_mode) {
                max_row = ctx->scroll_bottom_margin - ctx->scroll_top_margin;
                row_offset = ctx->scroll_top_margin;
            }
            if (ctx->esc_values[1] >= ctx->cols) {
                ctx->esc_values[1] = ctx->cols - 1;
            }
            if (ctx->esc_values[0] >= max_row) {
                ctx->esc_values[0] = max_row - 1;
            }
            ctx->set_cursor_pos(ctx, ctx->esc_values[1], ctx->esc_values[0] + row_offset);
            break;
        }
        case 'M': {
            if (y < ctx->scroll_top_margin || y >= ctx->scroll_bottom_margin) {
                break;
            }
            size_t old_scroll_top_margin = ctx->scroll_top_margin;
            ctx->scroll_top_margin = y;
            size_t max_count = ctx->scroll_bottom_margin - y;
            size_t count = ctx->esc_values[0] > max_count ? max_count : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++) {
                ctx->scroll(ctx);
            }
            ctx->scroll_top_margin = old_scroll_top_margin;
            break;
        }
        case 'L': {
            if (y < ctx->scroll_top_margin || y >= ctx->scroll_bottom_margin) {
                break;
            }
            size_t old_scroll_top_margin = ctx->scroll_top_margin;
            ctx->scroll_top_margin = y;
            size_t max_count = ctx->scroll_bottom_margin - y;
            size_t count = ctx->esc_values[0] > max_count ? max_count : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++) {
                ctx->revscroll(ctx);
            }
            ctx->scroll_top_margin = old_scroll_top_margin;
            break;
        }
        case 'n':
            switch (ctx->esc_values[0]) {
                case 5:
                    if (ctx->callback != NULL) {
                        ctx->callback(ctx, FLANTERM_CB_STATUS_REPORT, 0, 0, 0);
                    }
                    break;
                case 6:
                    if (ctx->callback != NULL) {
                        size_t report_y = ctx->origin_mode && y >= ctx->scroll_top_margin
                            ? y - ctx->scroll_top_margin : y;
                        ctx->callback(ctx, FLANTERM_CB_POS_REPORT, x + 1, report_y + 1, 0);
                    }
                    break;
            }
            break;
        case 'q':
            if (ctx->callback != NULL) {
                ctx->callback(ctx, FLANTERM_CB_KBD_LEDS, ctx->esc_values[0], 0, 0);
            }
            break;
        case 'J':
            switch (ctx->esc_values[0]) {
                case 0: {
                    // Erase from cursor to end: clear rest of current line,
                    // then clear full lines below, using explicit cursor
                    // positioning to avoid scroll region wrapping limits.
                    ctx->set_cursor_pos(ctx, x, y);
                    for (size_t xc = x; xc < ctx->cols; xc++) {
                        ctx->raw_putchar(ctx, ' ');
                    }
                    for (size_t yc = y + 1; yc < ctx->rows; yc++) {
                        ctx->set_cursor_pos(ctx, 0, yc);
                        for (size_t xc = 0; xc < ctx->cols; xc++) {
                            ctx->raw_putchar(ctx, ' ');
                        }
                    }
                    ctx->set_cursor_pos(ctx, x, y);
                    break;
                }
                case 1: {
                    // Erase from start to cursor: clear full lines above,
                    // then clear current line up to and including cursor.
                    for (size_t yc = 0; yc < y; yc++) {
                        ctx->set_cursor_pos(ctx, 0, yc);
                        for (size_t xc = 0; xc < ctx->cols; xc++) {
                            ctx->raw_putchar(ctx, ' ');
                        }
                    }
                    ctx->set_cursor_pos(ctx, 0, y);
                    for (size_t xc = 0; xc <= x; xc++) {
                        ctx->raw_putchar(ctx, ' ');
                    }
                    ctx->set_cursor_pos(ctx, x, y);
                    break;
                }
                case 2:
                case 3:
                    ctx->clear(ctx, false);
                    break;
            }
            break;
        case '@': {
            size_t n = ctx->esc_values[0];
            if (n == 0) {
                break;
            }
            if (n > ctx->cols - x) {
                n = ctx->cols - x;
            }
            for (size_t i = ctx->cols - 1; i >= x + n; i--) {
                ctx->move_character(ctx, i, y, i - n, y);
            }
            ctx->set_cursor_pos(ctx, x, y);
            for (size_t i = 0; i < n; i++) {
                ctx->raw_putchar(ctx, ' ');
            }
            ctx->set_cursor_pos(ctx, x, y);
            break;
        }
        case 'P':
            if (ctx->esc_values[0] > ctx->cols - x)
                ctx->esc_values[0] = ctx->cols - x;
            for (size_t i = x + ctx->esc_values[0]; i < ctx->cols; i++)
                ctx->move_character(ctx, i - ctx->esc_values[0], y, i, y);
            ctx->set_cursor_pos(ctx, ctx->cols - ctx->esc_values[0], y);
            // FALLTHRU
        case 'X': {
            size_t cx, cy;
            ctx->get_cursor_pos(ctx, &cx, &cy);
            ctx->set_cursor_pos(ctx, cx, cy);
            size_t remaining = ctx->cols - cx;
            size_t count = ctx->esc_values[0] > remaining ? remaining : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++)
                ctx->raw_putchar(ctx, ' ');
            ctx->set_cursor_pos(ctx, x, y);
            break;
        }
        case 'm':
            sgr(ctx);
            break;
        case 's':
            ctx->get_cursor_pos(ctx, &ctx->saved_cursor_x, &ctx->saved_cursor_y);
            break;
        case 'u':
            ctx->set_cursor_pos(ctx, ctx->saved_cursor_x, ctx->saved_cursor_y);
            break;
        case 'K':
            switch (ctx->esc_values[0]) {
                case 0: {
                    ctx->set_cursor_pos(ctx, x, y);
                    for (size_t i = x; i < ctx->cols; i++)
                        ctx->raw_putchar(ctx, ' ');
                    ctx->set_cursor_pos(ctx, x, y);
                    break;
                }
                case 1: {
                    ctx->set_cursor_pos(ctx, 0, y);
                    for (size_t i = 0; i <= x; i++)
                        ctx->raw_putchar(ctx, ' ');
                    ctx->set_cursor_pos(ctx, x, y);
                    break;
                }
                case 2: {
                    ctx->set_cursor_pos(ctx, 0, y);
                    for (size_t i = 0; i < ctx->cols; i++)
                        ctx->raw_putchar(ctx, ' ');
                    ctx->set_cursor_pos(ctx, x, y);
                    break;
                }
            }
            break;
        case 'r':
            ctx->scroll_top_margin = 0;
            ctx->scroll_bottom_margin = ctx->rows;
            if (ctx->esc_values_i > 0) {
                ctx->scroll_top_margin = ctx->esc_values[0] - 1;
            }
            if (ctx->esc_values_i > 1) {
                ctx->scroll_bottom_margin = ctx->esc_values[1];
            }
            if (ctx->scroll_top_margin >= ctx->rows
             || ctx->scroll_bottom_margin > ctx->rows
             || ctx->scroll_top_margin >= (ctx->scroll_bottom_margin - 1)) {
                ctx->scroll_top_margin = 0;
                ctx->scroll_bottom_margin = ctx->rows;
            }
            ctx->set_cursor_pos(ctx, 0, ctx->origin_mode ? ctx->scroll_top_margin : 0);
            break;
        case 'l':
        case 'h':
            mode_toggle(ctx, c);
            break;
        case 'S': {
            size_t region = ctx->scroll_bottom_margin - ctx->scroll_top_margin;
            size_t count = ctx->esc_values[0] > region ? region : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++) {
                ctx->scroll(ctx);
            }
            break;
        }
        case 'T': {
            size_t region = ctx->scroll_bottom_margin - ctx->scroll_top_margin;
            size_t count = ctx->esc_values[0] > region ? region : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++) {
                ctx->revscroll(ctx);
            }
            break;
        }
        case 'b': {
            if (!ctx->last_was_graphic) {
                break;
            }
            ctx->scroll_enabled = r;
            ctx->wrap_enabled = saved_wrap;
            size_t count = ctx->esc_values[0] > 65535 ? 65535 : ctx->esc_values[0];
            for (size_t i = 0; i < count; i++) {
                if (ctx->insert_mode == true) {
                    size_t ix, iy;
                    ctx->get_cursor_pos(ctx, &ix, &iy);
                    for (size_t j = ctx->cols - 1; j > ix; j--) {
                        ctx->move_character(ctx, j, iy, j - 1, iy);
                    }
                }
                ctx->raw_putchar(ctx, ctx->last_printed_char);
            }
            break;
        }
        case ']':
            linux_private_parse(ctx);
            break;
        default:
            if (c >= 0x40 && c <= 0x7E) {
                break;
            }
            ctx->scroll_enabled = r;
            ctx->wrap_enabled = saved_wrap;
            ctx->csi_unhandled = true;
            return;
    }

    ctx->scroll_enabled = r;
    ctx->wrap_enabled = saved_wrap;

cleanup:
    ctx->control_sequence = false;
    ctx->escape = false;
}

static void restore_state(struct flanterm_context *ctx) {
    ctx->bold = ctx->saved_state_bold;
    ctx->bg_bold = ctx->saved_state_bg_bold;
    ctx->reverse_video = ctx->saved_state_reverse_video;
    ctx->origin_mode = ctx->saved_state_origin_mode;
    ctx->wrap_enabled = ctx->saved_state_wrap_enabled;
    ctx->current_charset = ctx->saved_state_current_charset;
    ctx->charsets[0] = ctx->saved_state_charsets[0];
    ctx->charsets[1] = ctx->saved_state_charsets[1];
    ctx->current_primary = ctx->saved_state_current_primary;
    ctx->current_bg = ctx->saved_state_current_bg;
    ctx->scroll_top_margin = ctx->saved_state_scroll_top_margin;
    ctx->scroll_bottom_margin = ctx->saved_state_scroll_bottom_margin;

    ctx->restore_state(ctx);
}

static void save_state(struct flanterm_context *ctx) {
    ctx->save_state(ctx);

    ctx->saved_state_bold = ctx->bold;
    ctx->saved_state_bg_bold = ctx->bg_bold;
    ctx->saved_state_reverse_video = ctx->reverse_video;
    ctx->saved_state_origin_mode = ctx->origin_mode;
    ctx->saved_state_wrap_enabled = ctx->wrap_enabled;
    ctx->saved_state_current_charset = ctx->current_charset;
    ctx->saved_state_charsets[0] = ctx->charsets[0];
    ctx->saved_state_charsets[1] = ctx->charsets[1];
    ctx->saved_state_current_primary = ctx->current_primary;
    ctx->saved_state_current_bg = ctx->current_bg;
    ctx->saved_state_scroll_top_margin = ctx->scroll_top_margin;
    ctx->saved_state_scroll_bottom_margin = ctx->scroll_bottom_margin;
}

static void escape_parse(struct flanterm_context *ctx, uint8_t c) {
    ctx->escape_offset++;

    if (ctx->osc == true) {
        // ESC \ is one of the two possible terminators of OSC sequences,
        // so osc_parse consumes ESC.
        // If it is then followed by \ it cleans correctly,
        // otherwise it returns false, and it tries parsing it as another escape sequence
        if (osc_parse(ctx, c)) {
            return;
        }
        // OSC aborted by ESC + non-backslash; reset offset for new sequence
        ctx->escape_offset = 1;
    }

    if (ctx->control_sequence == true) {
        control_sequence_parse(ctx, c);
        return;
    }

    size_t x, y;
    ctx->get_cursor_pos(ctx, &x, &y);

    switch (c) {
        case 0x1b:
            ctx->escape_offset = 0;
            return;
        case ']':
            ctx->osc_escape = false;
            ctx->osc = true;
            ctx->osc_buf_i = 0;
            return;
        case '[':
            for (size_t i = 0; i < FLANTERM_MAX_ESC_VALUES; i++)
                ctx->esc_values[i] = 0;
            ctx->esc_values_i = 0;
            ctx->rrr = false;
            ctx->csi_unhandled = false;
            ctx->control_sequence = true;
            return;
        case '7':
            save_state(ctx);
            break;
        case '8':
            restore_state(ctx);
            break;
        case 'c':
            if (ctx->reverse_video) {
                ctx->swap_palette(ctx);
            }
            flanterm_context_reinit(ctx);
            ctx->set_text_bg_default(ctx);
            ctx->set_text_fg_default(ctx);
            ctx->clear(ctx, true);
            save_state(ctx);
            break;
        case 'D':
            if (y == ctx->scroll_bottom_margin - 1) {
                ctx->scroll(ctx);
                ctx->set_cursor_pos(ctx, x, y);
            } else if (y < ctx->rows - 1) {
                ctx->set_cursor_pos(ctx, x, y + 1);
            }
            break;
        case 'E':
            if (y == ctx->scroll_bottom_margin - 1) {
                ctx->scroll(ctx);
                ctx->set_cursor_pos(ctx, 0, y);
            } else if (y < ctx->rows - 1) {
                ctx->set_cursor_pos(ctx, 0, y + 1);
            } else {
                ctx->set_cursor_pos(ctx, 0, y);
            }
            break;
        case 'M':
            // "Reverse linefeed"
            if (y == ctx->scroll_top_margin) {
                ctx->revscroll(ctx);
                ctx->set_cursor_pos(ctx, x, y);
            } else if (y > 0) {
                ctx->set_cursor_pos(ctx, x, y - 1);
            }
            break;
        case 'Z':
            if (ctx->callback != NULL) {
                ctx->callback(ctx, FLANTERM_CB_PRIVATE_ID, 0, 0, 0);
            }
            break;
        case '(':
        case ')':
            ctx->g_select = c - '\'';
            break;
    }

    ctx->escape = false;
}

static bool dec_special_print(struct flanterm_context *ctx, uint8_t c) {
#define FLANTERM_DEC_SPCL_PRN(C) ctx->last_printed_char = (C); ctx->last_was_graphic = true; ctx->raw_putchar(ctx, (C)); return true;
    switch (c) {
        case '`': FLANTERM_DEC_SPCL_PRN(0x04)
        case '0': FLANTERM_DEC_SPCL_PRN(0xdb)
        case '-': FLANTERM_DEC_SPCL_PRN(0x18)
        case ',': FLANTERM_DEC_SPCL_PRN(0x1b)
        case '.': FLANTERM_DEC_SPCL_PRN(0x19)
        case 'a': FLANTERM_DEC_SPCL_PRN(0xb1)
        case 'f': FLANTERM_DEC_SPCL_PRN(0xf8)
        case 'g': FLANTERM_DEC_SPCL_PRN(0xf1)
        case 'h': FLANTERM_DEC_SPCL_PRN(0xb0)
        case 'j': FLANTERM_DEC_SPCL_PRN(0xd9)
        case 'k': FLANTERM_DEC_SPCL_PRN(0xbf)
        case 'l': FLANTERM_DEC_SPCL_PRN(0xda)
        case 'm': FLANTERM_DEC_SPCL_PRN(0xc0)
        case 'n': FLANTERM_DEC_SPCL_PRN(0xc5)
        case 'q': FLANTERM_DEC_SPCL_PRN(0xc4)
        case 's': FLANTERM_DEC_SPCL_PRN(0x5f)
        case 't': FLANTERM_DEC_SPCL_PRN(0xc3)
        case 'u': FLANTERM_DEC_SPCL_PRN(0xb4)
        case 'v': FLANTERM_DEC_SPCL_PRN(0xc1)
        case 'w': FLANTERM_DEC_SPCL_PRN(0xc2)
        case 'x': FLANTERM_DEC_SPCL_PRN(0xb3)
        case 'y': FLANTERM_DEC_SPCL_PRN(0xf3)
        case 'z': FLANTERM_DEC_SPCL_PRN(0xf2)
        case '~': FLANTERM_DEC_SPCL_PRN(0xfa)
        case '_': FLANTERM_DEC_SPCL_PRN(0xff)
        case '+': FLANTERM_DEC_SPCL_PRN(0x1a)
        case '{': FLANTERM_DEC_SPCL_PRN(0xe3)
        case '}': FLANTERM_DEC_SPCL_PRN(0x9c)
    }
#undef FLANTERM_DEC_SPCL_PRN

    return false;
}

// Following wcwidth related code inherited from:
// https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c

struct interval {
    uint32_t first;
    uint32_t last;
};

/* auxiliary function for binary search in interval table */
static int bisearch(uint32_t ucs, const struct interval *table, int max) {
  int min = 0;
  int mid;

  if (ucs < table[0].first || ucs > table[max].last)
    return 0;
  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > table[mid].last)
      min = mid + 1;
    else if (ucs < table[mid].first)
      max = mid - 1;
    else
      return 1;
  }

  return 0;
}

static int mk_wcwidth(uint32_t ucs) {
  /* sorted list of non-overlapping intervals of zero-width characters */
  /* Unicode 17.0.0 */
  static const struct interval combining[] = {
    { 0x0300, 0x036F }, { 0x0483, 0x0489 }, { 0x0591, 0x05BD },
    { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 }, { 0x05C4, 0x05C5 },
    { 0x05C7, 0x05C7 }, { 0x0610, 0x061A }, { 0x061C, 0x061C },
    { 0x064B, 0x065F }, { 0x0670, 0x0670 }, { 0x06D6, 0x06DC },
    { 0x06DF, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x0711, 0x0711 }, { 0x0730, 0x074A }, { 0x07A6, 0x07B0 },
    { 0x07EB, 0x07F3 }, { 0x07FD, 0x07FD }, { 0x0816, 0x0819 },
    { 0x081B, 0x0823 }, { 0x0825, 0x0827 }, { 0x0829, 0x082D },
    { 0x0859, 0x085B }, { 0x0897, 0x089F }, { 0x08CA, 0x08E1 },
    { 0x08E3, 0x0902 }, { 0x093A, 0x093A }, { 0x093C, 0x093C },
    { 0x0941, 0x0948 }, { 0x094D, 0x094D }, { 0x0951, 0x0957 },
    { 0x0962, 0x0963 }, { 0x0981, 0x0981 }, { 0x09BC, 0x09BC },
    { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 },
    { 0x09FE, 0x09FE }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
    { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
    { 0x0A51, 0x0A51 }, { 0x0A70, 0x0A71 }, { 0x0A75, 0x0A75 },
    { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 },
    { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD }, { 0x0AE2, 0x0AE3 },
    { 0x0AFA, 0x0AFF }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
    { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B44 }, { 0x0B4D, 0x0B4D },
    { 0x0B55, 0x0B56 }, { 0x0B62, 0x0B63 }, { 0x0B82, 0x0B82 },
    { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C00, 0x0C00 },
    { 0x0C04, 0x0C04 }, { 0x0C3C, 0x0C3C }, { 0x0C3E, 0x0C40 },
    { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
    { 0x0C62, 0x0C63 }, { 0x0C81, 0x0C81 }, { 0x0CBC, 0x0CBC },
    { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
    { 0x0CE2, 0x0CE3 }, { 0x0D00, 0x0D01 }, { 0x0D3B, 0x0D3C },
    { 0x0D41, 0x0D44 }, { 0x0D4D, 0x0D4D }, { 0x0D62, 0x0D63 },
    { 0x0D81, 0x0D81 }, { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 },
    { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A },
    { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EBC },
    { 0x0EC8, 0x0ECE }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
    { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
    { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F8D, 0x0F97 },
    { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
    { 0x1032, 0x1037 }, { 0x1039, 0x103A }, { 0x103D, 0x103E },
    { 0x1058, 0x1059 }, { 0x105E, 0x1060 }, { 0x1071, 0x1074 },
    { 0x1082, 0x1082 }, { 0x1085, 0x1086 }, { 0x108D, 0x108D },
    { 0x109D, 0x109D }, { 0x1160, 0x11FF }, { 0x135D, 0x135F },
    { 0x1712, 0x1714 }, { 0x1732, 0x1733 }, { 0x1752, 0x1753 },
    { 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
    { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
    { 0x180B, 0x180F }, { 0x1885, 0x1886 }, { 0x18A9, 0x18A9 },
    { 0x1920, 0x1922 }, { 0x1927, 0x1928 }, { 0x1932, 0x1932 },
    { 0x1939, 0x193B }, { 0x1A17, 0x1A18 }, { 0x1A1B, 0x1A1B },
    { 0x1A56, 0x1A56 }, { 0x1A58, 0x1A5E }, { 0x1A60, 0x1A60 },
    { 0x1A62, 0x1A62 }, { 0x1A65, 0x1A6C }, { 0x1A73, 0x1A7C },
    { 0x1A7F, 0x1A7F }, { 0x1AB0, 0x1ADD }, { 0x1AE0, 0x1AEB },
    { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 }, { 0x1B36, 0x1B3A },
    { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 }, { 0x1B6B, 0x1B73 },
    { 0x1B80, 0x1B81 }, { 0x1BA2, 0x1BA5 }, { 0x1BA8, 0x1BA9 },
    { 0x1BAB, 0x1BAD }, { 0x1BE6, 0x1BE6 }, { 0x1BE8, 0x1BE9 },
    { 0x1BED, 0x1BED }, { 0x1BEF, 0x1BF1 }, { 0x1C2C, 0x1C33 },
    { 0x1C36, 0x1C37 }, { 0x1CD0, 0x1CD2 }, { 0x1CD4, 0x1CE0 },
    { 0x1CE2, 0x1CE8 }, { 0x1CED, 0x1CED }, { 0x1CF4, 0x1CF4 },
    { 0x1CF8, 0x1CF9 }, { 0x1DC0, 0x1DFF }, { 0x200B, 0x200F },
    { 0x2028, 0x202E }, { 0x2060, 0x206F }, { 0x20D0, 0x20F0 },
    { 0x2CEF, 0x2CF1 }, { 0x2D7F, 0x2D7F }, { 0x2DE0, 0x2DFF },
    { 0x302A, 0x302D }, { 0x3099, 0x309A }, { 0x3164, 0x3164 },
    { 0xA66F, 0xA672 }, { 0xA674, 0xA67D }, { 0xA69E, 0xA69F },
    { 0xA6F0, 0xA6F1 }, { 0xA802, 0xA802 }, { 0xA806, 0xA806 },
    { 0xA80B, 0xA80B }, { 0xA825, 0xA826 }, { 0xA82C, 0xA82C },
    { 0xA8C4, 0xA8C5 }, { 0xA8E0, 0xA8F1 }, { 0xA8FF, 0xA8FF },
    { 0xA926, 0xA92D }, { 0xA947, 0xA951 }, { 0xA980, 0xA982 },
    { 0xA9B3, 0xA9B3 }, { 0xA9B6, 0xA9B9 }, { 0xA9BC, 0xA9BD },
    { 0xA9E5, 0xA9E5 }, { 0xAA29, 0xAA2E }, { 0xAA31, 0xAA32 },
    { 0xAA35, 0xAA36 }, { 0xAA43, 0xAA43 }, { 0xAA4C, 0xAA4C },
    { 0xAA7C, 0xAA7C }, { 0xAAB0, 0xAAB0 }, { 0xAAB2, 0xAAB4 },
    { 0xAAB7, 0xAAB8 }, { 0xAABE, 0xAABF }, { 0xAAC1, 0xAAC1 },
    { 0xAAEC, 0xAAED }, { 0xAAF6, 0xAAF6 }, { 0xABE5, 0xABE5 },
    { 0xABE8, 0xABE8 }, { 0xABED, 0xABED }, { 0xD7B0, 0xD7FF },
    { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F }, { 0xFE20, 0xFE2F },
    { 0xFEFF, 0xFEFF }, { 0xFFA0, 0xFFA0 }, { 0xFFF0, 0xFFFB },
    { 0x101FD, 0x101FD }, { 0x102E0, 0x102E0 }, { 0x10376, 0x1037A },
    { 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
    { 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x10AE5, 0x10AE6 },
    { 0x10D24, 0x10D27 }, { 0x10D69, 0x10D6D }, { 0x10EAB, 0x10EAC },
    { 0x10EFA, 0x10EFF }, { 0x10F46, 0x10F50 }, { 0x10F82, 0x10F85 },
    { 0x11001, 0x11001 }, { 0x11038, 0x11046 }, { 0x11070, 0x11070 },
    { 0x11073, 0x11074 }, { 0x1107F, 0x11081 }, { 0x110B3, 0x110B6 },
    { 0x110B9, 0x110BA }, { 0x110C2, 0x110C2 }, { 0x11100, 0x11102 },
    { 0x11127, 0x1112B }, { 0x1112D, 0x11134 }, { 0x11173, 0x11173 },
    { 0x11180, 0x11181 }, { 0x111B6, 0x111BE }, { 0x111C9, 0x111CC },
    { 0x111CF, 0x111CF }, { 0x1122F, 0x11231 }, { 0x11234, 0x11234 },
    { 0x11236, 0x11237 }, { 0x1123E, 0x1123E }, { 0x11241, 0x11241 },
    { 0x112DF, 0x112DF }, { 0x112E3, 0x112EA }, { 0x11300, 0x11301 },
    { 0x1133B, 0x1133C }, { 0x11340, 0x11340 }, { 0x11366, 0x1136C },
    { 0x11370, 0x11374 }, { 0x113BB, 0x113C0 }, { 0x113CE, 0x113CE },
    { 0x113D0, 0x113D0 }, { 0x113D2, 0x113D2 }, { 0x113E1, 0x113E2 },
    { 0x11438, 0x1143F }, { 0x11442, 0x11444 }, { 0x11446, 0x11446 },
    { 0x1145E, 0x1145E }, { 0x114B3, 0x114B8 }, { 0x114BA, 0x114BA },
    { 0x114BF, 0x114C0 }, { 0x114C2, 0x114C3 }, { 0x115B2, 0x115B5 },
    { 0x115BC, 0x115BD }, { 0x115BF, 0x115C0 }, { 0x115DC, 0x115DD },
    { 0x11633, 0x1163A }, { 0x1163D, 0x1163D }, { 0x1163F, 0x11640 },
    { 0x116AB, 0x116AB }, { 0x116AD, 0x116AD }, { 0x116B0, 0x116B5 },
    { 0x116B7, 0x116B7 }, { 0x1171D, 0x1171D }, { 0x1171F, 0x1171F },
    { 0x11722, 0x11725 }, { 0x11727, 0x1172B }, { 0x1182F, 0x11837 },
    { 0x11839, 0x1183A }, { 0x1193B, 0x1193C }, { 0x1193E, 0x1193E },
    { 0x11943, 0x11943 }, { 0x119D4, 0x119D7 }, { 0x119DA, 0x119DB },
    { 0x119E0, 0x119E0 }, { 0x11A01, 0x11A0A }, { 0x11A33, 0x11A38 },
    { 0x11A3B, 0x11A3E }, { 0x11A47, 0x11A47 }, { 0x11A51, 0x11A56 },
    { 0x11A59, 0x11A5B }, { 0x11A8A, 0x11A96 }, { 0x11A98, 0x11A99 },
    { 0x11B60, 0x11B60 }, { 0x11B62, 0x11B64 }, { 0x11B66, 0x11B66 },
    { 0x11C30, 0x11C36 }, { 0x11C38, 0x11C3D }, { 0x11C3F, 0x11C3F },
    { 0x11C92, 0x11CA7 }, { 0x11CAA, 0x11CB0 }, { 0x11CB2, 0x11CB3 },
    { 0x11CB5, 0x11CB6 }, { 0x11D31, 0x11D36 }, { 0x11D3A, 0x11D3A },
    { 0x11D3C, 0x11D3D }, { 0x11D3F, 0x11D45 }, { 0x11D47, 0x11D47 },
    { 0x11D90, 0x11D91 }, { 0x11D95, 0x11D95 }, { 0x11D97, 0x11D97 },
    { 0x11EF3, 0x11EF4 }, { 0x11F00, 0x11F01 }, { 0x11F36, 0x11F3A },
    { 0x11F40, 0x11F40 }, { 0x11F42, 0x11F42 }, { 0x11F5A, 0x11F5A },
    { 0x13430, 0x13440 }, { 0x13447, 0x13455 }, { 0x1611E, 0x16129 },
    { 0x1612D, 0x1612F }, { 0x16AF0, 0x16AF4 }, { 0x16B30, 0x16B36 },
    { 0x16F4F, 0x16F4F }, { 0x16F8F, 0x16F92 }, { 0x16FE4, 0x16FE4 },
    { 0x1BC9D, 0x1BC9E }, { 0x1BCA0, 0x1BCA3 }, { 0x1CF00, 0x1CF2D },
    { 0x1CF30, 0x1CF46 }, { 0x1D167, 0x1D169 }, { 0x1D173, 0x1D182 },
    { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD }, { 0x1D242, 0x1D244 },
    { 0x1DA00, 0x1DA36 }, { 0x1DA3B, 0x1DA6C }, { 0x1DA75, 0x1DA75 },
    { 0x1DA84, 0x1DA84 }, { 0x1DA9B, 0x1DA9F }, { 0x1DAA1, 0x1DAAF },
    { 0x1E000, 0x1E006 }, { 0x1E008, 0x1E018 }, { 0x1E01B, 0x1E021 },
    { 0x1E023, 0x1E024 }, { 0x1E026, 0x1E02A }, { 0x1E08F, 0x1E08F },
    { 0x1E130, 0x1E136 }, { 0x1E2AE, 0x1E2AE }, { 0x1E2EC, 0x1E2EF },
    { 0x1E4EC, 0x1E4EF }, { 0x1E5EE, 0x1E5EF }, { 0x1E6E3, 0x1E6E3 },
    { 0x1E6E6, 0x1E6E6 }, { 0x1E6EE, 0x1E6EF }, { 0x1E6F5, 0x1E6F5 },
    { 0x1E8D0, 0x1E8D6 }, { 0x1E944, 0x1E94A }, { 0xE0000, 0xE0FFF }
  };

  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, combining,
	       sizeof(combining) / sizeof(struct interval) - 1))
    return 0;

  /* sorted list of non-overlapping intervals of wide/fullwidth characters */
  /* Unicode 17.0.0 - East_Asian_Width W and F */
  static const struct interval wide[] = {
    { 0x1100, 0x115F }, { 0x231A, 0x231B }, { 0x2329, 0x232A },
    { 0x23E9, 0x23EC }, { 0x23F0, 0x23F0 }, { 0x23F3, 0x23F3 },
    { 0x25FD, 0x25FE }, { 0x2614, 0x2615 }, { 0x2630, 0x2637 },
    { 0x2648, 0x2653 }, { 0x267F, 0x267F }, { 0x268A, 0x268F },
    { 0x2693, 0x2693 }, { 0x26A1, 0x26A1 }, { 0x26AA, 0x26AB },
    { 0x26BD, 0x26BE }, { 0x26C4, 0x26C5 }, { 0x26CE, 0x26CE },
    { 0x26D4, 0x26D4 }, { 0x26EA, 0x26EA }, { 0x26F2, 0x26F3 },
    { 0x26F5, 0x26F5 }, { 0x26FA, 0x26FA }, { 0x26FD, 0x26FD },
    { 0x2705, 0x2705 }, { 0x270A, 0x270B }, { 0x2728, 0x2728 },
    { 0x274C, 0x274C }, { 0x274E, 0x274E }, { 0x2753, 0x2755 },
    { 0x2757, 0x2757 }, { 0x2795, 0x2797 }, { 0x27B0, 0x27B0 },
    { 0x27BF, 0x27BF }, { 0x2B1B, 0x2B1C }, { 0x2B50, 0x2B50 },
    { 0x2B55, 0x2B55 }, { 0x2E80, 0x2E99 }, { 0x2E9B, 0x2EF3 },
    { 0x2F00, 0x2FD5 }, { 0x2FF0, 0x303E }, { 0x3041, 0x3096 },
    { 0x3099, 0x30FF }, { 0x3105, 0x312F }, { 0x3131, 0x318E },
    { 0x3190, 0x31E5 }, { 0x31EF, 0x321E }, { 0x3220, 0x3247 },
    { 0x3250, 0xA48C }, { 0xA490, 0xA4C6 }, { 0xA960, 0xA97C },
    { 0xAC00, 0xD7A3 }, { 0xF900, 0xFAFF }, { 0xFE10, 0xFE19 },
    { 0xFE30, 0xFE52 }, { 0xFE54, 0xFE66 }, { 0xFE68, 0xFE6B },
    { 0xFF01, 0xFF60 }, { 0xFFE0, 0xFFE6 },
    { 0x16FE0, 0x16FE4 }, { 0x16FF0, 0x16FF6 },
    { 0x17000, 0x18CD5 }, { 0x18CFF, 0x18D1E }, { 0x18D80, 0x18DF2 },
    { 0x1AFF0, 0x1AFF3 }, { 0x1AFF5, 0x1AFFB },
    { 0x1AFFD, 0x1AFFE }, { 0x1B000, 0x1B122 }, { 0x1B132, 0x1B132 },
    { 0x1B150, 0x1B152 }, { 0x1B155, 0x1B155 }, { 0x1B164, 0x1B167 },
    { 0x1B170, 0x1B2FB }, { 0x1D300, 0x1D356 }, { 0x1D360, 0x1D376 },
    { 0x1F004, 0x1F004 }, { 0x1F0CF, 0x1F0CF }, { 0x1F18E, 0x1F18E },
    { 0x1F191, 0x1F19A }, { 0x1F200, 0x1F202 }, { 0x1F210, 0x1F23B },
    { 0x1F240, 0x1F248 }, { 0x1F250, 0x1F251 }, { 0x1F260, 0x1F265 },
    { 0x1F300, 0x1F320 }, { 0x1F32D, 0x1F335 }, { 0x1F337, 0x1F37C },
    { 0x1F37E, 0x1F393 }, { 0x1F3A0, 0x1F3CA }, { 0x1F3CF, 0x1F3D3 },
    { 0x1F3E0, 0x1F3F0 }, { 0x1F3F4, 0x1F3F4 }, { 0x1F3F8, 0x1F43E },
    { 0x1F440, 0x1F440 }, { 0x1F442, 0x1F4FC }, { 0x1F4FF, 0x1F53D },
    { 0x1F54B, 0x1F54E }, { 0x1F550, 0x1F567 }, { 0x1F57A, 0x1F57A },
    { 0x1F595, 0x1F596 }, { 0x1F5A4, 0x1F5A4 }, { 0x1F5FB, 0x1F64F },
    { 0x1F680, 0x1F6C5 }, { 0x1F6CC, 0x1F6CC }, { 0x1F6D0, 0x1F6D2 },
    { 0x1F6D5, 0x1F6D8 }, { 0x1F6DC, 0x1F6DF }, { 0x1F6EB, 0x1F6EC },
    { 0x1F6F4, 0x1F6FC }, { 0x1F7E0, 0x1F7EB }, { 0x1F7F0, 0x1F7F0 },
    { 0x1F90C, 0x1F93A }, { 0x1F93C, 0x1F945 }, { 0x1F947, 0x1F9FF },
    { 0x1FA70, 0x1FA7C }, { 0x1FA80, 0x1FA8A }, { 0x1FA8E, 0x1FAC6 },
    { 0x1FAC8, 0x1FAC8 }, { 0x1FACD, 0x1FADC }, { 0x1FADF, 0x1FAEA },
    { 0x1FAEF, 0x1FAF8 }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD }
  };

  if (bisearch(ucs, wide,
               sizeof(wide) / sizeof(struct interval) - 1))
    return 2;

  return 1;
}

// End of https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c inherited code

static int unicode_to_cp437(uint64_t code_point) {
    // Braille patterns U+2800-U+28FF: approximate using CP437 block/shade characters.
    // Braille dot layout (bit positions):
    //   bit0  bit3   (row 0)
    //   bit1  bit4   (row 1)
    //   bit2  bit5   (row 2)
    //   bit6  bit7   (row 3)
    if (code_point >= 0x2800 && code_point <= 0x28ff) {
        uint32_t dots = (uint32_t)(code_point - 0x2800);

        if (dots == 0) return 0x20;
        if (dots == 0xff) return 0xdb;

        bool has_top = dots & 0x1b;
        bool has_bottom = dots & 0xe4;
        bool has_left = dots & 0x47;
        bool has_right = dots & 0xb8;

        if (has_top && !has_bottom) return 0xdf; // ▀
        if (has_bottom && !has_top) return 0xdc; // ▄
        if (has_left && !has_right) return 0xdd; // ▌
        if (has_right && !has_left) return 0xde; // ▐

        // Count set bits for density-based shade
        uint32_t n = dots - ((dots >> 1) & 0x55);
        n = (n & 0x33) + ((n >> 2) & 0x33);
        n = (n + (n >> 4)) & 0x0f;

        if (n <= 2) return 0xb0; // ░
        if (n <= 4) return 0xb1; // ▒
        if (n <= 6) return 0xb2; // ▓
        return 0xdb;             // █
    }

    switch (code_point) {
        case 0x263a: return 1;
        case 0x263b: return 2;
        case 0x2665: return 3;
        case 0x2666: return 4;
        case 0x25c6: return 4;
        case 0x2663: return 5;
        case 0x2660: return 6;
        case 0x2022: return 7;
        case 0x25d8: return 8;
        case 0x25cb: return 9;
        case 0x25d9: return 10;
        case 0x2642: return 11;
        case 0x2640: return 12;
        case 0x266a: return 13;
        case 0x266b: return 14;
        case 0x263c: return 15;
        case 0x00a4: return 15;
        case 0x25ba: return 16;
        case 0x25b6: return 16;
        case 0x25c4: return 17;
        case 0x25c0: return 17;
        case 0x2195: return 18;
        case 0x203c: return 19;
        case 0x00b6: return 20;
        case 0x00a7: return 21;
        case 0x25ac: return 22;
        case 0x21a8: return 23;
        case 0x2191: return 24;
        case 0x2193: return 25;
        case 0x2192: return 26;
        case 0x2190: return 27;
        case 0x221f: return 28;
        case 0x2194: return 29;
        case 0x25b2: return 30;
        case 0x25bc: return 31;

        case 0x00a8: return 0x22;
        case 0x00b4: return 0x27;
        case 0x00b8: return 0x2c;
        case 0x00ad: return 0x2d;
        case 0x00c0: return 0x41;
        case 0x00c1: return 0x41;
        case 0x00c2: return 0x41;
        case 0x00c3: return 0x41;
        case 0x00a9: return 0x43;
        case 0x00d0: return 0x44;
        case 0x00c8: return 0x45;
        case 0x00ca: return 0x45;
        case 0x00cb: return 0x45;
        case 0x00cc: return 0x49;
        case 0x00cd: return 0x49;
        case 0x00ce: return 0x49;
        case 0x00cf: return 0x49;
        case 0x212a: return 0x4b;
        case 0x00d2: return 0x4f;
        case 0x00d3: return 0x4f;
        case 0x00d4: return 0x4f;
        case 0x00d5: return 0x4f;
        case 0x00ae: return 0x52;
        case 0x00d9: return 0x55;
        case 0x00da: return 0x55;
        case 0x00db: return 0x55;
        case 0x00dd: return 0x59;
        case 0x23bd: return 0x5f;
        case 0x00e3: return 0x61;
        case 0x00f5: return 0x6f;
        case 0x00d7: return 0x78;
        case 0x00fd: return 0x79;
        case 0x00a6: return 0x7c;

        case 0x2302: return 127;
        case 0x00c7: return 128;
        case 0x00fc: return 129;
        case 0x00e9: return 130;
        case 0x00e2: return 131;
        case 0x00e4: return 132;
        case 0x00e0: return 133;
        case 0x00e5: return 134;
        case 0x00e7: return 135;
        case 0x00ea: return 136;
        case 0x00eb: return 137;
        case 0x00e8: return 138;
        case 0x00ef: return 139;
        case 0x00ee: return 140;
        case 0x00ec: return 141;
        case 0x00c4: return 142;
        case 0x00c5: return 143;
        case 0x212b: return 143;
        case 0x00c9: return 144;
        case 0x00e6: return 145;
        case 0x00c6: return 146;
        case 0x00f4: return 147;
        case 0x00f6: return 148;
        case 0x00f2: return 149;
        case 0x00fb: return 150;
        case 0x00f9: return 151;
        case 0x00ff: return 152;
        case 0x00d6: return 153;
        case 0x00dc: return 154;
        case 0x00a2: return 155;
        case 0x00a3: return 156;
        case 0x00a5: return 157;
        case 0x20a7: return 158;
        case 0x0192: return 159;
        case 0x00e1: return 160;
        case 0x00ed: return 161;
        case 0x00f3: return 162;
        case 0x00fa: return 163;
        case 0x00f1: return 164;
        case 0x00d1: return 165;
        case 0x00aa: return 166;
        case 0x00ba: return 167;
        case 0x00bf: return 168;
        case 0x2310: return 169;
        case 0x00ac: return 170;
        case 0x00bd: return 171;
        case 0x00bc: return 172;
        case 0x00a1: return 173;
        case 0x00ab: return 174;
        case 0x00bb: return 175;
        case 0x2591: return 176;
        case 0x2592: return 177;
        case 0x2593: return 178;
        case 0x2502: return 179;
        case 0x2524: return 180;
        case 0x2561: return 181;
        case 0x2562: return 182;
        case 0x2556: return 183;
        case 0x2555: return 184;
        case 0x2563: return 185;
        case 0x2551: return 186;
        case 0x2557: return 187;
        case 0x255d: return 188;
        case 0x255c: return 189;
        case 0x255b: return 190;
        case 0x2510: return 191;
        case 0x2514: return 192;
        case 0x2534: return 193;
        case 0x252c: return 194;
        case 0x251c: return 195;
        case 0x2500: return 196;
        case 0x253c: return 197;
        case 0x255e: return 198;
        case 0x255f: return 199;
        case 0x255a: return 200;
        case 0x2554: return 201;
        case 0x2569: return 202;
        case 0x2566: return 203;
        case 0x2560: return 204;
        case 0x2550: return 205;
        case 0x256c: return 206;
        case 0x2567: return 207;
        case 0x2568: return 208;
        case 0x2564: return 209;
        case 0x2565: return 210;
        case 0x2559: return 211;
        case 0x2558: return 212;
        case 0x2552: return 213;
        case 0x2553: return 214;
        case 0x256b: return 215;
        case 0x256a: return 216;
        case 0x2518: return 217;
        case 0x250c: return 218;
        case 0x2588: return 219;
        case 0x2584: return 220;
        case 0x258c: return 221;
        case 0x2590: return 222;
        case 0x2580: return 223;
        case 0x03b1: return 224;
        case 0x00df: return 225;
        case 0x03b2: return 225;
        case 0x0393: return 226;
        case 0x03c0: return 227;
        case 0x03a3: return 228;
        case 0x03c3: return 229;
        case 0x00b5: return 230;
        case 0x03bc: return 230;
        case 0x03c4: return 231;
        case 0x03a6: return 232;
        case 0x00d8: return 232;
        case 0x0398: return 233;
        case 0x03a9: return 234;
        case 0x2126: return 234;
        case 0x03b4: return 235;
        case 0x00f0: return 235;
        case 0x221e: return 236;
        case 0x03c6: return 237;
        case 0x00f8: return 237;
        case 0x03b5: return 238;
        case 0x2208: return 238;
        case 0x2229: return 239;
        case 0x2261: return 240;
        case 0x00b1: return 241;
        case 0x2265: return 242;
        case 0x2264: return 243;
        case 0x2320: return 244;
        case 0x2321: return 245;
        case 0x00f7: return 246;
        case 0x2248: return 247;
        case 0x00b0: return 248;
        case 0x2219: return 249;
        case 0x00b7: return 250;
        case 0x221a: return 251;
        case 0x207f: return 252;
        case 0x00b2: return 253;
        case 0x25a0: return 254;
        case 0xfffd: return 254;
        case 0x00a0: return 255;

        // Approximate mappings for Unicode characters without exact CP437 equivalents

        // Rounded/arc box drawing corners
        case 0x256d: return 0xda; // ╭ → ┌
        case 0x256e: return 0xbf; // ╮ → ┐
        case 0x256f: return 0xd9; // ╯ → ┘
        case 0x2570: return 0xc0; // ╰ → └

        // Diagonal box drawing
        case 0x2571: return 0x2f; // ╱ → /
        case 0x2572: return 0x5c; // ╲ → \ (backslash)
        case 0x2573: return 0x58; // ╳ → X

        // Heavy box drawing → single-line equivalents
        case 0x2501: return 0xc4; // ━ → ─
        case 0x2503: return 0xb3; // ┃ → │
        case 0x250f: return 0xda; // ┏ → ┌
        case 0x2513: return 0xbf; // ┓ → ┐
        case 0x2517: return 0xc0; // ┗ → └
        case 0x251b: return 0xd9; // ┛ → ┘
        case 0x2523: return 0xc3; // ┣ → ├
        case 0x252b: return 0xb4; // ┫ → ┤
        case 0x2533: return 0xc2; // ┳ → ┬
        case 0x253b: return 0xc1; // ┻ → ┴
        case 0x254b: return 0xc5; // ╋ → ┼

        // Mixed heavy/light box drawing corners
        case 0x250d: return 0xda; // ┍ → ┌
        case 0x250e: return 0xda; // ┎ → ┌
        case 0x2511: return 0xbf; // ┑ → ┐
        case 0x2512: return 0xbf; // ┒ → ┐
        case 0x2515: return 0xc0; // ┕ → └
        case 0x2516: return 0xc0; // ┖ → └
        case 0x2519: return 0xd9; // ┙ → ┘
        case 0x251a: return 0xd9; // ┚ → ┘

        // Mixed heavy/light box drawing T-pieces
        case 0x251d: return 0xc3; // ┝ → ├
        case 0x251e: return 0xc3; // ┞ → ├
        case 0x251f: return 0xc3; // ┟ → ├
        case 0x2520: return 0xc3; // ┠ → ├
        case 0x2521: return 0xc3; // ┡ → ├
        case 0x2522: return 0xc3; // ┢ → ├
        case 0x2525: return 0xb4; // ┥ → ┤
        case 0x2526: return 0xb4; // ┦ → ┤
        case 0x2527: return 0xb4; // ┧ → ┤
        case 0x2528: return 0xb4; // ┨ → ┤
        case 0x2529: return 0xb4; // ┩ → ┤
        case 0x252a: return 0xb4; // ┪ → ┤
        case 0x252d: return 0xc2; // ┭ → ┬
        case 0x252e: return 0xc2; // ┮ → ┬
        case 0x252f: return 0xc2; // ┯ → ┬
        case 0x2530: return 0xc2; // ┰ → ┬
        case 0x2531: return 0xc2; // ┱ → ┬
        case 0x2532: return 0xc2; // ┲ → ┬
        case 0x2535: return 0xc1; // ┵ → ┴
        case 0x2536: return 0xc1; // ┶ → ┴
        case 0x2537: return 0xc1; // ┷ → ┴
        case 0x2538: return 0xc1; // ┸ → ┴
        case 0x2539: return 0xc1; // ┹ → ┴
        case 0x253a: return 0xc1; // ┺ → ┴

        // Mixed heavy/light box drawing crosses
        case 0x253d: return 0xc5; // ┽ → ┼
        case 0x253e: return 0xc5; // ┾ → ┼
        case 0x253f: return 0xc5; // ┿ → ┼
        case 0x2540: return 0xc5; // ╀ → ┼
        case 0x2541: return 0xc5; // ╁ → ┼
        case 0x2542: return 0xc5; // ╂ → ┼
        case 0x2543: return 0xc5; // ╃ → ┼
        case 0x2544: return 0xc5; // ╄ → ┼
        case 0x2545: return 0xc5; // ╅ → ┼
        case 0x2546: return 0xc5; // ╆ → ┼
        case 0x2547: return 0xc5; // ╇ → ┼
        case 0x2548: return 0xc5; // ╈ → ┼
        case 0x2549: return 0xc5; // ╉ → ┼
        case 0x254a: return 0xc5; // ╊ → ┼

        // Dashed/dotted box drawing → solid equivalents
        case 0x2504: return 0xc4; // ┄ → ─
        case 0x2505: return 0xc4; // ┅ → ─
        case 0x2506: return 0xb3; // ┆ → │
        case 0x2507: return 0xb3; // ┇ → │
        case 0x2508: return 0xc4; // ┈ → ─
        case 0x2509: return 0xc4; // ┉ → ─
        case 0x250a: return 0xb3; // ┊ → │
        case 0x250b: return 0xb3; // ┋ → │

        // Box drawing half-lines and fragments
        case 0x2574: return 0xc4; // ╴ → ─
        case 0x2575: return 0xb3; // ╵ → │
        case 0x2576: return 0xc4; // ╶ → ─
        case 0x2577: return 0xb3; // ╷ → │
        case 0x2578: return 0xc4; // ╸ → ─
        case 0x2579: return 0xb3; // ╹ → │
        case 0x257a: return 0xc4; // ╺ → ─
        case 0x257b: return 0xb3; // ╻ → │
        case 0x257c: return 0xc4; // ╼ → ─
        case 0x257d: return 0xb3; // ╽ → │
        case 0x257e: return 0xc4; // ╾ → ─
        case 0x257f: return 0xb3; // ╿ → │

        // Triangle variants → filled equivalents
        case 0x25b3: return 30;  // △ → ▲
        case 0x25b5: return 30;  // ▵ → ▲
        case 0x25b7: return 16;  // ▷ → ►
        case 0x25b9: return 16;  // ▹ → ►
        case 0x25bd: return 31;  // ▽ → ▼
        case 0x25bf: return 31;  // ▿ → ▼
        case 0x25c1: return 17;  // ◁ → ◄
        case 0x25c3: return 17;  // ◃ → ◄

        // Fractional block elements → closest CP437 block
        case 0x2581: return 0xdc; // ▁ (lower 1/8) → ▄
        case 0x2582: return 0xdc; // ▂ (lower 1/4) → ▄
        case 0x2583: return 0xdc; // ▃ (lower 3/8) → ▄
        case 0x2585: return 0xdc; // ▅ (lower 5/8) → ▄
        case 0x2586: return 0xdb; // ▆ (lower 3/4) → █
        case 0x2587: return 0xdb; // ▇ (lower 7/8) → █
        case 0x2589: return 0xdb; // ▉ (left 7/8) → █
        case 0x258a: return 0xdb; // ▊ (left 3/4) → █
        case 0x258b: return 0xdd; // ▋ (left 5/8) → ▌
        case 0x258d: return 0xdd; // ▍ (left 3/8) → ▌
        case 0x258e: return 0xdd; // ▎ (left 1/4) → ▌
        case 0x258f: return 0xdd; // ▏ (left 1/8) → ▌
        case 0x2594: return 0xdf; // ▔ (upper 1/8) → ▀
        case 0x2595: return 0xde; // ▕ (right 1/8) → ▐

        // Quadrant block elements
        case 0x2596: return 0xdc; // ▖ → ▄
        case 0x2597: return 0xdc; // ▗ → ▄
        case 0x2598: return 0xdf; // ▘ → ▀
        case 0x2599: return 0xdb; // ▙ → █
        case 0x259a: return 0xb1; // ▚ → ▒
        case 0x259b: return 0xdb; // ▛ → █
        case 0x259c: return 0xdb; // ▜ → █
        case 0x259d: return 0xdf; // ▝ → ▀
        case 0x259e: return 0xb1; // ▞ → ▒
        case 0x259f: return 0xdb; // ▟ → █

        // Circles and bullets
        case 0x25cf: return 0x07; // ● → •
        case 0x25c9: return 0x0a; // ◉ → ◙
        case 0x25ef: return 0x09; // ◯ → ○
        case 0x25e6: return 0x09; // ◦ → ○
        case 0x25aa: return 0xfe; // ▪ → ■
        case 0x25fc: return 0xfe; // ◼ → ■

        // Typographic punctuation
        case 0x2013: return 0x2d; // – (en dash) → -
        case 0x2014: return 0x2d; // — (em dash) → -
        case 0x2018: return 0x27; // ' (left single quote) → '
        case 0x2019: return 0x27; // ' (right single quote) → '
        case 0x201c: return 0x22; // " (left double quote) → "
        case 0x201d: return 0x22; // " (right double quote) → "
        case 0x2026: return 0xfa; // … (ellipsis) → ·
        case 0x2212: return 0x2d; // − (minus sign) → -

        // Check marks
        case 0x2713: return 0xfb; // ✓ → √
        case 0x2714: return 0xfb; // ✔ → √

        // Double arrows → single arrow equivalents
        case 0x21d0: return 27;  // ⇐ → ←
        case 0x21d1: return 24;  // ⇑ → ↑
        case 0x21d2: return 26;  // ⇒ → →
        case 0x21d3: return 25;  // ⇓ → ↓
        case 0x21d4: return 29;  // ⇔ → ↔
        case 0x21d5: return 18;  // ⇕ → ↕

        // Summation sign
        case 0x2211: return 0xe4; // ∑ → Σ

        // Horizontal line extension
        case 0x23af: return 0xc4; // ⎯ → ─

        // Media transport symbols
        case 0x23f4: return 17;   // ⏴ → ◄
        case 0x23f5: return 16;   // ⏵ → ►
        case 0x23f6: return 30;   // ⏶ → ▲
        case 0x23f7: return 31;   // ⏷ → ▼
        case 0x23f8: return 0xba; // ⏸ → ║
        case 0x23f9: return 0xfe; // ⏹ → ■
        case 0x23fa: return 0x07; // ⏺ → •

        // Square bracket pieces
        case 0x23a1: return 0xda; // ⎡ → ┌
        case 0x23a2: return 0xb3; // ⎢ → │
        case 0x23a3: return 0xc0; // ⎣ → └
        case 0x23a4: return 0xbf; // ⎤ → ┐
        case 0x23a5: return 0xb3; // ⎥ → │
        case 0x23a6: return 0xd9; // ⎦ → ┘

        // Curly bracket pieces
        case 0x23a7: return 0xda; // ⎧ → ┌
        case 0x23a8: return 0xc3; // ⎨ → ├
        case 0x23a9: return 0xc0; // ⎩ → └
        case 0x23aa: return 0xb3; // ⎪ → │
        case 0x23ab: return 0xbf; // ⎫ → ┐
        case 0x23ac: return 0xb4; // ⎬ → ┤
        case 0x23ad: return 0xd9; // ⎭ → ┘
        case 0x23ae: return 0xb3; // ⎮ → │

        // Vertical box lines
        case 0x23b8: return 0xb3; // ⎸ → │
        case 0x23b9: return 0xb3; // ⎹ → │

        // Horizontal scan lines (0x23bd already mapped above)
        case 0x23ba: return 0xc4; // ⎺ → ─
        case 0x23bb: return 0xc4; // ⎻ → ─
        case 0x23bc: return 0xc4; // ⎼ → ─

        // Dentistry/angle symbols
        case 0x23be: return 0xb3; // ⎾ → │
        case 0x23bf: return 0xc0; // ⎿ → └

        // Corner brackets
        case 0x231c: return 0xda; // ⌜ → ┌
        case 0x231d: return 0xbf; // ⌝ → ┐
        case 0x231e: return 0xc0; // ⌞ → └
        case 0x231f: return 0xd9; // ⌟ → ┘
    }

    return -1;
}

static void insert_shift(struct flanterm_context *ctx, size_t count) {
    if (ctx->insert_mode && count > 0) {
        size_t x, y;
        ctx->get_cursor_pos(ctx, &x, &y);
        if (count > ctx->cols - x) {
            count = ctx->cols - x;
        }
        for (size_t i = ctx->cols - 1; i >= x + count; i--) {
            ctx->move_character(ctx, i, y, i - count, y);
        }
    }
}

static void flanterm_putchar(struct flanterm_context *ctx, uint8_t c) {
    if (ctx->discard_next || (c == 0x18 || c == 0x1a)) {
        ctx->discard_next = false;
        ctx->escape = false;
        ctx->control_sequence = false;
        ctx->unicode_remaining = 0;
        ctx->osc = false;
        ctx->osc_escape = false;
        ctx->g_select = 0;
        ctx->last_was_graphic = false;
        return;
    }

    if (ctx->unicode_remaining != 0) {
        if ((c & 0xc0) != 0x80) {
            ctx->unicode_remaining = 0;
            insert_shift(ctx, 1);
            ctx->raw_putchar(ctx, 0xfe);
            goto unicode_error;
        }

        ctx->unicode_remaining--;
        ctx->code_point |= (uint64_t)(c & 0x3f) << (6 * ctx->unicode_remaining);

        // Reject overlong encodings and out-of-range codepoints early
        // by validating the first continuation byte against the lead byte.
        // 3-byte lead E0: first continuation must be >= 0xA0 (code_point >= 0x800)
        // 4-byte lead F0: first continuation must be >= 0x90 (code_point >= 0x10000)
        // 4-byte lead F4: first continuation must be <= 0x8F (code_point <= 0x10FFFF)
        if (ctx->unicode_remaining == 1 && ctx->code_point < 0x800) {
            ctx->unicode_remaining = 0;
            goto unicode_error;
        }
        if (ctx->unicode_remaining == 2 && ctx->code_point < 0x10000) {
            ctx->unicode_remaining = 0;
            goto unicode_error;
        }
        if (ctx->unicode_remaining == 2 && ctx->code_point > 0x10ffff) {
            ctx->unicode_remaining = 0;
            goto unicode_error;
        }

        if (ctx->unicode_remaining != 0) {
            return;
        }

        if (ctx->code_point >= 0xd800 && ctx->code_point <= 0xdfff) {
            goto unicode_error;
        }

        int cc = unicode_to_cp437(ctx->code_point);

        if (cc == -1) {
            int replacement_width = mk_wcwidth(ctx->code_point);
            if (replacement_width > 0) {
                insert_shift(ctx, replacement_width);
                ctx->last_printed_char = 0xfe;
                ctx->last_was_graphic = true;
                ctx->raw_putchar(ctx, 0xfe);
            }
            for (int i = 1; i < replacement_width; i++) {
                ctx->raw_putchar(ctx, ' ');
            }
        } else {
            insert_shift(ctx, 1);
            ctx->last_printed_char = cc;
            ctx->last_was_graphic = true;
            ctx->raw_putchar(ctx, cc);
        }
        return;
    }

unicode_error:
    if (c >= 0xc2 && c <= 0xf4) {
        ctx->g_select = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            ctx->unicode_remaining = 1;
            ctx->code_point = (uint64_t)(c & 0x1f) << 6;
        } else if (c >= 0xe0 && c <= 0xef) {
            ctx->unicode_remaining = 2;
            ctx->code_point = (uint64_t)(c & 0x0f) << (6 * 2);
        } else if (c >= 0xf0 && c <= 0xf4) {
            ctx->unicode_remaining = 3;
            ctx->code_point = (uint64_t)(c & 0x07) << (6 * 3);
        }
        return;
    }

    if (ctx->escape == true) {
        escape_parse(ctx, c);
        return;
    }

    if (ctx->g_select) {
        if (c <= 0x1f || c == 0x7f) {
            ctx->g_select = 0;
        } else {
            ctx->g_select--;
            switch (c) {
                case 'B':
                    ctx->charsets[ctx->g_select] = CHARSET_DEFAULT; break;
                case '0':
                    ctx->charsets[ctx->g_select] = CHARSET_DEC_SPECIAL; break;
            }
            ctx->g_select = 0;
            return;
        }
    }

    if ((c <= 0x1f && c != 0x1b) || c == 0x7f) {
        ctx->last_was_graphic = false;
    }

    switch (c) {
        case 0x00:
        case 0x7f:
            return;
        case 0x1b:
            ctx->escape_offset = 0;
            ctx->escape = true;
            return;
    }

    if (c < 0x20) {
        execute_c0(ctx, c);
        return;
    }

    insert_shift(ctx, 1);

    // Translate character set
    switch (ctx->charsets[ctx->current_charset]) {
        case CHARSET_DEFAULT:
            break;
        case CHARSET_DEC_SPECIAL:
            if (dec_special_print(ctx, c)) {
                return;
            }
            break;
    }

    if (c >= 0x20 && c <= 0x7e) {
        ctx->last_printed_char = c;
        ctx->last_was_graphic = true;
        ctx->raw_putchar(ctx, c);
    } else if (c >= 0x80) {
        ctx->last_printed_char = 0xfe;
        ctx->last_was_graphic = true;
        ctx->raw_putchar(ctx, 0xfe);
    }
}

void flanterm_flush(struct flanterm_context *ctx) {
    ctx->double_buffer_flush(ctx);
}

void flanterm_full_refresh(struct flanterm_context *ctx) {
    ctx->full_refresh(ctx);
}

void flanterm_deinit(struct flanterm_context *ctx, void (*_free)(void *, size_t)) {
    ctx->deinit(ctx, _free);
}

void flanterm_get_dimensions(struct flanterm_context *ctx, size_t *cols, size_t *rows) {
    *cols = ctx->cols;
    *rows = ctx->rows;
}

void flanterm_set_autoflush(struct flanterm_context *ctx, bool state) {
    ctx->autoflush = state;
}

void flanterm_set_callback(struct flanterm_context *ctx, void (*callback)(struct flanterm_context *, uint64_t, uint64_t, uint64_t, uint64_t)) {
    ctx->callback = callback;
}

void flanterm_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y) {
	ctx->get_cursor_pos(ctx, x, y);
}

void flanterm_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y) {
	if (x >= ctx->cols) {
		x = ctx->cols - 1;
	}

	if (y >= ctx->rows) {
		y = ctx->rows - 1;
	}

	ctx->set_cursor_pos(ctx, x, y);
}

void flanterm_set_text_fg(struct flanterm_context *ctx, size_t colour, bool bright) {
	ctx->current_primary = colour;

	if (bright) {
		if (!ctx->reverse_video) {
			ctx->set_text_fg_bright(ctx, colour);
		} else {
			ctx->set_text_bg_bright(ctx, colour);
		}
	} else {
		if (!ctx->reverse_video) {
			ctx->set_text_fg(ctx, colour);
		} else {
			ctx->set_text_bg(ctx, colour);
		}
	}
}

void flanterm_set_text_bg(struct flanterm_context *ctx, size_t colour, bool bright) {
	ctx->current_bg = colour;

	if (bright) {
		if (!ctx->reverse_video) {
			ctx->set_text_bg_bright(ctx, colour);
		} else {
			ctx->set_text_fg_bright(ctx, colour);
		}
	} else {
		if (!ctx->reverse_video) {
			ctx->set_text_bg(ctx, colour);
		} else {
			ctx->set_text_fg(ctx, colour);
		}
	}
}

void flanterm_reset_text_fg(struct flanterm_context *ctx) {
	ctx->current_primary = (size_t)-1;

	if (!ctx->bold) {
		if (!ctx->reverse_video) {
			ctx->set_text_fg_default(ctx);
		} else {
			ctx->set_text_bg_default(ctx);
		}
	} else {
		if (!ctx->reverse_video) {
			ctx->set_text_fg_default_bright(ctx);
		} else {
			ctx->set_text_bg_default_bright(ctx);
		}
	}
}

void flanterm_reset_text_bg(struct flanterm_context *ctx) {
	ctx->current_bg = (size_t)-1;

	if (!ctx->bg_bold) {
		if (!ctx->reverse_video) {
			ctx->set_text_bg_default(ctx);
		} else {
			ctx->set_text_fg_default(ctx);
		}
	} else {
		if (!ctx->reverse_video) {
			ctx->set_text_bg_default_bright(ctx);
		} else {
			ctx->set_text_fg_default_bright(ctx);
		}
	}
}

void flanterm_clear(struct flanterm_context *ctx, bool move) {
	ctx->clear(ctx, move);
}

