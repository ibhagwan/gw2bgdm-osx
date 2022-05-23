#pragma once
#include "core/TargetOS.h"

#if !defined(_WIN32) && !defined (_WIN64)
#define ARRAYSIZE(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#ifndef __cplusplus
#ifndef min
#define min(a,b)                \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b);    \
    _a < _b ? _a : _b; })
#endif

#ifndef max
#define max(a,b)                \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _a : _b; })
#endif
#endif

// Signed integer types.
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long int i64;

// Unsigned integer types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

// Floating point types.
typedef float f32;
typedef double f64;

#ifndef __cplusplus
// Boolean type.
//typedef u32 bool;
//#define false 0
//#define true 1
#include <stdbool.h>
#endif

// Two dimension vector.
typedef struct vec2
{
	f32 x;
	f32 y;
} vec2;

// Three dimension vector.
typedef struct vec3
{
	f32 x;
	f32 y;
	f32 z;
} vec3;

// Four dimension vector.
typedef struct vec4
{
	f32 x;
	f32 y;
	f32 z;
	f32 w;
} vec4;


#if (defined _WIN32) || (defined _WIN64)
#define utf16string std::wstring
#define utf16str    wchar_t*
#define utf16chr    wchar_t
#else
#define utf16string std::basic_string<char16_t>
#define utf16str    char16_t*
#define utf16chr    char16_t
#endif
