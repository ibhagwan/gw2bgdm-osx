#include "core/debug.h"
#include "core/logging.h"
#include "core/helpers.h"
#include "core/time.h"
#include "core/file.h"
#include "meter/dps.h"
#include "meter/config.h"
#include "meter/game.h"
#include "meter/gfx.h"
#include "meter/utf.h"
#include "meter/network.h"
#include "meter/renderer.h"
#include "meter/process.h"
#include "meter/fnchooks.h"
#include "meter/updater.h"
#include "meter/ForeignFncs.h"
#include "meter/lru_cache.h"
#include "meter/localdb.h"
#include "meter/imgui_bgdm.h"
#include "meter/resource.h"
#include "meter/WndProc.h"
#include "hook/hook.h"
#include "hook/hook2.h"
#include <Windows.h>
#include <Strsafe.h>
#pragma warning (push)
#pragma warning (disable: 4201)
#include <winternl.h>
#pragma warning (pop)
#include <TlHelp32.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <malloc.h>
#include <uthash.h>


// mod state
State g_state = { 0 };

// game window, acquired on load
HWND g_hWnd;

// Hooked WindowsProc
static LONG_PTR s_oldWindProc = 0;
static bool g_is_dragging = false;

// Data gathering state.
u64 g_client_ctx;
static u64 g_agent_ctx;
static u64 g_agent_av_ctx;
static u64 g_hide_ctx;
static u64 g_map_open_ctx;
static u64 g_ping_ctx;
static u64 g_fps_ctx;
static u64 g_sa_ctx;
static u64 g_ui_ctx;
static u64 g_actionCam_ctx;
u64 g_wv_ctx;
u64 g_codedTextFromHashId;
u64 g_decodeCodedText;
u64 g_getWmAgent;
u64 g_getChatCtx;
u64 g_getContactCtx;
u64 g_getSquadCtx;
u64 g_chat_ctx;
u64 g_contact_ctx;
u64 g_squad_ctx;
u64 g_comp_ctx;
static u64 g_pWndProc;
static u64 g_cbDmgLog;
static u64 g_cbCombatLog;
static u64 g_hkCbtLogResult = 0;
static u64 g_hkDamageResult = 0;
static u64 g_hkGameThread = 0;
static u64 g_hkDamageLog = 0;
static u64 g_hkCombatLog = 0;
static u64 g_hkWndProc = 0;
static u64 g_hkPlCliUser = 0;

// Mumble link.
u32 g_buildId = 0;
LinkedMem* g_mum_mem;
static HANDLE g_mum_link;
static CamData g_cam_data;
static u32 g_shard_id;
static u32 g_map_id;
static u32 g_map_type;

// current player
Player g_player = { 0 };
//Target g_curr_target = { 0 };

// DPS logging.
DPSTarget* g_targets;

// Closest players.
static u64* g_closest_pap;
static u32 g_closest_num;
static wchar_t g_closest_names[MSG_CLOSEST_PLAYERS][MSG_NAME_SIZE];
static ClosePlayer g_closest_players[MSG_CLOSEST_PLAYERS];

// Group DPS.
static u32 g_group_id;
static u32 g_group_num;
static DPSPlayer g_group[MSG_CLOSEST_PLAYERS];

// Timing state.
static bool g_is_init;
static i64 g_time_now;

static __inline bool isCompetitiveDisabled()
{
	static u32 const invalid_maps[] = {
		MAP_TYPE_PVP,
		MAP_TYPE_GVG,
		MAP_TYPE_TOURNAMENT,
		MAP_TYPE_WVW_EB,
		MAP_TYPE_WVW_BB,
		MAP_TYPE_WVW_GB,
		MAP_TYPE_WVW_RB,
		MAP_TYPE_WVW_FV,
		MAP_TYPE_WVW_OS,
		MAP_TYPE_WVW_EOTM,
		MAP_TYPE_WVW_EOTM2
	};

#if (defined _DEBUG) || (defined DEBUG)
	// We only bypass the competitve mode for debugging purposes
	// this can't be shipped with release code
    return false;
#endif

	// Test for competitive maps
	for (u32 i = 0; i < ARRAYSIZE(invalid_maps); ++i)
	{
		if (g_map_type == invalid_maps[i])
		{
			return true;
		}
	}

	return false;
}

u64 dps_get_client_context()
{
	return g_client_ctx;
}

u64 dps_get_av_context()
{
	return g_agent_av_ctx;
}

// Returns the difference between two hp values, allowing for wrap around from 0 to max.
static i32 hp_diff(i32 hp_new, i32 hp_old, i32 max)
{
	i32 diff = hp_old - hp_new;
	if (diff < 0 && diff < -max / 5)
	{
		diff = hp_old + (max - hp_new);
	}

	return diff;
}

// Reads the name from a given player pointer into the given buffer.
static bool read_nameW(wchar_t* dst, u64 pptr, u64 offset)
{
	u64 nptr = process_read_u64(pptr + offset);
	if (nptr == 0)
	{
		dst[0] = 0;
		return false;
	}

	u32 size = MSG_NAME_SIZE * sizeof(wchar_t);
	memset(dst, 0, size);
	process_read(nptr, dst, size);
	dst[MSG_NAME_LAST] = 0;

	/*static const wchar_t *wchar_ = L"中日友好";
	if (pptr == g_player.c.pptr)
		wcscpy(dst, wchar_);*/

	return true;
}

static bool read_name(i8* dst, u64 pptr, u64 offset)
{
	u64 nptr = process_read_u64(pptr + offset);
	if (nptr == 0)
	{
		dst[0] = 0;
		return false;
	}

	wchar_t name[MSG_NAME_SIZE] = { 0 };
	process_read(nptr, name, sizeof(name));
	name[MSG_NAME_LAST] = 0;

	memset(dst, 0, MSG_NAME_SIZE);
	for (u32 c = 0; c < MSG_NAME_LAST && name[c]; ++c)
	{
		dst[c] = (i8)name[c];
	}

	return true;
}


void read_buff_array(uintptr_t cbtBuffBar, struct BuffStacks *stacks)
{
	if (cbtBuffBar == 0) return;

	Dict buff_array = { 0 };
	struct Dict *buff_arr = &buff_array;
	process_read(cbtBuffBar + OFF_BUFFBAR_BUFF_ARR, &buff_array, sizeof(buff_array));

	if (!buff_arr || !stacks)
		return;

	assert(buff_arr->max < 1000);
	if (buff_arr->max >= 4000)
		return;

	for (u32 i = 0; i < buff_arr->max; ++i)
	{
		BuffEntry be;
		process_read(buff_arr->data + i * sizeof(BuffEntry), &be, sizeof(BuffEntry));
		if (be.pBuff)
		{
			i32 efType = process_read_i32((u64)be.pBuff + OFF_BUFF_EF_TYPE);
			switch (efType)
			{
			case(EFFECT_VIGOR):
				++stacks->vigor;
				break;
			case(EFFECT_SWIFTNESS):
				++stacks->swift;
				break;
			case(EFFECT_STABILITY):
				++stacks->stab;
				break;
			case(EFFECT_RETALIATION):
				++stacks->retal;
				break;
			case(EFFECT_RESISTANCE):
				++stacks->resist;
				break;
			case(EFFECT_REGENERATION):
				++stacks->regen;
				break;
			case(EFFECT_QUICKNESS):
				++stacks->quick;
				break;
			case(EFFECT_ALACRITY):
				++stacks->alacrity;
				break;
			case(EFFECT_PROTECTION):
				++stacks->prot;
				break;
			case(EFFECT_FURY):
				++stacks->fury;
				break;
			case(EFFECT_AEGIS):
				++stacks->aegis;
				break;
			case(EFFECT_MIGHT):
				++stacks->might;
				break;
			case(EFFECT_VAMPIRIC_AURA):
				++stacks->necVamp;
				break;
			case(EFFECT_EMPOWER_ALLIES):
				++stacks->warEA;
				break;
			case(EFFECT_BANNER_OF_TACTICS):
				++stacks->warTact;
				break;
			case(EFFECT_BANNER_OF_POWER):
				++stacks->warStr;
				break;
			case(EFFECT_BANNER_OF_DISCIPLINE):
				++stacks->warDisc;
				break;
			case(EFFECT_NATURALISTIC_RESONANCE):
				++stacks->revNR;
				break;
			case(EFFECT_ASSASSINS_PRESENCE):
				++stacks->revAP;
				break;
			case(EFFECT_SUN_SPIRIT):
				++stacks->sun;
				break;
			case(EFFECT_FROST_SPIRIT):
				++stacks->frost;
				break;
			case(EFFECT_STORM_SPIRIT):
				++stacks->storm;
				break;
			case(EFFECT_STONE_SPIRIT):
				++stacks->stone;
				break;
			case(EFFECT_SPOTTER):
				++stacks->spotter;
				break;
			case(EFFECT_GRACE_OF_THE_LAND):
			case(EFFECT_GRACE_OF_THE_LAND_CN):
				++stacks->GOTL;
				break;
			case(EFFECT_GLYPH_OF_EMPOWERMENT):
			case(EFFECT_GLYPH_OF_EMPOWERMENT_CN):
				++stacks->empGlyph;
				break;
			case(EFFECT_RITE_OF_THE_GREAT_DWARF):
				++stacks->revRite;
				break;
			case(EFFECT_SOOTHING_MIST):
				++stacks->sooMist;
				break;
			case(EFFECT_STRENGTH_IN_NUMBERS):
				++stacks->strInNum;
				break;
			case(EFFECT_PINPOINT_DISTRIBUTION):
				++stacks->engPPD;
				break;
			case(EFFECT_PORTAL_WEAVING):
			case(EFFECT_PORTAL_WEAVING2):
				++stacks->port_weave;
			}
		}
	}

	// Fix overstacking for might/gotl
	if (stacks->might > 25) stacks->might = 25;
	if (stacks->GOTL > 5) stacks->GOTL = 5;
}

// Read & calculate combat data
void read_combat_data(Character* c, CombatData *cd, i64 now)
{
	if (!c || !cd)
		return;

	static volatile LONG m_lock = 0;

	while (InterlockedCompareExchange(&m_lock, 1, 0))
	{
	}

	// Combat entry / exit times
	bool was_in_combat = InterlockedAnd(&cd->lastKnownCombatState, 1);
	if (!was_in_combat && c->in_combat)
	{
		bool combat_reenter = false;
		if (g_state.ooc_grace_period > 0) {

			// Adjsut to seconds
			int ooc_grace_period = g_state.ooc_grace_period * 1000000;
			if (now - cd->end <= ooc_grace_period)
				combat_reenter = true;
		}

		if (!combat_reenter) {
			// Combat enter
			if (cd->update) { DBGPRINT(TEXT("[%s] COMBAT ENTER"), c->name); }
			PortalData old_pd = cd->pd;
			memset(cd, 0, sizeof(CombatData));
			cd->begin = now;
			cd->pd = old_pd;
		}
		else {
			if (cd->update) { DBGPRINT(TEXT("[%s] COMBAT REENTER"), c->name); }
		}
	}
	else if (was_in_combat && !c->in_combat)
	{
		// Combat exit
		cd->end = now;
		cd->duration = (cd->end - cd->begin) / 1000.0f;

		DBGPRINT(TEXT("[%s] COMBAT EXIT  duration:%.2f"), c->name, (cd->duration / 1000.0f));
	}

	// Calc total combat time
	if (c->in_combat)
	{
		cd->duration = (now - cd->begin) / 1000.0f;
	}

	// Calc no. of downs
	bool was_downed = cd->lastKnownDownState;
	cd->lastKnownDownState = Ch_IsDowned(c->cptr);
	if (!was_downed && cd->lastKnownDownState) {
		cd->noDowned++;
	}


	// Calc buff combat uptime while
	// in combat & on combat exit
	if ( (c->in_combat || was_in_combat) &&
		cd->update &&
		c->cbptr )
	{
		f32 tick = (now - cd->update) / 1000.0f;

		// Scholar buff uptime
		if (c->hp_val > 0 &&
			c->hp_max > 0 &&
			c->hp_max >= c->hp_val) {
			u32 hpper = (u32)ceilf(c->hp_val / (f32)c->hp_max * 100.0f);
			if (hpper >= 90)
				cd->sclr_dura += tick;
		}

#define SWD_UPDATE_FREQ  500000

		if (cd->update_pos == 0)
			cd->update_pos = -SWD_UPDATE_FREQ;
		if (now - cd->update_pos >= SWD_UPDATE_FREQ)
		{
			f32 swd_tick = (now - cd->update_pos) / 1000.0f;
			cd->update_pos = now;

			// Seaweed salad buff uptime
			D3DXVECTOR4 pos = { 0 };
			/*if (!Ag_GetPos(c->aptr, &pos))*/ {
				pos.x = c->pos.x;
				pos.y = c->pos.y;
				pos.z = c->pos.z;
			}
			if (cd->pos.x == 0 &&
				cd->pos.y == 0 &&
				cd->pos.z == 0) {
				cd->seaw_dura += cd->duration;
				//if (c->pptr == g_player.c.pptr)
				//	DBGPRINT(TEXT("LAST POS INIT  %.2f %.2f %.2f"), pos.x, pos.y, pos.z);
			}
			else if (
				cd->pos.x != pos.x ||
				cd->pos.y != pos.y ||
				cd->pos.z != pos.z) {
				cd->seaw_dura += swd_tick;
				//if (c->pptr == g_player.c.pptr)
				//	DBGPRINT(TEXT("MOVE  dura %.2f total %.2f"), cd->seaw_dura, cd->duration);
			}
			else {
				//if (c->pptr == g_player.c.pptr)
				//	DBGPRINT(TEXT("STAND dura %.2f total %.2f "), cd->seaw_dura, cd->duration);
			}
			memcpy(&cd->pos, &pos, sizeof(cd->pos));
			cd->duration500 = cd->duration;
		}

		u64 buffBarPtr = process_read_u64(c->cbptr + OFF_CMBTNT_BUFFBAR);
		if (buffBarPtr) {

			BuffStacks stacks = { 0 };
			read_buff_array(buffBarPtr, &stacks);
			
			if (stacks.vigor)
				cd->vigor_dura += tick;
			if (stacks.swift)
				cd->swift_dura += tick;
			if (stacks.stab)
				cd->stab_dura += tick;
			if (stacks.retal)
				cd->retal_dura += tick;
			if (stacks.resist)
				cd->resist_dura += tick;
			if (stacks.regen)
				cd->regen_dura += tick;
			if (stacks.aegis)
				cd->aegis_dura += tick;
			if (stacks.necVamp)
				cd->necVA_dura += tick;
			if (stacks.warEA)
				cd->warEA_dura += tick;
			if (stacks.revNR)
				cd->revNR_dura += tick;
			if (stacks.revAP)
				cd->revAP_dura += tick;
			if (stacks.sun)
				cd->sun_dura += tick;
			if (stacks.frost)
				cd->frost_dura += tick;
			if (stacks.storm)
				cd->storm_dura += tick;
			if (stacks.stone)
				cd->stone_dura += tick;
			if (stacks.spotter)
				cd->spot_dura += tick;

			if (stacks.quick)
				cd->quik_dura += tick;
			if (stacks.alacrity)
				cd->alac_dura += tick;
			if (stacks.prot)
				cd->prot_dura += tick;
			if (stacks.fury)
				cd->fury_dura += tick;
			if (stacks.warStr)
				cd->bans_dura += tick;
			if (stacks.warDisc)
				cd->band_dura += tick;
			if (stacks.warTact)
				cd->bant_dura += tick;
			if (stacks.empGlyph)
				cd->glem_dura += tick;

			if (stacks.revRite)
				cd->revRD_dura += tick;
			if (stacks.sooMist)
				cd->eleSM_dura += tick;
			if (stacks.strInNum)
				cd->grdSIN_dura += tick;
			if (stacks.engPPD)
				cd->engPPD_dura += tick;

			cd->might_avg = (cd->might_dura * cd->might_avg + tick * stacks.might) / (cd->might_dura + tick);
			if (stacks.might)
				cd->might_dura += tick;
			if (cd->might_avg > 25)
				cd->might_avg = 25.0f;

			cd->gotl_avg = (cd->gotl_dura * cd->gotl_avg + tick * stacks.GOTL) / (cd->gotl_dura + tick);
			if (stacks.GOTL)
				cd->gotl_dura += tick;
			if (cd->gotl_avg > 5)
				cd->gotl_avg = 5.0f;
		}
	}

#if !(defined BGDM_TOS_COMPLIANT)
	if (&g_player.combat == cd) {

		u64 buffBarPtr = process_read_u64(c->cbptr + OFF_CMBTNT_BUFFBAR);
		if (buffBarPtr) {

			BuffStacks stacks = { 0 };
			read_buff_array(buffBarPtr, &stacks);

			if (stacks.port_weave && !cd->pd.is_weave) {
				cd->pd.is_weave = true;
				cd->pd.time = now;
				cd->pd.pos_trans = c->pos;
				cd->pd.map_id = g_mum_mem->map_id;
				cd->pd.shard_id = g_mum_mem->shard_id;
				cd->pd.pos_mum = g_mum_mem->avatar_pos;
			}
			else if (!stacks.port_weave) {
				cd->pd.is_weave = false;
			}
		}
	}
#endif

	// Save last combat state
	cd->update = now;
	InterlockedExchange(&cd->lastKnownCombatState, c->in_combat);

	InterlockedExchange(&m_lock, 0);
}

// Get character combatant
static void read_char_combatant(Character* c)
{
	if (!c || c->cptr == 0)
		return;

	u64 cptr = c->cptr;

	// Is the character in combat
	c->in_combat = process_read_i32(cptr + OFF_CHAR_FLAGS) & CHAR_FLAG_IN_COMBAT ? 1 : 0;

	// try to get the player ptr
	u64 cptrVt = process_read_u64(cptr);
	typedef uintptr_t(__fastcall*fpVoid)(uintptr_t);
	fpVoid fptrGetCmbtnt = (fpVoid)process_read_u64(cptrVt + OFF_CHAR_VT_GET_CMBTNT);

	if (fptrGetCmbtnt) {
		uint16_t opcode = process_read_u16((u64)fptrGetCmbtnt);
		if (opcode != 0x8548)
			fptrGetCmbtnt = 0;
	}

	if (fptrGetCmbtnt)
		c->cbptr = fptrGetCmbtnt(cptr);
}

static void read_agent_health(Character* c)
{
	if (c->aptr == 0 && c->cptr == 0)
		return;

	if (c->aptr==0 && c->cptr) {
		c->aptr = process_read_u64(c->cptr + OFF_CHAR_AGENT);
		if (c->aptr == 0)
			return;
	}

	if (c->aptr != 0 && c->id == 0) {
		c->id = process_read_u32(c->aptr + OFF_AGENT_ID);
		c->type = process_read_u32(c->aptr + OFF_AGENT_TYPE);
	}

	u64 hptr = 0;
	if (c->type == AGENT_TYPE_ATTACK)
	{
		u64 optr = process_read_u64(c->aptr + OFF_AGENT_GADGET);
		u64 wptr = process_read_u64(optr + OFF_GADGET_WBOSS);
		process_read(wptr + OFF_WBOSS_HP, &hptr, sizeof(hptr));
	}
	else if (c->type == AGENT_TYPE_GADGET)
	{
		u64 optr = process_read_u64(c->aptr + OFF_AGENT_GADGET);
		process_read(optr + OFF_GADGET_HP, &hptr, sizeof(hptr));
	}
	else if (c->cptr)
	{
		process_read(c->cptr + OFF_CHAR_HP, &hptr, sizeof(hptr));
	}

	if (hptr)
	{
		c->hp_val = (i32)process_read_f32(hptr + OFF_HP_VAL);
		c->hp_max = (i32)process_read_f32(hptr + OFF_HP_MAX);
	}
}

// Read character common
static void read_character(Character* c, Array *pa, i64 now)
{
	if (!c || c->aptr == 0 || c->cptr == 0)
	{
		return;
	}

	u64 cptr = c->cptr;

	c->attitude = process_read_u32(cptr + OFF_CHAR_ATTITUDE);
	c->inventory = process_read_u64(cptr + OFF_CHAR_INVENTORY);

	c->bptr = process_read_u64(cptr + OFF_CHAR_BREAKBAR);
	if (c->bptr) {
		c->bb_state = process_read_i32(c->bptr + OFF_BREAKBAR_STATE);
		c->bb_value = (u32)(process_read_f32(c->bptr + OFF_BREAKBAR_VALUE) * 100.0f);
		c->bb_value = min(c->bb_value, 100);
	}

	// Get the combatant (call the combatant VT)
	read_char_combatant(c);

	// try to get the player ptr
	u64 cptrVt = process_read_u64(cptr);
	//u64 csptr = process_read_u64(t->cptr + OFF_CHAR_SUB_CLASS);
	typedef u32(__fastcall*fpUInt)(uintptr_t);
	typedef uintptr_t(__fastcall*fpVoid)(uintptr_t);
	fpUInt fptrIsPlayer = (fpUInt)process_read_u64(cptrVt + OFF_CHAR_VT_IS_PLAYER);
	fpVoid fptrGetPlayer = (fpVoid)process_read_u64(cptrVt + OFF_CHAR_VT_GET_PLAYER);
	fpUInt fptrGetPlayerID = (fpUInt)process_read_u64(cptrVt + OFF_CHAR_VT_GET_PLAYER_ID);

	// Prevent false poritives by checking if the reference occurs in a LEA instruction.
	if (fptrIsPlayer) {
		uint16_t opcode = process_read_u16((u64)fptrIsPlayer);
		if (opcode != 0x918B)
			fptrIsPlayer = 0;
	}
	if (fptrGetPlayer) {
		uint16_t opcode = process_read_u16((u64)fptrGetPlayer + 6);
		if (opcode != 0x8B48)
			fptrGetPlayer = 0;
	}
	if (fptrGetPlayerID) {
		uint16_t opcode = process_read_u16((u64)fptrGetPlayerID + 6);
		if (opcode != 0x5340)
			fptrGetPlayerID = 0;
	}

	if (fptrIsPlayer)
		c->is_player = fptrIsPlayer(cptr);

	if (c->is_player) {

		// Get the player profession
		u64 sptr = process_read_u64(cptr + OFF_CHAR_STATS);
		if (sptr) {
			c->race = process_read_i8(sptr + OFF_STATS_RACE);
			c->gender = process_read_i8(sptr + OFF_STATS_GENDER);
			c->profession = process_read_i32(sptr + OFF_STATS_PROFESSION);
		}

		sptr = process_read_u64(cptr + OFF_CHAR_PROFESSION);
		if (sptr)
			c->stance = process_read_i32(sptr + OFF_PROFESSION_STANCE);

		// For some unknown reason causes a crash
		// and we don't really need it anymore since we can 
		// get the player idx in the player array
		/*if (!c->pptr && fptrGetPlayer) {
			DBGPRINT(TEXT("calling %p GetPlayer(%p)"), (u64)fptrGetPlayer, cptr);
			c->pptr = fptrGetPlayer(cptr);
			DBGPRINT(TEXT("GetPlayer(%p)=%p"), cptr, c->pptr);
		}*/

		if (!c->pptr && pa) {
			u64 playerID = process_read_i32(cptr + OFF_CHAR_PLAYER_ID);
			if ((!playerID || playerID >= MAX_PLAYERS) && fptrGetPlayerID)
			{
				playerID = fptrGetPlayerID(cptr);
				DBGPRINT(TEXT("GetPlayerID(%p)=%d"), cptr, playerID);
			}

			if (playerID && playerID < MAX_PLAYERS)
			{
				u64 pp = g_closest_pap[playerID];
				if (pp != 0)
				{
					u64 cp = process_read_u64(pp + OFF_PLAYER_CHAR);
					if (cp && cp == c->cptr) {
						c->pptr = pp;
					}
				}				
			}
		}

		// As last resort, we can always brute force
		// search the player array for a match
		if (!c->pptr && pa) {
			u32 pan = min(pa->cur, MAX_PLAYERS);
			for (u32 i = 0; i < pan; ++i)
			{
				// Verify that the player is valid.
				u64 pp = g_closest_pap[i];
				if (pp == 0)
					continue;

				// If char ptr is valid and matching target
				// set the player ptr
				u64 cp = process_read_u64(pp + OFF_PLAYER_CHAR);
				if (cp && cp == c->cptr) {
					c->pptr = pp;
					break;
				}
			}
		}
	}

	// Get species def for NPCs
	// Can only be called from main game thread
	/*if (!c->is_player) {
		Ch_GetSpeciesDef(c->cptr, &c->spdef);
	}*/

	// Get the contact def
	if (c->is_player && c->pptr) {
		c->ctptr = CtxContact_GetContact(g_contact_ctx, c->pptr);
		//DBGPRINT(TEXT("pptr %016I64x  contactDef %016I64x"), c->pptr, contactDef);
	}

	// if we have pptr, get the target name from the player class
	// since I can't find the name anymore on the char class
	//read_nameW(c->name, c->pptr ? c->pptr : c->cptr, c->pptr ? OFF_PLAYER_NAME : OFF_CHAR_NAME);
	if (c->pptr) read_nameW(c->name, c->pptr, OFF_PLAYER_NAME);

	// if name wasn't found in the cptr
	// resolve it using the codedName method
	if (c->cptr && c->name[0] == 0) {
		//c->decodedName = lru_find(CLASS_TYPE_CHAR, c->cptr, c->name, ARRAYSIZE(c->name));
		c->decodedName = lru_find(CLASS_TYPE_AGENT, c->aptr, c->name, ARRAYSIZE(c->name));
	}
}

// Read agent common
void read_agent(Character* c, u64 aptr, u64 cptr, Array *pa, i64 now)
{
	if (!c || (aptr == 0 && cptr == 0))
	{
		return;
	}

	if (!aptr && cptr) {
		aptr = process_read_u64(cptr + OFF_CHAR_AGENT);
		if (!aptr)
			return;
	}

	c->aptr = aptr;
	c->cptr = cptr;

	c->id = process_read_u32(aptr + OFF_AGENT_ID);
	c->type = process_read_u32(aptr + OFF_AGENT_TYPE);

	c->tptr = process_read_u64(aptr + OFF_AGENT_TRANSFORM);
	c->pos = process_read_vec3(c->tptr + OFF_TRANSFORM_POS);

	// Read the agent HP data
	read_agent_health(c);

	if (cptr)
		read_character(c, pa, now);
	else {

		// If we don't have the name yet
		// get it from the agentView ptr
		if (c->aptr && c->name[0] == 0) {
			c->decodedName = lru_find(CLASS_TYPE_AGENT, c->aptr, c->name, ARRAYSIZE(c->name));
		}
	}
}


// Reads player state.
static void read_player(Character* player, u64 cptr, u64 pptr, i64 now)
{
	if (cptr == 0)
	{
		return;
	}

	player->pptr = pptr;
	player->is_player = 1;
	read_agent(player, 0, cptr, NULL,  now);

	CharStats stats;
	process_read(process_read_u64(cptr + OFF_CHAR_STATS) + OFF_STATS_BASE, &stats, sizeof(stats));

	player->stats.pow = (u16)stats.pow;
	player->stats.pre = (u16)stats.pre;
	player->stats.tuf = (u16)stats.tuf;
	player->stats.vit = (u16)stats.vit;
	player->stats.fer = (u16)stats.fer;
	player->stats.hlp = (u16)stats.hlp;
	player->stats.cnd = (u16)stats.cnd;
	player->stats.con = (u16)stats.con;
	player->stats.exp = (u16)stats.exp;

	// Refresh current player stats
	// player changed or client requested reset
	if (g_player.c.aptr != player->aptr ||
		is_bind_down(&g_state.bind_dps_reset))
	{
		//memcpy(&g_player.c, player, sizeof(g_player.c));
		memset(&g_player.combat, 0, sizeof(g_player.combat));
	}

	// Always copy latest char
	memcpy(&g_player.c, player, sizeof(g_player.c));


	// Calculate combat state
	read_combat_data(player, &g_player.combat, now);
}


// Reads target state.
void read_target(Character* t, Array* ca, Array *pa, u64 aptr, i64 now)
{
	t->shard = g_shard_id;

	if (aptr == 0)
	{
		return;
	}

	t->id = process_read_u32(aptr + OFF_AGENT_ID);
	t->cptr = process_read_u64(ca->data + t->id * 8);

	read_agent(t, aptr, t->cptr, pa, now);
}

// Get the close players array
bool get_closest_players(ClosePlayer **pArr, u32 *pNum)
{
	if (!pArr || !pNum) return false;

	*pArr = g_closest_players;
	*pNum = g_closest_num;
	return true;
}

