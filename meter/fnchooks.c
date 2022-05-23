#include "core/debug.h"
#include "core/time.h"
#include "meter/game.h"
#include "meter/dps.h"
#include "meter/process.h"
#include "meter/renderer.h"
#include "meter/fnchooks.h"
#include "meter/ForeignFncs.h"
#include "meter/lru_cache.h"
#include <intrin.h>		// _AddressOfReturnAddress


hkCbtLogResult_t _imp_hkCbtLogResult = 0;
hkDamageResult_t _imp_hkDamageResult = 0;
hkDamageLog_t _imp_hkDamageLog = 0;
hkCombatLog_t _imp_hkCombatLog = 0;
hkGameThread_t _imp_hkGameThread = 0;
WndProc_t _imp_wndProc = 0;

extern __int64 __readRBX();

// Intercepts game client alerts
void __fastcall hkGameThread(uintptr_t pInst, int a2, int frame_time)
{

	static volatile LONG m_lock = 0;

	while (InterlockedCompareExchange(&m_lock, 1, 0))
	{
	}

	// Get the contact ctx
	g_chat_ctx = GetCtxChat(g_getChatCtx);
	g_squad_ctx = GetCtxSquad(g_getSquadCtx);
	g_contact_ctx = GetCtxContact(g_getContactCtx);

	// Get the targets speciesID
	dps_targets_get_species();

	// Update the camera data
	dps_update_cam_data();

	// Send missing IDs/cptr's for resolving
	// Can only be done under main thread context
	lru_resolve();

	InterlockedExchange(&m_lock, 0);

	if (_imp_hkGameThread)
	{
		_imp_hkGameThread(pInst, a2, frame_time);
	}
}

void __fastcall hkDamageLog(CpuContext *ctx)
{
#ifdef _WIN64
	int hit = (int)ctx->R9;
	uintptr_t *aptr_src = (uintptr_t*)(ctx->R8);
	uintptr_t *aptr_tgt = (uintptr_t*)(ctx->RDX);
	uintptr_t *skillDef = **(uintptr_t***)(ctx->RSP + 0x28);
#else
	int hit = *(int*)(ctx->EBP + 0x10);
	uintptr_t *pSrc = *(uintptr_t**)(ctx->EBP + 0xC);
	uintptr_t *pTgt = *(uintptr_t**)(ctx->EBP + 0x8);
	uintptr_t *skillDef = *(uintptr_t**)(ctx->EBP + 0x14);
#endif

	i32 efType = process_read_i32((u64)skillDef + OFF_SKILLDEF_EFFECT);

	const wchar_t *src = lru_find(CLASS_TYPE_AGENT, (u64)aptr_src, NULL, 0);
	const wchar_t *tgt = lru_find(CLASS_TYPE_AGENT, (u64)aptr_tgt, NULL, 0);
	const wchar_t *skill = lru_find(CLASS_TYPE_SKILL, (u64)skillDef, NULL, 0);
	UNREFERENCED_PARAMETER(hit);
	UNREFERENCED_PARAMETER(src);
	UNREFERENCED_PARAMETER(tgt);
	UNREFERENCED_PARAMETER(skill);
	UNREFERENCED_PARAMETER(efType);

	DBGPRINT(TEXT("[%s] hits [%s] using [%d:%s] for %d"), src, tgt, efType, skill, hit);

	//if (_imp_hkDamageLog)
	//	_imp_hkDamageLog(a1, aptr_tgt, aptr_src, hit, skillDef);
}

