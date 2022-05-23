#pragma once
#include "core/types.h"

#ifdef __cplusplus
extern "C" {
#endif
    
// Creates the time interface.
void time_create(void);

// Returns the current time in microseconds.
i64 time_get(void);
i64 time_now(void);
    
#ifdef __cplusplus
}
#endif