static bool read_closest_x(Character* player, Array* pa, i32 closest_ind[], f32 closest_len[], u32 closest_num, int mode)
{
	// If the player array is invalid, then stop.
	if (!pa || pa->data == 0 || pa->cur == 0 || pa->cur > pa->max || closest_num == 0)
	{
		return false;
	}

	for (u32 i = 0; i < closest_num; ++i)
	{
		closest_ind[i] = -1;
		closest_len[i] = FLT_MAX;
	}

	u64 player_cptr = player->cptr;
	vec3 player_pos = player->pos;

	u32 pan = min(pa->cur, MAX_PLAYERS);
	for (u32 i = 0; i < pan; ++i)
	{
		// Verify that the player is valid.
		u64 pp = g_closest_pap[i];
		if (pp == 0)
		{
			continue;
		}

		u64 cptr = process_read_u64(pp + OFF_PLAYER_CHAR);
		if (cptr == 0 || cptr == player_cptr)
		{
			continue;
		}

		u64 aptr = process_read_u64(cptr + OFF_CHAR_AGENT);
		if (aptr == 0)
		{
			continue;
		}

		u64 tptr = process_read_u64(aptr + OFF_AGENT_TRANSFORM);
		if (tptr == 0)
		{
			continue;
		}

		u32 attitude = process_read_u32(cptr + OFF_CHAR_ATTITUDE);
		if (isCompetitiveDisabled() && attitude != CHAR_ATTITUDE_FRIENDLY)
		{
			continue;
		}

		if (mode == 1 && attitude != CHAR_ATTITUDE_FRIENDLY) continue;
		if (mode == 2 && attitude == CHAR_ATTITUDE_FRIENDLY) continue;

		if (g_state.hideNonSquad) {
			u32 subGroup = 0;
			u64 ctptr = CtxContact_GetContact(g_contact_ctx, pp);
			if (ctptr)
				subGroup = Pl_GetSquadSubgroup(g_squad_ctx/*g_agent_av_ctx*/, ctptr);

			if (subGroup == 0)
				continue;
		}
		else if (g_state.hideNonParty) {
			if (!Pl_IsInPartyOrSquad(pp, 0, g_contact_ctx))
				continue;
		}

		// Check the distance to the player, insert or remove the farthest if full.
		vec3 pos;
		process_read(tptr + OFF_TRANSFORM_POS, &pos, sizeof(pos));

		f32 dist =
			(player_pos.x - pos.x) * (player_pos.x - pos.x) +
			(player_pos.y - pos.y) * (player_pos.y - pos.y) +
			(player_pos.z - pos.z) * (player_pos.z - pos.z);

		u32 largest_ind = 0;
		f32 largest_len = closest_len[0];

		for (u32 j = 0; j < closest_num; ++j)
		{
			if (closest_len[j] > largest_len)
			{
				largest_len = closest_len[j];
				largest_ind = j;
			}
		}

		if (dist < closest_len[largest_ind])
		{
			closest_ind[largest_ind] = (i32)i;
			closest_len[largest_ind] = dist;
		}
	}

	return true;
}

u32 read_closest_x_players(Character* player, ClosePlayer closest_arr[], u32 closest_num, i64 now, int mode)
{
	// Get players from the player array.
	static i32 closest_ind[SQUAD_SIZE_MAX] = { 0 };
	static f32 closest_len[SQUAD_SIZE_MAX] = { 0 };

	Array play_array = { 0 };
	process_read(g_client_ctx + OFF_CCTX_PLAYS, &play_array, sizeof(play_array));

	memset(closest_ind, 0, sizeof(closest_ind));
	memset(closest_len, 0, sizeof(closest_len));

	if (closest_num > SQUAD_SIZE_MAX)
		closest_num = SQUAD_SIZE_MAX;

	if (!read_closest_x(player, &play_array, closest_ind, closest_len, closest_num, mode)) {
		return 0;
	}

	u32 idx = 0;
	for (u32 i = 0; i < closest_num; ++i)
	{
		if (closest_ind[i] < 0)
		{
			continue;
		}

		ClosePlayer *cp = &closest_arr[idx];
		cp->c.pptr = g_closest_pap[closest_ind[i]];
		cp->c.cptr = process_read_u64(cp->c.pptr + OFF_PLAYER_CHAR);;

		assert(cp->c.cptr);
		read_agent(&cp->c, 0, cp->c.cptr, &play_array, now);
		read_combat_data(&cp->c, &cp->cd, now);

		++idx;
	}

	return idx;
}

// Updates the closest player names list.
static void read_closest(Character* player, Array* pa, i64 now)
{
	static bool s_reset = false;
	static u32 s_closest_num;
	static ClosePlayer s_closest_players[MSG_CLOSEST_PLAYERS];

	// If the reset keybind is down reset the closest.
	if (is_bind_down(&g_state.bind_dps_reset))
	{
		DBGPRINT(TEXT("RESET CLOSEST"));
		s_reset = true;
		return;
	}

	// Only update occasionally.
	// If user requested reset, refresh immediately
	static i64 last_update = -CLOSEST_UPDATE_FREQ;
	if (!s_reset) {
		if (now - last_update < CLOSEST_UPDATE_FREQ)
		{
			return;
		}
	}

	if (s_reset)
	{
		// Reset was requested
		s_reset = false;
		s_closest_num = 0;
		memset(s_closest_players, 0, sizeof(s_closest_players));
	}
	else
	{
		// Save last update data
		last_update = now;
		s_closest_num = g_closest_num;
		memcpy(s_closest_players, g_closest_players, sizeof(s_closest_players));
	}


	// Reset the closest player list.
	g_closest_num = 0;
	memset(g_closest_names, 0, sizeof(g_closest_names));
	memset(g_closest_players, 0, sizeof(g_closest_players));

	// Get players from the player array.
	i32 closest_ind[MSG_CLOSEST_PLAYERS] = { 0 };
	f32 closest_len[MSG_CLOSEST_PLAYERS] = { 0 };

	if (!read_closest_x(player, pa, closest_ind, closest_len, MSG_CLOSEST_PLAYERS, 0)) {
		return;
	}

	// Read the names of the closest players.
	for (u32 i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
	{
		if (closest_ind[i] < 0)
		{
			continue;
		}
		u64 pptr = g_closest_pap[closest_ind[i]];
		u64 cptr = process_read_u64(pptr + OFF_PLAYER_CHAR);

		ClosePlayer *cp = &g_closest_players[g_closest_num];
		bool found = false;

		// Search for the old combatant
		for (u32 j = 0; j < s_closest_num; ++j)
		{
			if (cptr == s_closest_players[j].c.cptr)
			{
				memcpy(&g_closest_players[g_closest_num], &s_closest_players[j], sizeof(g_closest_players[g_closest_num]));
				found = true;
			}
		}

		if (!found) {
			cp->c.cptr = cptr;
			cp->c.pptr = pptr;
		}

		assert(cp->c.cptr);
		read_agent(&cp->c, 0, cp->c.cptr, pa, now);
		read_combat_data(&cp->c, &cp->cd, now);

		// Fallback to get name from decodedName
		// func inside read_agent (through lru_find())
		if (cp->c.name[0] != 0)
			memcpy(g_closest_names[g_closest_num], cp->c.name, sizeof(g_closest_names[g_closest_num]));
		else if (cp->c.decodedName && cp->c.decodedName[0] != 0)
			StringCchCopyW(g_closest_names[g_closest_num], ARRAYSIZE(g_closest_names[g_closest_num]), cp->c.decodedName);

		// Skip if we have no name
		if (g_closest_names[g_closest_num][0] == 0)
			continue;

		cp->name = &g_closest_names[g_closest_num][0];

		++g_closest_num;
	}
}


// Retrieve a dps target.
DPSTarget* dps_targets_get(u32 i)
{
	if (g_targets)
		return &g_targets[i];
	else
		return NULL;
}

// Deletes a dps target.
static void dps_targets_delete(u32 i)
{
	ImGui_RemoveGraphData(g_targets[i].id);
	memset(&g_targets[i], 0, sizeof(g_targets[i]));
}

// Finds a DPS target. Returns -1 if the target doesn't exist.
static u32 dps_targets_find(u64 target)
{
	if (target == 0)
	{
		return MAX_TARGETS;
	}

	u32 id = process_read_u32(target + OFF_AGENT_ID);
	if (id == 0)
	{
		return MAX_TARGETS;
	}

	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		if (g_targets[i].id == id)
		{
			return i;
		}
	}

	return MAX_TARGETS;
}

// Inserts a DPS target. Returns -1 on failure.
u32 dps_targets_insert(u64 target, u64 cptr, Array* ca)
{
	if (target == 0)
	{
		return MAX_TARGETS;
	}

	i64 old_time = MAXINT64;
	u32 old_index = 0;
	u32 shard_id = g_shard_id;
	u32 id = process_read_u32(target + OFF_AGENT_ID);

	if (id == 0)
	{
		return MAX_TARGETS;
	}

	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		if (g_targets[i].id == id)
		{
			g_targets[i].aptr = target;

			return i;
		}

		if (g_targets[i].time_update < old_time)
		{
			old_index = i;
			old_time = g_targets[i].time_update;
		}
	}

	dps_targets_delete(old_index);

	i64 now = g_time_now;

	if (cptr == 0)
		cptr = Ag_GetChar(target, ca);

	g_targets[old_index].aptr = target;
	g_targets[old_index].cptr = cptr;
	g_targets[old_index].shard = shard_id;
	g_targets[old_index].id = id;
	g_targets[old_index].time_begin = now;
	g_targets[old_index].time_update = now;
	g_targets[old_index].hp_last = -1;

	// Make sure cache doesn't have old names for this target
	lru_delete(CLASS_TYPE_AGENT, target);
	lru_delete(CLASS_TYPE_CHAR, cptr);

	Character c = { 0 };
	c.aptr = target;
	c.cptr = cptr;
	read_agent_health(&c);

	g_targets[old_index].hp_last = c.hp_val;

	return old_index;
}


void dps_targets_reset()
{
	DBGPRINT(TEXT("RESET TARGETS"));
	memset(g_targets, 0, sizeof(*g_targets)*MAX_TARGETS);
	ImGui_ResetGraphData(0);
}

void dps_targets_get_species()
{
	Array char_array;
	process_read(g_client_ctx + OFF_CCTX_CHARS, &char_array, sizeof(char_array));

	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		DPSTarget* t = &g_targets[i];
		if (!t ||
			t->aptr == 0 ||
			t->cptr == 0 ||
			t->species_id != 0 ||
			!Ch_ValidatePtr(t->cptr, &char_array))
			continue;

		uint64_t speciesDef = 0;
		if (Ch_GetSpeciesDef(t->cptr, &speciesDef)) {
			DBGPRINT(TEXT("aptr %p cptr %p speciesDef %p"), t->aptr, t->cptr, speciesDef);
			t->npc_id = process_read_u32(speciesDef + OFF_SPECIES_DEF_ID);
			t->species_id = process_read_u32(speciesDef + OFF_SPECIES_DEF_HASHID);
		}
	}
}


static __inline void hit_array_calc_interval(u32 *dmg, const DPSHit *hit_arr, u32 hit_cnt, i64 interval, i64 now)
{
	*dmg = 0;
	for (i32 i = hit_cnt - 1; i >= 0; --i) {
		if (now - hit_arr[i].time > interval)
			break;
		*dmg += hit_arr[i].dmg;
	}
}

// Removes invalid or old targets from the array.
// Calculates targets dps numbers
static void dps_targets_update(Character* player, Array* ca, i64 now)
{
	static i64 last_update = 0;

	// No need to calc twice for the same frame
	if (now == last_update)
		return;

	// If reset was requested reset all target counters
	if (is_bind_down(&g_state.bind_dps_reset))
	{
		last_update = now;
		dps_targets_reset();
		return;
	}

	bool isPlayerDead = !Ch_IsAlive(player->cptr) && !Ch_IsDowned(player->cptr);
	bool isPlayerOOC = !InterlockedAnd(&g_player.combat.lastKnownCombatState, 1);

	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		DPSTarget* t = &g_targets[i];
		if (!t || t->aptr == 0)
		{
			continue;
		}

		// Remove aged and invalid ptrs
		if (t->aptr == player->aptr ||
			g_shard_id != t->shard ||
			now - t->time_update > MAX_TARGET_AGE ||
			(t->cptr && !Ch_ValidatePtr(t->cptr, ca)))
		{
			DBGPRINT(TEXT("target delete <aptr=%p>"), t->aptr);
			dps_targets_delete(i);
			continue;
		}

		// Target is new  and was never hit, skip
		if (t->num_hits == 0)
			continue;

		// Target is already dead, skip calculation
		if (t->isDead)
			continue;

		Character c = { 0 };
		c.aptr = t->aptr;
		c.cptr = t->cptr;
		read_agent_health(&c);
		// If the mob is at full HP and it's been a while since we hit it, autoreset.
		if (c.hp_val == c.hp_max && now - t->time_hit > RESET_TIMER)
		{
			dps_targets_delete(i);
			continue;
		}

		// We don't calculate invuln time or OOC time
		if (t->invuln || isPlayerDead || isPlayerOOC) {
			goto next_target;
		}

		// Total time always starts from first hit
		if (t->duration == 0 || t->last_update == 0)
			t->duration = now - t->hits[0].time;
		else
			t->duration += now - t->last_update;

		// Update the total DPS and damage.
		if (t->hp_last >= 0)
		{
			i32 diff = hp_diff(c.hp_val, t->hp_last, c.hp_max);
			if (diff > 0) {
				// only calculate a hit if the target lost health
				// otherwise can get buggy if the target uses a healing skill
				t->hp_lost += diff;
				i32 hi = t->num_hits_team;
				if (hi < MAX_HITS)
				{
					t->hits_team[hi].dmg = diff;
					t->hits_team[hi].time = now;
					++t->num_hits_team;
				}
			}
		}

		// The game reports more damage than the target's HP on the last hit
		// for the sake of consistency, equalize both numbers if we did more dmg
		// than the target's HP, shouldn't matter much only for small targets
		// where we are the only attacker
		if (t->hp_lost < (i32)t->tdmg) {
			DBGPRINT(TEXT("target dmg adjust <aptr=%p> <hp_lost=%d> <tdmg=%d>"), t->aptr, t->hp_lost, t->tdmg);
			t->hp_lost = (i32)t->tdmg;
		}

		hit_array_calc_interval(&t->c1dmg, t->hits, t->num_hits, DPS_INTERVAL_1, now);
		hit_array_calc_interval(&t->c2dmg, t->hits, t->num_hits, DPS_INTERVAL_2, now);

		hit_array_calc_interval(&t->c1dmg_team, t->hits_team, t->num_hits_team, DPS_INTERVAL_1, now);
		hit_array_calc_interval(&t->c2dmg_team, t->hits_team, t->num_hits_team, DPS_INTERVAL_2, now);

	next_target:
#ifdef _DEBUG
		bool was_dead = t->isDead;
#endif
		if (t->cptr)
			t->isDead = !Ch_IsAlive(t->cptr) && !Ch_IsDowned(t->cptr);
		else
			t->isDead = c.hp_val <= 0;
#ifdef _DEBUG
		if (!was_dead && t->isDead)
			DBGPRINT(TEXT("target is now dead <aptr=%p> <cptr=%p>"), t->aptr, t->cptr);
#endif
		if (t->hp_max==0) t->hp_max = c.hp_max;
		t->hp_last = c.hp_val;
		t->last_update = now;
	}

	last_update = now;
}

static const char * name_by_profession(u32 prof)
{
	static char* const prof_strings[] = {
		"###",
		"GRD",
		"WAR",
		"ENG",
		"RGR",
		"THF",
		"ELE",
		"MES",
		"NEC",
		"REV",
		"###",
	};

	if (prof > PROFESSION_END)
		return prof_strings[PROFESSION_END];
	else 
		return prof_strings[prof];
}


static u32 color_by_profession(const Character *c, bool bUseColor)
{
	assert(c != NULL);
	if (!c)
		return COLOR_WHITE;

	if (g_state.profColor_disable && !bUseColor) {
		bool isPlayerDead = !Ch_IsAlive(c->cptr) && !Ch_IsDowned(c->cptr);
		if (isPlayerDead)
			return COLOR_RED;
		else if (g_player.c.cptr == c->cptr)
			return COLOR_CYAN;
		else
			return COLOR_WHITE;
	}

	u32 col = COLOR_GRAY;
	switch (c->profession)
	{
	case (PROFESSION_GUARDIAN):
		col = COLOR_CYAN;
		break;
	case (PROFESSION_WARRIOR):
		col = COLOR_ORANGE;
			break;
	case (PROFESSION_ENGINEER):
		col = COLOR_LIGHT_PURPLE;
		break;
	case (PROFESSION_RANGER):
		col = COLOR_GREEN;
		break;
	case (PROFESSION_THIEF):
		col = COLOR_RED;
		break;
	case (PROFESSION_ELEMENTALIST):
		col = COLOR_YELLOW;
		break;
	case (PROFESSION_MESMER):
		col = COLOR_PINK;
		break;
	case (PROFESSION_NECROMANCER):
		col = COLOR_DARK_GREEN;
		break;
	case (PROFESSION_REVENANT):
		col = COLOR_LIGHT_BLUE;
		break;
	case (PROFESSION_NONE):
	case (PROFESSION_END):
	default:
		break;
	}

	return col;
}

static inline u32 color_by_rarity(u32 rarity)
{
	u32 color = COLOR_GRAY;

	switch (rarity) {

	case (ITEM_RARITY_NONE):
		color = COLOR_WHITE;
		break;
	case (ITEM_RARITY_FINE):
		color = COLOR_LIGHT_BLUE;// COLOR_BLUE;
		break;
	case (ITEM_RARITY_MASTER):
		color = COLOR_GREEN;
		break;
	case (ITEM_RARITY_RARE):
		color = COLOR_YELLOW;
		break;
	case (ITEM_RARITY_EXOTIC):
		color = COLOR_ORANGE;
		break;
	case (ITEM_RARITY_ASCENDED):
		color = COLOR_PINK;
		break;
	case (ITEM_RARITY_LEGENDARY):
		color = COLOR_PURPLE;
		break;
	default:
		color = COLOR_GRAY;
		break;
	}

	return color;
}


/*
static u32 color_by_item_quality(u64 itemptr)
{
	u32 color = COLOR_GRAY;

	if (!itemptr)
		return color;

	u64 itemDef = process_read_u64(itemptr + OFF_EQITEM_ITEM_DEF);
	if (!itemDef)
		return color;

	u32 rarity = process_read_u32(itemDef + OFF_ITEM_DEF_RARITY);
	if (rarity>8)
		return color;

	return color_by_rarity(rarity);
}

static u32 stat_id_from_item(u64 itemptr, bool iswep)
{
	if (!itemptr)
		return 0xFFFF;

	u64 statptr = 0;
	if (iswep)
		statptr = process_read_u64(itemptr + OFF_EQITEM_STATS_WEP);
	else
		statptr = process_read_u64(itemptr + OFF_EQITEM_STATS_ARMOR);

	if (!statptr)
		return 0;

	return process_read_u32(statptr + OFF_STAT_ID);
}

static u32 wep_id_from_item(u64 itemptr)
{
	if (!itemptr)
		return WEP_TYPE_NOEQUIP;

	u64 itemdef = process_read_u64(itemptr + OFF_EQITEM_ITEM_DEF);
	if (itemdef) {
		u64 wep = process_read_u64(itemdef + OFF_ITEM_DEF_WEAPON);
		if (wep)
			return process_read_u32(wep + OFF_WEP_TYPE);
	}

	return WEP_TYPE_UNKNOWN;
}

static u32 upgrade_id_from_item(u64 itemptr, u32 type)
{
	if (!itemptr)
		return 0;

	u64 itemDef = 0;
	switch (type)
	{
	case (0):
		itemDef = process_read_u64(itemptr + OFF_EQITEM_UPGRADE_ARMOR);
		break;
	case (1):
		itemDef = process_read_u64(itemptr + OFF_EQITEM_UPGRADE_WEP1);
		break;
	case (2):
		itemDef = process_read_u64(itemptr + OFF_EQITEM_UPGRADE_WEP2);
		break;
	};

	if (itemDef)
		return process_read_u32(itemDef + OFF_ITEM_DEF_ID);

	return 0;
}
*/

static inline bool WeaponIs2H(uint32_t type)
{
	switch (type)
	{
	case (WEP_TYPE_HAMMER):
	case (WEP_TYPE_LONGBOW):
	case (WEP_TYPE_SHORTBOW):
	case (WEP_TYPE_GREATSWORD):
	case (WEP_TYPE_RIFLE):
	case (WEP_TYPE_STAFF):
		return true;
	default:
		return false;
	}
}

static __inline void draw_panel_window(i32 *out_y, i32 gui_x, i32 gui_y, u32 w, u32 h, const wchar_t *title, Panel* panel)
{
	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	//u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 font7_width = renderer_get_font7_x();
	u32 font7_height = renderer_get_font7_y();
	bool useFont7 = (font7_height < font_height);
	if (!useFont7) {
		font7_width = font_width;
		font7_height = font_height;
	}

	renderer_draw_rect(gui_x, gui_y, w, h, COLOR_BLACK_OVERLAY(panel->alpha), &panel->rect);

	renderer_printfW(
		false,
		cur_x + 5, cur_y + 5,
		COLOR_WHITE, title
	);

	panel->cfg.close.top = gui_y + 5;
	panel->cfg.close.bottom = panel->cfg.close.top + font7_height;
	panel->cfg.close.left = gui_x + w - (u32)(font7_width*3.5f);
	panel->cfg.close.right = panel->cfg.close.left + (u32)(font7_width*3.0f);
	if (!g_state.dbg_noclosebtn) {
		renderer_printfW(useFont7, panel->cfg.close.left, panel->cfg.close.top, COLOR_WHITE, L"[x]");
	}

	if (panel->cfg.canLocalizedText) {
		panel->cfg.localText.top = gui_y + (u32)(font_height*1.75f);
		panel->cfg.localText.bottom = panel->cfg.localText.top + font7_height;
		panel->cfg.localText.left = gui_x + w - (u32)(font7_width*14.5f);
		panel->cfg.localText.right = panel->cfg.localText.left + (u32)(font7_width*15.0f);
		renderer_printfW(
			useFont7,
			panel->cfg.localText.left,
			panel->cfg.localText.top,
			COLOR_WHITE, L"Local Text [%c]",
			panel->cfg.useLocalizedText ? 'x' : ' '
		);
	}

	cur_x += font_width;
	cur_y += (u32)(font_height*1.75f);

	if (panel->cfg.tabNo == 0) {
		// No tabs, stop here
		*out_y = cur_y;
		return;
	} else {
		*out_y = cur_y + (u32)(font_height*1.25f);
	}

	u32 selected_tab = panel->enabled - 1;

	for (u32 i = 0; i < panel->cfg.tabNo; i++)
	{
		if (!panel->cfg.tab_name[i] || panel->cfg.tab_name[i][0] == 0)
			continue;

		panel->cfg.tab_rect[i].top = cur_y;
		panel->cfg.tab_rect[i].bottom = cur_y + font7_height;
		panel->cfg.tab_rect[i].left = cur_x;

		renderer_printfW(
			useFont7,
			cur_x, cur_y,
			(i == selected_tab) ? COLOR_CYAN : COLOR_WHITE,
			L"[%s]",
			panel->cfg.tab_name[i]
		);

		cur_x += (u32)(font7_width*(wcslen(panel->cfg.tab_name[i])+2));
		panel->cfg.tab_rect[i].right = cur_x;

		cur_x += font7_width;
	}
}

static __inline const wchar_t *last2Words(const wchar_t *wstr)
{
	if (!wstr)
		return NULL;

	size_t len = wcslen(wstr);
	int w = 0;
	while (--len > 0) {
		if (wstr[len] == ' ')
			if (++w == 2)
				return &wstr[len + 1];
	}
	return wstr;
}

#if !(defined BGDM_TOS_COMPLIANT)

static __inline void draw_item(i32 *gui_x, i32 *gui_y, wchar_t* title, EquipItem *eqItem, u32 *ar, bool bUseLocalizedText, bool printItemName)
{
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 font7_width = renderer_get_font7_x();
	u32 font7_height = renderer_get_font7_y();
	u32 color = RGBA(255, 255, 255, 185);
	i32 cur_x = *gui_x;
	i32 cur_y = *gui_y;
	u32 line = 0;
	bool useFont7 = (font7_height < font_height);

	renderer_printfW(false, cur_x, cur_y, color, L"%s", /*bUseLocalizedText ? eqItem->name : */title);

	line = 0;
	cur_x += (u32)(font_width * (wcslen(title)+1));

	if (!bUseLocalizedText) {
		renderer_printf(false, cur_x, cur_y + (u32)(font_height * line++),
			color_by_rarity(eqItem->item_def.rarity),
			"%s", stat_name_from_id(eqItem->stat_def.id));
	} else {
		renderer_printfW(false, cur_x, cur_y + (u32)(font_height * line++),
			color_by_rarity(eqItem->item_def.rarity),
			L"%.20s %s",
			eqItem->ptr ? (eqItem->stat_def.name ? eqItem->stat_def.name : L"NOSTATS") : L"NOEQUIP",
			printItemName ? (eqItem->name ? eqItem->name : (eqItem->item_def.name ? eqItem->item_def.name : L"")) : L"");
	}

	bool isArmor = false;
	bool isWeapon = false;
	bool isAccessory = false;

	if (eqItem->type < EQUIP_SLOT_ACCESSORY1) {
		isArmor = true;
	} else if (eqItem->type > EQUIP_SLOT_AMULET) {
		isWeapon = true;
	} else {
		isAccessory = true;
	}

	if (isWeapon) {
		// Print weapon type
		if (true/*!bUseLocalizedText*/) {
			renderer_printf(false, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->item_def.rarity), "%s",
				wep_name_from_id(eqItem->wep_type));
		} else {
			renderer_printfW(false, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->item_def.rarity), L"%.20s", eqItem->name);
		}
	}


	if (isWeapon || isArmor ||
		(isAccessory && (eqItem->item_def.rarity < RARITY_ASCENDED || eqItem->infus_len == 0)))
	{
		// Upgrade #1, skip it for ascended accessories
		// as they can't have an upgrade only infusions
		if (!bUseLocalizedText) {
			const char *name = isWeapon ? sigil_name_from_id(eqItem->upgrade1.id) : rune_name_from_id(eqItem->upgrade1.id);
			renderer_printf(useFont7, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->upgrade1.rarity), "%s", name);
		} else {
			renderer_printfW(useFont7, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->upgrade1.rarity), L"%.20s",
				eqItem->upgrade1.name ? last2Words(eqItem->upgrade1.name) : L"NONE");
		}
	}

	if (isWeapon && WeaponIs2H(eqItem->wep_type)) {

		// Upgrade #2 only valid for 2h weapons
		if (!bUseLocalizedText) {
			renderer_printf(useFont7, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->upgrade2.rarity),
				"%s", sigil_name_from_id(eqItem->upgrade2.id));
		} else {
			renderer_printfW(useFont7, cur_x, cur_y + (u32)(font_height * line++),
				color_by_rarity(eqItem->upgrade2.rarity),
				L"%.20s", eqItem->upgrade2.name ? last2Words(eqItem->upgrade2.name) : L"NONE");
		}
	}

	size_t l = 0;
	u32 items_per_line = (isWeapon || bUseLocalizedText) ? 1 : 2;

	for (int i = 0; i < eqItem->infus_len; i++) {

		if (i%items_per_line == 1)
		{
			renderer_printf(useFont7,
				cur_x + (u32)(font7_width * l++),
				cur_y + (u32)(font_height * line), 
				COLOR_GRAY, "|");
		}	
		else if (i%items_per_line == 0)
		{	
			l = 0; 
			if (i > 0)
				++line;
		}

		if (!bUseLocalizedText) {
			const char* name = infusion_name_from_id(eqItem->infus_arr[i].id, ar);
			renderer_printf(useFont7,
				cur_x + (u32)(font7_width * l),
				cur_y + (u32)(font_height * line),
				color_by_rarity(eqItem->infus_arr[i].rarity), "%s", name);
			l += name ? strlen(name) : 0;
		} else {
			renderer_printfW(useFont7,
				cur_x + (u32)(font7_width * l),
				cur_y + (u32)(font_height * line),
				color_by_rarity(eqItem->infus_arr[i].rarity), L"%.20s",
				eqItem->infus_arr[i].name ? eqItem->infus_arr[i].name : L"NONE");
			l += eqItem->infus_arr[i].name ? wcslen(eqItem->infus_arr[i].name) : 4;
		}
	}

	*gui_y = cur_y + (u32)(font_height * ++line);
}

