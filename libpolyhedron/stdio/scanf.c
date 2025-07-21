/**
 * @file libpolyhedron/stdio/scanf.c
 * @brief scanf
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

int vsscanf(const char *restrict str, const char *restrict format, va_list ap) {
    char *p = (char*)format;
    char *s = (char*)str;

    int match_count = 0;

    while (*p) {
        if (!(*s)) return match_count;

        // Handle characters
        if (*p == ' ') {
            // Whitespace character indicates to ignore any whitespace.
            while (*s && isspace(*s)) s++;
        } else if (*p == '%') {
            // Format specifier
            p++;

            // A format specifier for scanf looks like:
            // %[*][width][length]specifier
            int asterisk = 0;
            int width = -1;

            int length = 0; // A table for identifiers on this is available at https://cplusplus.com/reference/cstdio/scanf

            // Check to see if there's an asterisk
            if (*p == '*') { asterisk = 1; p++; }
        
            // Get width
            while (isdigit(*p)) {
                if (width == -1) width = 0;
                width *= 10;
                width += (int)(*p - '0');
                p++;
            }

            // Handle length
            switch (*p) {
                case 'h':
                    p++;
                    if (*p == 'h') {
                        p++;
                        length = 1; 
                    } else {
                        length = 2;
                    }

                    break;
                    
                case 'l':
                    p++;
                    if (*p == 'l') {
                        p++;
                        length = 4;
                    } else {
                        length = 3;
                    }
                    break;

                case 'j':
                    p++;
                    length = 5;
                    break;
            
                case 'z':
                    p++;
                    length = 6;
                    break;
                case 't':
                    p++;
                    length = 7;
                    break;
                case 'L':
                    p++;
                    length = 8;
                    break;
                    
            }

            char *end;
            switch (*p) {
                case 'i':
                case 'd':
                    // Integer/decimal
                    // TODO: Maybe do strtoll on length = 4?
                    long integer = strtol(s, &end, 10);
                    if (end == s) return match_count;
                    s = end;

                    if (asterisk) {
                        match_count++;
                        break;
                    }
                
                    if (length == 1) {
                        *(va_arg(ap, signed int*)) = (signed int)integer;
                    } else if (length == 2) {
                        *(va_arg(ap, short int*)) = (short int)integer;
                    } else if (length == 3) {
                        *(va_arg(ap, long int*)) = (long int)integer;
                    } else if (length == 4) {
                        *(va_arg(ap, long long int*)) = (long long int)integer;
                    } else if (length == 5) {
                        *(va_arg(ap, intmax_t*)) = (intmax_t)integer;
                    } else if (length == 6) {
                        *(va_arg(ap, size_t*)) = (size_t)integer;
                    } else if (length == 7) {
                        *(va_arg(ap, ptrdiff_t*)) = (ptrdiff_t)integer;
                    } else {
                        *(va_arg(ap, int*)) = (int)integer;
                    }

                    match_count++;
                    break;
                    

                case 'u':
                case 'o':
                case 'x':
                case 'p':
                    unsigned long unsign;

                    if (*p == 'u') {
                        unsign = strtoul(s, &end, 10);
                    } else if (*p == 'o') {
                        unsign = strtoul(s, &end, 8);
                    } else {
                        unsign = strtoul(s, &end, 16);
                    }

                    if (end == s) return match_count;
                    s = end;
                    
                    if (asterisk) {
                        match_count++;
                        break;
                    }

                    if (*p == 'p') {
                        *(va_arg(ap, void**)) = (void*)unsign;
                        match_count++;
                        break;
                    }

                    if (length == 1) {
                        *(va_arg(ap, unsigned int*)) = (unsigned int)unsign;
                    } else if (length == 2) {
                        *(va_arg(ap, unsigned char*)) = (unsigned char)unsign;
                    } else if (length == 3) {
                        *(va_arg(ap, unsigned long int*)) = (unsigned long int)unsign;
                    } else if (length == 4) {
                        *(va_arg(ap, unsigned long long int*)) = (unsigned long long int)unsign;
                    } else if (length == 5) {
                        *(va_arg(ap, uintmax_t*)) = (uintmax_t)unsign;
                    } else if (length == 6) {
                        *(va_arg(ap, size_t*)) = (size_t)unsign;
                    } else if (length == 7) {
                        *(va_arg(ap, ptrdiff_t*)) = (ptrdiff_t)unsign;
                    } else {
                        *(va_arg(ap, unsigned int*)) = (unsigned int)unsign;
                    }

                    match_count++;
                    break;

                case 'f':
                case 'e':
                case 'g':
                case 'a':
                    double d = strtod(s, &end);
                    if (end == s) return match_count;
                    s = end;
                    
                    if (asterisk) {
                        match_count++;
                        break;
                    }

                    if (length == 3) {
                        *(va_arg(ap, double*)) = d;
                    } else if (length == 8) {
                        *(va_arg(ap, long double*)) = (long double)d;
                    } else {
                        *(va_arg(ap, float*)) = (float)d;
                    }

                    match_count++;
                    break;

                case 'c':
                    char c = *s++;
                    if (!c) return match_count;

                    // wchar_t not supported
                    // TODO: width
                    if (!asterisk) *(va_arg(ap, char*)) = c;
                    match_count++;
                    break;
                
                case 's':
                    if (!asterisk) {
                        char *vstr = va_arg(ap, char*);

                        size_t left = width;
                        while (*s && !isblank(*s) && !(*s == '\n') && (left || width == -1)) {
                            *vstr = *s;
                            s++;
                            vstr++;
                            if (width != -1) left--;
                        }

                        *vstr = 0;
                    } else {
                        size_t left = width;
                        while (*s && !isblank(*s) && !(*s == '\n') && (left || width == -1)) {
                            s++;
                            if (width != -1) left--;
                        }
                    }

                    match_count++;
                    break;
            

                case '%':
                    if (*s != '%') {
                        return match_count;
                    } else {
                        s++;
                    }

                    break;
            }

        } else {
            // These must be matched exactly
            if (*p == *s) {
                s++;
            } else {
                break;
            }
        }
        
        p++;
    }

    return match_count;
}

int vfscanf(FILE *restrict stream, const char *restrict format, va_list ap) {
    fprintf(stderr, "vfscanf: Unimplemented\n");
    return 0;
}

int vscanf(const char *restrict format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

int fscanf(FILE *restrict stream, const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfscanf(stream, format, ap);
    va_end(ap);
    return r;
}

int scanf(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfscanf(stdin, format, ap);
    va_end(ap);
    return r;
}

int sscanf(const char *restrict str, const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vsscanf(str, format, ap);
    va_end(ap);
    return r;
}