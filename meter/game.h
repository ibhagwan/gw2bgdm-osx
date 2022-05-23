#pragma once
#include "core/types.h"
#include "core/message.h"
#include "meter/enums.h"
#include "meter/offsets.h"
#if defined(_WIN32) || defined (_WIN64)
#pragma warning (push)
#pragma warning (disable: 4201)
#include "dxsdk/d3dx9math.h"
#pragma warning (pop)
#else
#include <pthread.h>
#include <sys/types.h>
#endif

// Agent types.
#define AGENT_TYPE_CHAR 0
#define AGENT_TYPE_GADGET 10
#define AGENT_TYPE_ATTACK 11
#define AGENT_TYPE_ITEM 15

// Breakbar states.
#define BREAKBAR_NONE -1
#define BREAKBAR_READY 0
#define BREAKBAR_REGEN 1
#define BREAKBAR_BLOCK 2

// Char attitudes.
#define CHAR_ATTITUDE_FRIENDLY 0
#define CHAR_ATTITUDE_HOSTILE 1
#define CHAR_ATTITUDE_INDIFFERENT 2
#define CHAR_ATTITUDE_NEUTRAL 3

// Char statuses.
#define CHAR_STATUS_ALIVE 0
#define CHAR_STATUS_DEAD 1
#define CHAR_STATUS_DOWN 2

// Char Inventory slots.
#define CHAR_INVENTORY_INVALID_SLOT -1	

// Map types.
#define MAP_TYPE_REDIRECT 0
#define MAP_TYPE_CREATE 1
#define MAP_TYPE_PVP 2
#define MAP_TYPE_GVG 3
#define MAP_TYPE_INSTANCE 4
#define MAP_TYPE_PUBLIC 5
#define MAP_TYPE_TOURNAMENT 6
#define MAP_TYPE_TUTORIAL 7
#define MAP_TYPE_WVW_EB 8
#define MAP_TYPE_WVW_BB 9
#define MAP_TYPE_WVW_GB 10
#define MAP_TYPE_WVW_RB 11
#define MAP_TYPE_WVW_FV 12
#define MAP_TYPE_WVW_OS 13
#define MAP_TYPE_WVW_EOTM 14
#define MAP_TYPE_WVW_EOTM2 15

#define MAP_ID_GILDED_HOLLOW 1121
#define MAP_ID_LOST_PRECIPICE 1124

// Squad information
#define SQUAD_SIZE_MAX 100
#define SQUAD_MAX_SUBGROUPS 15


// Update frequency for closest entites, in microseconds.
#define CLOSEST_UPDATE_FREQ 1000000

// DPS interval for current timing, in microseconds.
#define DPS_INTERVAL_1 10000000
#define DPS_INTERVAL_2 30000000

// Maximum inactive age in microseconds before a target is dropped from the array.
#define MAX_TARGET_AGE 180000000

// Maximum number of recorded DPS hits.
#define MAX_HITS 65536

// Maximum number of players for the player array.
#define MAX_PLAYERS 16384

// Maximum number of DPS targets.
#define MAX_TARGETS 256

// Debug buffer size.
#define MAX_BUF 16384

// The time before a mob resets when at full HP.
#define RESET_TIMER 10000000


// A resizable array.
typedef struct Array
{
	u64 data;
	u32 max;
	u32 cur;
} Array;

// Buffs have a different type of array
typedef struct Dict
{
	u32 max;
	u32 cur;
	u64 data;
} Dict;


// PortalData
typedef struct PortalData
{
    bool is_weave;
    u32 map_id;
    u32 shard_id;
    vec3 tpos;
    vec3 apos;
#if defined(_WIN32) || defined (_WIN64)
    vec3 mpos;
#endif
    i64 time;
} PortalData;

// CombatData
typedef struct CombatData
{
#if defined(_WIN32) || defined (_WIN64)
    volatile LONG lock;
#else
    pthread_mutex_t lock;
#endif
    
    volatile long lastKnownCombatState;
    volatile long damageOut;
    volatile long damageIn;
    volatile long healOut;
    bool lastKnownDownState;
    u32 noDowned;
    i64 begin;
    i64 end;
    i64 update;
    f32 duration;
    u32 damage_out;
    
    // buff uptime
    f32 vigor_dura;
    f32 swift_dura;
    f32 stab_dura;
    f32 retal_dura;
    f32 resist_dura;
    f32 regen_dura;
    f32 aegis_dura;
    f32 warEA_dura;
    f32 revNR_dura;
    f32 revAP_dura;
    f32 necVA_dura;
    f32 sun_dura;
    f32 frost_dura;
    f32 storm_dura;
    f32 stone_dura;
    f32 spot_dura;
    f32 engPPD_dura;
    f32 sclr_dura;
    f32 prot_dura;
    f32 quik_dura;
    f32 alac_dura;
    f32 fury_dura;
    f32 might_dura;
    f32 might_avg;
    f32 gotl_dura;
    f32 gotl_avg;
    f32 glem_dura;
    f32 bans_dura;
    f32 band_dura;
    f32 bant_dura;
    f32 seaw_dura; // Seaweed salad food buff
    f32 revRD_dura;
    f32 eleSM_dura;
    f32 grdSIN_dura;
    
    // Last combat frame position
    // for calculating seaweed buff
    // since we calculate it every 500ms
    // we need a separate duration & update vars
    f32 duration500;
    i64 update_pos;
    vec3 tpos;
    vec3 apos;
    
} CombatData;