static void draw_inventory(Character *c, i32 gui_x, i32 gui_y, const wchar_t * title, Panel* panel, bool bUseLocalizedText)
{

	if (!c || !c->aptr || !c->inventory)
		return;

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 w = bUseLocalizedText ? (u32)(60.5f*font_width) : (u32)(50.5f*font_width);
	u32 h = bUseLocalizedText ? (u32)(31.5*font_height) : (u32)(30.5*font_height);
	u32 ar_armor = 0;
	u32 ar_wep1m = 0;
	u32 ar_wep1o = 0;
	u32 ar_wep2m = 0;
	u32 ar_wep2o = 0;


	draw_panel_window(&cur_y, gui_x, gui_y, w, h, title, panel);

	cur_x += font_width;
	renderer_printfW(false, cur_x, cur_y, color_by_profession(c, true), L"%s", c->name);

	EquipItems eq = { 0 };
	if (!Ch_GetInventory(c->cptr, &eq))
		return;

	cur_y += (u32)(font_height*1.5f);
	u32 old_y = cur_y;

	draw_item(&cur_x, &cur_y, L"HEAD    :", &eq.head, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"SHOULDER:", &eq.shoulder, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"CHEST   :", &eq.chest, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"GLOVES  :", &eq.gloves, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"LEGS    :", &eq.leggings, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"BOOTS   :", &eq.boots, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"WEP1_M  :", &eq.wep1_main, &ar_wep1m, bUseLocalizedText, false);
	if (!WeaponIs2H(eq.wep1_main.wep_type))
		draw_item(&cur_x, &cur_y, L"WEP1_O  :", &eq.wep1_off, &ar_wep1o, bUseLocalizedText, false);

	cur_y = old_y;
	cur_x += bUseLocalizedText ? (u32)(font_width * 30) : (u32)(font_width * 24);

	draw_item(&cur_x, &cur_y, L"BACK  :", &eq.back, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"EAR1  :", &eq.acc_ear1, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"EAR2  :", &eq.acc_ear2, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"RING1 :", &eq.acc_ring1, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"RING2 :", &eq.acc_ring2, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"AMULET:", &eq.acc_amulet, &ar_armor, bUseLocalizedText, false);
	draw_item(&cur_x, &cur_y, L"WEP2_M:", &eq.wep2_main, &ar_wep2m, bUseLocalizedText, false);
	if (!WeaponIs2H(eq.wep2_main.wep_type))
		draw_item(&cur_x, &cur_y, L"WEP2_O:", &eq.wep2_off, &ar_wep2o, bUseLocalizedText, false);


	u32 ar_main = ar_armor;
	u32 ar_alt = ar_armor;

	if (WeaponIs2H(eq.wep1_main.wep_type)) {
		ar_main += ar_wep1m;
	} else {
		if (eq.wep1_main.ptr)
			ar_main += ar_wep1m;
		else if (!WeaponIs2H(eq.wep2_main.wep_type) && eq.wep2_main.ptr)
			ar_main += ar_wep2m;
		if (eq.wep1_off.ptr)
			ar_main += ar_wep1o;
		else if (!WeaponIs2H(eq.wep2_main.wep_type) && eq.wep2_off.ptr)
			ar_main += ar_wep2o;
	}
	if (WeaponIs2H(eq.wep2_main.wep_type)) {
		ar_alt += ar_wep2m;
	} else {
		if (eq.wep2_main.ptr)
			ar_alt += ar_wep2m;
		else if (!WeaponIs2H(eq.wep1_main.wep_type) && eq.wep1_main.ptr)
			ar_alt += ar_wep1m;
		if (eq.wep2_off.ptr)
			ar_alt += ar_wep2o;
		else if (!WeaponIs2H(eq.wep1_main.wep_type) && eq.wep1_off.ptr)
			ar_alt += ar_wep1o;
	}

	if (ar_main > 0 || ar_alt > 0) {
		renderer_printf(
			false,
			gui_x + (u32)(font_width*27.0f),
			gui_y + (u32)(font_height*3.0f),
			COLOR_PINK,
			"[AR(main|alt): %3d|%3d]", ar_main, ar_alt
		);
	}
}

static void draw_spec(Character *c, i32 gui_x, i32 gui_y, const wchar_t * title, Panel* panel, bool bUseLocalizedText)
{
	static char* const spec_strings[] = {
		"NONE",
		"Dueling",
		"Death Magic",
		"Invoation",
		"Strength",
		"Druid",
		"Explosives",
		"DareDevil",
		"Marksmanship",
		"Retribution",
		"Domination",
		"Tactics",
		"Salvation",
		"Valor",
		"Corruption",
		"Devastation",
		"Radiance",
		"Water",
		"Berserker",
		"Blood Magic",
		"Shadow Arts",
		"Tools",
		"Defense",
		"Inspiration",
		"Illusions",
		"Nature Magic",
		"Earth",
		"Dragon Hunter",
		"Deadly Arts",
		"Alchemy",
		"Skirmishing",
		"Fire",
		"Beastmastery",
		"Survival",
		"Reaper",
		"Crit Strikes",
		"Arms",
		"Arcane",
		"Firearms",
		"Curses",
		"Chronomancer",
		"Air",
		"Zeal",
		"Scrapper",
		"Trickery",
		"Chaos",
		"Virtues",
		"Inventions",
		"Tempest",
		"Honor",
		"Soul Reaping",
		"Discipline",
		"Herald",
		"Spite",
		"Acrobatics",
		"INVALID"
	};

	static wchar_t buf[1025];

	memset(buf, 0, 1025);

	if (!c || !c->aptr || !c->is_player || !c->pptr)
		return;

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 w = (u32)(50.5f*font_width);
	u32 h = (u32)(18.5*font_height);
	int n = 0;


	draw_panel_window(&cur_y, gui_x, gui_y, w, h, title, panel);

	cur_x += font_width;
	renderer_printfW(false, cur_x, cur_y, color_by_profession(c, true), L"%s", c->name);

	cur_y += (u32)(font_height*1.5f);

	Spec spec = { 0 };
	if (!Pl_GetSpec(c->pptr, &spec))
		return;

	for (u32 i = 0; i < SPEC_SLOT_END; ++i)
	{
		if (bUseLocalizedText)
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"%s\n", spec.specs[i].name);
		else
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"%S\n", spec_strings[spec.specs[i].id]);

		for (u32 j = 0; j < TRAIT_SLOT_END; ++j) {

			if (bUseLocalizedText)
				n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"      %s\n", spec.traits[i][j].name);
			else
				n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"      %S\n", trait_name_from_id(spec.specs[i].id, spec.traits[i][j].id));
		}
	}

	renderer_draw_textW(cur_x, cur_y, color, buf, false);

	EquipItems eq = { 0 };
	if (!Ch_GetInventory(c->cptr, &eq))
		return;

	n = 0;
	n = _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| [WEAPONS]\n");

	// TODO: since I'm unable to find the hashId of the weapon type name
	// ifnore the localaized version of weapon names
	// (most likely at mInventory->equipItem->ItemDef+0xF8)
	bUseLocalizedText = false;

	if (eq.wep1_main.ptr)
		if (bUseLocalizedText)
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %s\n", eq.wep1_main.name);
		else 
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %S\n", wep_name_from_id(eq.wep1_main.wep_type));
	if (eq.wep1_off.ptr)
		if (bUseLocalizedText)
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %s\n", eq.wep1_off.name);
		else
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %S\n", wep_name_from_id(eq.wep1_off.wep_type));
	if (eq.wep2_main.ptr)
		if (bUseLocalizedText)
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %s\n", eq.wep2_main.name);
		else
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %S\n", wep_name_from_id(eq.wep2_main.wep_type));
	if (eq.wep2_off.ptr)
		if (bUseLocalizedText)
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %s\n", eq.wep2_off.name);
		else
			n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"| %S\n", wep_name_from_id(eq.wep2_off.wep_type));


	cur_x += (u32)(font_width*37.0f);
	renderer_draw_textW(cur_x, cur_y, COLOR_WHITE, buf, false);
}

static void draw_fashion(Character *c, i32 gui_x, i32 gui_y, const wchar_t * title, Panel* panel, bool bUseLocalizedText)
{
	static wchar_t buf[1025];

	memset(buf, 0, 1025);

	if (!c || !c->aptr || !c->is_player || !c->pptr || !c->inventory)
		return;

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 w = (u32)(50.5f*font_width);
	u32 h = (u32)(18.5*font_height);
	int n = 0;

	draw_panel_window(&cur_y, gui_x, gui_y, w, h, title, panel);

	cur_x += font_width;
	renderer_printfW(false, cur_x, cur_y, color_by_profession(c, true), L"%s", c->name);

	cur_y += (u32)(font_height*1.5f);

	EquipItems eq = { 0 };
	if (!Ch_GetInventory(c->cptr, &eq))
		return;

	/*
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"HEAD  eqItem: ptr:%p\n", eq.head.ptr);
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP1M eqItem: ptr:%p\n", eq.wep1_main.ptr);
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP1O eqItem: ptr:%p\n", eq.wep1_off.ptr);
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP2M eqItem: ptr:%p\n", eq.wep2_main.ptr);
	*/

	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"BACK    : %s\n", eq.back.ptr ? eq.back.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"HEAD    : %s\n", eq.head.ptr ? eq.head.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"SHOULDER: %s\n", eq.shoulder.ptr ? eq.shoulder.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"CHEST   : %s\n", eq.chest.ptr ? eq.chest.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"GLOVES  : %s\n", eq.gloves.ptr ? eq.gloves.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"LEGS    : %s\n", eq.leggings.ptr ? eq.leggings.skin_def.name : L"NONE");
	n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"BOOTS   : %s\n", eq.boots.ptr ? eq.boots.skin_def.name : L"NONE");
	if (eq.wep1_main.ptr)
		n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP1_M  : %s\n", eq.wep1_main.skin_def.name);
	if (eq.wep1_off.ptr)
		n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP1_O  : %s\n", eq.wep1_off.skin_def.name);
	if (eq.wep2_main.ptr)
		n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP2_M  : %s\n", eq.wep2_main.skin_def.name);
	if (eq.wep2_off.ptr)
		n += _snwprintf(buf + n, ARRAYSIZE(buf) - n, L"WEP2_O  : %s\n", eq.wep2_off.skin_def.name);

	renderer_draw_textW(cur_x+font_width, cur_y+font_height, color, buf, false);

}

static void draw_char_inspect(Character *c, Panel* panel, bool bUseLocalizedText)
{
	static const wchar_t* title = L"======   CHARACTER INSPECT   ======";
									
	if (!panel->enabled)
		return;
	else if (panel->enabled == 1)
		draw_inventory(c, panel->pos.x, panel->pos.y, title, panel, panel->cfg.useLocalizedText);
	else if (panel->enabled == 2)
		draw_spec(c, panel->pos.x, panel->pos.y, title, panel, panel->cfg.useLocalizedText);
	else if (panel->enabled == 3)
		draw_fashion(c, panel->pos.x, panel->pos.y, title, panel, panel->cfg.useLocalizedText);
}

// Update inventory inspection
static void module_char_inspect(Character *target, Character *player, bool bUseLocalizedText)
{
	// Check if the target is a valid friendly player character
	if (target
		&& target->aptr
		&& target->is_player
		&& target->type == AGENT_TYPE_CHAR
		&& (!isCompetitiveDisabled() || target->attitude == CHAR_ATTITUDE_FRIENDLY)
		)
	{
		if (g_state.renderImgui)
			ImGui_CharInspect(&g_state.panel_gear_target, target);
		else
			draw_char_inspect(target, &g_state.panel_gear_target, bUseLocalizedText);
	}

	if (player) {
		if (g_state.renderImgui)
			ImGui_CharInspect(&g_state.panel_gear_self, player);
		else
			draw_char_inspect(player, &g_state.panel_gear_self, bUseLocalizedText);
	}
}

#endif

static bool module_toggle_bind(Bind* bind, bool *toggle)
{
	if (!bind || !toggle)
		return false;

	bool key = is_bind_down(bind);

	if (key && bind->toggle == false)
	{
		*toggle = !(*toggle);
		bind->toggle = true;
		return true;
	}
	else if (key == false)
	{
		bind->toggle = false;
	}
	return false;
}


// Toggle the visibility mode of the client panels
// calculations are still done in the background
static void module_toggle_bind_bool(Panel* panel)
{
	if (!panel)
		return;

	bool key = is_bind_down(&panel->bind);

	if (key && panel->bind.toggle == false)
	{
		panel->enabled = !panel->enabled;
		panel->bind.toggle = true;
		config_set_int(panel->section, "Enabled", panel->enabled);
	}
	else if (key == false)
	{
		panel->bind.toggle = false;
	}
}

static void module_toggle_bind_i32(Panel* panel)
{
	if (!panel)
		return;

	bool key = is_bind_down(&panel->bind);

	if (key && panel->bind.toggle == false)
	{
		if (++panel->enabled > panel->cfg.tabNo) {
			panel->enabled = 0;
			panel->mode = 0;
		}
		else {
			panel->mode = panel->enabled - 1;
		}
		panel->bind.toggle = true;
		config_set_int(panel->section, "Enabled", panel->enabled);
		config_set_int(panel->section, "Mode", panel->mode);
	}
	else if (key == false)
	{
		panel->bind.toggle = false;
	}
}


/*typedef struct {
	i32 threadId;
	HHOOK hHook;
} HookData;
static HookData g_mouseHook[256] = { 0 };

LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 ) { //|| nCode != HC_ACTION) {  // do not process message
		i32 threadId = GetCurrentThreadId();
		for (int i = 0; i < 256; i++) {
			if (!g_mouseHook[i].hHook)
				break;
			if (g_mouseHook[i].threadId &&
				g_mouseHook[i].hHook &&
				g_mouseHook[i].threadId == threadId) {
				return CallNextHookEx(g_mouseHook[i].hHook, nCode, wParam, lParam);
			}
		}
	}

	//
	//MSLLHOOKSTRUCT* p = (PMSLLHOOKSTRUCT)lParam;
	//if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)
	//

	//	At this point we don't care what event arrived
	//	Disable all mouse movements from the client until
	//	the keybind is released
	module_drag_drop();
	return 1;
}*/


static void module_click(i32 isBindDown);
static void module_drag_drop(i32 isBindDown);

static LRESULT CALLBACK WndProc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	if (g_is_dragging && is_bind_down(&g_state.bind_wnd_drag))
		uMsg = WM_NULL;

	/*if (s_oldWindProc)
		return ((WNDPROC)s_oldWindProc)(hwnd, uMsg, wParam, lParam);*/

	if (_imp_wndProc)
		return _imp_wndProc(hwnd, uMsg, wParam, lParam);

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/*
void hkWndProc(CpuContext *ctx) {
#ifdef _WIN64
	HWND *phWnd = (HWND*)(&ctx->RCX);
	UINT *pMsg = (UINT*)(&ctx->RDX);
	WPARAM *pwParam = (WPARAM*)(&ctx->R8);
	LPARAM *plParam = (LPARAM*)(&ctx->R9);
#else
	HWND &hWnd = *(HWND*)(ctx->EBP + 0x8);
	UINT &msg = *(UINT*)(ctx->EBP + 0xc);
	WPARAM &wParam = *(WPARAM*)(ctx->EBP + 0x10);
	LPARAM &lParam = *(LPARAM*)(ctx->EBP + 0x14);
#endif

	UNREFERENCED_PARAMETER(phWnd);
	UNREFERENCED_PARAMETER(pwParam);
	UNREFERENCED_PARAMETER(plParam);

	if (!pMsg)
		return;

	if (phWnd && pMsg && pwParam && plParam) {
		if (!ImGui_WndProcHandler(*phWnd, *pMsg, *pwParam, *plParam)) {
			*pMsg = WM_NULL;
			return;
		}
	}

	UINT uMsg = *pMsg;
	if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
		
		for (u32 i = 0; i < MAX_PANELS; ++i)
		{
			Panel *panel = g_state.panel_arr[i];
			if (!panel)
				continue;

			if (is_bind_down(&panel->bind))
				*pMsg = WM_NULL;
		}
	}

	if (g_is_dragging && is_bind_down(&g_state.bind_wnd_drag))
		*pMsg = WM_NULL;
}
*/

