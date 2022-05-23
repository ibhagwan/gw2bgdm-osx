//
//  autolock.c
//  bgdm
//
//  Created by Bhagwan on 7/4/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include <stdlib.h>
#include <uthash.h>
#include "autolock.h"
#include "core/types.h"
#include "meter/config.h"


const AutoLockTarget g_raid_bosses[] = {
    { 0x3C4E, 0x6F1B6,	"Vale Guardian" },
    { 0x3C45, 0x847A5,	"Gorseval" },
    { 0x3C0F, 0x653AE,	"Sabetha" },
    { 0x3EFB, 0x97981,	"Slothasor" },
    { 0x3ED8, 0x0,		"Berg" },
    { 0x3F09, 0x0,		"Zane" },
    { 0x3EFD, 0x0,		"Narella" },
    { 0x3EF3, 0x0,		"Matthias" },
    { 0x3F6B, 0x0,		"Keep Construct" },
    { 0x3F76, 0xA77AD,	"Xera" },
    { 0x3F9E, 0xADCE0,	"Xera" },
    { 0x432A, 0xB23B6,	"Cairn" },
    { 0x4314, 0xB1ED3,	"Mursaat Overseer" },
    { 0x4324, 0x0,		"Samarog" },
    { 0x4302, 0xB091A,	"Deimos" },
};

struct autolock_entry {
    i32 key;
    bool value;
    UT_hash_handle hh;
};

static struct autolock_entry *g_autolock_raid = NULL;
static struct autolock_entry *g_autolock_custom = NULL;

#pragma warning( push )
#pragma warning( disable : 4127 )

void autolock_set_internal(struct autolock_entry **p_autolock, u32 id, bool autolock)
{
    if (id == 0) return;
    struct autolock_entry *entry = 0;
    HASH_FIND_INT(*p_autolock, &id, entry);
    if (!autolock && entry) {
        HASH_DEL(*p_autolock, entry);
        free(entry);
    }
    else if (autolock && !entry) {
        entry = malloc(sizeof(struct autolock_entry));
        if (entry) {
            entry->key = id;
            entry->value = 0;
            HASH_ADD_INT(*p_autolock, key, entry);
        }
    }
}

void autolock_set(u32 id, bool autolock)
{
    bool isRaid = false;
    for (int i = 0; i < ARRAYSIZE(g_raid_bosses); i++) {
        if (id == g_raid_bosses[i].npc_id) {
            isRaid = true;
            break;
        }
    }
    
    if (isRaid) autolock_set_internal(&g_autolock_raid, id, autolock);
    else autolock_set_internal(&g_autolock_custom, id, autolock);
}

bool autolock_get_internal(struct autolock_entry **p_autolock, u32 id)
{
    if (id == 0) return false;
    struct autolock_entry *entry = 0;
    HASH_FIND_INT(*p_autolock, &id, entry);
    if (entry)
        return true;
    return false;
}

bool autolock_get(u32 id)
{
    if (autolock_get_internal(&g_autolock_raid, id))
        return true;
    return autolock_get_internal(&g_autolock_custom, id);
}

void autolock_iter_internal(struct autolock_entry **p_autolock, autolock_cb cb)
{
    struct autolock_entry* cur, *tmp;
    HASH_ITER(hh, *p_autolock, cur, tmp)
    {
        cb(cur);
    }
}

void autolock_iter(autolock_cb cb, bool is_custom)
{
    autolock_iter_internal(is_custom ? &g_autolock_custom : &g_autolock_raid, cb);
}

#pragma warning( pop )

void autolock_init()
{
    char *autolock_raid = config_get_str(CONFIG_SECTION_GLOBAL, "AutoLockRaid", NULL);
    char *autolock_custom = config_get_str(CONFIG_SECTION_GLOBAL, "AutoLockCustom", NULL);
    
    for (int i = 0; i < 2; i++) {
        char *buf = (i == 0) ? autolock_raid : autolock_custom;
        if (!buf) continue;
        
        size_t len = strlen(buf);
        if (len == 0) continue;
        
        
        i8* token = strtok (buf, "|");
        while (token != NULL)
        {
            autolock_set_internal( (i > 0) ? &g_autolock_custom : &g_autolock_raid, atoi(token), true);
            token = strtok (NULL, "|");
        }
    }
    
    if (autolock_raid) free(autolock_raid);
    if (autolock_custom) free(autolock_custom);
}

void autolock_free_internal(struct autolock_entry **p_autolock)
{
    struct autolock_entry* cur, *tmp;
    HASH_ITER(hh, *p_autolock, cur, tmp)
    {
        HASH_DEL(*p_autolock, cur);
        free(cur);
    }
    *p_autolock = NULL;
}

void autolock_free()
{
    autolock_free_internal(&g_autolock_raid);
    autolock_free_internal(&g_autolock_custom);
}

void autolock_allraid()
{
    // Push all raid bosses
    for (int i = 0; i < ARRAYSIZE(g_raid_bosses); i++) {
        autolock_set_internal(&g_autolock_raid, g_raid_bosses[i].npc_id, true);
    }
}

void autolock_freeraid()
{
    autolock_free_internal(&g_autolock_raid);
}

void autolock_freecustom()
{
    autolock_free_internal(&g_autolock_custom);
}