// Intercepts damage result
// We onlu yse this to detect invuln phases
i64 __fastcall hkDamageResult(i64 a1, i32 type, u32 dmg, u32 a4, void* a5, i64 a6, i64 a7, u64 target)
{
	static const i32 DMG_RESULT_BLOCK = 0;
	static const i32 DMG_RESULT_INVULN_0 = 18;		// '0' dmg type of invuln
	static const i32 DMG_RESULT_INTERRUPTED = 21;	// SELF INTERRUPTED
	static const i32 DMG_RESULT_INTERRUPT = 22;
	static const i32 DMG_RESULT_INVULN = 23;
	static const i32 DMG_RESULT_MISS = 31;
	static const i32 DMG_RESULT_OBSTRUCTED = 38;
	static const i32 DMG_RESULT_OUTOFRANGE = 40;

	u64 client_ctx = dps_get_client_context();
	Array char_array = { 0 };
	process_read(client_ctx + OFF_CCTX_CHARS, &char_array, sizeof(char_array));

	const wchar_t* name = lru_find(CLASS_TYPE_AGENT, target, NULL, 0);
	UNREFERENCED_PARAMETER(name);

	if (dmg == 0) {

		if (type == DMG_RESULT_INVULN ||
			type == DMG_RESULT_INVULN_0) {

			u32 ti = dps_targets_insert(target, 0, &char_array);

			if (ti < MAX_TARGETS)
			{
				//i64 now = dps_get_now();
				DPSTarget* t = dps_targets_get(ti);
				t->invuln = true;
				DBGPRINT(TEXT("[%s] INVULNERABLE"), name);
			}
		}
#ifdef _DEBUG
		else {
			if (type == DMG_RESULT_INTERRUPT) {
				DBGPRINT(TEXT("[%s] INTERRUPT"), name);
			}
			else if (type == DMG_RESULT_INTERRUPTED) {
				DBGPRINT(TEXT("[%s] INTERRUPTED"), name);
			}
			else if (type == DMG_RESULT_MISS) {
				DBGPRINT(TEXT("[%s] MISS"), name);
			}
			else if (type == DMG_RESULT_BLOCK) {
				DBGPRINT(TEXT("[%s] BLOCK"), name);
			}
			else {
				DBGPRINT(TEXT("[%s] type:%d  dmg:%d  a4:%d a5:%p a6:%p a7:%p"), name, type, dmg, a4, a5, a6, a7);
			}
		}
#endif
	}


	if (_imp_hkDamageResult)
		return _imp_hkDamageResult(a1, type, dmg, a4, a5, a6, a7, target);

	return 0;
}

void __fastcall hkCombatLogResult(CpuContext *ctx)
{
	uint32_t result = *(uint32_t*)(ctx->RBX + 0x2C);
	uintptr_t *aptr_tgt = *(uintptr_t**)(ctx->RBX + 0x58);

	UNREFERENCED_PARAMETER(result);
	UNREFERENCED_PARAMETER(aptr_tgt);
}

i64 __fastcall hkCbtLogResult(uintptr_t a1, uintptr_t a2)
{

	uint32_t result = process_read_u32(a1 + 0x2c);
	uintptr_t aptr_tgt = process_read_u64(a1 + 0x58);

	u64 client_ctx = dps_get_client_context();
	Array char_array = { 0 };
	process_read(client_ctx + OFF_CCTX_CHARS, &char_array, sizeof(char_array));


	if (result != CBT_RESULT_NORMAL)
		DBGPRINT(TEXT("%p %d"), aptr_tgt, result);

	if (result == CBT_RESULT_ABSORB) {
		u32 ti = dps_targets_insert((u64)aptr_tgt, 0, &char_array);

		if (ti < MAX_TARGETS)
		{
			//i64 now = dps_get_now();
			DPSTarget* t = dps_targets_get(ti);
			const wchar_t* name = lru_find(CLASS_TYPE_AGENT, t->aptr, NULL, 0);
			DBGPRINT(TEXT("[%s] INVULNERABLE"), name);
			UNREFERENCED_PARAMETER(name);
		}
	}

	if (_imp_hkCbtLogResult)
		return _imp_hkCbtLogResult(a1, a2);
	return 0;
}