static void module_drag_drop(i32 isBindDown)
{
	static POINT last_p = { 0 };
	static LONG *p_x=0, *p_y=0;

	if (g_state.renderImgui)
		return;

	if (isBindDown<0)
		isBindDown = is_bind_down(&g_state.bind_wnd_drag);

	if (isBindDown) {

		POINT p;
		if (GetCursorPos(&p)) {
			void* window = process_get_window();
			if (window)
				ScreenToClient(window, &p);
			if (!g_is_dragging) {

				if (g_state.panel_dps_self.enabled && PtInRect(&g_state.panel_dps_self.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_dps_self.pos.x;
					p_y = &g_state.panel_dps_self.pos.y;
				}
				if (g_state.panel_dps_group.enabled && PtInRect(&g_state.panel_dps_group.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_dps_group.pos.x;
					p_y = &g_state.panel_dps_group.pos.y;
				}
				if (g_state.panel_buff_uptime.enabled && PtInRect(&g_state.panel_buff_uptime.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_buff_uptime.pos.x;
					p_y = &g_state.panel_buff_uptime.pos.y;
				}
				if (g_state.panel_compass.enabled && PtInRect(&g_state.panel_compass.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_compass.pos.x;
					p_y = &g_state.panel_compass.pos.y;
				}
				if (g_state.panel_gear_target.enabled && PtInRect(&g_state.panel_gear_target.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_gear_target.pos.x;
					p_y = &g_state.panel_gear_target.pos.y;
				}
				if (g_state.panel_gear_self.enabled && PtInRect(&g_state.panel_gear_self.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_gear_self.pos.x;
					p_y = &g_state.panel_gear_self.pos.y;
				}
				if (g_state.panel_version.enabled && PtInRect(&g_state.panel_version.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_version.pos.x;
					p_y = &g_state.panel_version.pos.y;
				}
				if (g_state.panel_debug.enabled && g_state.dbg_buf && PtInRect(&g_state.panel_debug.rect, p)) {
					g_is_dragging = true;
					p_x = &g_state.panel_debug.pos.x;
					p_y = &g_state.panel_debug.pos.y;
				}

				// hook the mouse so we can disable further processsing while dragging
				// we don't want the camera to move while we drag
				if (g_is_dragging) {

					/*if (g_hWnd && s_oldWindProc == 0)
						s_oldWindProc = SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

					/*HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
					if (snap)
					{
						int i = 0;
						HHOOK hHook = NULL;
						THREADENTRY32 te;
						te.dwSize = sizeof(te);
						for (BOOL res = Thread32First(snap, &te); res; res = Thread32Next(snap, &te))
						{
							if (te.th32OwnerProcessID != process_get_id())
								continue;

							hHook = SetWindowsHookEx(WH_MOUSE, mouseHookProc, NULL, te.th32ThreadID);
							if (hHook) {
								g_mouseHook[i].hHook = hHook;
								g_mouseHook[i].threadId = te.th32ThreadID;
								if (i++ == 256)
									break;
							}
						}
						CloseHandle(snap);
					}*/
				}
			}
			else
			{
				if (last_p.x >= 0 &&
					last_p.y >= 0 && 
					p_x && p_y)
				{
					LONG newX = (*p_x) - (last_p.x - p.x);
					LONG newY = (*p_y) - (last_p.y - p.y);
					if (newX < 0) newX = 0;
					if (newY < 0) newY = 0;
					(*p_x) = newX;
					(*p_y) = newY;
				}
			}

			last_p.x = p.x;
			last_p.y = p.y;
		}

	}
	else {
		// Reset last known coordinates
		if (g_is_dragging) {
			if (p_x == &g_state.panel_dps_self.pos.x) {
				config_set_int(CONFIG_SECTION_DPS_TARGET, "x", g_state.panel_dps_self.pos.x);
				config_set_int(CONFIG_SECTION_DPS_TARGET, "y", g_state.panel_dps_self.pos.y);
			}
			if (p_x == &g_state.panel_dps_group.pos.x) {
				config_set_int(CONFIG_SECTION_DPS_GROUP, "x", g_state.panel_dps_group.pos.x);
				config_set_int(CONFIG_SECTION_DPS_GROUP, "y", g_state.panel_dps_group.pos.y);
			}
			if (p_x == &g_state.panel_buff_uptime.pos.x) {
				config_set_int(CONFIG_SECTION_BUFF_UPTIME, "x", g_state.panel_buff_uptime.pos.x);
				config_set_int(CONFIG_SECTION_BUFF_UPTIME, "y", g_state.panel_buff_uptime.pos.y);
			}
			else if (p_x == &g_state.panel_compass.pos.x) {
				config_set_int(CONFIG_SECTION_CONMPASS, "x", g_state.panel_compass.pos.x);
				config_set_int(CONFIG_SECTION_CONMPASS, "y", g_state.panel_compass.pos.y);
			}
			else if (p_x == &g_state.panel_gear_target.pos.x) {
				config_set_int(CONFIG_SECTION_GEAR_TARGET, "x", g_state.panel_gear_target.pos.x);
				config_set_int(CONFIG_SECTION_GEAR_TARGET, "y", g_state.panel_gear_target.pos.y);
			}
			else if (p_x == &g_state.panel_gear_self.pos.x) {
				config_set_int(CONFIG_SECTION_GEAR_SELF, "x", g_state.panel_gear_self.pos.x);
				config_set_int(CONFIG_SECTION_GEAR_SELF, "y", g_state.panel_gear_self.pos.y);
			}
			else if (p_x == &g_state.panel_version.pos.x) {
				config_set_int(CONFIG_SECTION_OPTIONS, "x", g_state.panel_version.pos.x);
				config_set_int(CONFIG_SECTION_OPTIONS, "y", g_state.panel_version.pos.y);
			}
			else if (p_x == &g_state.panel_debug.pos.x) {
				config_set_int(CONFIG_SECTION_DBG, "x", g_state.panel_debug.pos.x);
				config_set_int(CONFIG_SECTION_DBG, "y", g_state.panel_debug.pos.y);
			}
			/*if (s_oldWindProc) {
				SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)s_oldWindProc);
				s_oldWindProc = 0;
			}
			/*for (int i = 0; i < 256; i++) {
				if (g_mouseHook[i].hHook) {
					UnhookWindowsHookEx(g_mouseHook[i].hHook);
					g_mouseHook[i].hHook = NULL;
					g_mouseHook[i].threadId = 0;
				}
			}*/
			g_is_dragging = false;
		}
		p_x = 0;
		p_y = 0;
		last_p.x = -1;
		last_p.y = -1;
	}
}

// Module click for sorting
static void module_click(i32 isBindDown)
{
	static char config_sec[32] = { 0 };
	static char config_str[256] = { 0 };
	static bool  s_clicked = false;;
	static POINT s_clickpos;

	if (g_state.renderImgui)
		return;

	if (isBindDown<0)
		isBindDown = is_bind_down(&g_state.bind_wnd_ctrl);

	if (!s_clicked && !isBindDown)
		return;

	if (isBindDown) {

		POINT p = { 0 };
		if (GetCursorPos(&p)) {
			HWND window = (HWND)process_get_window();
			if (window)
				ScreenToClient(window, &p);

			s_clickpos.x = p.x;
			s_clickpos.y = p.y;
			s_clicked = true;
		}
	} else if (s_clicked) {

		// We only handle the event on button up
		s_clicked = false;
		POINT p = { 0 };
		p.x = s_clickpos.x;
		p.y = s_clickpos.y;


		// Check panel close button, localized text click
		// or tab switch
		for (u32 i=0; i<MAX_PANELS; ++i)
		{
			Panel *panel = g_state.panel_arr[i];
			if (!panel)
				continue;

			if (panel->enabled && PtInRect(&panel->rect, p)) {

				if (PtInRect(&panel->cfg.close, p)) {
					panel->enabled = 0;
					config_set_int(panel->section, "Enabled", panel->enabled);
				}

				if (panel->cfg.canLocalizedText && PtInRect(&panel->cfg.localText, p)) {
					panel->cfg.useLocalizedText = !panel->cfg.useLocalizedText;
					config_set_int(panel->section, "LocalizedText", panel->cfg.useLocalizedText);
				}

				for (u32 j = 0; j < panel->cfg.tabNo; ++j)
				{
					if (!panel->cfg.tab_name[j] || panel->cfg.tab_name[j][0] == 0)
						continue;

					if (PtInRect(&panel->cfg.tab_rect[j], p)) {
						panel->enabled = j + 1;
						config_set_int(panel->section, "Enabled", panel->enabled);
					}
				}
			}
		}

		if (g_state.panel_dps_group.enabled && PtInRect(&g_state.panel_dps_group.rect, p)) {

			for (i32 i = 0; i < GDPS_COL_END; ++i)
			{
				if (g_state.panel_dps_group.cfg.col_visible[i] &&
					PtInRect(&g_state.panel_dps_group.cfg.col_hdr[i], p)) {

					g_state.panel_dps_group.cfg.sortCol = i;

					if(g_state.panel_dps_group.cfg.sortColLast != GDPS_COL_END &&
						g_state.panel_dps_group.cfg.sortColLast == g_state.panel_dps_group.cfg.sortCol)
						g_state.panel_dps_group.cfg.asc = !g_state.panel_dps_group.cfg.asc;
					else
						g_state.panel_dps_group.cfg.asc = 0;

					g_state.panel_dps_group.cfg.sortColLast = i;

					DBGPRINT(TEXT("Sorting Group DPS by col[%d] asc:%d"),
						g_state.panel_dps_group.cfg.sortCol,
						g_state.panel_dps_group.cfg.asc
					);
					return;
				}
			}

			if (PtInRect(&g_state.panel_dps_group.cfg.title, p)) {
				g_state.panel_dps_group.cfg.is_expanded = !g_state.panel_dps_group.cfg.is_expanded;
				return;
			}

			if (g_state.panel_dps_group.cfg.is_expanded) {
				bool found = false;
				for (i32 i = GDPS_COL_CLS; i < GDPS_COL_END; ++i)
				{
					if (PtInRect(&g_state.panel_dps_group.cfg.col_cfg[i], p)) {
						g_state.panel_dps_group.cfg.col_visible[i] =
							!g_state.panel_dps_group.cfg.col_visible[i];
						found = true;
						break;
					}
				}
				if (found) {
					int len = 0;
					int count = 0;
					memset(config_sec, 0, sizeof(config_sec));
					memset(config_str, 0, sizeof(config_str));
					for (i32 i = GDPS_COL_CLS; i < GDPS_COL_END; ++i)
					{
						if (g_state.panel_dps_group.cfg.col_visible[i]) {
							if (count > 0)
								len += _snprintf(&config_str[len], sizeof(config_str) - len, "|");
							len += _snprintf(&config_str[len], sizeof(config_str) - len, gdps_col_str[i]);
							++count;
						}
					}
					config_set_str(CONFIG_SECTION_DPS_GROUP, "Columns", config_str);
					return;
				}
			}

		}
		if (g_state.panel_buff_uptime.enabled && PtInRect(&g_state.panel_buff_uptime.rect, p)) {

			for (i32 i = 0; i < BUFF_COL_END; ++i)
			{
				if (g_state.panel_buff_uptime.cfg.col_visible[i] &&
					PtInRect(&g_state.panel_buff_uptime.cfg.col_hdr[i], p)) {

					g_state.panel_buff_uptime.cfg.sortCol = i;

					if (g_state.panel_buff_uptime.cfg.sortColLast != BUFF_COL_END &&
						g_state.panel_buff_uptime.cfg.sortColLast == g_state.panel_buff_uptime.cfg.sortCol)
						g_state.panel_buff_uptime.cfg.asc = !g_state.panel_buff_uptime.cfg.asc;
					else
						g_state.panel_buff_uptime.cfg.asc = 0;

					g_state.panel_buff_uptime.cfg.sortColLast = i;

					DBGPRINT(TEXT("Sorting Buff Uptime by col[%d] asc:%d"),
						g_state.panel_buff_uptime.cfg.sortCol,
						g_state.panel_buff_uptime.cfg.asc
					);
					return;
				}
			}

			if (PtInRect(&g_state.panel_buff_uptime.cfg.title, p)) {
				g_state.panel_buff_uptime.cfg.is_expanded =
					!g_state.panel_buff_uptime.cfg.is_expanded;
				return;
			}

			if (g_state.panel_buff_uptime.cfg.is_expanded) {
				bool found = false;
				for (i32 i = BUFF_COL_CLS; i < BUFF_COL_END; ++i)
				{
					if (PtInRect(&g_state.panel_buff_uptime.cfg.col_cfg[i], p)) {
						g_state.panel_buff_uptime.cfg.col_visible[i] =
							!g_state.panel_buff_uptime.cfg.col_visible[i];
						found = true;
						break;
					}
				}
				if (found) {
					int len = 0;
					int count = 0;
					memset(config_sec, 0, sizeof(config_sec));
					memset(config_str, 0, sizeof(config_str));
					for (i32 i = BUFF_COL_CLS; i < BUFF_COL_END; ++i)
					{
						if (g_state.panel_buff_uptime.cfg.col_visible[i]) {
							if (count > 0)
								len += _snprintf(&config_str[len], sizeof(config_str) - len, "|");
							len += _snprintf(&config_str[len], sizeof(config_str) - len, buff_col_str[i]);
							++count;
						}
					}
					config_set_str(CONFIG_SECTION_BUFF_UPTIME, "Columns", config_str);
					return;
				}
			}
		}
	}
}



// Draws personal DPS.
static void draw_dps_solo(i32 gui_x, i32 gui_y, const DPSData* dps_data, const DPSTargetEx* dps_target)
{
	static char str_num1[32] = { 0 };
	static char str_num2[32] = { 0 };

	if (!dps_data || !dps_target) {
		return;
	}

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 w = 37*font_width;
	u32 h = (g_state.panel_dps_self.enabled>1) ? (u32)(11.5f*font_height) : (u32)(9.0f*font_height);

	if (dps_target->c.name[0])
		h += font_height;

	draw_panel_window(&cur_y, gui_x, gui_y, w, h, L"===== SINGLE TARGET STATS =====", &g_state.panel_dps_self);

	if (dps_target->locked)
		renderer_draw_frame((f32)gui_x + 2, (f32)gui_y + 2, (f32)w - 4, (f32)h - 4, COLOR_RED);

	cur_x += font_width;

	if (dps_target->c.name[0] || dps_target->c.decodedName) {
		renderer_printfW(false, cur_x, cur_y, COLOR_RED, L"%s",
			dps_target->c.decodedName ? dps_target->c.decodedName : dps_target->c.name);
		cur_y += (u32)(font_height*1.25f);
	}

	renderer_printf(false, cur_x, cur_y, COLOR_WHITE, "TIME:");
	u32 pdps = dps_data->tot_dur ? (u32)(dps_data->tot_done / dps_data->tot_dur) : 0;
	u32 tdps = dps_data->tot_dur ? (u32)(dps_data->hp_lost / dps_data->tot_dur) : 0;
	sprintf_num(pdps, str_num1, 32);
	sprintf_num(tdps, str_num2, 32);
	renderer_printf(
		false,
		cur_x, cur_y,
		color,
		"       %dm %.2fs\n"
		"PDPS:  %s\n"
		"TDPS:  %s",
		(int)dps_data->tot_dur / 60,
		fmod(dps_data->tot_dur, 60),
		str_num1,
		str_num2
	);
	cur_x += font_width * 18;

	renderer_printf(false, cur_x, cur_y, COLOR_WHITE, "L10s");
	sprintf_num(dps_data->l1_dur ? (i32)(dps_data->l1_done / dps_data->l1_dur) : 0, str_num1, 32);
	sprintf_num(dps_data->l1_dur ? (i32)(dps_data->l1_done_team / dps_data->l1_dur) : 0, str_num2, 32);
	renderer_printf(
		false,
		cur_x, cur_y,
		color,
		"\n%s\n%s",
		str_num1,
		str_num2
	);
	cur_x += font_width * 9;

	renderer_printf(false, cur_x, cur_y, COLOR_WHITE, "L30s");
	sprintf_num(dps_data->l2_dur ? (i32)(dps_data->l2_done / dps_data->l2_dur) : 0, str_num1, 32);
	sprintf_num(dps_data->l2_dur ? (i32)(dps_data->l2_done_team / dps_data->l2_dur) : 0, str_num2, 32);
	renderer_printf(
		false,
		cur_x, cur_y,
		color,
		"\n%s\n%s",
		str_num1,
		str_num2
	);
	cur_y += (u32)(font_height * 3.25f);

	if (g_state.panel_dps_self.enabled > 1) {

		sprintf_num(dps_data->tot_done, str_num1, 32);
		sprintf_num(dps_data->hp_lost, str_num2, 32);
		cur_x = gui_x + font_width;
		renderer_printf(
			false,
			cur_x, cur_y,
			color,
			"PDMG:  %s\n"
			"TDMG:  %s",
			str_num1,
			str_num2
		);
		cur_x += font_width * 18;

		sprintf_num(dps_data->l1_done, str_num1, 32);
		sprintf_num(dps_data->l1_done_team, str_num2, 32);
		renderer_printf(
			false,
			cur_x, cur_y,
			color,
			"%s\n%s",
			str_num1,
			str_num2
		);
		cur_x += font_width * 9;

		sprintf_num(dps_data->l2_done, str_num1, 32);
		sprintf_num(dps_data->l2_done_team, str_num2, 32);
		renderer_printf(
			false,
			cur_x, cur_y,
			color,
			"%s\n%s",
			str_num1,
			str_num2
		);

		cur_y += (u32)(font_height * 2.25f);
	}

	u32 ttk = (dps_target && tdps > 0) ? dps_target->c.hp_val / tdps : 0;
	renderer_printf(false, gui_x + font_width, cur_y, COLOR_RED,
		"TTK:   %dm %ds", ttk / 60, ttk % 60);
	cur_y += (u32)(font_height*1.5f);

	renderer_printf(
		true,
		gui_x + font_width, cur_y,
		COLOR_WHITE,
		"['%s'=%s  '%s'=Reset]",
		g_state.bind_dps_lock.str,
		dps_target->locked ? "Unlock" : "Lock",
		g_state.bind_dps_reset.str
	);
}

// Draws the group DPS panel.
static void draw_dps_group(Panel *panel, PanelConfig *cfg,
	const DPSTargetEx* target, DPSPlayer* players, u32 num,
	i32 gui_x, i32 gui_y, POINT cursor)
{
	static i8 buf[1025];
	static char str_buf1[32];
	static char str_buf2[32];
	static char str_buf3[32];
	static char str_buf4[32];
	static char str_buf5[32];

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 font7_width = renderer_get_font7_x();
	u32 font7_height = renderer_get_font7_y();
	i32 spacing = (u32)(font_height*0.5f);
	u32 w = (u32)(font_width*26.0f);
	u32 h = (u32)(font_height*(MSG_CLOSEST_PLAYERS + 7));

	if (target)
		h += (u32)(font_height + 1.0f);

	if (cfg->is_expanded)
		h += (u32)(font7_height*5.0f);

	for (i32 i = GDPS_COL_CLS; i < GDPS_COL_END; ++i) {
		if (panel->cfg.col_visible[i]) {
			switch (i) {
			case(GDPS_COL_CLS):
			case(GDPS_COL_PER):
				w += (u32)(font_width*5.0f);
				break;
			case(GDPS_COL_DPS):
			case(GDPS_COL_DMGOUT):
			case(GDPS_COL_DMGIN):
			case(GDPS_COL_HPS):
			case(GDPS_COL_HEAL):
				w += (u32)(font_width*8.0f);
				break;
			case(GDPS_COL_TIME):
				w += (u32)(font_width*10.0f);
				break;
			};
		}
	}

	draw_panel_window(&cur_y, gui_x, gui_y, w, h, L"=== TOTAL COMBAT | GROUP STATS ===", &g_state.panel_dps_group);

	if (target && target->locked) {
		renderer_draw_frame((f32)gui_x + 2, (f32)gui_y + 2, (f32)w - 4, (f32)h - 4, COLOR_RED);
	}

	cur_x += font_width;

	if (target) {

		renderer_printfW(true, cur_x, cur_y, COLOR_WHITE, L"<%s>",
			target->t.invuln ? L"invuln" : 
			(target->locked ? L"locked" : L"unlocked"));
		renderer_printfW(false,
			cur_x + (u32)(font7_width*10.5f),
			cur_y, COLOR_RED, L"%-20s",
			target->c.decodedName ? target->c.decodedName : target->c.name);
		cur_y += (u32)(font_height*1.0f);
	}

	cur_y += (u32)(font_height*0.25f);
	memset(buf, 0, sizeof(buf));

	i32 len = 0;
	i32 lastcol = 0;
	for (i32 i = 0; i < GDPS_COL_END; ++i) {

		if (panel->cfg.col_visible[i])
		{
			panel->cfg.col_hdr[i].top = cur_y;
			panel->cfg.col_hdr[i].bottom = panel->cfg.col_hdr[i].top + font_height;

			switch (i) {
			case(GDPS_COL_NAME):
				panel->cfg.col_hdr[i].left = cur_x + (u32)(font_width*1.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*21.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%-23s", gdps_col_str[i]);
				renderer_draw_text(cur_x + (u32)(font_width*1.0f), cur_y, COLOR_WHITE, buf, false);
				memset(buf, 0, sizeof(buf));
				len = 0;
				break;
			case(GDPS_COL_CLS):
			case(GDPS_COL_PER):
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*2.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*3.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%-5s", gdps_col_str[i]);
				break;
			case(GDPS_COL_DPS):
			case(GDPS_COL_DMGOUT):
			case(GDPS_COL_DMGIN):
			case(GDPS_COL_HPS):
			case(GDPS_COL_HEAL):
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*2.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*6.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%-8s", gdps_col_str[i]);
				break;
			case(GDPS_COL_TIME):
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*2.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*5.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%s", gdps_col_str[i]);
				break;
			};
		}
	}

	if (panel->cfg.col_visible[panel->cfg.sortCol]) {
		renderer_printf(true, panel->cfg.col_hdr[panel->cfg.sortCol].left,
			cur_y - 5, COLOR_WHITE, panel->cfg.asc ? COL_ASC_STR : COL_DESC_STR);
	}
	renderer_draw_text(cur_x + (u32)(font_width*24.0f), cur_y, COLOR_WHITE, buf, false);
	cur_y += font_height;

	memset(buf, 0, sizeof(buf));

	i32 left = cur_x;
	i32 right = cur_x +(u32)(font_width*23.0f);;
	i32 si = -1;

	// Calculate total damage for the % calc
	u32 tot_dmg = 0;
	if (target) {
		tot_dmg = target->c.hp_max;
	} else {
		for (u32 i = 0; i < num; ++i)
			tot_dmg += players[i].damage;
	}

	for (u32 i = 0; i < num; ++i)
	{
		i32 top = cur_y + i * font_height;
		i32 bottom = top + font_height;
		i32 colX = cur_x;

		// Draw the colored player name
		renderer_printfW(
			false,
			cur_x, top,
			color_by_profession(players[i].c, false),
			L"%2d. %-20.20s",
			i+1, players[i].name
		);

		colX += (u32)(font_width*24.0f);
		if (panel->cfg.col_visible[GDPS_COL_CLS]) {

			renderer_printf(
				false,
				colX, top,
				color,
				name_by_profession(players[i].profession));
			colX += (u32)(font_width*5.0f);
		}

		// Draw the stats or line break
		if (players[i].damage == 0 && players[i].time == 0)
		{
			len += snprintf(buf + len, sizeof(buf) - len - 1, "\n");
		}
		else
		{
			len = 0;
			memset(buf, 0, sizeof(buf));

			f32 dur = players[i].time / 1000.0f;
			u32 per = (u32)roundf(players[i].damage / (f32)tot_dmg * 100.0f);
			per = per > 100 ? 100 : per;

			i32 pdps = dur ? (i32)(players[i].damage / dur) : 0;
			i32 phps = dur ? (i32)(players[i].heal_out / dur) : 0;

			if (panel->cfg.col_visible[GDPS_COL_DPS]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1, "%-7s ",
					sprintf_num(pdps, str_buf4, sizeof(str_buf4)));
			}

			if (panel->cfg.col_visible[GDPS_COL_PER]) {
				if (per > 99)
				{
					len += snprintf(buf + len, sizeof(buf) - len - 1, "1#%%  ");
				}
				else
				{
					len += snprintf(buf + len, sizeof(buf) - len - 1, "%2u%%  ", per);
				}
			}


			if (panel->cfg.col_visible[GDPS_COL_DMGOUT]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1, "%-7s ",
					sprintf_num(players[i].damage, str_buf1, sizeof(str_buf1)));
			}

			if (panel->cfg.col_visible[GDPS_COL_DMGIN]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1, "%-7s ",
					sprintf_num(players[i].damage_in, str_buf2, sizeof(str_buf2)));
			}

			if (panel->cfg.col_visible[GDPS_COL_HPS]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1, "%-7s ",
					sprintf_num(phps, str_buf5, sizeof(str_buf5)));
			}

			if (panel->cfg.col_visible[GDPS_COL_HEAL]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1, "%-7s ",
					sprintf_num(players[i].heal_out, str_buf3, sizeof(str_buf3)));
			}

			if (panel->cfg.col_visible[GDPS_COL_TIME]) {
				len += snprintf(buf + len, sizeof(buf) - len - 1,
					"%dm %.2fs\n",
					(int)dur / 60, fmod(dur, 60)
				);
			}

			renderer_draw_text(colX, top, color, buf, false);
		}

#pragma warning( push )  
#pragma warning( disable : 4204 )  
		RECT rect = { .left=left, .top=top, .right=right, .bottom=bottom };
#pragma warning( pop )   
		if (PtInRect(&rect, cursor))
		{
			si = i;
		}
	}

	//cur_x += (u32)(font_width*24.0f);
	//renderer_draw_text(cur_x, cur_y, COLOR_GRAY, buf, false);


	cfg->title.left = cur_x;
	cfg->title.right = cfg->title.left + (u32)(font_width*50.0f);
	cfg->title.top = cur_y + (u32)(font_height*(MSG_CLOSEST_PLAYERS + 1)) + spacing;
	cfg->title.bottom = cfg->title.top + font_height;
	renderer_printf(
		true,
		cfg->title.left, cfg->title.top,
		COLOR_WHITE,
		"[%s %s]",
		g_state.bind_wnd_ctrl.str,
		cfg->is_expanded ? "<<<<" : ">>>>"
	);


	static const int cols_per_line = 2;

	if (cfg->is_expanded)
	{
		len = 0;
		memset(buf, 0, sizeof(buf));

		// Time is always VISIBILE
		for (int i = GDPS_COL_CLS; i < GDPS_COL_END; i++)
		{
			int pos = i - 1;
			int line = (u32)(pos / cols_per_line);
			int crlf = ((pos + 1) % cols_per_line == 0);
			cfg->col_cfg[i].left = cfg->title.left + (u32)(font7_width*16.0f) * (pos  % cols_per_line);
			cfg->col_cfg[i].right = cfg->col_cfg[i].left + (u32)(font7_width*14.0f);
			cfg->col_cfg[i].top = cfg->title.top + font_height + spacing + font7_height * line;
			cfg->col_cfg[i].bottom = cfg->col_cfg[i].top + font7_height;
			len += snprintf(&buf[len], sizeof(buf) - len,
				"[%c] %-12s",
				cfg->col_visible[i] ? 'x' : ' ',
				gdps_col_str[i]
			);

			if (pos > 0 && crlf)
				len += snprintf(&buf[len], sizeof(buf) - len, "\n");
		}

		renderer_draw_text(
			cfg->title.left,
			cfg->title.top + font_height + spacing,
			COLOR_WHITE, buf, true);
	}


	// Draw stat box.
#if !(defined BGDM_TOS_COMPLIANT)
		if (si >= 0)
		{
			w = (u32)(font_width*22.0f);
			h = (u32)(font_height*12.0f);
			cur_x = cursor.x;
			cur_y = cursor.y + spacing;

			renderer_draw_rect(cur_x, cur_y, w, h, COLOR_BLACK, NULL);

			cur_x += font_width;
			cur_y += spacing * 2;
			renderer_printfW(
				false,
				cur_x, cur_y,
				color_by_profession(players[si].c, false),
				L"%-20.20s",
				players[si].name
			);

			cur_y += font_height + spacing;
			renderer_printf(
				false,
				cur_x, cur_y, COLOR_WHITE,
				"POW %u\nPRE %u\nTUF %u\nVIT %u\nFER %u\nCND %u\nHLP %u\nCON %u\nEXP %u",
				players[si].stats.pow,
				players[si].stats.pre,
				players[si].stats.tuf,
				players[si].stats.vit,
				players[si].stats.fer,
				players[si].stats.cnd,
				players[si].stats.hlp,
				players[si].stats.con,
				players[si].stats.exp);
		}
#endif
}


bool get_cam_data(CamData *camData, i64 now)
{
	static volatile LONG m_lock = 0;

	while (InterlockedCompareExchange(&m_lock, 1, 0))
	{
	}

	if (!camData)
		return false;

	if (camData->valid &&
		camData->lastUpdate > 0 &&
		now > 0)
	{
		if (camData->lastUpdate == now) {
			InterlockedExchange(&m_lock, 0);
			return true;
		}
	}

	camData->valid = false;
	if (g_wv_ctx)
	{
		u64 wvctx = process_read_u64(g_wv_ctx);
		if (wvctx)
		{
			int wvctxStatus = process_read_i32(wvctx + OFF_WVCTX_STATUS);
			if (wvctxStatus == 1)
			{
				// TODO: Check why this raises an exception
				// right now it's ok as it's wrapped in try/except clause
				if (WV_GetMetrics(wvctx, 1, &camData->camPos, &camData->lookAt, &camData->upVec, &camData->fovy)) {

					D3DXVECTOR3 viewVec;
					D3DXVec3Subtract(&viewVec, (const D3DXVECTOR3*)&camData->lookAt, (const D3DXVECTOR3*)&camData->camPos);
					D3DXVec3Normalize((D3DXVECTOR3 *)&camData->viewVec, &viewVec);

					camData->curZoom = process_read_f32(wvctx + OFF_CAM_CUR_ZOOM);
					camData->minZoom = process_read_f32(wvctx + OFF_CAM_MIN_ZOOM);
					camData->maxZoom = process_read_f32(wvctx + OFF_CAM_MAX_ZOOM);

					camData->lastUpdate = now;
					camData->valid = true;
				}

				InterlockedExchange(&m_lock, 0);

				return true;
			}
		}
	}

	InterlockedExchange(&m_lock, 0);

	return false;
}

bool dps_update_cam_data()
{
	i64 now = time_get();
	return get_cam_data(&g_cam_data, now);
}


bool project2d(POINT* outPoint, const D3DXVECTOR3 *camPos, const D3DXVECTOR3 *viewVec, const D3DXVECTOR3 *upVec, const D3DXVECTOR3 *worldPt, float fovy)
{
	D3DXVECTOR3 lookAtPosition = { 0.f, 0.f, 0.f };
	D3DXMATRIX matProj, matView, matWorld;
	D3DXVECTOR3 screenPos;
	IDirect3DDevice9* pDevice = renderer_get_dev();
	D3DVIEWPORT9 vp;

	pDevice->lpVtbl->GetViewport(pDevice, &vp);

	D3DXVec3Add(&lookAtPosition, camPos, viewVec);
	D3DXMatrixLookAtLH(&matView, camPos, &lookAtPosition, upVec);
	D3DXMatrixPerspectiveFovLH(&matProj, fovy, (float)vp.Width / (float)vp.Height, 1.0f, 10000.0f);

	D3DXMatrixIdentity(&matWorld);
	D3DXVec3Project(&screenPos, worldPt, &vp, &matProj, &matView, &matWorld);

	if (outPoint)
	{
		outPoint->x = (LONG)(screenPos.x);
		outPoint->y = (LONG)(screenPos.y);
	}

	// Verify Pt is inside the screen
	if (screenPos.x < 0 || screenPos.x > vp.Width ||
		screenPos.y < 0 || screenPos.y > vp.Height || screenPos.z > 1.0f)
		return false;
	else
		return true;
}


float get_mum_fovy()
{
	float ret = 0;
	if (!g_mum_mem)
		return ret;
	wchar_t *pwcFov = wcsstr(g_mum_mem->identity, L"\"fov\":");
	if (pwcFov) {
		wchar_t *pwcEnd = NULL;
		ret = wcstof(pwcFov + 6, &pwcEnd);
	}
	return ret;
}

bool project2d_agent(POINT* outPoint, vec4* outVec, u64 aptr, i64 now)
{
	if (!aptr)
		return false;

	// Update cam data
	// We only call this now from main thread hkGameThread
	// as it will randomly raise exceptions being called
	// outside of main game thread
	//get_cam_data(&g_cam_data, now);

	if (!g_cam_data.valid)
		return false;

	D3DXVECTOR3 vUpVec = { 0.0f, 0.0f, -1.0f }; // Z is UP
	D3DXVECTOR4 worldPt;
	
	if (Ag_GetPos(aptr, &worldPt))
	{
		if (outVec) {
			outVec->x = worldPt.x;
			outVec->y = worldPt.y;
			outVec->z = worldPt.z;
			outVec->w = worldPt.w;
		}

		return project2d(outPoint, (const D3DXVECTOR3*)&g_cam_data.camPos, (const D3DXVECTOR3*)&g_cam_data.viewVec, &vUpVec, (const D3DXVECTOR3*)&worldPt, g_cam_data.fovy);
	}

	return false;
}

bool project2d_mum(POINT* outPoint, const vec3 *pos, u64 aptr, bool invertYZ)
{	
	if (!pos || get_mum_fovy() <= 0)
		return false;

	static const float inch2meter = 0.0254f;
	static const float meter2inch = 39.370078740157f;
	static const float d3dx2mum = 0.8128f;

	D3DXVECTOR3 vUpVec = { 0.0f, 0.0f, -1.0f }; // Z is UP
	D3DXVECTOR3 camPos = { 0.f, 0.f, 0.f };
	D3DXVECTOR3 viewVec = { 0.f, 0.f, 0.f };
	D3DXVECTOR3 worldPt = { 0.f, 0.f, 0.f };

	// mumble data has Y for up and an inverted Z for cam_front (at y)
	// and swiched x/y
	camPos.x = g_mum_mem->cam_pos.x;
	camPos.y = g_mum_mem->cam_pos.z;
	camPos.z = -(g_mum_mem->cam_pos.y);
	viewVec.x = g_mum_mem->cam_front.x;
	viewVec.y = g_mum_mem->cam_front.z;
	viewVec.z = -(g_mum_mem->cam_front.y);
	if (invertYZ) {
		// When using g_mum_mem->avatar_pos
		worldPt.x = pos->x;
		worldPt.y = pos->z;
		worldPt.z = -(pos->y);
	} else {
		
		// Fuck it, use the AgPos and convert inches to meters
		D3DXVECTOR4 agPos;
		if (aptr && Ag_GetPos(aptr, &agPos))
		{
			worldPt.x = agPos.x * inch2meter;
			worldPt.y = agPos.y * inch2meter;
			worldPt.z = agPos.z * inch2meter;
		}
		else
		{
			// TODO: figure out where this stupid 0.8218f is coming from
			// perhaps tan of fovy?
			worldPt.x = pos->x * d3dx2mum;
			worldPt.y = pos->y * d3dx2mum;
			worldPt.z = -(pos->z * d3dx2mum);
		}
	}	

	/*if (aptr)
	{
		D3DXVECTOR4 rot;
		AgTrans_GetRot(aptr, &rot);
		return project2d(outPoint, &camPos, &viewVec, &vUpVec, (const D3DXVECTOR3*)&rot, get_mum_fovy());
	}
	else*/

	// TODO: on 2017-02-08 patch ANet fucked up the mumble FOV, it resets over 1.0f
	return project2d(outPoint, &camPos, &viewVec, &vUpVec, (const D3DXVECTOR3*)&worldPt,
		g_cam_data.valid ? g_cam_data.fovy : get_mum_fovy());
}

static void draw_buff_uptime(Panel *panel, PanelConfig* cfg, ClosePlayer *players, u32 num, i32 gui_x, i32 gui_y)
{

	static i8 buf[1025];

	memset(buf, 0, 1025);

	i32 cur_x = gui_x;
	i32 cur_y = gui_y;
	u32 color = RGBA(255, 255, 255, 185);
	u32 font_width = g_state.font_width;
	u32 font_height = g_state.font_height;
	u32 font7_width = renderer_get_font7_x();
	u32 font7_height = renderer_get_font7_y();
	i32 spacing = (u32)(font_height*0.5f);
	u32 w = (u32)(font_width*27.0f);
	u32 h = (u32)(font_height*(MSG_CLOSEST_PLAYERS + 6));
	
	if (cfg->is_expanded)
		h += (u32)(font7_height*13.0f);

	for (i32 i = BUFF_COL_CLS; i < BUFF_COL_END; ++i) {
		if (panel->cfg.col_visible[i]) {
			switch (i) {
			case(BUFF_COL_MIGHT):
			case(BUFF_COL_GOTL):
				w += (u32)(font_width*9.0f);
				break;
			case(BUFF_COL_TIME):
				w += (u32)(font_width*10.0f);
				break;
			default:
				w += (u32)(font_width*4.0f);
				break;
			};
		}
	}

	draw_panel_window(&cur_y, gui_x, gui_y, w, h, L"=== TOTAL COMBAT | BUFF UPTIME ===", panel);

	cur_x += font_width;
	cur_y += (u32)(font_height * 0.5f);

	memset(buf, 0, sizeof(buf));

	i32 len = 0;
	i32 lastcol = 0;
	for (i32 i = 0; i < BUFF_COL_END; ++i) {

		if (panel->cfg.col_visible[i])
		{
			panel->cfg.col_hdr[i].top = cur_y;
			panel->cfg.col_hdr[i].bottom = panel->cfg.col_hdr[i].top + font_height;

			switch (i) {
			case(BUFF_COL_NAME):
				panel->cfg.col_hdr[i].left = cur_x + (u32)(font_width*1.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*22.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%-23s", buff_col_str[i]);
				renderer_draw_text(cur_x + (u32)(font_width*1.0f), cur_y, COLOR_WHITE, buf, false);
				memset(buf, 0, sizeof(buf));
				len = 0;
				break;
			case(BUFF_COL_MIGHT):
			case(BUFF_COL_GOTL):
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*1.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*8.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%s(avg) ", buff_col_str[i]);
				break;
			case(BUFF_COL_TIME):
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*2.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*5.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, " %s", buff_col_str[i]);
				break;
			default:
				panel->cfg.col_hdr[i].left = panel->cfg.col_hdr[lastcol].right + (u32)(font_width*1.0f);
				panel->cfg.col_hdr[i].right = panel->cfg.col_hdr[i].left + (u32)(font_width*3.0f);
				lastcol = i;
				len += snprintf(&buf[len], sizeof(buf) - len, "%-4s", buff_col_str[i]);
				break;
			};
		}
	}


	if (panel->cfg.col_visible[panel->cfg.sortCol]) {
		renderer_printf(true, panel->cfg.col_hdr[panel->cfg.sortCol].left,
			cur_y - 5, COLOR_WHITE, panel->cfg.asc ? COL_ASC_STR : COL_DESC_STR);
	}
	renderer_draw_text(cur_x + (u32)(font_width*24.0f), cur_y, COLOR_WHITE, buf, false);
	cur_y += font_height;


	memset(buf, 0, sizeof(buf));

	len = 0;
	for (u32 i = 0; i < num; ++i)
	{
		ClosePlayer *player = &players[i];
		i32 top = cur_y + i * font_height;
		i32 colX = cur_x;

		// Draw the colored player name
		renderer_printfW(
			false,
			cur_x, top,
			color_by_profession(&player->c, false),
			L"%2d. %-20.20s",
			i + 1, player->name
		);

		colX += (u32)(font_width*23.0f);
		if (panel->cfg.col_visible[BUFF_COL_CLS]) {

			renderer_printf(
				false,
				colX, top,
				color,
				" %s",
				name_by_profession(player->c.profession));
			colX += (u32)(font_width*4.0f);
		}

		if (panel->cfg.col_visible[BUFF_COL_SUB]) {

			renderer_printf(
				false,
				colX, top,
				color,
				player->subGroup > 0 ? " %3d" : "    ",
				player->subGroup);
			colX += (u32)(font_width*4.0f);
		}

		// Draw the stats or line break
		if (player->cd.duration == 0)
		{
			i32 l = snprintf(buf + len, sizeof(buf) - len - 1, "\n");
			if (l > 0)
				len += l;
		}
		else
		{
			len = 0;
			memset(buf, 0, sizeof(buf));

			f32 dur = player->cd.duration / 1000.0f;
			i32 l = 0;

#define PERCENT_STR(x, dura) {														\
		u32 per = (u32)(x / (f32)(dura) * 100.0f);									\
		if(per>99) { l = snprintf(buf + len, sizeof(buf) - len - 1, " 1#%%");		\
			if (l > 0) len += l; }													\
		else { l = snprintf(buf + len, sizeof(buf) - len - 1, " %2u%%", per);		\
			if (l > 0) len += l; }													\
		}

			if (panel->cfg.col_visible[BUFF_COL_DOWN])
				len += snprintf(buf + len, sizeof(buf) - len - 1, " %3d", player->cd.noDowned);

			if (panel->cfg.col_visible[BUFF_COL_SCLR])
				PERCENT_STR(player->cd.sclr_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_SEAW])
				PERCENT_STR(player->cd.seaw_dura, player->cd.duration500);
			if (panel->cfg.col_visible[BUFF_COL_PROT])
				PERCENT_STR(player->cd.prot_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_QUIK])
				PERCENT_STR(player->cd.quik_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_ALAC])
				PERCENT_STR(player->cd.alac_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_FURY])
				PERCENT_STR(player->cd.fury_dura, player->cd.duration);

			if (panel->cfg.col_visible[BUFF_COL_MIGHT]) {
				PERCENT_STR(player->cd.might_dura, player->cd.duration);
				l = snprintf(buf + len, sizeof(buf) - len - 1, "(%2d) ", (u16)player->cd.might_avg);
				if (l > 0) len += l;
			}

			if (panel->cfg.col_visible[BUFF_COL_VIGOR])
				PERCENT_STR(player->cd.vigor_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_SWIFT])
				PERCENT_STR(player->cd.swift_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_STAB])
				PERCENT_STR(player->cd.stab_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_RETAL])
				PERCENT_STR(player->cd.retal_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_RESIST])
				PERCENT_STR(player->cd.resist_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_REGEN])
				PERCENT_STR(player->cd.regen_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_AEGIS])
				PERCENT_STR(player->cd.aegis_dura, player->cd.duration);

			if (panel->cfg.col_visible[BUFF_COL_GOTL]) {
				PERCENT_STR(player->cd.gotl_dura, player->cd.duration);
				l = snprintf(buf + len, sizeof(buf) - len - 1, "(%1.1f)", player->cd.gotl_avg);
				if (l > 0) len += l;
			}

			if (panel->cfg.col_visible[BUFF_COL_GLEM])
				PERCENT_STR(player->cd.glem_dura, player->cd.duration);

			if (panel->cfg.col_visible[BUFF_COL_BANS])
				PERCENT_STR(player->cd.bans_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_BAND])
				PERCENT_STR(player->cd.band_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_BANT])
				PERCENT_STR(player->cd.bant_dura, player->cd.duration);

			if (panel->cfg.col_visible[BUFF_COL_EA])
				PERCENT_STR(player->cd.warEA_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_SPOTTER])
				PERCENT_STR(player->cd.spot_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_FROST])
				PERCENT_STR(player->cd.frost_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_SUN])
				PERCENT_STR(player->cd.sun_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_STORM])
				PERCENT_STR(player->cd.storm_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_STONE])
				PERCENT_STR(player->cd.stone_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_ENGPP])
				PERCENT_STR(player->cd.engPPD_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_REVNR])
				PERCENT_STR(player->cd.revNR_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_REVAP])
				PERCENT_STR(player->cd.revAP_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_REVRD])
				PERCENT_STR(player->cd.revRD_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_ELESM])
				PERCENT_STR(player->cd.eleSM_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_GRDSN])
				PERCENT_STR(player->cd.grdSIN_dura, player->cd.duration);
			if (panel->cfg.col_visible[BUFF_COL_NECVA])
				PERCENT_STR(player->cd.necVA_dura, player->cd.duration);

			if (panel->cfg.col_visible[BUFF_COL_TIME]) {
				l = snprintf(buf + len, sizeof(buf) - len - 1,
					"  %dm %.2fs\n",
					(int)dur / 60, fmod(dur, 60)
				);
			}

			if (l > 0)
				len += l;

			renderer_draw_text(colX, top, color, buf, false);
		}
	}

	len = 0;
	memset(buf, 0, sizeof(buf));

	cfg->title.left = cur_x;
	cfg->title.right = cfg->title.left + (u32)(font_width*50.0f);
	cfg->title.top = cur_y + (u32)(font_height*(MSG_CLOSEST_PLAYERS + 1)) + spacing;
	cfg->title.bottom = cfg->title.top + font_height;
	renderer_printf(
		true,
		cfg->title.left, cfg->title.top,
		COLOR_WHITE,
		"[%s %s]",
		g_state.bind_wnd_ctrl.str,
		cfg->is_expanded ? "<<<<" : ">>>>"
	);

	static const int cols_per_line = 3;

	if (cfg->is_expanded)
	{
		len = 0;
		memset(buf, 0, sizeof(buf));

		// Time is always VISIBILE
		for (int i = BUFF_COL_CLS; i < BUFF_COL_END; i++)
		{
			int pos = i - 1;
			int line = (u32)(pos / cols_per_line);
			int crlf = ((pos + 1) % cols_per_line == 0);
			cfg->col_cfg[i].left = cfg->title.left + (u32)(font7_width*12.0f) * (pos  % cols_per_line);
			cfg->col_cfg[i].right = cfg->col_cfg[i].left + (u32)(font7_width*10.0f);
			cfg->col_cfg[i].top = cfg->title.top + font_height + spacing + font7_height * line;
			cfg->col_cfg[i].bottom = cfg->col_cfg[i].top + font7_height;
			len += snprintf(&buf[len], sizeof(buf) - len,
				"[%c] %-8s",
				cfg->col_visible[i] ? 'x' : ' ',
				buff_col_str[i]
			);

			if (pos > 0 && crlf)
				len += snprintf(&buf[len], sizeof(buf) - len, "\n");
		}

		renderer_draw_text(
			cfg->title.left,
			cfg->title.top + font_height + spacing,
			COLOR_WHITE, buf, true);
	}

}

