//
//  boyermoore.c
//  bgdm
//
//  Created by Bhagwan on 6/28/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#include "boyermoore.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

static inline size_t max(size_t a, size_t b) { return (a < b) ? b : a; }

static inline bool is_prefix(char *word, size_t wordlen, size_t pos)
{
    size_t suffixlen = wordlen - pos;
    return strncmp(word, &word[pos], suffixlen) == 0;
}

static size_t suffix_length(char *word, size_t wordlen, size_t pos)
{
    size_t i;
    for (i = 0; word[pos - i] == word[wordlen - 1 - i] && i < pos; i++);
    return i;
}

static void make_delta1(size_t delta[static CHAR_MAX], char *pat, size_t patlen)
{
    for (size_t i = 0; i < CHAR_MAX; i++)
        delta[i] = patlen;
    for (size_t i = 0; i < patlen - 1; i++)
        delta[(int)pat[i]] = patlen - 1 - i;
}

static void make_delta2(size_t *delta, char *pat, size_t patlen)
{
    size_t last_prefix_index = patlen - 1;
    for (size_t p = patlen; p > 0; p--) {
        if (is_prefix(pat, patlen, p))
            last_prefix_index = p;
        delta[p - 1] = last_prefix_index + patlen - p;
    }
    for (size_t p = 0; p < patlen - 1; p++) {
        size_t slen = suffix_length(pat, patlen, p);
        if (pat[p - slen] != pat[patlen - 1 - slen])
            delta[patlen - 1 - slen] = patlen - 1 - p + slen;
    }
}

int boyer_moore_compile(boyer_moore_t *bm, char *pat, size_t patlen)
{
    bm->delta2 = malloc(sizeof(int) * patlen);
    bm->pattern = strndup(pat, patlen);
    bm->length = patlen;
    make_delta1(bm->delta1, pat, patlen);
    make_delta2(bm->delta2, pat, patlen);
    return 0;
}

char *boyer_moore_search(char *haystack, size_t len, boyer_moore_t *bm)
{
    size_t i = bm->length;
    while (i < len) {
        size_t j = bm->length;
        for (; j > 0 && haystack[i - 1] == bm->pattern[j - 1]; --i, --j);
        if (j == 0)
            return &haystack[i];
        i += max(bm->delta1[(int)haystack[i - 1]], bm->delta2[j - 1]);
    }
    return NULL;
}

void boyer_moore_free(boyer_moore_t *bm)
{
    if (!bm) return;
    if (bm->delta2) free(bm->delta2);
    if (bm->pattern) free(bm->pattern);
}