void __fastcall hkCombatLogProcess(int type, int hit, u64 aptr_tgt, u64 aptr_src, u64 skillDef)
{
	static i8* const CombatLogTypeStr[] = {
		"CL_CONDI_DMG_IN",
		"CL_CRIT_DMG_IN",
		"CL_GLANCE_DMG_IN",
		"CL_HEAL_IN",
		"CL_PHYS_DMG_IN",
		"CL_UNKNOWN_5",
		"CL_UNKNOWN_6",
		"CL_CONDI_DMG_OUT",
		"CL_CRIT_DMG_OUT",
		"CL_GLANCE_DMG_OUT",
		"CL_HEAL_OUT",
		"CL_PHYS_DMG_OUT",
		"CL_UNKNOWN_12",
		"CL_UNKNOWN_13"
	};

	//DBGPRINT(TEXT("%S(%d) hit=%d [target:%p] [source:%p] [skillDef:%p]"), CombatLogTypeStr[type], type, hit, aptr_tgt, aptr_src, skillDef);

	if (aptr_tgt == 0 || aptr_src == 0 || hit == 0)
		return;

	i64 now = dps_get_now();

	Player* player = dps_get_current_player();
	u64 client_ctx = dps_get_client_context();
	Array char_array = { 0 };
	process_read(client_ctx + OFF_CCTX_CHARS, &char_array, sizeof(char_array));

	// If the player isn't in combat yet, enter combat
	if (InterlockedAnd(&player->combat.lastKnownCombatState, 1) == 0)
	{
		DBGPRINT(TEXT("UPDATE COMBAT STATE [%s]"), &player->c.name[0]);
		Character c = { 0 };
		c.aptr = aptr_src;
		c.cptr = process_read_u64(client_ctx + OFF_CCTX_CONTROLLED_CHAR);
		c.pptr = process_read_u64(client_ctx + OFF_CCTX_CONTROLLED_PLAYER);
		read_agent(&c, c.aptr, c.cptr, NULL, now);
		read_combat_data(&c, &player->combat, now);
	}

	if (type == CL_CONDI_DMG_OUT ||
		type == CL_CRIT_DMG_OUT ||
		type == CL_GLANCE_DMG_OUT ||
		type == CL_PHYS_DMG_OUT)
	{
		u32 ti = dps_targets_insert(aptr_tgt, 0, &char_array);

		if (ti < MAX_TARGETS)
		{
			DPSTarget* t = dps_targets_get(ti);
			if (t)
			{
				t->invuln = false;
				t->tdmg += hit;
				t->time_update = now;
				t->time_hit = now;

				i32 hi = t->num_hits;
				if (hi < MAX_HITS)
				{
					t->hits[hi].dmg = hit;
					t->hits[hi].time = now;
					t->hits[hi].eff_id = skillDef ? process_read_u32(skillDef + OFF_SKILLDEF_EFFECT) : 0;
					t->hits[hi].eff_hash = skillDef ? process_read_u32(skillDef + OFF_SKILLDEF_HASHID) : 0;
					++t->num_hits;
				}
			}
		}
	}


	// Get the target HP data and validate the hit makes sense
	Character t = { 0 };
	read_target(&t, &char_array, NULL, aptr_tgt, now);

	u32 eff = skillDef ? process_read_u32(skillDef + OFF_SKILLDEF_EFFECT) : 0;
	DBG_UNREFERENCED_LOCAL_VARIABLE(eff);

	/*wchar_t skillName[40] = { 0 };
	DBG_UNREFERENCED_LOCAL_VARIABLE(skillName);
	if (lru_find(CLASS_TYPE_SKILL, skillDef, skillName, ARRAYSIZE(skillName))) {
	//DBGPRINT(TEXT("%S(%d) hit=%d [target:%p] [source:%p] [skillID:%d] [skillName=%s]"),
	//	CombatLogTypeStr[type], type, hit, aptr_tgt, aptr_src, eff, skillName);
	}*/

	if (t.hp_max > 0 && hit > t.hp_max) {
		DBGPRINT(TEXT("WARN: %S(%d) hit=%d > hp_max=%d [target:%p] [source:%p] [skillID:%d]"), CombatLogTypeStr[type], type, hit, t.hp_max, aptr_tgt, aptr_src, eff);
		return;
	}

	// if the player is now in combat, add hit to totals
	if (InterlockedAnd(&player->combat.lastKnownCombatState, 1))
	{

		if (type == CL_CONDI_DMG_OUT ||
			type == CL_CRIT_DMG_OUT ||
			type == CL_GLANCE_DMG_OUT ||
			type == CL_PHYS_DMG_OUT)
		{
			InterlockedExchangeAdd(&player->combat.damageOut, hit);
		}

		if (type == CL_CONDI_DMG_IN ||
			type == CL_CRIT_DMG_IN ||
			type == CL_GLANCE_DMG_IN ||
			type == CL_PHYS_DMG_IN)
		{
			InterlockedExchangeAdd(&player->combat.damageIn, hit);
		}

		if (type == CL_HEAL_OUT)
		{
			InterlockedExchangeAdd(&player->combat.healOut, hit);
		}
	}
}