/*float vec_angle(vec3 vA, vec3 vB)
{
	float fCrossX = vA.y * vB.z - vA.z * vB.y;
	float fCrossY = vA.z * vB.x - vA.x * vB.z;
	float fCrossZ = vA.x * vB.y - vA.y * vB.x;
	float fCross = (float)sqrt(fCrossX * fCrossX + fCrossY * fCrossY + fCrossZ * fCrossZ);
	float fDot = vA.x * vB.x + vA.y * vB.y + vA.z + vB.z;
	return (float)atan2(fCross, fDot) * (float)(180 / M_PI);
}

float vec_angle_signed(vec3 vA, vec3 vB, vec3 vN) {

	D3DXVECTOR3 cross;
	D3DXVec3Cross(&cross, (LPD3DXVECTOR3)&vA, (LPD3DXVECTOR3)&vB);

	// angle in [0,180]
	float angle = vec_angle(vA, vB);
	float sign = D3DXVec3Dot((LPD3DXVECTOR3)&vN, &cross);

	// angle in [-179,180]
	if (sign < 0)
		angle = -angle;

	// angle in [0,360] (not used but included here for completeness)
	//float angle360 =  (signed_angle + 180) % 360;

	return angle;
}*/


//
// http://stackoverflow.com/questions/14066933/direct-way-of-computing-clockwise-angle-between-2-vectors#16544330
float vec_angle_normalplane(vec3 vA, vec3 vB, vec3 vN)
{
	float dot = vA.x*vB.x + vA.y*vB.y + vA.z*vB.z;
	float det = vA.x*vB.y*vN.z + vB.x*vN.y*vA.z + vN.x*vA.y*vB.z -
		vA.z*vB.y*vN.x - vB.z*vN.y*vA.x - vN.z*vA.y*vB.x;
	float angle = (float)atan2(det, dot) * (float)(180 / M_PI);
	return angle;
}


float get_cam_direction(vec3 cam_dir)
{
	vec3 north = { 0 ,0, 1 };
	vec3 normal = { 0 ,1, 1 };
	float angle = vec_angle_normalplane(cam_dir, north, normal);
	bool to_north = 0;
	bool is_east = 0;
	if (abs((int)angle) <= 90) to_north = true;
	if (angle > 0) is_east = true;
	return (-angle);
}

bool is_adjacent_to_cardinal(float angle, float distance, int count)
{
	float mod_angle = (float)(abs((int)angle) % 90);
	float fwd_angle = mod_angle + distance*count;
	float bwd_angle = mod_angle - distance*count;

	if (mod_angle <= 0 && fwd_angle > 0)
		return true;
	if (mod_angle >= 0 && bwd_angle < 0)
		return true;
	if (mod_angle <= 90 && fwd_angle > 90)
		return true;
	if (mod_angle >= 90 && bwd_angle < 90)
		return true;

	return false;
}

static inline double shadefactor_from_idx(int idx, int size, float shade)
{
	double d = abs(idx - size / 2);
	return ((100 - d*shade) / 100);
}

static inline float round_to_multiple_of(f32 f, f32 multiple)
{
	f32 remainder = (f32)fmod(fabs(f), multiple);
	if (remainder == 0)
		return f;

	if (f < 0)
		//return -(abs(n) - remainder);
		return -((float)fabs(f) + multiple - remainder);
	else
		return f + multiple - remainder;
}

static inline float round_up_to_multiple_of(f32 f, f32 multiple)
{
	f32 remainder = (f32)fmod(fabs(f), multiple);
	if (remainder == 0)
		return f;

	if (f < 0)
		return -((float)fabs(f) - remainder);
	else
		return f + multiple - remainder;
}

/*
// Draw the compass
static void draw_compass(i32 gui_x, i32 gui_y, i32 gui_w, i32 font_x, i32 font_y)
{

#define SHADE_FACTOR			1
#define SHADE_WHITE(idx)		RGBA(255,	255, 255, (int)(255 * shadefactor_from_idx(idx, compass_arr_size, SHADE_FACTOR)))
#define SHADE_GRAY(idx)			RGBA(255,	255, 255, (int)(150 * shadefactor_from_idx(idx, compass_arr_size, SHADE_FACTOR)))
#define SHADE_CYAN(idx)			RGBA(0,		255, 255, (int)(255 * shadefactor_from_idx(idx, compass_arr_size, SHADE_FACTOR)))
#define COMPASS_SIZE			252										// theoretical compass cylinder size
#define COMPASS_DRAWN			0.6										// the portion of the compass cylinder
																		// that will be drawn on screen
	static const f32 compass_inc_size = (f32)(360.00 / COMPASS_SIZE);	// adjust the angle incements to our compas size
	static const int compass_arr_size = (int)(COMPASS_SIZE*COMPASS_DRAWN);

	if (!g_mum_mem)
	{
		return;
	}

	//vec3 *cam_dir = (vec3*)((char*)g_mum_mem+ OFF_MUM_CAM_DIR);
	float angle = get_cam_direction(g_mum_mem->cam_front);

	// width must be multiplier of the drawn size
	gui_w -= gui_w % compass_arr_size;

	// Draw the border rect
	i32 spacing = (gui_w / compass_arr_size);
	i32 cur_x = gui_x - gui_w/2 - spacing;
	renderer_draw_rect(
		cur_x,							//	x
		gui_y - font_y / 2,				//	y
		gui_w + 2*spacing +font_x,		//	w
		(font_y * 2),					//	h
		COLOR_BLACK_OVERLAY(g_state.panel_compass.alpha),
		&g_state.panel_compass.rect);


	// Divide bearings into 64 shards out of which only 33 will be drawn
	// determine the starting angle of the compass assuming angle==center
	// N == 0	W == -90	S == abs(180)	E = 90
	//  N  |     | 	 < magnified shard, assuming shard_size == 5
	// -2  3  8  13
	f32 cur_angle;
	f32 start_angle = round_to_multiple_of(angle - (compass_arr_size /2) * compass_inc_size, compass_inc_size);
	if (start_angle < -180) {
		start_angle += 360;
	}

	i32 font7_x = renderer_get_font7_x();
	i32 font7_y = renderer_get_font7_y();
	bool draw_left = true;
	int cardinal_index = -1;
	cur_angle = start_angle;
	cur_x += spacing;
	for (int i = 0; i < compass_arr_size; i++)
	{
		u32 color = 0;
		bool is_south = false;
		f32 next_angle = cur_angle + compass_inc_size;
		if (next_angle > 180) {
			next_angle -= 360;
			is_south = true;
		}

		char str[] = { 0 };
		if (is_south) {
			cardinal_index = i;
			color = SHADE_CYAN(i);
			str[0] = 'S';
		} else if (cur_angle <= -90.00 && next_angle > -90.00) {
			cardinal_index = i;
			color = SHADE_CYAN(i);
			str[0] = 'W';
		} else if (cur_angle <= 0.00 && next_angle > 0.00) {
			cardinal_index = i;
			color = SHADE_CYAN(i);
			str[0] = 'N';
		} else if (cur_angle <= 90.00 && next_angle > 90.00) {
			cardinal_index = i;
			color = SHADE_CYAN(i);
			str[0] = 'E';
		} else if ( cardinal_index >= 0 && (i - cardinal_index) % (compass_arr_size /2/8) == 0) {
			color = SHADE_WHITE(i);
			str[0] = '|';
		} else if ( cardinal_index >= 0 && (i - cardinal_index) % (compass_arr_size /2/8/3) == 0) {
			color = SHADE_GRAY(i);
			str[0] = -1;
		}

		if (str[0] > 0) {
			renderer_printf(false, cur_x, gui_y, color, "%c", str[0]);
		}
		else if (str[0] == -1) {
			renderer_printf(true, cur_x, gui_y+font_y-font7_y, color, "|");
		}

		// Draw markers to the left of the first cardinal
		if (draw_left &&  cardinal_index >= 0) {
			draw_left = false;
			for (int j = i - 1; j >= 0; j--) {
				int dist = i - j;
				if (dist % (compass_arr_size /2/8/3))
					continue;
				bool is_big_indicator = (dist % (compass_arr_size /2/8)) == 0;
				renderer_printf(
					is_big_indicator ? false : true,
					is_big_indicator ? cur_x - dist*spacing - 2 : cur_x - dist*spacing,
					is_big_indicator ? gui_y : (gui_y + font_y - font7_y),
					is_big_indicator ? SHADE_WHITE(j) : SHADE_GRAY(j),
					"|");
			}
		}

		// Draw the mid marker
		if (i == compass_arr_size / 2) {
			renderer_printf(true, cur_x-font7_x/2, gui_y + font7_y*2, COLOR_WHITE, "^");
		}

		cur_x += spacing;
		cur_angle = next_angle;
	}
}
*/

// Draw the compass
static void draw_compass2(i32 gui_x, i32 gui_y, i32 gui_w, i32 font_x, i32 font_y)
{
	#define COMPASS_STR_LEN	38
	static char cardinals_str[COMPASS_STR_LEN+1];
	static char indicator_long_str[COMPASS_STR_LEN+1];
	static char indicator_short_str[COMPASS_STR_LEN+1];
	static const f32 compass_inc_size = 5.0f;
	static float last_seen_angle = 181.0f;
	
	if (!g_mum_mem)
	{
		return;
	}

	// Draw the border rect
	i32 font7_x = renderer_get_font7_x();
	i32 font7_y = renderer_get_font7_y();
	i32 w = font_x * (COMPASS_STR_LEN + 2);
	i32 cur_x = gui_x - w / 2;
	renderer_draw_rect(
		cur_x,							//	x
		gui_y - font_y / 2,				//	y
		w,								//	w
		(font_y * 2),					//	h
		COLOR_BLACK_OVERLAY(g_state.panel_compass.alpha),
		&g_state.panel_compass.rect);

	//vec3 *cam_dir = (vec3*)((char*)g_mum_mem+ OFF_MUM_CAM_DIR);
	f32 angle = get_cam_direction(g_mum_mem->cam_front);
	f32 start_angle = angle - (COMPASS_STR_LEN / 2) * compass_inc_size;
	if (start_angle < -180) {
		start_angle += 360;
	}
	f32 rounded_angle = round_up_to_multiple_of(start_angle, compass_inc_size);
	cur_x += (i32)(font_x / 2 + font_x * ((rounded_angle - start_angle) / compass_inc_size));

	// redraw the strings only if cam changed
	if (last_seen_angle == 181.00f ||
		last_seen_angle != angle) {

		// save angle
		last_seen_angle = angle;

		// reset the current compass strings
		memset(&cardinals_str[0],		' ', COMPASS_STR_LEN); cardinals_str[COMPASS_STR_LEN] = 0;
		memset(&indicator_long_str[0],	' ', COMPASS_STR_LEN); indicator_long_str[COMPASS_STR_LEN] = 0;
		memset(&indicator_short_str[0], ' ', COMPASS_STR_LEN); indicator_short_str[COMPASS_STR_LEN] = 0;

		// round up to nearest compass inc multiple
		// and adjust the cursor relative to starting position
		f32 cur_angle = rounded_angle;
		for (int i = 0; i < COMPASS_STR_LEN; i++)
		{
			f32 next_angle = cur_angle + compass_inc_size;
			if (next_angle > 180) {
				next_angle -= 360;
			}

			if (fabs(cur_angle) == 180.00) {
				cardinals_str[i] = 'S';
			}
			else if (cur_angle == -90.00) {
				cardinals_str[i] = 'W';
			}
			else if (cur_angle == 0.00) {
				cardinals_str[i] = 'N';
			}
			else if (cur_angle == 90.00) {
				cardinals_str[i] = 'E';
			}
			else if (fmod(cur_angle, compass_inc_size * 3) == 0) {
				indicator_long_str[i] = '|';
			}
			else {
				indicator_short_str[i] = '|';
			}

			cur_angle = next_angle;
		}
	}

	renderer_printf(false, cur_x, gui_y, COLOR_CYAN, cardinals_str);
	renderer_printf(false, cur_x, gui_y, COLOR_WHITE, indicator_long_str);
	renderer_printf(true, cur_x, gui_y + font_y - font7_y, COLOR_GRAY, indicator_short_str);
	
	// Draw the mid marker
	renderer_printf(true, gui_x - font7_x / 2, gui_y + font7_y * 2, COLOR_WHITE, "^");
}



// Draws the player's health.
static void draw_player_health(Character* player, i32 gui_x, i32 gui_y, i32 font_x)
{
	if (player->hp_max == 0)
	{
		return;
	}

	u32 hp = (u32)ceilf(player->hp_val / (f32)player->hp_max * 100.0f);
	hp = min(hp, 100);

	u32 num = 1;
	num += (hp > 9);
	num += (hp > 99);

	renderer_printf(true, gui_x - num * font_x / 2, gui_y, COLOR_WHITE, "%u", hp);
}

// Draws the breakbar percent for the target.
static void draw_target_breakbar(Character* target, i32 gui_x, i32 gui_y, i32 font_x)
{
	if (target->bptr == 0 || (target->bb_state != BREAKBAR_READY && target->bb_state != BREAKBAR_REGEN))
	{
		return;
	}

	u32 bb = target->bb_value;

	u32 num = 1;
	num += (bb > 9);
	num += (bb > 99);

	renderer_printf(true, gui_x - num * font_x / 2, gui_y, COLOR_WHITE, "%u", bb);
}

// Draws the target's health.
static void draw_target_health(Character* target, i32 gui_x, i32 gui_y)
{
	if (target->aptr == 0 || target->hp_max == 0)
	{
		return;
	}

	if (target->is_player || Ch_IsClone(target->cptr)) {
#if (defined BGDM_TOS_COMPLIANT)
		return;
#else
		if (isCompetitiveDisabled() && target->attitude != CHAR_ATTITUDE_FRIENDLY)
			return;
#endif
	}

	u32 hp = (u32)(target->hp_val / (f32)target->hp_max * 100.f);
	if (!g_state.hpCommas_disable) {
		char strHpVal[32] = { 0 };
		char strHpMax[32] = { 0 };
		renderer_printf(true, gui_x, gui_y, COLOR_WHITE, "[%d%%] %s / %s", hp,
			sprintf_num_commas(target->hp_val, strHpVal, sizeof(strHpVal)),
			sprintf_num_commas(target->hp_max, strHpMax, sizeof(strHpMax)));
	}
	else {
		renderer_printf(true, gui_x, gui_y, COLOR_WHITE, "[%d%%] %d / %d", hp,
			target->hp_val, target->hp_max);
	}
}

// Draws the distance to the target.
static void draw_target_distance(Character* player, Character* target, i32 gui_x, i32 gui_y, i32 font_x)
{
	if (player->tptr == 0 || target->tptr == 0)
	{
		return;
	}

	if (target->is_player || Ch_IsClone(target->cptr)) {
		if (isCompetitiveDisabled() && target->attitude != CHAR_ATTITUDE_FRIENDLY)
			return;
	}

	vec3 len;
	len.x = player->pos.x - target->pos.x;
	len.y = player->pos.y - target->pos.y;
	len.z = player->pos.z - target->pos.z;

	u32 dist = (u32)(sqrtf(len.x * len.x + len.y * len.y + len.z * len.z) * SCALE);
	dist = min(dist, 9999);

	u32 num = 1;
	num += (dist > 9);
	num += (dist > 99);
	num += (dist > 999);

	renderer_printf(true, gui_x - num * font_x, gui_y, COLOR_WHITE, "%d", dist);
}

// Enables logging of hits to file.
static void module_log_hits(Character* target, i64 now)
{
	static u64 last_target;
	static i64 log_start;
	static bool log_toggle;
	static bool is_logging;

	if (target->aptr)
	{
		last_target = target->aptr;
	}

	bool log_key = is_bind_down(&g_state.bind_dps_log);
	bool last_state = is_logging;

	if (log_key && log_toggle == false)
	{
		is_logging = !is_logging;
		if (is_logging)
		{
			log_start = now;
		}

		log_toggle = true;
	}
	else if (log_key == false)
	{
		log_toggle = false;
	}

	if (last_state == false)
	{
		return;
	}

	u32 ti = dps_targets_find(last_target);
	if (ti >= MAX_TARGETS)
	{
		renderer_draw_text(5, 30, COLOR_WHITE, "LOGGING", true);
		return;
	}

	DPSTarget* t = g_targets + ti;
	u32 start = 0;
	
	while (start < t->num_hits && log_start > t->hits[start].time)
	{
		++start;
	}

	u32 num = t->num_hits - start;
	renderer_printf(true, 5, 30, COLOR_WHITE, "LOGGING: %d hits", num);

	if (is_logging || num == 0)
	{
		return;
	}

	// Open the file with the current time as its filename.
	i8 buf[MAX_PATH + 1] = { 0 };
	i8 dbuf[9];

	strncat(buf, g_state.log_dir, MAX_PATH);
	strncat(buf, "\\", MAX_PATH);

	_strdate(dbuf);
	strncat(buf, dbuf, MAX_PATH);

	strncat(buf, "_", MAX_PATH);

	_strtime(dbuf);
	strncat(buf, dbuf, MAX_PATH);

	strncat(buf, ".txt", MAX_PATH);

	for (u32 i = 0; i < MAX_PATH; ++i)
	{
		if (buf[i] == '/' || buf[i] == ':')
		{
			buf[i] = '-';
		}
	}

	FILE *fp = fopen(buf, "w");
	if (fp == 0)
	{
		return;
	}

	u32 end = t->num_hits - 1;
	i64 tdur = t->hits[start].time;
	i64 last = t->hits[end].time;

	DBGPRINT(TEXT("Logging %u hits [start=%u] [end=%u] [total_hits=%d]"), num, start, end, t->num_hits);
	for (u32 i = start; i <= end; ++i)
	{
		DBGPRINT(TEXT("[hit(%i).time=%I64u]\t[hit(%i).dmg=%u]"), i, t->hits[i].time, i, t->hits[i].dmg);
	}

	while (tdur <= last+100000-1)
	{
		u32 tdmg = 0;
			
		for (u32 i = start; i <= end && tdur >= t->hits[i].time; ++i)
		{
			tdmg += t->hits[i].dmg;
			DBGPRINT(TEXT("[hit(%i).time=%I64u]\t[hit(%i).dmg=%u]\t[TIME=%I64u]\t[PDMG=%u]"), i, t->hits[i].time, i, t->hits[i].dmg, tdur, tdmg);
		}

		f32 d = (tdur - t->hits[start].time) / 1000000.0f;
		fprintf(fp, "TIME=%.3f\tPDMG=%u\tPDPS=%u\n", d, tdmg, d ? (u32)(tdmg / d) : 0);
		DBGPRINT(TEXT("TIME=%.3f\tPDMG=%u\tPDPS=%u\n"), d, tdmg, d ? (u32)(tdmg / d) : 0);

		tdur += 100000;
	}

	fclose(fp);
}

// Draw the compass 
static void module_compass(i32 default_x, i32 default_y, i32 default_w)
{

	if (!g_state.panel_compass.enabled)
		return;

	// no controlled char, no need for compass
	u64 player_cptr = process_read_u64(g_client_ctx + OFF_CCTX_CONTROLLED_CHAR);
	if (player_cptr == 0)
		return;

	i32 fontu_x = renderer_get_fontu_x();
	i32 fontu_y = renderer_get_fontu_y();

	// default UI location
	if (g_state.panel_compass.pos.x < 0)
		g_state.panel_compass.pos.x = default_x;
	if (g_state.panel_compass.pos.y < 0)
		g_state.panel_compass.pos.y = default_y;

	if (g_state.renderImgui)
		ImGui_Compass(&g_state.panel_compass, g_state.panel_compass.pos.x, g_state.panel_compass.pos.y, default_w);
	else
		draw_compass2(g_state.panel_compass.pos.x, g_state.panel_compass.pos.y,
			default_w, fontu_x, fontu_y);
}

static void module_hp_dist(Panel* panel, Character *player, Character *target, i32 center, GFX* ui, GFXTarget* set)
{
	if (!panel->enabled)
		return;

	i32 res_y = renderer_get_res_y();
	i32 font7_x = renderer_get_font7_x();

	if (panel->cfg.col_visible[HP_ROW_PLAYER_HP])
		draw_player_health(player, center, res_y - ui->hp, font7_x);
	if (panel->cfg.col_visible[HP_ROW_TARGET_HP])
		draw_target_health(target, center + set->hp.x, set->hp.y);
	if (panel->cfg.col_visible[HP_ROW_TARGET_DIST])
		draw_target_distance(player, target, center + set->dst, set->hp.y, font7_x);
	if (panel->cfg.col_visible[HP_ROW_TARGET_BB])
		draw_target_breakbar(target, center + set->bb.x, set->bb.y, font7_x);
}


// Calc target DPS

static DPSTargetEx* dps_target_get_pref(Character* sel_target, Array *ca, i64 now);
struct DPSTargetEx* dps_target_get_lastknown(i64 now)
{
	return dps_target_get_pref(NULL, NULL, now);
}

static DPSTargetEx* dps_target_get_pref(Character* sel_target, Array *ca, i64 now)
{
	static DPSTargetEx dps_target = { 0 };
	static i64 last_update;
	static u64 last_autolock_aptr = 0;

	if (is_bind_down(&g_state.bind_dps_reset)) {
		last_update = 0;
		last_autolock_aptr = 0;
		memset(&dps_target, 0, sizeof(dps_target));
		return NULL;
	}

	// No need to update twice in the same frame
	// GUI also uses this one to get last known target
	if (now == last_update &&
		dps_target.c.aptr &&
		dps_target.t.npc_id)
		return &dps_target;
	last_update = now;


	DPSTarget* t = NULL;
	Character* target = NULL;
	bool targetIsSelected = false;
	bool selectedTargetIsValid = false;
	bool selectedTargetAutolock = false;

	// Do we have a valid target selected
	if (sel_target &&
		sel_target->aptr &&
		(sel_target->type != AGENT_TYPE_CHAR || sel_target->attitude != CHAR_ATTITUDE_FRIENDLY)) {

		selectedTargetIsValid = true;
	}

	if (selectedTargetIsValid &&
		sel_target->aptr != last_autolock_aptr) {

		u32 ti = dps_targets_find(sel_target->aptr);
		if (ti >= MAX_TARGETS)
		{
			// Insert the target into the array
			ti = dps_targets_insert(sel_target->aptr, 0, ca);
			DBGPRINT(TEXT("target insert [aptr=%p] [cptr=%p] [hp_val=%i] [hp_max=%u]"),
				sel_target->aptr, sel_target->cptr, sel_target->hp_val, sel_target->hp_max);
		}
		else {
			t = g_targets + ti;
			if (t->npc_id) {
				last_autolock_aptr = sel_target->aptr;
				selectedTargetAutolock = autolock_get(t->npc_id);
			}
			if (!selectedTargetAutolock) t = NULL;
			else {
				DBGPRINT(TEXT("target autolock [aptr=%p] [cptr=%p] [id=%i]"), t->aptr, t->cptr, t->npc_id);
			}

		}
	}

	// Valid target selected and we aren't target locked?
	// or selected target needs to be auto-locked
	if (selectedTargetIsValid &&
		(selectedTargetAutolock || !dps_target.locked || (dps_target.locked && sel_target->aptr == dps_target.c.aptr))) {

		target = sel_target;
		dps_target.time_sel = now;
		targetIsSelected = true;
	}

	// We don't have a valid target, try last known target
	if (!target && dps_target.t.aptr) {
		
		target = &dps_target.c;
	}

	// Ignore invalid targets.
	if (!target || target->aptr == 0)
		return NULL;

	// Traget is selected, check for target swap
	if (targetIsSelected &&
		target->aptr != dps_target.c.aptr) {
		memset(&dps_target, 0, sizeof(dps_target));
	}

	// No target selected, linger time has passed
	// reset and hide the UI (by returning NULL)
	if (!targetIsSelected &&
		!dps_target.locked &&
		now - dps_target.time_sel >= g_state.target_retention_time)
	{
		last_update = 0;
		last_autolock_aptr = 0;
		memset(&dps_target, 0, sizeof(dps_target));
		return NULL;
	}

	// Toggle target lock
	module_toggle_bind(&g_state.bind_dps_lock, &dps_target.locked);
	if (selectedTargetAutolock) dps_target.locked = true;

	if (t == NULL) {
		// Check if the target has a DPS entry already.
		u32 ti = dps_targets_find(target->aptr);

		// If we don't have a valid target
		// display last known target
		if (ti >= MAX_TARGETS)
		{
			return &dps_target;
		}

		// Get the DPS target.
		t = g_targets + ti;
	}

	assert(t != NULL);
	t->time_update = now;
	dps_target.t = *t;
	dps_target.c = *target;

	return &dps_target;
}


