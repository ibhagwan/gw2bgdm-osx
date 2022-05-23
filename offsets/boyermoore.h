//
//  boyermoore.h
//  bgdm
//
//  Created by Bhagwan on 6/28/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#pragma once

#ifndef boyermoore_h
#define boyermoore_h

#include <stddef.h>
#include <limits.h>

typedef struct boyer_moore {
    size_t  length;
    char    *pattern;
    size_t  *delta2;
    size_t  delta1[CHAR_MAX];
} boyer_moore_t;

int boyer_moore_compile(boyer_moore_t *bm, char *pat, size_t patlen);
char *boyer_moore_search(char *haystack, size_t len, boyer_moore_t *bm);
void boyer_moore_free(boyer_moore_t *bm);

#endif /* boyermoore_h */
