//
//  utf.cpp
//  bgdm
//
//  Created by Bhagwan on 6/18/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#include <string.h>
#include "core/utf.h"
#include "core/utf.hpp"

char *utf16_to_utf8(const void* utf16, char* buff, size_t bytes)
{
    if (!utf16 || !buff || bytes == 0)
        return NULL;
    
    std::string utf8 = std_utf16_to_utf8(std::basic_string<char16_t>((const char16_t *)utf16));
    size_t len = std::min(utf8.length(), bytes);
    memcpy(buff, utf8.c_str(), len);
    buff[bytes-1] = 0;
    return buff;
}


void *utf8_to_utf16(const char* utf8, void* buff, size_t bytes)
{
    if (!utf8 || !buff || bytes == 0)
        return NULL;
    
    std::basic_string<char16_t> utf16 = std_utf8_to_utf16(std::string(utf8));
    size_t len = std::min(utf16.length() * sizeof(char16_t), bytes);
    memcpy(buff, utf16.c_str(), len);
    ((char16_t*)buff)[bytes-1] = 0;
    return buff;
}

char *wchar_to_utf8(const wchar_t* utf16, char* buff, size_t bytes)
{
    if (!utf16 || !buff || bytes == 0)
        return NULL;
    
    std::string utf8 = std_wchar_to_utf8(std::wstring(utf16));
    size_t len = std::min(utf8.length(), bytes);
    memcpy(buff, utf8.c_str(), len);
    buff[bytes-1] = 0;
    return buff;
}


wchar_t *utf8_to_wchar(const char* utf8, wchar_t* buff, size_t bytes)
{
    if (!utf8 || !buff || bytes == 0)
        return NULL;
    
    std::wstring utf16 = std_utf8_to_wchar(std::string(utf8));
    size_t len = std::min(utf16.length() * sizeof(wchar_t), bytes);
    memcpy(buff, utf16.c_str(), len);
    buff[bytes-1] = 0;
    return buff;
}