// Updates DPS.
static void module_dps_solo(Character *target, Array *char_array, i64 now, i32 gui_x, i32 gui_y)
{
	DPSData dps_data;
	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	if (dps_target) {

		dps_data.tot_dur = (f32)((u32)(dps_target->t.duration / 1000.0f) / 1000.0f);

		dps_data.l1_dur = min(dps_data.tot_dur, (DPS_INTERVAL_1 / 1000000.0f));
		dps_data.l2_dur = min(dps_data.tot_dur, (DPS_INTERVAL_2 / 1000000.0f));

		dps_data.l1_done = dps_target->t.c1dmg;
		dps_data.l2_done = dps_target->t.c2dmg;
		dps_data.l1_done_team = dps_target->t.c1dmg_team;
		dps_data.l2_done_team = dps_target->t.c2dmg_team;
		dps_data.tot_done = dps_target->t.tdmg;
		dps_data.hp_lost = dps_target->t.hp_lost;

		if (g_state.panel_dps_self.enabled) {
			if (g_state.renderImgui)
				ImGui_PersonalDps(&g_state.panel_dps_self, gui_x, gui_y, &dps_data, dps_target);
			else
				draw_dps_solo(gui_x, gui_y, &dps_data, dps_target);
		}
	}
}


static void get_recent_targets(Character *target, Array* char_array, ClientTarget out_targets[], u32 arrsize, u32 *outsize, i64 now)
{
	if (!out_targets || !arrsize)
		return;

	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	u32 total = 0;
	u32 arr_targets[MSG_TARGETS] = { 0 };

	if (arrsize > MSG_TARGETS)
		arrsize = MSG_TARGETS;

	for (u32 i = 0; i < arrsize; ++i)
	{
		arr_targets[i] = MAX_TARGETS;
	}

	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		if (g_targets[i].aptr == 0 ||
			g_targets[i].id == 0 ||
			g_targets[i].time_update == 0)
		{
			continue;
		}

		if (total < arrsize)
		{
			arr_targets[total++] = i;
			continue;
		}

		i64 old_time = MAXINT64;
		u32 old_id = arrsize;

		for (u32 j = 0; j < arrsize; ++j)
		{
			i64 t = g_targets[arr_targets[j]].time_update;
			if (t < old_time)
			{
				old_time = t;
				old_id = j;
			}
		}

		if (old_id < arrsize && g_targets[i].time_update > old_time)
		{
			arr_targets[old_id] = i;
		}
	}

	bool arr_contains_dps_target = false;
	i64 old_time = MAXINT64;
	u32 old_idx = total;

	for (u32 i = 0; i < total; ++i)
	{
		i32 ind = arr_targets[i];
		if (ind < MAX_TARGETS)
		{
			const DPSTarget* t = g_targets + ind;
			if (t && t->aptr)
			{
				out_targets[i].id = t->id;
				out_targets[i].tdmg = t->tdmg;
				out_targets[i].invuln = t->invuln;
				out_targets[i].reserved1 = t->c1dmg;
				out_targets[i].time = (u32)(t->duration / 1000.0f);

				if (t->time_update < old_time)
				{
					old_time = t->time_update;
					old_idx = i;
				}

				if (dps_target &&
					dps_target->t.id == t->id)
					arr_contains_dps_target = true;
			}
		}
	}

	// If the array doesn't have the preferred/locked target
	// Overwrite the oldest with it
	if (dps_target &&
		!arr_contains_dps_target) {

		u32 idx = 0;
		if (total < arrsize)
			idx = total++;
		else {
			assert(old_idx < total);
			if (old_idx < total)
				idx = old_idx;
		}

		assert(idx < arrsize);

		if (idx < arrsize) {
			const DPSTarget* t = &dps_target->t;
			out_targets[idx].id = t->id;
			out_targets[idx].tdmg = t->tdmg;
			out_targets[idx].invuln = t->invuln;
			out_targets[idx].reserved1 = t->c1dmg;
			out_targets[idx].time = (u32)(t->duration / 1000.0f);
			//DBGPRINT(TEXT("added target <%d:%s> to  out_targets[%d]"), t->id, dps_target->c.decodedName, idx);
		}
	}

	if (outsize)
		*outsize = total;
}

// Compare buff player
static int __cdecl DPSPlayerCompare(void* pCtx, void const* pItem1, void const* pItem2)
{
	i32 retVal = 0;
	u32 sortBy = LOBYTE(*(u32*)pCtx);
	bool isAsc = HIBYTE(*(u32*)pCtx);
	const DPSPlayer* p1 = pItem1;
	const DPSPlayer* p2 = pItem2;

#define DPS_COMPARE(x) \
	if (p1->x < p2->x) retVal = -1; \
	else if (p1->x > p2->x) retVal = 1; \
	else retVal = 0;

	u32 p1time = 0;
	u32 p2time = 0;
	u32 p1dmg = 0;
	u32 p2dmg = 0;

	if (g_state.renderImgui && g_state.panel_dps_group.enabled > 1) {

		p1time = (u32)(p1->target_time / 1000.0f);
		p2time = (u32)(p2->target_time / 1000.0f);
		p1dmg = p1->target_dmg;
		p2dmg = p2->target_dmg;
	}
	else {

		p1time = (u32)(p1->time / 1000.0f);
		p2time = (u32)(p2->time / 1000.0f);
		p1dmg = p1->damage;
		p2dmg = p2->damage;
	}

	u32 p1dps = p1time == 0 ? 0 : (u32)(p1dmg / p1time);
	u32 p2dps = p2time == 0 ? 0 : (u32)(p2dmg / p2time);
	u32 p1hps = p1->time == 0 ? 0 : (u32)(p1->heal_out / (p1->time / 1000.0f));
	u32 p2hps = p2->time == 0 ? 0 : (u32)(p2->heal_out / (p2->time / 1000.0f));
	

	const char * profName1 = name_by_profession(p1->profession);
	const char * profName2 = name_by_profession(p2->profession);

	switch (sortBy)
	{
	case(GDPS_COL_NAME):
		if (!p1->name && p2->name)
			retVal = -1;
		else if (!p2->name && p1->name)
			retVal = 1;
		else if (!p1->name && !p2->name)
			retVal = 0;
		else
			retVal = wcscmp(p1->name, p2->name);
		break;
	case(GDPS_COL_CLS):
		retVal = strcmp(profName1, profName2);
		break;
	case(GDPS_COL_DPS):
		if (p1dps < p2dps) retVal = -1;
		else if (p1dps > p2dps) retVal = 1;
		else retVal = 0;
		break;
	case(GDPS_COL_HPS):
		if (p1hps < p2hps) retVal = -1;
		else if (p1hps > p2hps) retVal = 1;
		else retVal = 0;
		break;
	case(GDPS_COL_PER):
	case(GDPS_COL_DMGOUT):
		if (p1dmg < p2dmg) retVal = -1;
		else if (p1dmg > p2dmg) retVal = 1;
		else retVal = 0;
		break;
	case(GDPS_COL_DMGIN):
		DPS_COMPARE(damage_in);
		break;
	case(GDPS_COL_HEAL):
		DPS_COMPARE(heal_out);
		break;
	case(GDPS_COL_TIME):
	{
		if (p1time == 0 && p2time != 0) retVal = -1;
		else if (p2time == 0 && p1time != 0) retVal = 1;
		else if (p1time < p2time) retVal = -1;
		else if (p1time > p2time) retVal = 1;
		else retVal = 0;
	}
		break;
	};

	if (isAsc)
		return retVal;
	else
		return -(retVal);
}

static DPSPlayer* get_closest_arr(Character *target, Character* player, Array* char_array, i64 now, u32 *num)
{
	static DPSPlayer temp_players[MSG_CLOSEST_PLAYERS + 1];
	u32 total = 0;

	// Reset our player pool
	memset(temp_players, 0, sizeof(temp_players));

	// Find out which of the closest players matches the group dps information.
	for (u32 i = 0; i < g_group_num; ++i)
	{
		bool bFound = false;
		u32 foundIdx = 0;

		for (u32 j = 0; j < g_closest_num; ++j)
		{
			if (g_closest_names[j][0] &&
				g_group[i].name[0] &&
				wcscmp(g_closest_names[j], g_group[i].name) == 0)
			{
				bFound = true;
				foundIdx = j;
				break;
			}
		}

		if (bFound)
		{
			ClosePlayer *cp = &g_closest_players[foundIdx];
			assert(cp->c.cptr &&  cp->c.pptr);
			temp_players[total] = g_group[i];
			temp_players[total].c = &cp->c;
			temp_players[total].cd = &cp->cd;
			++total;
		}
	}

	// Add the current player into the group listing.
	get_recent_targets(target, char_array, temp_players[total].targets, MSG_TARGETS, NULL, now);
	temp_players[total].c = &g_player.c;
	temp_players[total].cd = &g_player.combat;
	temp_players[total].time = g_player.combat.duration;
	temp_players[total].damage = InterlockedCompareExchange(&g_player.combat.damageOut, 0, 0);
	temp_players[total].damage_in = InterlockedCompareExchange(&g_player.combat.damageIn, 0, 0);
	temp_players[total].heal_out = InterlockedCompareExchange(&g_player.combat.healOut, 0, 0);
	temp_players[total].stats = player->stats;
	temp_players[total].profession = player->profession;
	wmemcpy(temp_players[total].name, player->name, MSG_NAME_SIZE);
	++total;

	if (num)
		*num = total;

	return temp_players;
}


static void module_dps_group_combined(Character* player, Character* target, Array* char_array, i64 now, i32 gui_x, i32 gui_y, Panel* panel)
{
	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	u32 total = 0;
	DPSPlayer* temp_players = get_closest_arr(target, player, char_array, now, &total);

	// Set total dmg/duration to current target
	for (u32 i = 0; i < total; i++) {

		if (dps_target && dps_target->t.id) {
			for (u32 j = 0; j < MSG_TARGETS; j++) {
				if (temp_players[i].targets[j].id == dps_target->t.id) {
					temp_players[i].target_time = (f32)temp_players[i].targets[j].time;
					temp_players[i].target_dmg = temp_players[i].targets[j].tdmg;
					break;
				}
			}
		}
	}

	// Sort players by most damage done.
	u32 sortBy = MAKEWORD(panel->cfg.sortCol, panel->cfg.asc);
	qsort_s(&temp_players[0], total, sizeof(temp_players[0]), DPSPlayerCompare, &sortBy);

	if (panel->enabled)
		ImGui_GroupDps(panel, gui_x, gui_y, dps_target, temp_players, total);
}


static void module_dps_group_target(Character* player, Character* target, Array* char_array, i64 now, i32 gui_x, i32 gui_y, Panel* panel)
{
	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	u32 total = 0;
	DPSPlayer* temp_players = get_closest_arr(target, player, char_array, now, &total);

	// Set total dmg/duration to current target
	for (u32 i = 0; i < total; i++) {

		temp_players[i].time = 0;
		temp_players[i].damage = 0;
		
		if (dps_target && dps_target->t.id) {
			for (u32 j = 0; j < MSG_TARGETS; j++) {
				if (temp_players[i].targets[j].id == dps_target->t.id) {
					temp_players[i].time = (f32)temp_players[i].targets[j].time;
					temp_players[i].damage = temp_players[i].targets[j].tdmg;
					break;
				}
			}
		}
	}

	// Sort players by most damage done.
	u32 sortBy = MAKEWORD(panel->cfg.sortCol, panel->cfg.asc);
	qsort_s(&temp_players[0], total, sizeof(temp_players[0]), DPSPlayerCompare, &sortBy);

	// Get cursor position for stat checking.
	POINT cursor = { 0 };
	GetCursorPos(&cursor);
	void* window = process_get_window();
	if (window)
		ScreenToClient(window, &cursor);

	if (panel->enabled)
		draw_dps_group(panel, &panel->cfg, dps_target, temp_players, total, gui_x, gui_y, cursor);
}

// Group DPS module.
static void module_dps_group_cleave(Character* player, Character* target, Array* char_array, i64 now, i32 gui_x, i32 gui_y, Panel* panel)
{
	u32 total = 0;
	DPSPlayer* temp_players = get_closest_arr(target, player, char_array, now, &total);

	// Sort players by most damage done.
	u32 sortBy = MAKEWORD(panel->cfg.sortCol, panel->cfg.asc);
	qsort_s(&temp_players[0], total, sizeof(temp_players[0]), DPSPlayerCompare, &sortBy);

	// Get cursor position for stat checking.
	POINT cursor = { 0 };
	GetCursorPos(&cursor);
	void* window = process_get_window();
	if (window)
		ScreenToClient(window, &cursor);

	if (panel->enabled)
		draw_dps_group(panel, &panel->cfg, NULL, temp_players, total, gui_x, gui_y, cursor);
}

static void module_dps_group(Character* player, Character* target, Array* char_array, i64 now, i32 gui_x, i32 gui_y)
{
	Panel *panel = &g_state.panel_dps_group;

	if (g_state.renderImgui) {
		module_dps_group_combined(player, target, char_array, now, gui_x, gui_y, panel);
	} else {
		if (!panel->enabled)
			return;
		else if (panel->enabled == 1)
			module_dps_group_cleave(player, target, char_array, now, gui_x, gui_y, panel);
		else
			module_dps_group_target(player, target, char_array, now, gui_x, gui_y, panel);
	}
}

static void module_dps_skills(Character* player, Character* target, Array* char_array, i64 now, i32 gui_x, i32 gui_y)
{
	Panel *panel = &g_state.panel_skills;
	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	if (g_state.renderImgui && panel->enabled) {
		ImGui_SkillBreakdown(panel, gui_x, gui_y, dps_target);
	}
}

// Group DPS network module.
static void module_network_dps(Character* player, Character* target, Array* char_array, i64 now)
{
	assert(g_player.c.aptr == player->aptr);

	// User requested network DPS disable
	if (g_state.netDps_disable)
		return;
	
	// Check if we need to send a packet.
	static i64 last_update;
	if (now - last_update < MSG_FREQ_CLIENT_DAMAGE)
	{
		return;
	}

	const DPSTargetEx *dps_target = dps_target_get_pref(target, char_array, now);

	last_update = now;

	if (g_state.netWchar_enable) {

		// Start preparing the message.
		MsgClientDamageW msg = { 0 };
		msg.msg_type = MSG_TYPE_CLIENT_DAMAGE_W;

		msg.shard_id = g_shard_id;
		msg.selected_id = dps_target ? dps_target->t.id : 0;
		msg.stats = player->stats;
		msg.in_combat = player->in_combat;
		msg.profession = player->profession;
		msg.time = g_player.combat.duration;

		//msg.damage = g_player.combat.damage_out;
		msg.damage = InterlockedCompareExchange(&g_player.combat.damageOut, 0, 0);
		msg.damage_in = InterlockedCompareExchange(&g_player.combat.damageIn, 0, 0);
		msg.heal_out = InterlockedCompareExchange(&g_player.combat.healOut, 0, 0);

		wmemcpy(msg.name, player->name, MSG_NAME_SIZE);
		memcpy(msg.closest, g_closest_names, sizeof(g_closest_names));

		// Figure out the most recent targets.
		get_recent_targets(target, char_array, msg.targets, MSG_TARGETS, NULL, now);

		// Send the message.
		network_send(&msg, sizeof(msg));
	}
	else {

		MsgClientDamageUTF8 msg = { 0 };
		msg.msg_type = MSG_TYPE_CLIENT_DAMAGE_UTF8;

		msg.shard_id = g_shard_id;
		msg.selected_id = dps_target ? dps_target->t.id : 0;
		msg.stats = player->stats;
		msg.in_combat = player->in_combat;
		msg.profession = player->profession;
		msg.time = g_player.combat.duration;

		//msg.damage = g_player.combat.damage_out;
		msg.damage = InterlockedCompareExchange(&g_player.combat.damageOut, 0, 0);
		msg.damage_in = InterlockedCompareExchange(&g_player.combat.damageIn, 0, 0);
		msg.heal_out = InterlockedCompareExchange(&g_player.combat.healOut, 0, 0);

		utf16_to_utf8(player->name, msg.name, sizeof(player->name));

		for (int i=0; i<ARRAYSIZE(g_closest_names); i++)
			utf16_to_utf8(g_closest_names[i], msg.closest[i].name, sizeof(msg.closest[i].name));

		// Figure out the most recent targets.
		get_recent_targets(target, char_array, msg.targets, MSG_TARGETS, NULL, now);

		// Send the message.
		network_send(&msg, sizeof(msg));
	}
}

// Compare buff player
static int __cdecl ClosePlayerCompare(void* pCtx, void const* pItem1, void const* pItem2)
{
	i32 retVal = 0;
	u32 sortBy = LOBYTE(*(u32*)pCtx);
	bool isAsc = HIBYTE(*(u32*)pCtx);
	const  ClosePlayer* p1 = pItem1;
	const  ClosePlayer* p2 = pItem2;

#define BUFF_PERCENT(x, y) ((u32)(x->cd.y / (f32)(x->cd.duration) * 100.0f))
#define BUFF_COMPARE(x) \
		if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1; \
		else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1; \
		else if (BUFF_PERCENT(p1, x) < BUFF_PERCENT(p2, x)) retVal = -1; \
		else if (BUFF_PERCENT(p1, x) > BUFF_PERCENT(p2, x)) retVal = 1; \
		else retVal = 0;

#define BUFF_COMPARE_NOPCT(x) \
		if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1; \
		else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1; \
		else if (p1->cd.x < p2->cd.x) retVal = -1; \
		else if (p1->cd.x > p2->cd.x) retVal = 1; \
		else retVal = 0;


	const char * profName1 = name_by_profession(p1->c.profession);
	const char * profName2 = name_by_profession(p2->c.profession);

	switch (sortBy)
	{
	case(BUFF_COL_NAME):
		if (!p1->name && p2->name)
			retVal = -1;
		else if (!p2->name && p1->name)
			retVal = 1;
		else if (!p1->name && !p2->name)
			retVal = 0;
		else
			retVal = wcscmp(p1->name, p2->name);
		break;
	case(BUFF_COL_CLS):
		retVal = strcmp(profName1, profName2);
		break;
	case(BUFF_COL_SUB):
		if (p1->subGroup <= 0 && p2->subGroup > 0) retVal = -1;
		else if (p2->subGroup <= 0 && p1->subGroup > 0) retVal = 1;
		else if (p1->subGroup < p2->subGroup) retVal = -1;
		else if (p1->subGroup > p2->subGroup) retVal = 1;
		else retVal = 0;
		break;
	case(BUFF_COL_DOWN):
		if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1;
		else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1;
		else if (p1->cd.noDowned < p2->cd.noDowned) retVal = -1;
		else if (p1->cd.noDowned > p2->cd.noDowned) retVal = 1;
		else retVal = 0;
		break;
	case(BUFF_COL_SCLR):
		BUFF_COMPARE(sclr_dura);
		break;
	case(BUFF_COL_SEAW):
		BUFF_COMPARE(seaw_dura);
		break;
	case(BUFF_COL_PROT):
		BUFF_COMPARE(prot_dura);
		break;
	case(BUFF_COL_QUIK):
		BUFF_COMPARE(quik_dura);
		break;
	case(BUFF_COL_ALAC):
		BUFF_COMPARE(alac_dura);
		break;
	case(BUFF_COL_FURY):
		BUFF_COMPARE(fury_dura);
		break;
	case(BUFF_COL_MIGHT):
		BUFF_COMPARE_NOPCT(might_avg);
		break;
	case(BUFF_COL_GOTL):
		BUFF_COMPARE_NOPCT(gotl_avg);
		break;
	case(BUFF_COL_GLEM):
		BUFF_COMPARE(glem_dura);
		break;
	case(BUFF_COL_VIGOR):
		BUFF_COMPARE(vigor_dura);
		break;
	case(BUFF_COL_SWIFT):
		BUFF_COMPARE(swift_dura);
		break;
	case(BUFF_COL_STAB):
		BUFF_COMPARE(stab_dura);
		break;
	case(BUFF_COL_RETAL):
		BUFF_COMPARE(retal_dura);
		break;
	case(BUFF_COL_RESIST):
		BUFF_COMPARE(resist_dura);
		break;
	case(BUFF_COL_REGEN):
		BUFF_COMPARE(regen_dura);
		break;
	case(BUFF_COL_AEGIS):
		BUFF_COMPARE(aegis_dura);
		break;
	case(BUFF_COL_BANS):
		BUFF_COMPARE(bans_dura);
		break;
	case(BUFF_COL_BAND):
		BUFF_COMPARE(band_dura);
		break;
	case(BUFF_COL_BANT):
		BUFF_COMPARE(bant_dura);
		break;
	case(BUFF_COL_EA):
		BUFF_COMPARE(warEA_dura);
		break;
	case(BUFF_COL_SPOTTER):
		BUFF_COMPARE(spot_dura);
		break;
	case(BUFF_COL_FROST):
		BUFF_COMPARE(frost_dura);
		break;
	case(BUFF_COL_SUN):
		BUFF_COMPARE(sun_dura);
		break;
	case(BUFF_COL_STORM):
		BUFF_COMPARE(storm_dura);
		break;
	case(BUFF_COL_STONE):
		BUFF_COMPARE(stone_dura);
		break;
	case(BUFF_COL_ENGPP):
		BUFF_COMPARE(engPPD_dura);
		break;
	case(BUFF_COL_REVNR):
		BUFF_COMPARE(revNR_dura);
		break;
	case(BUFF_COL_REVAP):
		BUFF_COMPARE(revAP_dura);
		break;
	case(BUFF_COL_NECVA):
		BUFF_COMPARE(necVA_dura);
		break;
	case(BUFF_COL_TIME):
	{
		u32 p1time = (u32)(p1->cd.duration / 1000.0f);
		u32 p2time = (u32)(p2->cd.duration / 1000.0f);
		if (p1time == 0 && p2time != 0) retVal = -1;
		else if (p2time == 0 && p1time != 0) retVal = 1;
		else if (p1time < p2time) retVal = -1;
		else if (p1time > p2time) retVal = 1;
		else retVal = 0;
	}
		break;
	};

	if (isAsc)
		return retVal;
	else
		return -(retVal);
}

// Group DPS module.
static void module_buff_uptime(Character* player, Array* char_array, i64 now, i32 gui_x, i32 gui_y)
{

	static ClosePlayer temp_players[MSG_CLOSEST_PLAYERS + 1];
	u32 total = 0;

	// Reset our player pool
	memset(temp_players, 0, sizeof(temp_players));

	// Add current player to our temp array
	temp_players[total].c = g_player.c;
	temp_players[total].name = &g_player.c.name[0];
	memcpy(&temp_players[total].cd, &g_player.combat, sizeof(temp_players[total].cd));
	++total;

	// Add closest player data
	for (u32 i = 0; i < g_closest_num; ++i)
	{
		temp_players[total].c = g_closest_players[i].c;
		temp_players[total].name = &g_closest_players[i].name[0];
		memcpy(&temp_players[total].cd, &g_closest_players[i].cd, sizeof(temp_players[total].cd));
		++total;
	}


	// Get squad subgroup
	for (u32 i = 0; i < total; ++i) {
		temp_players[i].subGroup = Pl_GetSquadSubgroup(g_squad_ctx/*g_agent_av_ctx*/, temp_players[i].c.ctptr);
	}

	// Default sort: alphabetically (exclude self)
	u32 sortBy = 0;
	if (g_state.panel_buff_uptime.cfg.sortCol == BUFF_COL_NAME &&
		g_state.panel_buff_uptime.cfg.sortColLast == BUFF_COL_END &&
		g_state.panel_buff_uptime.cfg.asc == 1)
	{
		sortBy = MAKEWORD(g_state.panel_buff_uptime.cfg.sortCol, g_state.panel_buff_uptime.cfg.asc);
		qsort_s(&temp_players[1], total - 1, sizeof(temp_players[0]), ClosePlayerCompare, &sortBy);
	}
	else
	{
		sortBy = MAKEWORD(g_state.panel_buff_uptime.cfg.sortCol, g_state.panel_buff_uptime.cfg.asc);
		qsort_s(&temp_players[0], total, sizeof(temp_players[0]), ClosePlayerCompare, &sortBy);
	}


	if (g_state.panel_buff_uptime.enabled) {
		if (g_state.renderImgui)
			ImGui_BuffUptime(&g_state.panel_buff_uptime, gui_x, gui_y, temp_players, total);
		else
			draw_buff_uptime(&g_state.panel_buff_uptime, &g_state.panel_buff_uptime.cfg, temp_players, total, gui_x, gui_y);
	}
}

// Special action notification module.
static void module_special_action(Character* player, i64 now)
{
	// Check if the scene is loading.
	static i64 load_time;
	static vec3 last_pos;
	static bool is_loaded;

	vec3 player_pos = player->pos;
	f32 dist =
		(player_pos.x - last_pos.x) * (player_pos.x - last_pos.x) +
		(player_pos.y - last_pos.y) * (player_pos.y - last_pos.y) +
		(player_pos.z - last_pos.z) * (player_pos.z - last_pos.z);

	if (player->hp_val == 0 || dist > 122500.0f)
	{
		is_loaded = false;
		load_time = now;
	}

	last_pos = player->pos;
	is_loaded = (now - load_time > 10000000);

	// Check if the special action is on, and beep if it is.
	if (g_sa_ctx && is_loaded)
	{
		u32 sa = process_read_u32(g_sa_ctx + OFF_SACTX_ACTIVE);
		static bool sa_on;

		if (sa == true && sa_on == false)
		{
			PlaySoundA("bin64\\boop.wav", 0, SND_FILENAME | SND_NODEFAULT | SND_ASYNC);
			sa_on = true;
		}
		else if (sa == false)
		{
			sa_on = false;
		}
	}
}

