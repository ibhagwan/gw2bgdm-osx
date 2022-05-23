#pragma once
#include <stdint.h>
#include "core/TargetOS.h"
#include "core/types.h"
#include "core/segv.h"

#ifdef TARGET_OS_WIN
#pragma warning (push)
#pragma warning (disable: 4201)
#include "dxsdk/d3dx9math.h"
#pragma warning (pop)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Array;
bool Ch_ValidatePtr(uintptr_t cptr, struct Array* ca);

uintptr_t TR_GetHPBarPtr();
bool TR_SetHPChars(int mode);
bool TR_SetHPCharsBright(int mode);

#ifdef __cplusplus
}
#endif
