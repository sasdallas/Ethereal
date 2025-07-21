/**
 * @file libpolyhedron/wchar/wctype.c
 * @brief wctype
 * 
 * @todo actually support wide characters
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <wctype.h>
#include <ctype.h>

wint_t towlower(wint_t wc) {
    return tolower(wc); // !!!
}

wint_t towupper(wint_t wc) {
    return toupper(wc); // !!!
}

int iswlower(wint_t wc) {
    return islower(wc); // !!!
}

int iswupper(wint_t wc) {
    return isupper(wc); // !!!
}