// Draw version info
void dps_version(f32 time)
{
	static bool update_init = 0;
	static i8 ver_buff[1024] = { 0 };

	module_toggle_bind_bool(&g_state.panel_version);
	if (!g_state.panel_version.enabled)
		return;

	// default UI location
	if (g_state.panel_version.pos.x < 0)
		g_state.panel_version.pos.x = 2;
	if (g_state.panel_version.pos.y < 0)
		g_state.panel_version.pos.y = 2;

	i32 cur_x = g_state.panel_version.pos.x;
	i32 cur_y = g_state.panel_version.pos.y;
	u32 color = COLOR_GRAY;
	u32 font_width = renderer_get_font7_x();
	u32 font_height = renderer_get_font7_y();
	i32 spacing = (u32)(font_height*0.5f);
	u32 w = (u32)(font_width*19.0f);
	u32 h = (u32)(font_height*1.0f);
	u32 noLines = 0;
	Panel *panel = &g_state.panel_version;

	for (i32 i = VER_ROW_VER; i < VER_ROW_END; ++i) {
		if (panel->cfg.col_visible[i]) {
			h += (u32)(font_height*1.0f);
			if (i >= VER_ROW_SRV_TEXT && w < (u32)(font_width*20.0f)) {
				w += (u32)(font_width*7.0f);
				h += (u32)(font_height*0.5f);
			}
			else if (i < VER_ROW_SRV_TEXT) {
				noLines++;
			}
		}
	}

	renderer_draw_rect(cur_x, cur_y, w, h, COLOR_BLACK_OVERLAY(g_state.panel_version.alpha), &g_state.panel_version.rect);

	int len = 0;
	memset(ver_buff, 0, sizeof(ver_buff));

	if (panel->cfg.col_visible[VER_ROW_VER])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "Version:\n");
	if (panel->cfg.col_visible[VER_ROW_SIG])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "Sig:\n");
	if (panel->cfg.col_visible[VER_ROW_PING])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "Ping:\n");
	if (panel->cfg.col_visible[VER_ROW_FPS])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "FPS:\n");
	if (panel->cfg.col_visible[VER_ROW_PROC])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "Proc:\n");

	renderer_draw_text(cur_x + 5, cur_y + spacing, COLOR_WHITE, ver_buff, true);


	len = 0;
	memset(ver_buff, 0, sizeof(ver_buff));
	if (panel->cfg.col_visible[VER_ROW_VER])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "%s\n", updater_get_version_str());
	if (panel->cfg.col_visible[VER_ROW_SIG])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "%x\n", updater_get_cur_version());
	if (panel->cfg.col_visible[VER_ROW_PING]) {
		u32 ping = process_read_i32(g_ping_ctx);
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "%dms\n", ping);
	}
	if (panel->cfg.col_visible[VER_ROW_FPS]) {
		u32 fps = process_read_i32(g_fps_ctx);
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "%d\n", fps);
	}
	if (panel->cfg.col_visible[VER_ROW_PROC])
		len += snprintf(ver_buff + len, sizeof(ver_buff) - len, "%.1fms\n", time);

	renderer_draw_text(cur_x + 5 + (u32)(font_width*9.0f), cur_y + spacing, color, ver_buff, true);


	// Display status message if updating.
	cur_y += spacing*2 + (u32)(font_height*noLines);

	if (panel->cfg.col_visible[VER_ROW_SRV_TEXT]) {

		u32 update_offset, update_size;
		if (updater_is_updating(&update_offset, &update_size))
		{
			update_init = 1;
			renderer_printf(
				true,
				cur_x + 5, cur_y,
				COLOR_WHITE,
				"UPDATING: %.1f/%.1fKB",
				update_offset / 1024.0f, update_size / 1024.0f);
		}
		else
		{
			if (g_state.autoUpdate_disable) {

				renderer_printf(
					true,
					cur_x + 5, cur_y,
					COLOR_WHITE,
					"AUTO-UPDATE DISABLED"
				);
			}
			else if (updater_get_srv_version() != 0)
			{
				if (updater_get_srv_version() == updater_get_cur_version())
				{
					renderer_printf(
						true,
						cur_x + 5, cur_y,
						COLOR_WHITE,
						update_init ? "UPDATE SUCCESS" : "NO UPDATE REQUIRED"
					);
				}
				else
				{
					renderer_printf(
						true,
						cur_x + 5, cur_y,
						COLOR_WHITE,
						update_init ? "UPDATE %x FAILED" : "SRV UPDATE %x",
						updater_get_srv_version()
					);
				}
			}
			else
			{
				renderer_printf(
					true,
					cur_x + 5, cur_y,
					COLOR_WHITE,
					"SERVER NOT CONNECTED"
				);
			}
		}

		cur_y += (u32)(font_height*1.0f);
	}

	if (panel->cfg.col_visible[VER_ROW_SRV_TIME]) {
		time_t srv_time = updater_get_srv_time();
		if (srv_time != 0)
		{
			struct tm tm = { 0 };
			len = 0;
			memset(ver_buff, 0, sizeof(ver_buff));
			localtime_s(&tm, &srv_time);
			strftime(&ver_buff[len], sizeof(ver_buff) - len, "%Y-%m-%dT%H:%M:%SZ", &tm);
			renderer_draw_text(cur_x + 5, cur_y, color, ver_buff, true);
		}
	}
}

// Updates and draws debug information.
static void module_debug(Character* player, Character* target, Array* char_array, Array* play_array, i64 now)
{
	// Disable if not in debug mode.
	if (g_state.panel_debug.enabled == false || g_state.dbg_buf == 0)
	{
		return;
	}

	if (g_state.renderImgui) {
		ImGui_Debug(&g_state.panel_debug, player, target, char_array, play_array);
		return;
	}

	// Check if we need to toggle the debug panel.
	static bool is_toggled;
	static bool is_visible;

	bool key = is_bind_down(&g_state.panel_debug.bind);

	if (key && is_toggled == false)
	{
		is_toggled = true;
		is_visible = !is_visible;
	}
	else if (key == false)
	{
		is_toggled = false;
	}

	// Check if the debug panel is visible.
	if (is_visible == false)
	{
		return;
	}

	// default UI location
	if (g_state.panel_debug.pos.x < 0)
		g_state.panel_debug.pos.x = 300;
	if (g_state.panel_debug.pos.y < 0)
		g_state.panel_debug.pos.y = 2;

	i32 cur_x = g_state.panel_debug.pos.x;
	i32 cur_y = g_state.panel_debug.pos.y;
	u32 color = COLOR_WHITE;
	u32 font_width = renderer_get_font7_x();
	u32 font_height = renderer_get_font7_y();
	i32 spacing = (u32)(font_height*0.5f);
	u32 w = (u32)(font_width*175.0f);
	u32 h = (u32)(font_height*44.0f);

	// Draw game, player, and target state.
	_snprintf(g_state.dbg_buf, MAX_BUF,
		"== STATE ==\n"
		"actx  %016I64X\n"
		"avctx %016I64X\n"
		"cctx  %016I64X\n"
		"hctx  %016I64X\n"
		"sctx  %016I64X\n"
		"uctx  %016I64X\n"
		"ctctx %016I64X\n"
		"sqctx %016I64X\n"
		"cap   %016I64X\n"
		"can   %u\n"
		"cam   %u\n"
		"mapi  %u\n"
		"mapt  %u\n"
		"shrd  %u\n"
		"actp  %016I64X\n"
		"actn  %u\n"
		"now   %lld\n"

		"\n== TARGET ==\n"
		"aptr  %016I64X\n"
		"cptr  %016I64X\n"
		"pptr  %016I64X\n"
		"avptr %016I64X\n"
		"ctptr %016I64X\n"
		"cmbt  %016I64X\n"
		"invt  %016I64X\n"
		"id    %u\n"
		"att   %u\n"
		"type  %u\n"
		"ispl  %u\n"
		"iscb  %u\n"
		"prof  %u\n"

		"\n== PLAYER ==\n"
		"aptr  %016I64X\n"
		"cptr  %016I64X\n"
		"pptr  %016I64X\n"
		"avptr %016I64X\n"
		"ctptr %016I64X\n"
		"cmbt  %016I64X\n"
		"invt  %016I64X\n"
		"iscb  %u\n"
		"prof  %u\n"
		,
		g_agent_ctx, g_agent_av_ctx, g_client_ctx, g_hide_ctx, g_sa_ctx, g_ui_ctx,
		g_contact_ctx, g_squad_ctx, char_array->data, char_array->cur, char_array->max,
		g_map_id, g_map_type, g_shard_id,
		g_actionCam_ctx, g_actionCam_ctx ? process_read_u32(g_actionCam_ctx) : 0, now,
		target->aptr, target->cptr, target->pptr, Ag_GetWmAgemt(target->aptr), target->ctptr,
		target->cbptr, target->inventory, target->id, target->attitude,
		target->type, target->is_player, target->in_combat, target->profession,
		player->aptr, player->cptr, player->pptr, Ag_GetWmAgemt(player->aptr), player->ctptr, player->cbptr, player->inventory,
		player->in_combat, player->profession
	);


	renderer_draw_rect(cur_x, cur_y, w, h, COLOR_BLACK_OVERLAY(g_state.panel_debug.alpha), &g_state.panel_debug.rect);
	renderer_draw_text(cur_x+5, cur_y+spacing, color, g_state.dbg_buf, true);

	// Draw dps target information.
	u32 n = 0;
	memset(g_state.dbg_buf, 0, sizeof(g_state.dbg_buf));
	n += snprintf(g_state.dbg_buf, MAX_BUF, "  ====  TARGETS ARRAY  ====\n");
	for (u32 i = 0; i < MAX_TARGETS; ++i)
	{
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "%3d %4d %016I64X %lld\n",
			i, g_targets[i].id, g_targets[i].aptr, g_targets[i].time_update);
	}

	renderer_draw_text(cur_x+ spacing +(u32)(font_width*22.0f), cur_y+5, color, g_state.dbg_buf, true);

	// Draw buff bar information
	n = 0;
	u64 cbptr = (target && target->is_player && target->cbptr) ? target->cbptr : player->cbptr;
	memset(g_state.dbg_buf, 0, sizeof(g_state.dbg_buf));

	n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "==  SPEC  ==\n");
	
	Spec spec = { 0 };
	u64 pptr = (target && target->is_player && target->pptr) ? target->pptr : player->pptr;
	if (Pl_GetSpec(pptr, &spec))
	{
		for (u32 i = 0; i < SPEC_SLOT_END; ++i)
		{
			n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "Spec %d:%2d ", i, spec.specs[i].id);
			for (u32 j = 0; j < TRAIT_SLOT_END; ++j)
			{
				n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, " :%5d ", spec.traits[i][j].id);
			}
			n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "\n");
		}
	}

	n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "\n== BUFF BAR ==\n\n");

	/*u64 buffBarPtr = process_read_u64(cbptr + OFF_CMBTNT_BUFFBAR);
	if (buffBarPtr) {

		u32 idx = 1;
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "CAPACITY = %4d   COUNT = %4d\n", buff_array.max, buff_array.cur);
		for (u32 i = 0; i < buff_array.max; ++i)
		{
			BuffEntry be;
			process_read(buff_array.data + i * sizeof(BuffEntry), &be, sizeof(BuffEntry));
			if (be.pBuff)
			{
				i32 efType = process_read_i32((u64)be.pBuff + OFF_BUFF_EF_TYPE);
				n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, (idx%4==0) ? "%3d:%6d\n" : "%3d:%6d  ", i, efType);
				++idx;
			}
		}

		BuffStacks stacks = { 0 };
		read_buff_array(buffBarPtr, &stacks);
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "\n\nMGHT = %2d    FURY = %2d", stacks.might, stacks.fury);
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n,   "\nQUIK = %2d    ALAC = %2d", stacks.quick, stacks.alacrity);
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n,   "\nGOTL = %2d    PROT = %2d", stacks.GOTL, stacks.prot);
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n,   "\nBANS = %2d    BAND = %2d", stacks.warStr, stacks.warDisc);
	}*/

	renderer_draw_text(cur_x + spacing + (u32)(font_width*60.0f), cur_y + spacing, color, g_state.dbg_buf, true);
	renderer_printfW(true,
		cur_x + spacing + (u32)(font_width*60.0f),
		cur_y + spacing + (u32)(font_height*6.0f),
		color_by_profession((target && cbptr == target->cbptr) ? target : player, true),
		L"NAME: %s",
		(target && cbptr == target->cbptr) ? target->name : player->name
	);


	POINT ppAg = { 0, 0 };
	POINT ptAg = { 0, 0 };
	POINT ppMum = { 0, 0 };
	POINT ptMum = { 0, 0 };
	vec4 tPos = { 0.f, 0.f, 0.f };
	vec4 pPos = { 0.f, 0.f, 0.f };
	project2d_agent(&ppAg, &pPos, player->aptr, now);
	project2d_agent(&ptAg, &tPos, target ? target->aptr : 0, now);
	project2d_mum(&ppMum, &g_mum_mem->avatar_pos/*&player->pos*/, player->aptr, true);
	project2d_mum(&ptMum, target ? &target->pos : 0, target ? target->aptr : 0, false);

	// Draw mumble link information
	memset(g_state.dbg_buf, 0, sizeof(g_state.dbg_buf));
	n = snprintf(g_state.dbg_buf, MAX_BUF,
		"== CAM INFO ==\n"
		"wv_ptr        %016I64X\n"
		"wv_ctx        %016I64X\n"
		"camPos        %03.2f  %03.2f %03.2f\n"
		"upVec         %03.2f  %03.2f %03.2f\n"
		"lookAt        %03.2f  %03.2f %03.2f\n"
		"viewVec       %03.2f  %03.2f %03.2f\n"
		"fovy          %f\n"
		"curZoom       %f\n"
		"minZoom       %f\n"
		"maxZoom       %f\n"

		"\n== MUMBLE INFO ==\n"
		"mum_ctx       %p\n"
		"cam_fovy      %f\n"
		"cam_front     %03.2f  %03.2f %03.2f\n"
		"cam_pos       %03.2f  %03.2f %03.2f\n"
		"avatar_front  %03.2f  %03.2f %03.2f\n"
		"avatar_pos    %03.2f  %03.2f %03.2f\n"

		"\n== VIEWPORT INFO ==\n"
		"res_x         %d\n"
		"res_y         %d\n"

		"\n== POSITION INFO ==\n"
		"player_pos    %03.2f  %03.2f %03.2f  (%d,%d)\n"
		"target_pos    %03.2f  %03.2f %03.2f  (%d,%d)\n"
		"player_agpos  %03.2f  %03.2f %03.2f %03.2f (%d,%d)\n"
		"target_agpos  %03.2f  %03.2f %03.2f %03.2f (%d,%d)\n"
		, g_wv_ctx, g_wv_ctx ? process_read_u64(g_wv_ctx) : 0
		, g_cam_data.camPos.x, g_cam_data.camPos.y, g_cam_data.camPos.z
		, g_cam_data.upVec.x, g_cam_data.upVec.y, g_cam_data.upVec.z
		, g_cam_data.lookAt.x, g_cam_data.lookAt.y, g_cam_data.lookAt.z
		, g_cam_data.viewVec.x, g_cam_data.viewVec.y, g_cam_data.viewVec.z
		, g_cam_data.fovy, g_cam_data.curZoom, g_cam_data.minZoom, g_cam_data.maxZoom
		, g_mum_mem, get_mum_fovy()
		, g_mum_mem->cam_front.x, g_mum_mem->cam_front.y, g_mum_mem->cam_front.z
		, g_mum_mem->cam_pos.x, g_mum_mem->cam_pos.y, g_mum_mem->cam_pos.z
		, g_mum_mem->avatar_front.x, g_mum_mem->avatar_front.y, g_mum_mem->avatar_front.z
		, g_mum_mem->avatar_pos.x, g_mum_mem->avatar_pos.y, g_mum_mem->avatar_pos.z
		, renderer_get_res_x(), renderer_get_res_y()
		, player->pos.x, player->pos.y, player->pos.z, ppMum.x, ppMum.y
		, target ? target->pos.x : 0.f, target ? target->pos.y : 0.f , target ? target->pos.z : 0.f, ptMum.x, ptMum.y
		, pPos.x, pPos.y, pPos.z, pPos.w, ppAg.x, ppAg.y
		, tPos.x, tPos.y, tPos.z, tPos.w, ptAg.x, ptAg.y
	);

	n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "\n  ====  CLOSEST ARRAY  ====\n");
	for (u32 i = 0; i < g_closest_num; ++i)
	{
		n += snprintf(g_state.dbg_buf + n, MAX_BUF - n, "%d %016I64X %-20S\n",
			i, g_closest_players[i].c.cptr, g_closest_names[i]
		);
	}

	renderer_draw_text(cur_x + spacing + (u32)(font_width*110.0f), cur_y + 5, color, g_state.dbg_buf, true);

	// Draw self
	draw_player_tag(player, player, now);

}



