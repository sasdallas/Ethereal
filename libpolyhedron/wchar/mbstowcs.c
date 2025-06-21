/**
 * @file libpolyhedron/wchar/mbstowcs.c
 * @brief mbstowcs
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Stanislas Orsola
 */

#include <wchar.h>
#include <stdlib.h>

size_t mbstowcs(wchar_t *dest,const char *src,size_t n){
    size_t len = 0;
    while(*src && n > 0){
        int len = mbtowc(dest,src,4);
        if(len < 0)return (size_t)-1;
        src+=len;
        n--;
        dest++;
    }
    if(n > 0)*dest = 0;
    return len;
}


