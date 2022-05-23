//
//  utf.hpp
//  bgdm
//
//  Created by Bhagwan on 6/18/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef utf_h
#define utf_h

#include <string>
#include <locale>
#include <codecvt>

#include <memory>
#include <iostream>
#include <cstdio>


inline std::string std_wchar_to_utf8(const std::wstring &s)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s);
}
inline std::string std_wchar_to_utf8(const wchar_t *s, size_t len)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s, s + len);
}
inline std::wstring std_utf8_to_wchar(const std::string &s)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s);
}
inline std::wstring std_utf8_to_wchar(const char *s, size_t len)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s, s + len);
}
inline std::string std_utf16_to_utf8(const std::basic_string<char16_t> &s)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes(s);
}
inline std::string std_utf16_to_utf8(const char16_t *s, size_t len)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes(s, s + len);
}
inline std::basic_string<char16_t> std_utf8_to_utf16(const std::string &s)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().from_bytes(s);
}
inline std::basic_string<char16_t> std_utf8_to_utf16(const char *s, size_t len)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().from_bytes(s, s + len);
}
template <size_t OUTSIZE>
inline void std_utf16_to_utf8(const std::wstring &s, char(&target)[OUTSIZE])
{
    const auto o = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s);
    std::copy(o.begin(), o.end(), target);
}
template <size_t OUTSIZE>
inline void std_utf8_to_utf16(const std::string &s, wchar_t(&target)[OUTSIZE])
{
    const auto o = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s);
    std::copy(o.begin(), o.end(), target);
}


template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}


#endif /* utf_h */
