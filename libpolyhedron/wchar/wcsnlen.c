/**
 * @file libpolyhedron/wchar/wcsnlen.c
 * @brief wcsnlen
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <wchar.h>

size_t wcsnlen(const wchar_t *ws, size_t maxlen) {
    size_t r = 0;
    while (ws[r] && r < maxlen) r++;
    return r;
}