#include "offsets/offset_scan.h"

GameOffsets g_offsets;

const GameOffsets offsets_default = {

#undef OFFSET
#define OFFSET(_type, _var, _default, _pattern, _is_str, _occurence, _ref_occur, _cbArr, _verify)		\
	._var = { 0, 0, _default },
#ifdef __APPLE__
#include "offsets/offsets_osx.def"
#else
#include "offsets/offsets.def"
#endif
#undef OFFSET
};
