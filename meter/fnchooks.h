#pragma once
#include <stdint.h>
#include "core/types.h"
#include "hook/hook2.h"

// Hooked functions.
typedef i64(__fastcall *hkCbtLogResult_t)(u64 a1, u64 a2);
typedef i64(__fastcall *hkDamageResult_t)(i64 a1, i32 a2, u32 a3, u32 a4, void* a5, i64 a6, i64 a7, u64 a8);
typedef void(__fastcall *hkGameThread_t)(uintptr_t, int, int);
typedef void(__fastcall *hkDamageLog_t)(uintptr_t, uintptr_t, uintptr_t, int, uintptr_t);
typedef uintptr_t(__fastcall *hkCombatLog_t)(uintptr_t, uintptr_t, int);
typedef LRESULT(CALLBACK *WndProc_t)(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
	);

extern i64 hkCbtLogResult(uintptr_t a1, uintptr_t a2);
extern i64  hkDamageResult(i64 a1, i32 type, u32 dmg, u32 a4, void* a5, i64 a6, i64 a7, u64 target);
extern void hkGameThread(uintptr_t pInst, int a2, int frame_time);
extern uintptr_t hkCombatLog(uintptr_t a1, uintptr_t a2, int type);
extern void hkCombatLogDetour(CpuContext *ctx);
extern void hkCombatLogResult(CpuContext *ctx);
extern void hkPlCliUserDetour(CpuContext *ctx);
extern void hkDamageLog(CpuContext *ctx);
//extern void hkDamageLog(uintptr_t a1, uintptr_t aptr_tgt, uintptr_t aptr_src, int hit, uintptr_t skillDef);

extern hkCbtLogResult_t _imp_hkCbtLogResult;
extern hkDamageResult_t _imp_hkDamageResult;
extern hkDamageLog_t _imp_hkDamageLog;
extern hkCombatLog_t _imp_hkCombatLog;
extern hkGameThread_t _imp_hkGameThread;
extern WndProc_t _imp_wndProc;