// Buff stacks information
typedef struct BuffStacks {
    u16 vigor;
    u16 swift;
    u16 stab;
    u16 retal;
    u16 resist;
    u16 regen;
    u16 quick;
    u16 alacrity;
    u16 prot;
    u16 fury;
    u16 aegis;
    u16 might;
    u16 necVamp;	// Necro Vampiric Aura
    u16 warEA;		// Warrior Empower Allies
    u16 warTact;	// Warrior Banner of Tactics
    u16 warStr;		// Warrior Banner of Strength
    u16 warDisc;	// Warrior Banner of Discipline
    u16 revNR;		// Revenant Naturalistic Resonance
    u16 revAP;		// Revenant Assasin's Presence
    u16 revRite;	// Revenant Rite of great Dwarf
    u16 sun;		// Ranger Sun Spirit
    u16 frost;		// Ranger Frost Spirit
    u16 storm;		// Ranger Storm Spirit
    u16 stone;		// Ranger Stone Spirit
    u16 spotter;	// Ranger Spotter
    u16 GOTL;		// Druid Grace of the Land
    u16 empGlyph;	// Druid Glyph of Empowerment
    u16 sooMist;	// Elementalist soothing mist
    u16 strInNum;	// Guard Strength in numbers
    u16 engPPD;		// Engineer Pinpoint Distribution
    u16 port_weave;
} BuffStacks;

// Character data.
typedef struct Character
{
    u32 agent_id;
    u32 player_id;
    u64 aptr;
    u64 bptr;
    u64 cptr;
    u64 tptr;
    u64 pptr;
    u64 hptr;
    u64 cbptr;
    u64 wmptr;	// wmAgent
    u64 ctptr;  // contactDef
    u64 gdptr;
    u64 atptr;
    u64 spdef;
    u64 inventory;
    u64 uuid;
    u32 type;
    u32 attitude;
    u32 profession;
    u32 stance;
    i8 race;
    i8 gender;
    bool is_player;
    bool in_combat;
    bool is_clone;
    bool is_alive;
    bool is_downed;
    u32 is_elite;
    u32 is_party;
    u32 subgroup;
    i32 hp_max;
    i32 hp_val;
    i32 bb_state;
    u32 bb_value;
    float energy_cur;
    float energy_max;
    vec3 tpos;
    vec3 apos;
    ClientStats stats;
    unsigned char name[MSG_NAME_SIZE*2];
    const unsigned char *decodedName;
} Character;

typedef struct DPSData
{
    bool is_connected;
    bool in_combat;
    f32 target_time;
    u32 target_dmg;
    f32 time;
    u32 damage;
    u32 damage_in;
    u32 heal_out;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
} DPSData;

typedef struct Player
{
    Character c;
    CombatData cd;
    BuffStacks buffs;
    DPSData dps;
    float distance;
    bool markedForDeletion;
    unsigned char acctName[MSG_NAME_SIZE*4];
} Player;

// DPS hit.
typedef struct DPSHit
{
    i64 time;
    u32 dmg;
    u32 eff_id;
    u32 eff_hash;
} DPSHit;

// DPS target.
typedef struct DPSTarget
{
    u64 aptr;
    u64 wmptr;
    u32 type;
    u32 agent_id;
    u32 shard_id;
    u32 npc_id;
    u32 species_id;
    bool invuln;
    bool isDead;
    i64 duration;
    i64 last_update;
    i64 time_update;
    i64 time_begin;
    i64 time_hit;
    i32 hp_max;
    i32 hp_val;
    i32 hp_last;
    i32 hp_lost;
    u32 tdmg;
    u32 c1dmg;
    u32 c2dmg;
    u32 c1dmg_team;
    u32 c2dmg_team;
    u32 num_hits;
    u32 num_hits_team;
    DPSHit hits[MAX_HITS];
    DPSHit hits_team[MAX_HITS];
    const unsigned char *decodedName;
} DPSTarget;

