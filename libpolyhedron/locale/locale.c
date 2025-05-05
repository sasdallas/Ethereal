/**
 * @file libpolyhedron/locale/locale.c
 * @brief locale
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <locale.h>

/* Default en_US locale */
static struct lconv __locale_en_US = {
    .decimal_point      = ".",
    .currency_symbol    = "$",
    .int_curr_symbol    = "USD ",
    .grouping           = "\x03\x03",
    .mon_grouping       = "\x03\x03",
    .mon_decimal_point  = ".",
    .mon_thousands_sep  = ",",
    .frac_digits        = 2,
    .int_frac_digits    = 2,
    .p_cs_precedes      = 1,
    .n_cs_precedes      = 1,
    .p_sep_by_space     = 0,
    .n_sep_by_space     = 0,
    .p_sign_posn        = 1,
    .n_sign_posn        = 1,
    .int_p_sign_posn    = 1,

    // TODO: fill in other fields
};

/* Current locale */
struct lconv *__current_locale = &__locale_en_US;