/**
 * @file libpolyhedron/stdlib/strtod.c
 * @brief strtod
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>

double strtod(const char *str, char **endptr) {
    // First discard any whitespace
    char *ptr = (char*)str;
    while (isspace(*ptr)) ptr++;

    // Then, check if there's a sign
    int sign = 1; // default to +
    if (*ptr == '-') {
        sign = -1;
        ptr++;
    } else if (*ptr == '+') {
        ptr++;
    }

    long long normal_part = 0;
    while (*ptr && *ptr != '.' && isdigit(*ptr)) {
        normal_part *= 10;
        normal_part += (*ptr - '0');
        ptr++;
    }

    // Now, if there is a decimal point, handle it.
    double decimal_part = 0;
    double decimal_base = 0.1; // Multiply by this every loop iteration
    if (*ptr == '.') {
        ptr++;

        while (*ptr && isdigit(*ptr)) {
            decimal_part += (*ptr - '0') * decimal_base;
            decimal_base *= 0.1;
            ptr++;
        }
    }

    // There's also the possibility for an exponent to be provided
    double exponent = (double)sign;
    if (tolower(*ptr) == 'e') {
        ptr++;

        // This exponent has its own sign
        int exp_sign = 1; // assume +
        if (*ptr == '-') {
            exp_sign = -1;
            ptr++;
        } else if (*ptr == '+') {
            ptr++;
        }

        // Now we should start getting the exponent. Exponents are strictly integers
        int exp_int = 0;
        while (*ptr && isdigit(*ptr)) {
            exp_int *= 10;
            exp_int += (*ptr - '0');
            ptr++;
        }

        // Pow!
        exponent = pow(10.0, (double)(exp_int * exp_sign));
    }


    // Handle endptr
    if (endptr) *endptr = ptr;

    return ((double)normal_part + decimal_part) * exponent;
}