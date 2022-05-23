#pragma once
#include <stdint.h>     // uintptr_t
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __Offset
{
	uintptr_t addr;
    intptr_t rva;
	uintptr_t val;
} Offset;

typedef struct __GameOffsets {

#undef OFFSET
#define OFFSET(_type, _var, _default, _pattern, _is_str, _occurence, _ref_occur, _cbArr, _verify)		\
	Offset _var;
#ifdef __APPLE__
#include "offsets/offsets_osx.def"
#else
#include "offsets/offsets.def"
#endif
#undef OFFSET

} GameOffsets;

extern GameOffsets g_offsets;
extern const GameOffsets offsets_default;

bool scanOffsets(int pid, const char *module, bool print);

#ifdef __cplusplus
}
#endif