bool GetEmbeddedResource(int ResID, LPVOID *ppData, LPDWORD pBytes)
{
	HMODULE hMod = g_hInstance;

	HRSRC hRes = FindResource(hMod, MAKEINTRESOURCE(ResID), RT_RCDATA);
	if (!hRes)
	{
		DBGPRINT(TEXT("FindResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return false;
	}

	DWORD len = SizeofResource(hMod, hRes);
	if ((len == 0) && (GetLastError() != 0))
	{
		DBGPRINT(TEXT("SizeofResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return false;
	}

	HGLOBAL hMem = LoadResource(hMod, hRes);
	if (!hMem)
	{
		DBGPRINT(TEXT("LoadResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return false;
	}

	PVOID pData = LockResource(hMem);
	if (!pData)
	{
		DBGPRINT(TEXT("LockResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return false;
	}
	if (ppData) *ppData = pData;
	if (pBytes) *pBytes = len;
	return (pData != NULL);
}


void mumble_link_create(void)
{
	// Open the mumble link.
	g_mum_link = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, TEXT(MUMBLE_LINK));
	if (g_mum_link == 0)
	{
		g_mum_link = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, sizeof(LinkedMem), TEXT(MUMBLE_LINK));
	}

	if (g_mum_link)
	{
		g_mum_mem = (LinkedMem*)MapViewOfFile(g_mum_link, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
	}
}

void mumble_link_destroy(void)
{
	if (g_mum_mem)
	{
		UnmapViewOfFile((LPVOID)g_mum_mem);
		g_mum_mem = NULL;
	}

	if (g_mum_link)
	{
		CloseHandle(g_mum_link);
		g_mum_link = NULL;
	}
}


void dps_create(void)
{
	if (g_state.log_dir == 0)
	{
		return;
	}

	// Allocate memory for DPS resources.
	g_targets = calloc(MAX_TARGETS, sizeof(*g_targets));
	if (g_targets == 0)
	{
		return;
	}

	g_closest_pap = calloc(MAX_PLAYERS, sizeof(*g_closest_pap));
	if (g_closest_pap == 0)
	{
		return;
	}

	// Allocate debug resources.
	if (g_state.panel_debug.enabled)
	{
		g_state.dbg_buf = calloc(1, MAX_BUF);
	}

	// Create the logging directory.
	CreateDirectoryA(g_state.log_dir, 0);

	// Get the agent view context.
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap)
	{
		THREADENTRY32 te;
		te.dwSize = sizeof(te);

		for (BOOL res = Thread32First(snap, &te); res; res = Thread32Next(snap, &te))
		{
			if (te.th32OwnerProcessID != process_get_id())
			{
				continue;
			}

			HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
			if (thread == 0)
			{
				continue;
			}

			struct
			{
				NTSTATUS ExitStatus;
				PVOID TebBaseAddress;
				struct
				{
					HANDLE UniqueProcess;
					HANDLE UniqueThread;
				} ClientId;
				KAFFINITY AffinityMask;
				LONG Priority;
				LONG BasePriority;
			} tbi;

			if (NtQueryInformationThread(thread, (THREADINFOCLASS)0, &tbi, sizeof(tbi), 0) == 0)
			{
				u64 tls = process_read_u64((u64)tbi.TebBaseAddress + 0x58);
				u64 main = process_read_u64(tls);
				u64 slot = process_read_u64(main + OFF_TLS_SLOT);
				u64 ctx = process_read_u64(slot + OFF_CTX_CCTX);

				if (ctx)
				{
					g_client_ctx = ctx;
					break;
				}
			}

			CloseHandle(thread);
		}

		CloseHandle(snap);
	}

#if !(defined BGDM_TOS_COMPLIANT)
	// "Gw2China"
	// 47 00 77 00 32 00 43 00 68 00 69 00 6E 00 61 00  G.w.2.C.h.i.n.a.   <- didn't work, we search for the CTX instead
	// B8 6F 00 00 00 C3 CC CC CC CC CC CC CC CC CC CC 48 8D 05 ?? ?? ?? ?? C3
	u64 gw2china = process_scan(
		"\xB8\x6F\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x8D\x05\x00\x00\x00\x00\xC3",
		"xxxxxxxxxxxxxxxxxxx????x");
	if (gw2china > 0) g_state.is_gw2china = true;
#endif

	// Get the build ID
	// "Build %u"
	// B9 01 00 00 00 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 15
	u64 build_ptr = g_agent_ctx = process_follow(
		"\xB9\x01\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8D\x15",
		"xxxxxx????x????xxx", 0xB);

	if (build_ptr)
		g_buildId = process_read_u32(build_ptr + 0x1);

	DBGPRINT(TEXT("<GW2_buildId=%d>"), g_buildId);


	// Get the Agent context
	// "ViewAdvanceAgentSelect"
	// 48 83 C4 70 5E C3 CC CC CC CC CC CC CC CC 48 8D 05 ?? ?? ?? ?? C3
	g_agent_ctx = process_follow(
		"\x48\x83\xC4\x70\x5E\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x8D\x05\x00\x00\x00\x00\xC3",
		"xxxxxxxxxxxxxxxxx????x", 17);

	// Get the AgentView context
	// "ViewAdvanceAgentView"
	// 48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 8B 81 ?? ?? ?? ?? 48 85 C0 74 05
	g_agent_av_ctx = process_follow(
		"\x48\x8D\x05\x00\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x8B\x81\x00\x00\x00\x00\x48\x85\xC0\x74\x05",
		"xxx????xxxxxxxxxxxx????xxxxx", 3);

	// Get the squadContext
	// search for "squadCliContext" RETx3 and look for ref to [RSI+480]
	// rsi is the ctx, find ref to RSI and you get the function that returns it
	// 48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 8B 81 ?? ?? ?? ?? 48 85 C0 74 05
	// LOL -> Turned out to be AgentView ctx ^^


	// Get the UI context.
	// Searcg for "m_curScale > -10" scroll up 1 whole function till you see the context LEA
	// reference hit #7 for search for ctx LEA should return a function with both
	// "!((unsigned_ptr)this & 3)" & "!TermCheck(ptr)" x2
	g_ui_ctx = process_follow(
		"\x48\x8D\x05\x00\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x89\x5C\x24\x18\x48\x89\x74\x24\x20\x57",
		"xxx????xxxxxxxxxxxxxxxxxxxx", 3);

	// Get the special action context.
	// Search for "flags < ANCHOR_POINT_FLAG_USABLE_END" (1st assert)
	// and look for the first LEA instruction below the assert
	g_sa_ctx = process_follow(
		"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x20\x48\x8B\x3D\x00\x00\x00\x00",
		"xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxx????", 41);

	// Get the UI hiding offset.
	g_hide_ctx = process_follow(
		"\x8B\x15\x00\x00\x00\x00\x45\x33\xC9\x85\xD2\x45\x8B\xC1\x41\x0F\x99\xC0\x85\xC9\x74\x04\xFF\xC2",
		"xx????xxxxxxxxxxxxxxxxxx", 2);

	// For map/ping/fps search for:
	// 'CameraSettings()->GetDebugControllerType()'
	// Get the map open offset
	// 83 3D ?? ?? ?? ?? 00 74 0A B8 10 00 00 00 E9
	g_map_open_ctx = process_follow(
		"\x83\x3D\x00\x00\x00\x00\x00\x74\x0A\xB8\x10\x00\x00\x00\xE9",
		"xx????xxxxxxxxx", 0x15);


	// Get the ping context
	// CC 4C 8B DA 33 C0 4C 8D 0D ?? ?? ?? ?? 48 8B D1
	g_ping_ctx = process_follow(
		"\xCC\x4C\x8B\xDA\x33\xC0\x4C\x8D\x0D\x00\x00\x00\x00\x48\x8B\xD1",
		"xxxxxxxxx????xxx", 0x9);


	// Get the fps context
	// CC 83 0D ?? ?? ?? ?? 20 89 0D ?? ?? ?? ?? C3 CC
	g_fps_ctx = process_follow(
		"\xCC\x83\x0D\x00\x00\x00\x00\x20\x89\x0D\x00\x00\x00\x00\xC3\xCC",
		"xxx????xxx????xx", 0xA);

	// Compass (minimap context)
	// Search for "CompassManager()->IsCompassFixed()"
	// first call after the assert is the context
	// 48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 8B 41 30 89 02
	g_comp_ctx = process_follow(
		"\x48\x8D\x05\x00\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x8B\x41\x30\x89\x02",
		"xxx????xxxxxxxxxxxxxx", 0x3);

	// Get the WorldView context
	// Search for ViewAdvanceWorldView
	// E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 41 0F 28 D0 0F 28 CE
	u64 p_worldview = process_follow(
		"\xE8\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x41\x0F\x28\xD0\x0F\x28\xCE",
		"x????xxx????x????x????xxxxxxx", 0x12);

	if (p_worldview)
		g_wv_ctx = process_follow_rel_addr(p_worldview + 0x7);


	// Action cam ctx
	// search for "!s_interactiveContext->filePath[0]"
	// go to 3rd call from start of function and then roughly 3rd call again
	// 4C 8D 4C 24 38 8D 50 06 44 8D 40 04 48 8B CB 89 05 ?? ?? ?? ?? 89 05 ?? ?? ?? ??
	g_actionCam_ctx = process_follow(
		"\x4C\x8D\x4C\x24\x38\x8D\x50\x06\x44\x8D\x40\x04\x48\x8B\xCB\x89\x05\x00\x00\x00\x00\x89\x05\x00\x00\x00\x00",
		"xxxxxxxxxxxxxxxxx????xx", 0x17);


	// Get wmAgent from agent ptr
	// search for "guildTagLogoFrame" and go the 2nd "agent" assert going up
	// first call after that is GetWmAgentFromAgent
	// 49 8B CF E8 ?? ?? ?? ?? 48 85 C0 74 1C 48 8B 10 48 8B C8 FF 52 70 48 85 C0
	g_getWmAgent = process_follow(
		"\x49\x8B\xCF\xE8\x00\x00\x00\x00\x48\x85\xC0\x74\x1C\x48\x8B\x10\x48\x8B\xC8\xFF\x52\x70\x48\x85\xC0",
		"xxxx????xxxxxxxxxxxxxxxxx", 0x4);


	// Get chat box ctx
	// Search for "selectedTab" and look at the first function up
	// 48 83 EC 28 E8 ?? ?? ?? ?? 48 8B 80 98 00 00 00 48 83 C4 28 C3 CC
	g_getChatCtx = process_scan(
		"\x48\x83\xEC\x28\xE8\x00\x00\x00\x00\x48\x8B\x80\x98\x00\x00\x00\x48\x83\xC4\x28\xC3\xCC",
		"xxxxx????xxxxxxxxxxxxx");


	// Get contact ctx function
	// Search for "..\\..\\..\\Game\\Ui\\Widgets\\AgentStatus\\AsInfoBarFloating.cpp"
	// follow down a bit until you see a call with retval deref to r8 and called at VT+0x238 with RDX = pptr RCX = cptr
	// C3 CC CC CC CC CC 48 83 EC 28 E8 ?? ?? ?? ?? 48 8B 80 ?? ?? 00 00 48 83 C4 28 C3 CC CC CC CC CC CC CC CC CC CC CC 40 57
	g_getContactCtx = process_scan(
		"\xC3\xCC\xCC\xCC\xCC\xCC\x48\x83\xEC\x28\xE8\x00\x00\x00\x00\x48\x8B\x80\x00\x00\x00\x00\x48\x83\xC4\x28\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x40\x57",
		"xxxxxxxxxxx????xxx??xxxxxxxxxxxxxxxxxxxx");

	if (g_getContactCtx)
		g_getContactCtx += 6;

	// Get squad context function
	// Search for "squadContext" and look at the first call up
	// CC 48 83 EC 28 E8 ?? ?? ?? ?? 48 8B 80 78 02 00 00 48 83 C4 28 C3 CC CC CC CC CC CC CC CC CC CC CC 48
	g_getSquadCtx = process_scan(
		"\xCC\x48\x83\xEC\x28\xE8\x00\x00\x00\x00\x48\x8B\x80\x78\x02\x00\x00\x48\x83\xC4\x28\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48",
		"xxxxxx????xxxxxxxxxxxxxxxxxxxxxxxx");

	if (g_getSquadCtx)
		g_getSquadCtx += 1;

	// Search for:
	// "!buffer->Count() || (CParser::Validate(buffer->Ptr(), buffer->Term(), true ) == buffer->Term())"
	// The function that has "codedString" and "codedString[0]" below the assert
	// The function above it is what we need, it will have the below assert:
	// "CParser::Validate(buffer->Ptr(), buffer->Term(), true ) == buffer->Term()"
	// 53 57 48 83 EC 48 8B D9 E8 ?? ?? ?? ?? 48 8B 48 50 E8 ?? ?? ?? ?? 44 8B 4C 24 68 48 8D 4C 24 30 48 8B F8
	// - 0xE
	g_codedTextFromHashId = process_scan(
		"\x53\x57\x48\x83\xEC\x48\x8B\xD9\xE8\x00\x00\x00\x00\x48\x8B\x48\x50\xE8\x00\x00\x00\x00\x44\x8B\x4C\x24\x68\x48\x8D\x4C\x24\x30\x48\x8B\xF8",
		"xxxxxxxxx????xxxxx????xxxxxxxxxxxxx"
	);

	if (g_codedTextFromHashId)
		g_codedTextFromHashId -= 0xE;

	// Search for "resultFunc"
	// 49 8B E8 48 8B F2 48 8B F9 48 85 C9 75 19 48 8D 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 41 B8 65 05 00 00 E8
	g_decodeCodedText = process_scan(
		"\x49\x8B\xE8\x48\x8B\xF2\x48\x8B\xF9\x48\x85\xC9\x75\x19\x48\x8D\x15\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00\x41\xB8\x65\x05\x00\x00\xE8",
		"xxxxxxxxxxxxxxxxx????xxx????xxxxxxx"
	);

	if (g_decodeCodedText)
		g_decodeCodedText -= 0x14;


	// Damage Log & Combat Log
	// For combat log search for "logType < UI_COMBAT_LOG_TYPES"
	// For dmg log search for "targetAgent"
	// 41 56 48 83 EC 40 45 8B F1 49 8B F8 48 8B DA 48 85 D2 75 17
	u64 p_dmg_log = process_scan(
		"\x41\x56\x48\x83\xEC\x40\x45\x8B\xF1\x49\x8B\xF8\x48\x8B\xDA\x48\x85\xD2\x75\x17",
		"xxxxxxxxxxxxxxxxxxxx");
	if (p_dmg_log) {
		//p_dmg_log -= 0xF;
		g_cbDmgLog = p_dmg_log - 0xF;
		p_dmg_log = 0;
	}

	u64 p_cbt_log = process_scan(
		"\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x41\x8B\xF8\x48\x8B\xDA\x41\x83\xF8\x0E\x7C\x19\x48\x8D\x15\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00",
		"xxxxxxxxxxxxxxxxxxxxxxxxx????xxx????");

	// Hook the DPS processing function.
	// Search for assert string "type < AGENT_STATUS_COMBAT_EVENT_TYPES"
	u64 p_dmg_result = process_scan(
		"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x60\x4C\x63\xFA",
		"xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxx");

	// A tiny bit more up the stack we can find the funcion that gets called after (or before) 
	// the CBT log - here we can find the result of the hit (evade, abosrb, normal, etc)
	// Search for "channellOut"
	// WE DONT NEED THIS NOW SINCE WE USE hkDmgResult instead
	// result@2c aptr@0x58
	// 40 53 41 54 48 81 EC B8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 70
	u64 p_cbt_log_result = 0;/* process_scan(
		"\x40\x53\x41\x54\x48\x81\xEC\xB8\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x44\x24\x70",
		"xxxxxxxxxxxxxx????xxxxxxxx");*/


	// search for:
	// "..\\..\\..\\Game\\Portal\\Cli\\PlCliUser.cpp"
	// "index"
	// "index < m_count"
	u64 p_pl_cli_user = process_scan(
		"\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xF1\x48\x85\xD2\x75\x17\x44\x8D\x42\x65\x48\x8D\x0D",
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");


	//
	// Scan for the account name
	// Search for strings "DisplayName" or "hasVerifiedEmail"
	/*u64 p_pl_cli_get_user = process_scan(
		"\x33\xC0\x8B\xDA\x83\xFA\x08\x0F\x43\xD8\x48\x8B\xE9\xE8\x00\x00\x00\x00\x4C\x8B\x08\x48\x8D\x54\x24\x30\x41\xB8\x40\x00\x00\x00\x48\x8B\xC8",
		"xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxx");

	if (p_get_login_context)
	{
		p_get_login_context += 0xE;
		u64 p_login_context = 0;
		u64 pp_login_context_vt = 0;
		typedef uintptr_t(__fastcall*fpGetLoginCtx)(void);
		typedef void(__fastcall*fpGetAcctName)(uintptr_t, uintptr_t, int);
		fpGetLoginCtx fptrGetLoginCtx = (fpGetLoginCtx)process_read_rel_addr(p_get_login_context);
		if (fptrGetLoginCtx) {
			uint16_t opcode = process_read_u16((u64)fptrGetLoginCtx);
			if (opcode == 0x8D48)
			{
				p_login_context = process_read_rel_addr(process_read_rel_addr(p_get_login_context) + 0x3);
				if (p_login_context == fptrGetLoginCtx())
				{
					pp_login_context_vt = process_read_u64(p_login_context);
					DBGPRINT(TEXT("p_login_context: %p"), p_login_context);
				}
			}
		}

		if (pp_login_context_vt)
		{
			fpGetAcctName fptrGetAcctName = (fpGetAcctName)process_read_u64(pp_login_context_vt + 0xB0);
			if (fptrGetAcctName) {
				uint16_t opcode = process_read_u16((u64)fptrGetAcctName);
				if (opcode == 0x8948)
				{
					WCHAR buff[40];
					fptrGetLoginCtx();
					fptrGetAcctName(fptrGetLoginCtx(), (uintptr_t)buff, sizeof(buff));
					DBGPRINT(TEXT("AccountName: %s"), buff);
				}
			}
		}
	}*/


	DBGPRINT(
		TEXT("[g_client_ctx=0x%016llX] [agent_ctx=0x%016llX] [agent_av_ctx=0x%016llX] [ui_ctx=0x%016llX]\r\n")
		TEXT("[sa_ctx=0x%016llX] [hide_ctx=0x%016llX] [map_ctx=0x%016llX]\r\n")
		TEXT("[ping_ctx=0x%016llX] [fps_ctx=0x%016llX] [wv_ctx=0x%016llX]\r\n")
		TEXT("[p_dmg_result=0x%016llX] [p_dmg_log=0x%016llX]\r\n")
		TEXT("[p_cbt_log=0x%016llX] [p_cbt_log_result=0x%016llX]\r\n")
		TEXT("[g_actionCam_ctx=0x%016llX] [g_getWmAgent=0x%016llX]\r\n")
		TEXT("[g_getContactCtx=0x%016llX] [g_getSquadCtx=0x%016llX]\r\n")
		TEXT("[g_codedTextFromHashId=0x%016llX] [g_decodeCodedText=0x%016llX]"),
		g_client_ctx, g_agent_ctx, g_agent_av_ctx, g_ui_ctx,
		g_sa_ctx, g_hide_ctx, g_map_open_ctx,
		g_ping_ctx, g_fps_ctx, g_wv_ctx,
		p_dmg_result, p_dmg_log,
		p_cbt_log, p_cbt_log_result,
		g_actionCam_ctx,
		g_getWmAgent, g_getContactCtx, g_getSquadCtx,
		g_codedTextFromHashId, g_decodeCodedText
	);


	if (p_cbt_log)
	{
		if (hookDetour(p_cbt_log, /*16,*/ hkCombatLogDetour))
		{
			DBGPRINT(TEXT("Combat log hook created [p_cbt_log=0x%016llX]"), p_cbt_log);
			g_cbCombatLog = p_cbt_log;
		}
		else
		{
			DBGPRINT(TEXT("Failed to create combat log hook [p_cbt_log=0x%016llX]"), p_cbt_log);
		}
	}

	if (p_cbt_log_result)
	{
		/*if (hookDetour(p_cbt_log_result, 18, hkCombatLogResult)) {
			DBGPRINT(TEXT("Combat log results hook created [p_cbt_log_result=0x%016llX]"), p_cbt_log_result);
		} else {
			DBGPRINT(TEXT("Failed to create combat log results hook [p_cbt_log_result=0x%016llX]"), p_cbt_log_result);
		}*/
		if (MH_CreateHook((LPVOID)p_cbt_log_result, (LPVOID)&hkCbtLogResult, (LPVOID*)&_imp_hkCbtLogResult) == MH_OK)
		{
			if (MH_EnableHook((LPVOID)p_cbt_log_result) == MH_OK)
			{
				DBGPRINT(TEXT("cbtlog result hook created [p_cbt_log_result=0x%016llX]"), p_cbt_log_result);

			}

			g_hkCbtLogResult = p_cbt_log_result;
		}
		else
		{
			DBGPRINT(TEXT("Failed to create cbtlog result hook [p_cbt_log_result=0x%016llX]"), p_cbt_log_result);
		}
	}

	if (p_dmg_log)
	{
		if (hookDetour(p_dmg_log, /*15,*/ hkDamageLog))
		{
			DBGPRINT(TEXT("Damage log hook created [p_dmg_log=0x%016llX]"), p_dmg_log);
		}
		else
		{
			DBGPRINT(TEXT("Failed to create damage log hook [p_dmg_log=0x%016llX]"), p_dmg_log);
		}
	}

	if (p_dmg_result)
	{
		if (MH_CreateHook((LPVOID)p_dmg_result, (LPVOID)&hkDamageResult, (LPVOID*)&_imp_hkDamageResult) == MH_OK)
		{
			if (MH_EnableHook((LPVOID)p_dmg_result) == MH_OK)
			{
				DBGPRINT(TEXT("Damage result hook created [p_dmg_result=0x%016llX]"), p_dmg_result);

			}

			g_hkDamageResult = p_dmg_result;
		}
		else
		{
			DBGPRINT(TEXT("Failed to create damage result hook [p_dmg_result=0x%016llX]"), p_dmg_result);
		}
	}


	if (p_pl_cli_user)
	{
		if (hookDetour(p_pl_cli_user, /*15,*/ hkPlCliUserDetour))
		{
			DBGPRINT(TEXT("Current user hook created [p_pl_cli_user=0x%016llX]"), p_pl_cli_user);
			g_hkPlCliUser = p_pl_cli_user;
		}
		else
		{
			DBGPRINT(TEXT("Failed to create current user hook [p_pl_cli_user=0x%016llX]"), p_pl_cli_user);
		}
	}


	// Alert game thread context
	// "ViewAdvanceDevice" -> dereference the ptr it's pointing too
	// -> hook VT index 0
	// alternatively we search for:
	// "!m_fileLocks.Count()"
	// "48 89 5C 24 08 57 48 83 EC 20 83 B9 14 03 00 00 00 8B FA 48 8B D9 74 19 48 8D 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 41 B8 B1 00 00 00"
	u64 p_game_thread = process_scan(
		"\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x83\xB9\x14\x03\x00\x00\x00\x8B\xFA\x48\x8B\xD9\x74\x19\x48\x8D\x15\x00\x00\x00\x00\x48\x8D\x0D",
		"xxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx");

	if (p_game_thread)
	{
		if (MH_CreateHook((LPVOID)p_game_thread, (LPVOID)&hkGameThread, (LPVOID*)&_imp_hkGameThread) == MH_OK)
		{
			if (MH_EnableHook((LPVOID)p_game_thread) == MH_OK) {
				DBGPRINT(TEXT("Game thread hook created [p_game_thread=0x%016llX]"), p_game_thread);
			}

			g_hkGameThread = p_game_thread;
		}
		else
		{
			DBGPRINT(TEXT("Failed to create game thread hook [p_game_thread=0x%016llX]"), p_game_thread);
		}
	}

	/*if (g_hWnd)
	{
		//s_oldWindProc = GetWindowLongPtr(g_hWnd, GWLP_WNDPROC);
		//DBGPRINT(TEXT("GetWindowLongPtr(%p) = %p"), g_hWnd, s_oldWindProc);
		s_oldWindProc = SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
		DBGPRINT(TEXT("WindowProc hook created [g_hWnd=%p] [s_oldWindProc=%p]"), g_hWnd, s_oldWindProc);
	}*/

	// WndProc hook
	// 40 53 55 56 41 55 41 56 41 57 48 83 EC 68 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 48 89 54 24 20 48 89 7C 24 60 8B FA
	u64 pWndProc = process_scan(
		"\x40\x53\x55\x56\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x68\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x44\x24\x48\x89\x54\x24\x20\x48\x89\x7C\x24\x60\x8B\xFA",
		"xxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxx");
	if (pWndProc)
	{
		/*if (MH_CreateHook((LPVOID)pWndProc, (LPVOID)&WndProc, (LPVOID*)&_imp_wndProc) == MH_OK)
		{
			if (MH_EnableHook((LPVOID)pWndProc) == MH_OK)
			{
				DBGPRINT(TEXT("WndProc hook created [p_wndProc=0x%016llX]"), pWndProc);
			}

			g_hkWndProc = pWndProc;
		}*/
		if (hookDetour(pWndProc, /*14,*/ hkWndProc))
		{
			DBGPRINT(TEXT("WndProc hook created [p_wndProc=0x%016llX]"), pWndProc);
			g_pWndProc = pWndProc;
		}
		else
		{
			DBGPRINT(TEXT("Failed to hook WndProc [p_wndProc=0x%016llX]"), pWndProc);
		}
	}

	g_is_init = true;
}

void dps_destroy(void)
{
	if (s_oldWindProc) {
		SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)s_oldWindProc);
		DBGPRINT(TEXT("WindowProc hook removed [g_hWnd=%p] [s_oldWindProc=%p]"), g_hWnd, s_oldWindProc);
		s_oldWindProc = 0;
	}

	if (g_hkWndProc)
	{
		MH_RemoveHook((LPVOID)g_hkWndProc);
	}

	if (g_hkGameThread)
	{
		MH_RemoveHook((LPVOID)g_hkGameThread);
	}

	if (g_hkCbtLogResult)
	{
		MH_RemoveHook((LPVOID)g_hkCbtLogResult);
	}

	if (g_hkDamageResult)
	{
		MH_RemoveHook((LPVOID)g_hkDamageResult);
	}

	if (g_hkDamageLog)
	{
		MH_RemoveHook((LPVOID)g_hkDamageLog);
	}

	if (g_hkCombatLog)
	{
		MH_RemoveHook((LPVOID)g_hkCombatLog);
	}


	if (g_state.panel_debug.enabled && g_state.dbg_buf)
	{
		free(g_state.dbg_buf);
	}

	free(g_closest_pap);
	free(g_targets);
}

void dps_handle_msg_damage(MsgServerDamage* msg)
{
	g_group_id = msg->selected_id;
	g_group_num = 0;
	memset(g_group, 0, sizeof(g_group));

	for (u32 i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
	{
		ServerPlayer* s = msg->closest + i;

		s->name[MSG_NAME_LAST] = 0;
		if (s->name[0] == 0)
		{
			continue;
		}

		DPSPlayer* p = g_group + g_group_num;
		p->in_combat = s->in_combat;
		p->time = s->time;
		p->damage = s->damage;
		p->damage_in = 0;
		p->heal_out = 0;
		p->stats = s->stats;
		p->profession = s->profession;
		_snwprintf(p->name, MSG_NAME_SIZE, L"%S", s->name);
		memcpy(p->targets, s->targets, sizeof(p->targets));

		++g_group_num;
	}
}

void dps_handle_msg_damage_v2(MsgServerDamageV2* msg)
{
	g_group_id = msg->selected_id;
	g_group_num = 0;
	memset(g_group, 0, sizeof(g_group));

	for (u32 i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
	{
		ServerPlayerV2* s = msg->closest + i;

		s->name[MSG_NAME_LAST] = 0;
		if (s->name[0] == 0)
		{
			continue;
		}

		DPSPlayer* p = g_group + g_group_num;
		p->in_combat = s->in_combat;
		p->time = s->time;
		p->damage = s->damage;
		p->damage_in = s->damage_in;
		p->heal_out = s->heal_out;
		p->stats = s->stats;
		p->profession = s->profession;
		_snwprintf(p->name, MSG_NAME_SIZE, L"%S", s->name);
		memcpy(p->targets, s->targets, sizeof(p->targets));

		++g_group_num;
	}
}

void dps_handle_msg_damageW(MsgServerDamageW* msg)
{
	g_group_id = msg->selected_id;
	g_group_num = 0;
	memset(g_group, 0, sizeof(g_group));

	for (u32 i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
	{
		ServerPlayerW* s = msg->closest + i;

		s->name[MSG_NAME_LAST] = 0;
		if (s->name[0] == 0)
		{
			continue;
		}

		DPSPlayer* p = g_group + g_group_num;
		p->in_combat = s->in_combat;
		p->time = s->time;
		p->damage = s->damage;
		p->damage_in = s->damage_in;
		p->heal_out = s->heal_out;
		p->stats = s->stats;
		p->profession = s->profession;
		wmemcpy(p->name, s->name, MSG_NAME_SIZE);
		memcpy(p->targets, s->targets, sizeof(p->targets));

		for (u32 j = 0; j < MSG_TARGETS; ++j)
		{
			// Since we hijacked the old c1dmg only value of '1' is valid
			if (p->targets[j].invuln != 1) {
				continue;
			}

			u32 id = p->targets[j].id;
			DBGPRINT(TEXT("[%s] reported target [id:%d] is invulnerable"), p->name, id);

			for (u32 x = 0; x < MAX_TARGETS; ++x)
			{
				DPSTarget* t = &g_targets[x];
				if (t->aptr == 0 || t->id == 0) {
					continue;
				}

				if (t->id == id && !t->invuln) {
					DBGPRINT(TEXT("[%d:%016I64X] is now invuln, source [%s]"), t->id, t->aptr, p->name);
					t->invuln = 1;
				}
			}

		}

		++g_group_num;
	}
}

void dps_handle_msg_damage_utf8(MsgServerDamageUTF8* msg)
{
	g_group_id = msg->selected_id;
	g_group_num = 0;
	memset(g_group, 0, sizeof(g_group));

	for (u32 i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
	{
		ServerPlayerUTF8* s = msg->closest + i;

		s->name[MSG_NAME_SIZE_UTF8-1] = 0;
		if (s->name[0] == 0)
		{
			continue;
		}

		DPSPlayer* p = g_group + g_group_num;
		p->in_combat = s->in_combat;
		p->time = s->time;
		p->damage = s->damage;
		p->damage_in = s->damage_in;
		p->heal_out = s->heal_out;
		p->stats = s->stats;
		p->profession = s->profession;

		utf8_to_utf16(s->name, p->name, sizeof(p->name));

		memcpy(p->targets, s->targets, sizeof(p->targets));

		for (u32 j = 0; j < MSG_TARGETS; ++j)
		{
			// Since we hijacked the old c1dmg only value of '1' is valid
			if (p->targets[j].invuln != 1) {
				continue;
			}

			u32 id = p->targets[j].id;
			DBGPRINT(TEXT("[%s] reported target [id:%d] is invulnerable"), p->name, id);

			for (u32 x = 0; x < MAX_TARGETS; ++x)
			{
				DPSTarget* t = &g_targets[x];
				if (t->aptr == 0 || t->id == 0) {
					continue;
				}

				if (t->id == id && !t->invuln) {
					DBGPRINT(TEXT("[%d:%016I64X] is now invuln, source [%s]"), t->id, t->aptr, p->name);
					t->invuln = 1;
				}
			}

		}

		++g_group_num;
	}
}

const GamePtrs* dps_get_game_ptrs()
{
	static GamePtrs GamePtrs = { 0 };
	if (!GamePtrs.pTlsCtx) {

		GamePtrs.pTlsCtx = (void*)g_client_ctx;
		GamePtrs.pAlertCtx = (void*)g_hkGameThread; // Not accurate
		GamePtrs.pAgentSelectionCtx = (void*)g_agent_ctx;
		GamePtrs.pAgentViewCtx = (void*)g_agent_av_ctx;
		GamePtrs.pWorldView = (void*)g_wv_ctx;
		GamePtrs.pWorldViewContext = (void*)process_read_u64(g_wv_ctx);
		GamePtrs.pShardId = (void*)&g_mum_mem->shard_id;
		GamePtrs.pMapId = (void*)&g_mum_mem->map_id;
		GamePtrs.pMapType = (void*)&g_mum_mem->map_type;
		GamePtrs.pPing = (void*)g_ping_ctx;
		GamePtrs.pFps = (void*)g_fps_ctx;
		GamePtrs.pIfHide = (void*)g_hide_ctx;
		GamePtrs.pMapOpen = (void*)g_map_open_ctx;
		GamePtrs.pUiCtx = (void*)g_ui_ctx;
		GamePtrs.pSaCtx = (void*)g_sa_ctx;
		GamePtrs.pCompass = (void*)g_comp_ctx;
		GamePtrs.pCam = (void*)&g_cam_data;
		GamePtrs.pMumble = (void*)g_mum_mem;
		GamePtrs.pActionCam = (void*)g_actionCam_ctx;

		GamePtrs.pfcGetWmAgent = (void*)g_getWmAgent;
		GamePtrs.pfcGetChatCtx = (void*)g_getChatCtx;
		GamePtrs.pfcGetContactCtx = (void*)g_getContactCtx;
		GamePtrs.pfcGetSquadCtx = (void*)g_getSquadCtx;
		GamePtrs.pfcCodedTextFromHashId = (void*)g_codedTextFromHashId;
		GamePtrs.pfcDecodeCodedText = (void*)g_decodeCodedText;
		GamePtrs.pWndProc = (void*)g_pWndProc;
		GamePtrs.pGameThread = (void*)g_hkGameThread;
		GamePtrs.cbDmgLog = (void*)g_cbDmgLog;
		GamePtrs.cbDmgLogResult = (void*)g_hkDamageResult;
		GamePtrs.cbCombatLog = (void*)g_cbCombatLog;
		GamePtrs.cbCombatLogResult = (void*)g_hkCbtLogResult;
		GamePtrs.cbPlCliUser = (void*)g_hkPlCliUser;
	}

	return &GamePtrs;

}

bool dps_is_dragging()
{
	return g_is_dragging;
}

Player* dps_get_current_player()
{
	return &g_player;
}

i64 dps_get_now()
{
	return g_time_now;
}

void dps_update(i64 now)
{
	if (g_is_init == false)
	{
		return;
	}

	if (module_toggle_bind(&g_state.bind_on_off, &g_state.global_on_off)) {
		config_set_int(CONFIG_SECTION_GLOBAL, "Enabled", g_state.global_on_off);
	}
	if (!g_state.global_on_off)
		return;

	// Update timing.
	g_time_now = now;

	// Update data from the mumble link.
	if (g_mum_mem)
	{
		g_shard_id = g_mum_mem->shard_id;
		g_map_type = g_mum_mem->map_type;
		g_map_id = g_mum_mem->map_id;
	}

	// Update UI variables.
	i32 res_x = renderer_get_res_x();
	i32 res_y = renderer_get_res_y();
	i32 fontu_x = renderer_get_fontu_x();
	i32 fontu_y = renderer_get_fontu_y();
	i32 center = res_x / 2;

	// Read the ui scale.
	i32 ui_scale = process_read_i32(g_ui_ctx + OFF_UCTX_SCALE) + 1;
	ui_scale = ui_scale > 3 ? 3 : ui_scale;
	ui_scale = ui_scale < 0 ? 0 : ui_scale;

	// Override for the chinese client
	if (g_state.is_gw2china && ui_scale > 0) ui_scale = 2;

	// Check if the UI is hidden.
	bool is_visible = (process_read_i32(g_hide_ctx) == 0) && (process_read_i32(g_map_open_ctx) == 0);

	// Set up the UI placement.
	GFX* ui = g_gfx + ui_scale;
	GFXTarget set;

	// check for user toggle and 
	// draw the compass if need be
	module_toggle_bind_bool(&g_state.panel_compass);
	if (is_visible) 
		module_compass(center, res_y - ui->hp - 120, 320);

	// Check for drag and drop
	module_drag_drop(-1);

	// Check for a sort click
	module_click(-1);

	// Get the character and player arrays.
	Array char_array, play_array;
	process_read(g_client_ctx + OFF_CCTX_CHARS, &char_array, sizeof(char_array));
	process_read(g_client_ctx + OFF_CCTX_PLAYS, &play_array, sizeof(play_array));

	// Read the player array
	if (play_array.data &&
		play_array.cur &&
		play_array.cur <= play_array.max)
	{
		u32 pan = min(play_array.cur, MAX_PLAYERS);
		process_read(play_array.data, g_closest_pap, pan * 8);
	}

	// Get the player and the current target data.
	Character player = { 0 };
	Character target = { 0 };

	u64 player_cptr = process_read_u64(g_client_ctx + OFF_CCTX_CONTROLLED_CHAR);
	u64 player_pptr = process_read_u64(g_client_ctx + OFF_CCTX_CONTROLLED_PLAYER);

	if (player_cptr == 0 || player_pptr == 0)
	{
		module_debug(&player, &target, &char_array, &play_array, now);
		return;
	}

	read_player(&player, player_cptr, player_pptr, now);
	read_target(&target, &char_array, &play_array, process_read_u64(g_agent_ctx + OFF_ACCTX_AGENT), now);
	read_closest(&player, &play_array, now);

	// Cleanup the target list.
	dps_targets_update(&player, &char_array, now);


	// Update visibility mode.
	module_toggle_bind_bool(&g_state.panel_hp);
	module_toggle_bind_bool(&g_state.panel_float);
	module_toggle_bind_i32(&g_state.panel_skills);
	module_toggle_bind_bool(&g_state.panel_buff_uptime);
	module_toggle_bind_i32(&g_state.panel_gear_self);
	module_toggle_bind_i32(&g_state.panel_gear_target);
	module_toggle_bind_i32(&g_state.panel_dps_group);
	if (!g_state.renderImgui) {
		module_toggle_bind_i32(&g_state.panel_dps_self);
	}
	else {
		module_toggle_bind_bool(&g_state.panel_dps_self);
	}

	// Update logging modules.
	module_log_hits(&target, now);

	// Setup special action monitoring.
	module_special_action(&player, now);


	if (target.type == AGENT_TYPE_ATTACK)
	{
		set = ui->attack;
	}
	else if (target.type == AGENT_TYPE_GADGET)
	{
		set = ui->gadget;
	}
	else
	{
		set = ui->normal;
	}

	if (g_map_type >= MAP_TYPE_WVW_EB && g_map_type <= MAP_TYPE_WVW_EOTM2)
	{
		static const i32 wvw_offset[4] = { 27, 30, 33, 36 };;
		set.hp.y += wvw_offset[ui_scale];
		set.bb.y += wvw_offset[ui_scale];
	}
	if ((target.type == AGENT_TYPE_GADGET || target.type == AGENT_TYPE_ATTACK) && ui_scale < 3)
		set.hp.y -= 1;

	// update default screen locations
	if (g_state.panel_dps_self.pos.x < 0)
		g_state.panel_dps_self.pos.x = center + set.dps;
	if (g_state.panel_dps_self.pos.y < 0)
		g_state.panel_dps_self.pos.y = set.hp.y - fontu_y * 5;
	if (g_state.panel_dps_group.pos.x < 0)
		g_state.panel_dps_group.pos.x = center + set.dps +(u32)(42.0f*fontu_x);
	if (g_state.panel_dps_group.pos.y < 0)
		g_state.panel_dps_group.pos.y = set.hp.y - fontu_y * 5;
	if (g_state.panel_skills.pos.x < 0)
		g_state.panel_skills.pos.x = g_state.panel_dps_group.pos.x;
	if (g_state.panel_skills.pos.y < 0)
		g_state.panel_skills.pos.y = g_state.panel_dps_group.pos.y + (u32)(fontu_y*(MSG_CLOSEST_PLAYERS + 28));
	if (g_state.panel_buff_uptime.pos.x < 0)
		g_state.panel_buff_uptime.pos.x = g_state.panel_dps_group.pos.x;
	if (g_state.panel_buff_uptime.pos.y < 0)
		g_state.panel_buff_uptime.pos.y = g_state.panel_dps_group.pos.y + (u32)(fontu_y*(MSG_CLOSEST_PLAYERS + 17));
	if (g_state.panel_gear_target.pos.x < 0)
		g_state.panel_gear_target.pos.x = renderer_get_res_x() - 400;
	if (g_state.panel_gear_target.pos.y < 0)
		g_state.panel_gear_target.pos.y = renderer_get_res_y() - 800;
	if (g_state.panel_gear_self.pos.x < 0)
		g_state.panel_gear_self.pos.x = renderer_get_res_x() - 400;
	if (g_state.panel_gear_self.pos.y < 0)
		g_state.panel_gear_self.pos.y = g_state.panel_gear_target.pos.y + (u32)(31.5f*fontu_y);


	// Draw player and target information.
	if (is_visible)
	{
		// Player HP %, target HP/breakbar & dist
		if (g_state.renderImgui) {
#if !(defined BGDM_TOS_COMPLIANT)
			ImGui_FloatBars(&g_state.panel_float, &player, &target, center, ui, &set);
#endif
			ImGui_HpAndDistance(&g_state.panel_hp, &player, &target, center, ui, &set);
		}
		else
			module_hp_dist(&g_state.panel_hp, &player, &target, center, ui, &set);

		// Update and draw DPS.
		module_dps_solo(&target, &char_array, now, g_state.panel_dps_self.pos.x, g_state.panel_dps_self.pos.y);
		module_dps_group(&player, &target, &char_array, now, g_state.panel_dps_group.pos.x, g_state.panel_dps_group.pos.y);
		module_dps_skills(&player, &target, &char_array, now, g_state.panel_skills.pos.x, g_state.panel_skills.pos.y);

		// Draw Buff uptime panel
		module_buff_uptime(&player, &char_array, now, g_state.panel_buff_uptime.pos.x, g_state.panel_buff_uptime.pos.y);

#if !(defined BGDM_TOS_COMPLIANT)
		// Update and draw inventory
		module_char_inspect(&target, &player, g_state.use_localized_text);
#endif
	}

	// Send out network data.
	module_network_dps(&player, &target, &char_array, now);

	// Draw debug information.
	module_debug(&player, &target, &char_array, &play_array, now);
}
