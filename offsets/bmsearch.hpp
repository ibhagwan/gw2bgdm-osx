//
//  bmsearch.hpp
//  bgdm
//
//  Created by Bhagwan on 6/28/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef bmsearch_h
#define bmsearch_h

#ifdef __APPLE__
#include <sys/types.h>
#endif

enum {
    NHASH = 256, /* size of state hash table array */
};

template <typename T>
struct bm_hash_item {
    T key;
    size_t* shifts;
    struct bm_hash_item* next; /* next in hash table */
};

static bm_hash_item<char>*      pattern_hash_char[NHASH];   /* hash table of symbols */
static bm_hash_item<char16_t>*  pattern_hash_char16[NHASH]; /* hash table of symbols */
static bm_hash_item<wchar_t>*   pattern_hash_wchar_t[NHASH];/* hash table of symbols */

template <typename T>
inline bm_hash_item<T>** get_pattern_hash()
{
    return NULL;
}

template <>
inline bm_hash_item<char>** get_pattern_hash()
{
    return pattern_hash_char;
}

template <>
inline bm_hash_item<char16_t>** get_pattern_hash()
{
    return pattern_hash_char16;
}

template <>
inline bm_hash_item<wchar_t>** get_pattern_hash()
{
    return pattern_hash_wchar_t;
}

/*!
 * lookup: search for wchar_t; create if requested.
 * \return returns pointer if present or created; NULL if not.
 */
template <typename T>
static struct bm_hash_item<T>* lookup(T key, int create) {
    int h = 0;
    bm_hash_item<T>* result = NULL;
    bm_hash_item<T>** pattern_hash = get_pattern_hash<T>();
    
    h = std::abs(key % NHASH);
    for(result = pattern_hash[h]; result != NULL; result = result->next) {
        if(result->key == key) { /* found it */
            return result;
        }
    }
    if(create) {
        result = (bm_hash_item<T> *)malloc(sizeof(bm_hash_item<T>));
        result->key = key;
        result->shifts = NULL;
        result->next = pattern_hash[h];
        pattern_hash[h] = result;
    }
    return result;
}

/*!
 * Searches substring position in a text
 */
template <typename T>
long long bm_search(const T* text, size_t text_length, size_t start_pos, size_t pattern_length,
                    size_t* other_shifts) {
    size_t pos = start_pos;
    size_t i = 0;
    bm_hash_item<T>* item = NULL;
    size_t shift = 0;
    
    if(pattern_length == 0) {
        return -1;
    }
    
    while(pos <= text_length - pattern_length) {
        for(i = pattern_length - 1; i != -1; --i) {
            item = lookup(text[pos + i], false);
            if(item == NULL) {
                pos += other_shifts[i]; // shifting
                break;
            }
            shift = item->shifts[i];
            if(shift != 0) {
                pos += shift; // shifting
                break;
            }
            if(i == 0) { // we came to the leftmost pattern character so pattern matches
                return pos; // return matching substring start index
            }
        }
    }
    return -1; // Nothing found
}


/*!
 * Does necessary cleanup
 */
template <typename T>
void bm_clean() {
    bm_hash_item<T>* item = NULL;
    bm_hash_item<T>* old_item = NULL;
    bm_hash_item<T>** pattern_hash = get_pattern_hash<T>();
    int i = 0;
    
    for(; i < NHASH; ++i) {
        item = pattern_hash[i];
        if(item == NULL) {
            continue;
        }
        do {
            if(item->shifts != NULL) {
                free(item->shifts);
                item->shifts = NULL;
            }
            old_item = item;
            item = item->next;
            old_item->next = NULL;
            free(old_item);
        } while(item != NULL);
        pattern_hash[i] = NULL;
    }
}


/*!
 * Builds substring shift table
 * @param pattern input pattern to build
 * @param pattern_length input pattern length
 * @param other_shifts result shifts
 */