uintptr_t __fastcall hkCombatLog(uintptr_t a1, uintptr_t a2, int type)
{
	u64 rbx = __readRBX();
	u64 rsp = (u64)_AddressOfReturnAddress();

	i32 hit = *(int32_t*)rsp + 0x4c;		// process_read_i32(rsp + 0x4c);
	u64 aptr_src = *(int64_t*)rbx + 0x40;	// process_read_i64(rbx + 0x40);
	u64 aptr_tgt = *(int64_t*)rbx + 0x58;	// process_read_i64(rbx + 0x58);
	u64 skillDef = *(int64_t*)rbx + 0x48;	// process_read_i64(rbx + 0x48);

	hkCombatLogProcess(type, hit, aptr_tgt, aptr_src, skillDef);

	if (_imp_hkCombatLog)
		return _imp_hkCombatLog(a1, a2, type);
	return 0;
}

void __fastcall hkCombatLogDetour(CpuContext *ctx)
{

#ifdef _WIN64
	int type = (int)(ctx->R8);
	int hit = *(int*)(ctx->RSP + 0x4c);
	uintptr_t *aptr_src = *(uintptr_t**)(ctx->RBX + 0x40);
	uintptr_t *aptr_tgt = *(uintptr_t**)(ctx->RBX + 0x58);
	uintptr_t *skillDef = *(uintptr_t**)(ctx->RBX + 0x48);
#else
	int type = *(int*)(ctx->EBP + 0xC);
	int hit = *(int*)(ctx->EBP + 0x18);
	uintptr_t *aptr_src = *(uintptr_t**)(*(uintptr_t*)(ctx->EBP + 0x14) + 0x28);
	uintptr_t *aptr_tgt = *(uintptr_t**)(*(uintptr_t*)(ctx->EBP + 0x14) + 0x34);
	uintptr_t *skillDef = *(uintptr_t**)(*(uintptr_t*)(ctx->EBP + 0x14) + 0x2c);
#endif

	hkCombatLogProcess(type, hit, (u64)aptr_tgt, (u64)aptr_src, (u64)skillDef);
}



void __fastcall hkPlCliUserDetour(CpuContext *ctx)
{
	if (!ctx || !ctx->RCX)
		return;

	//DBGPRINT(TEXT("PlCliUser: %p  %s %s"), ctx->RCX, (u64)ctx->RCX + 0x222, (u64)ctx->RCX + 0x2B0);

	Player* player = dps_get_current_player();
	//if (player->acctName[0] == 0)
	{
		wchar_t *name = (wchar_t *)((u64)ctx->RCX + 0x21A);
		if (name && name[0]) {
			/*int size = WideCharToMultiByte(CP_ACP, 0, name, -1, NULL, 0, NULL, 0);
			size = min(size, sizeof(player->acctName));
			WideCharToMultiByte(CP_ACP, 0, name, -1, player->acctName, size, NULL, 0);*/
			size_t len = min(wcslen(name), ARRAYSIZE(player->acctName));
			wmemset(player->acctName, 0, ARRAYSIZE(player->acctName));
			wcsncpy(player->acctName, name, len);
			player->acctName[ARRAYSIZE(player->acctName) - 1] = 0;
		}
	}

	//if (player->charName[0] == 0)
	{
		wchar_t *name = (wchar_t *)((u64)ctx->RCX + 0x2A8);
		if (name && name[0]) {
			/*int size = WideCharToMultiByte(CP_ACP, 0, name, -1, NULL, 0, NULL, 0);
			size = min(size, sizeof(player->charName));
			WideCharToMultiByte(CP_ACP, 0, name, -1, player->charName, size, NULL, 0);*/
			size_t len = min(wcslen(name), ARRAYSIZE(player->charName));
			wmemset(player->charName, 0, ARRAYSIZE(player->charName));
			wcsncpy(player->charName, name, len);
			player->charName[ARRAYSIZE(player->charName) - 1] = 0;
		}
	}

	DBGPRINT(TEXT("AccountName: %s"), player->acctName);
	DBGPRINT(TEXT("CharName: %s"), player->charName);
}