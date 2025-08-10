/**
 * @file libpolyhedron/include/libintl.h
 * @brief libintl
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _LIBINTL_H
#define _LIBINTL_H

/**** INCLUDES ****/
#include <limits.h>
#include <locale.h>

/**** FUNCTIONS ****/
char   *bindtextdomain(const char *, const char *);
char   *bind_textdomain_codeset(const char *, const char *);
char   *dcgettext(const char *, const char *, int);
char   *dcgettext_l(const char *, const char *, int, locale_t);
char   *dcngettext(const char *, const char *, const char *,
            unsigned long int, int);
char   *dcngettext_l(const char *, const char *, const char *,
            unsigned long int, int, locale_t);
char   *dgettext(const char *, const char *);
char   *dgettext_l(const char *, const char *, locale_t);
char   *dngettext(const char *, const char *, const char *,
            unsigned long int);
char   *dngettext_l(const char *, const char *, const char *,
            unsigned long int, locale_t);
char   *gettext(const char *);
char   *gettext_l(const char *, locale_t);
char   *ngettext(const char *, const char *, unsigned long int);
char   *ngettext_l(const char *, const char *,
            unsigned long int, locale_t);
char   *textdomain(const char *);

#endif

_End_C_Header