template <typename T>
void bm_build(const T* pattern, size_t pattern_length, size_t* other_shifts) {
    
    std::basic_string<T> new_pattern;
    std::basic_string<T> tmp;
    T* suffix = NULL;
    T* new_suffix = NULL;
    size_t max_shift = pattern_length;
    size_t ix_pattern = 0;
    size_t ix_shift = 0;
    bm_hash_item<T>* item = NULL;
    bm_hash_item<T>** pattern_hash = get_pattern_hash<T>();
    bool is_starts_with = false;
    size_t i = 0;
    size_t ix_last;
    
    bm_clean<T>();
    
    for(ix_pattern = 0; ix_pattern < pattern_length; ++ix_pattern) {
        lookup(pattern[ix_pattern], true);
    }
    for(ix_pattern = 0; ix_pattern < pattern_length; ++ix_pattern) { // Loop uniqueue keys
        item = lookup(pattern[ix_pattern], false);
        if(item->shifts != NULL) {
            continue;
        }
        item->shifts = (size_t *)malloc(sizeof(size_t) * pattern_length);
        for(ix_shift = 0; ix_shift < pattern_length; ++ix_shift) {
            item->shifts[ix_shift] = max_shift;
        }
    }
    // Calculating other shifts (filling each column from PatternLength - 2 to 0 (from right to left)
    for(ix_pattern = pattern_length - 1; ix_pattern != -1; --ix_pattern) {
        bool found;
        size_t num_suff = 0;
        if(suffix != NULL) {
            free(suffix);
        }
        num_suff = pattern_length - ix_pattern;
        suffix = (T *)malloc(sizeof(T) * num_suff);
        
        memcpy(suffix, pattern + ix_pattern + 1, sizeof(T) * num_suff);
        
        for(i = 0; i < num_suff - 1; ++i) {
            is_starts_with = suffix[i] == pattern[i];
            if(!is_starts_with) {
                break; // not start with
            }
        }
        if(is_starts_with) {
            max_shift = ix_pattern + 1;
        }
        other_shifts[ix_pattern] = max_shift;
        
        //if(new_pattern != NULL) {
        //    free(new_pattern);
        //}
        
        //new_pattern = wcsdup(pattern);
        new_pattern = pattern;
        new_pattern[pattern_length - 1] = 0;
        
        //found = wcsstr(new_pattern, suffix) != NULL || num_suff == 0;
        found = new_pattern.find(suffix) != std::string::npos || num_suff == 0;
        for(i = 0; i < NHASH; ++i) {
            item = pattern_hash[i];
            if(item == NULL) {
                continue;
            }
            do {
                if(found) {
                    size_t new_suffix_len = num_suff + 1;
                    if(new_suffix != NULL) {
                        free(new_suffix);
                    }
                    // the first is key and the second id trailing zero
                    new_suffix = (T *)malloc(sizeof(T) * new_suffix_len);
                    memset(new_suffix, 0, sizeof(T) * new_suffix_len);
                    new_suffix[0] = item->key;
                    memcpy(new_suffix + 1, suffix, num_suff);
                    //tmp = wcsstr(new_pattern, new_suffix);
                    //if(tmp == NULL) {
                    size_t idx = new_pattern.find(new_suffix);
                    tmp = (idx==std::string::npos) ? std::basic_string<T>() : new_pattern.substr(idx);
                    if (tmp.empty()) {
                        item->shifts[ix_pattern] = max_shift;
                    } else {
                        do { // searching last occurrence
                            //ix_last = (size_t)(tmp - new_pattern);
                            //tmp = wcsstr(new_pattern + ix_last + 1, new_suffix);
                            //size_t i1 = tmp.length();
                            //size_t i2 = new_pattern.length();
                            //ix_last = i1 - i2;
                            ix_last = (size_t)(new_pattern.length() - tmp.length());
                            idx = new_pattern.find(new_suffix, ix_last+1);
                            tmp = (idx==std::string::npos) ? std::basic_string<T>() : new_pattern.substr(idx);
                        //} while (tmp != NULL);
                        } while (!tmp.empty());
                        item->shifts[ix_pattern] = ix_pattern - ix_last;
                    }
                }
                if(item->key == pattern[ix_pattern]) {
                    item->shifts[ix_pattern] = 0;
                }
                item = item->next;
            } while(item != NULL);
        }
    }
    
    /*if(new_pattern != NULL) {
     free(new_pattern);
     }*/
    if(suffix != NULL) {
        free(suffix);
    }
    if(new_suffix != NULL) {
        free(new_suffix);
    }
}

#endif /* bmsearch_h */
