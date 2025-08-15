/**
 * @file hexahedron/klib/string/strto.c
 * @brief strto 
 * @ref https://cplusplus.com/reference/cstdlib/strtol/
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <ctype.h>

#define __strto_common_macro(ret_type) ({ \
    char *str = (char*)s; \
    while (*str == ' ') str++; \
    int sign = (*str == '-') ? -1 : 1; \
    if (*str == '-' || *str == '+') str++; \
    if (base == 16 && *str == '0') { if (*(str+1) == 'x') str += 2; } \
    ret_type ret = 0; \
    while (*str && ((isdigit(*str)) || (base > 10 && tolower(*str) >= 'a' && tolower(*str) <= ('a'+(base-10))))) { \
        ret *= base; \
        if ((*str >= '0' && *str <= '9')) ret += (*str - '0'); \
        else ret += (tolower(*str) - 'a' + 10); \
        str++; \
    }; \
    if (endptr) *endptr = str; \
    return ret*sign; \
})

long int strtol(const char *s, char **endptr, int base) { __strto_common_macro(long); }
unsigned long int strtoul(const char *s, char **endptr, int base) { __strto_common_macro(unsigned long); }
unsigned long long int strtoull(const char *s, char **endptr, int base) { __strto_common_macro(unsigned long long); }
int atoi(const char *nptr) { return strtol(nptr, NULL, 10); }