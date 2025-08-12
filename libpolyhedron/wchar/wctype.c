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
#include <string.h>

#define WCTYPE_CHECK_NAME(prop) if (!strcmp(name, #prop)) return __##prop

typedef enum {
    __alnum = 1,
    __alpha,
    __blank,
    __cntrl,
    __digit,
    __graph,
    __lower,
    __print,
    __punct,
    __space,
    __upper,
    __xdigit
} __wctype_property;

wint_t towlower(wint_t wc) {
    return tolower(wc); // !!!
}

wint_t towupper(wint_t wc) {
    return toupper(wc); // !!!
}

int iswalnum(wint_t wc) {
    return isalnum(wc); // !!!
}

int iswalpha(wint_t wc) {
    return isalpha(wc); // !!!
}

int iswblank(wint_t wc) {
    return isblank(wc); // !!!
}

int iswcntrl(wint_t wc) {
    return iscntrl(wc); // !!!
}

int iswdigit(wint_t wc) {
    return isdigit(wc); // !!!
}

int iswgraph(wint_t wc) {
    return isgraph(wc); // !!!
}

int iswlower(wint_t wc) {
    return islower(wc); // !!!
}

int iswprint(wint_t wc) {
    return isprint(wc); // !!!
}

int iswpunct(wint_t wc) {
    return ispunct(wc); // !!!
}

int iswspace(wint_t wc) {
    return isspace(wc); // !!!
}

int iswupper(wint_t wc) {
    return isupper(wc); // !!!
}

int iswxdigit(wint_t wc) {
    return isxdigit(wc); // !!!
}


wctype_t wctype(const char *name) {
    WCTYPE_CHECK_NAME(alnum);
    WCTYPE_CHECK_NAME(alpha);
    WCTYPE_CHECK_NAME(blank);
    WCTYPE_CHECK_NAME(cntrl);
    WCTYPE_CHECK_NAME(digit);
    WCTYPE_CHECK_NAME(graph);
    WCTYPE_CHECK_NAME(lower);
    WCTYPE_CHECK_NAME(print);
    WCTYPE_CHECK_NAME(punct);
    WCTYPE_CHECK_NAME(space);
    WCTYPE_CHECK_NAME(upper);
    WCTYPE_CHECK_NAME(xdigit);

    return 0;
}

int iswctype(wint_t wc, wctype_t desc) {
    switch (desc) {
        case __alnum: return iswalnum(wc);
        case __alpha: return iswalpha(wc);
        case __blank: return iswblank(wc);
        case __cntrl: return iswcntrl(wc);
        case __digit: return iswdigit(wc);
        case __graph: return iswgraph(wc);
        case __lower: return iswalnum(wc);
        case __print: return iswalnum(wc);
        case __punct: return iswalnum(wc);
        case __space: return iswalnum(wc);
        case __upper: return iswalnum(wc);
        case __xdigit: return iswalnum(wc);
    }

    return 0;
}