typedef struct DPSTargetEx
{
    bool invalid;
    bool locked;
    i64 time_sel;
    DPSTarget t;
} DPSTargetEx;


// DPS player data.
typedef struct DPSPlayer
{
    Character *c;
    CombatData *cd;
    bool in_combat;
    f32 target_time;
    u32 target_dmg;
    f32 time;
    u32 damage;
    u32 damage_in;
    u32 heal_out;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    unsigned char name[MSG_NAME_SIZE*2];
} DPSPlayer;

// Buff bar dictionary entry
typedef struct BuffEntry {
	size_t buffId;
	uintptr_t *pBuff;
	size_t hash;
} BuffEntry;

// Character stat structure.
typedef struct CharStats
{
	i32 pow;
	i32 pre;
	i32 tuf;
	i32 vit;
	i32 fer;
	i32 hlp;
	i32 cnd;
	i32 con;
	i32 exp;
} CharStats;

// Infusions
typedef struct __ItemDef
{
	u64 ptr;
	u32 id;
	i32 rarity;
	u32 level;
	const unsigned char *name;
} ItemDef;

// Maximum number of infusions
#define MAX_UPGRADES 10

// EquippedItems
typedef struct __EquipItem
{
	const unsigned char *name;
	u64 ptr;
	i32 type;
	bool is_wep;
	i32 wep_type;
	ItemDef stat_def;
	ItemDef item_def;
	ItemDef skin_def;
	ItemDef upgrade1;
	ItemDef upgrade2;
	ItemDef infus_arr[MAX_UPGRADES];
	i32 infus_len;
} EquipItem;

typedef struct __EquipItems
{
	EquipItem head_aqua;
	EquipItem back;
	EquipItem chest;
	EquipItem boots;
	EquipItem gloves;
	EquipItem head;
	EquipItem leggings;
	EquipItem shoulder;
	EquipItem acc_ear1;
	EquipItem acc_ear2;
	EquipItem acc_ring1;
	EquipItem acc_ring2;
	EquipItem acc_amulet;
	EquipItem wep1_aqua;
	EquipItem wep2_aqua;
	EquipItem wep1_main;
	EquipItem wep1_off;
	EquipItem wep2_main;
	EquipItem wep2_off;
	EquipItem tool_foraging;
	EquipItem tool_logging;
	EquipItem tool_mining;
} EquipItems;

typedef struct __TraitDef
{
	u64 ptr;
	u32 id;
	u32 hash;
	const unsigned char *name;
} TraitDef;

typedef struct __Spec
{
#ifdef __cplusplus
	TraitDef specs[GW2::SPEC_SLOT_END];
	TraitDef traits[GW2::SPEC_SLOT_END][GW2::TRAIT_SLOT_END];
#else
	TraitDef specs[SPEC_SLOT_END];
	TraitDef traits[SPEC_SLOT_END][TRAIT_SLOT_END];
#endif
} Spec;

// Mumble linked memory.
typedef struct LinkedMem
{
	u32	ui_version;
	u32	ui_tick;
	vec3 avatar_pos;
	vec3 avatar_front;
	vec3 avatar_top;
	u16	name[256];
	vec3 cam_pos;
	vec3 cam_front;
	vec3 cam_top;
	u16	identity[256];
	u32	context_len;
	u8 server[28];
	u32 map_id;
	u32 map_type;
	u32 shard_id;
	u32 instance;
	u32 build_id;
	u8 context[208];
	u16 description[2048];
} LinkedMem;

// Mumble data
#ifdef __cplusplus
extern "C" LinkedMem* g_mum_mem;
#else
extern LinkedMem* g_mum_mem;
#endif


// Camera data
typedef struct __CamData
{
	bool valid;
	i64 lastUpdate;
	vec3 camPos;
	vec3 upVec;
	vec3 lookAt;
	vec3 viewVec;
	float fovy;
	float curZoom;
	float minZoom;
	float maxZoom;
} CamData;

#pragma warning (push)
#pragma warning (disable: 4214)
typedef struct __CompassData {
    uintptr_t pComp;
    float width;
    float height;
    float maxWidth;
    float maxHeight;
    int zoom;
    struct {
        unsigned int : 28; // unknown
        bool mouseOver : 1; // true when hovering mouse over minimap
        bool position : 1; // position of compass on screen (top = 1/bottom = 0)
        bool : 1; // unknown (possibly bottom position width snap to skillbar)
        bool rotation : 1; // rotation lock (true if map rotation is enabled)
    } flags;
} CompassData;
#pragma warning (pop)


