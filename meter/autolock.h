//
//  autolock.h
//  bgdm
//
//  Created by Bhagwan on 7/4/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef autolock_h
#define autolock_h

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include "core/TargetOS.h"
#include "core/message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AutoLockTarget
{
    uint32_t	npc_id;
    uint32_t	species_id;
    char        name[MSG_NAME_SIZE*2];
} AutoLockTarget;

extern const AutoLockTarget g_raid_bosses[];

// Auto-locking
void autolock_init();
void autolock_free();
void autolock_allraid();
void autolock_freeraid();
void autolock_freecustom();
bool autolock_get(uint32_t id);
void autolock_set(uint32_t id, bool autolock);
typedef void(*autolock_cb)(void *data);
void autolock_iter(autolock_cb cb, bool is_custom);
    
#ifdef __cplusplus
}
#endif

#endif /* autolock_h */
