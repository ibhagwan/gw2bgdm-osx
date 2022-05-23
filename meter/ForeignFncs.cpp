#include "meter/offsets.h"
#include "meter/enums.h"
#include "meter/game.h"
#include "meter/game_data.h"
#include "meter/lru_cache.h"
#include "meter/ForeignClass.h"
#include "meter/ForeignFncs.h"
#include "meter/logging.h"
#include "offsets/offset_scan.h"

#define GLM_FORCE_NO_CTOR_INIT
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <string.h>
#include <cstdint>
#include <algorithm>
#include <type_traits> // std::conditional


#ifdef __cplusplus
extern "C" {
#endif

#include "meter/process.h"
#include "meter/dps.h"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "core/debug.h"

#ifdef TARGET_OS_WIN
#pragma warning( push )
#pragma warning( disable: 4189 )
    
DWORD ExceptHandler(const char *msg, DWORD code, EXCEPTION_POINTERS *ep, const char *file, const char *func, int line) {
	EXCEPTION_RECORD *er = ep->ExceptionRecord;
	CONTEXT *ctx = ep->ContextRecord;
	LPCTSTR fmt_dbg = TEXT("%S: 0x%08X - addr: 0x%p");
	LPCTSTR fmt_rel = TEXT("%S");

#if (defined _DEBUG) || (defined DEBUG)
	DBGPRINT(fmt_dbg, msg, code, er->ExceptionAddress);
#else
	DBGPRINT(fmt_rel, msg);
#endif

	return EXCEPTION_EXECUTE_HANDLER;
}

#pragma warning( pop )
#else
    
#define __fastcall
#define __thiscall
    
#include "osx.bund/entry_osx.h"
#include "osx.bund/imgui_osx.h"
#include "osx.common/Logging.h"
    
static void except_handler(const char *exception)
{
    LogCrit("Unhandled exception in %s", exception);
}
    
#endif

bool Ch_ValidatePtr(uintptr_t cptr, struct Array* ca)
{
	__TRY

		hl::ForeignClass c = (void*)cptr;
		if (!c || !ca)
			return false;

		hl::ForeignClass a = (void*)c.get<void*>(OFF_CHAR_AGENT);
		if (!a)
			return false;

		uint32_t id = a.get<uint32_t>(OFF_AGENT_ID);
		if (id > ca->max)
			return false;

		uint64_t parr = process_read_u64(ca->data + id * 8);
		if (cptr == parr) {
			//DBGPRINT(TEXT("<cptr %p> <id=%d> is valid"), cptr, id);
			return true;
		}

		DBGPRINT(TEXT("<aptr %p> <cptr %p> <id=%d> no longer valid"), a.data(), cptr, id);
	
	__EXCEPT("[Ch_ValidatePtr] access violation")
		;
	__FINALLY
    
	return false;
}

static bool memRead(uintptr_t offset, void* data, size_t size)
{
#if TARGET_OS_WIN
	return ReadProcessMemory(GetCurrentProcess(), (void*)offset, data, size, NULL) == TRUE;
#else
    __TRY
        memcpy(data, (void*)offset, size);
    __EXCEPT("[memRead] access violation")
        ;
    __FINALLY
    return true;
#endif
}

static bool memWrite(uintptr_t offset, void* data, size_t size)
{
#if TARGET_OS_WIN
	// Apply the hook by writing the jump.
	DWORD  oldProtect;
	if (VirtualProtect((LPVOID)offset, size, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		memcpy((void*)offset, data, size);
		VirtualProtect((LPVOID)offset, size, oldProtect, &oldProtect);
		return true;
	}
	return false;
#else
    __TRY
        memcpy((void*)offset, data, size);
    __EXCEPT("[memWrite] access violation")
        ;
    __FINALLY
    return true;
#endif
}

static bool memPatch(uintptr_t ptr, intptr_t offset, const uint8_t *orig, const uint8_t *patch, size_t size, int mode)
{
	static uint8_t buff[128];
	if (ptr == 0 || ptr == (uintptr_t)(-1)) return false;
	if (!orig || !patch || size == 0) return false;
	if (size > sizeof(buff)) size = sizeof(buff);

	__TRY

		ptr += offset;
		memset(buff, 0, size);
		memRead(ptr, buff, size);

		// 3 - IsEnabled
		if (mode == 3) {
			if (memcmp(buff, patch, size) == 0) return true;
			else return false;
		}

		if (memcmp(buff, orig, size) == 0 ||
			memcmp(buff, patch, size) == 0) {

			// 2 - IsPtrValid check
			if (mode == 2) return true;

			// 1 - Patch
			// 0 - Restore
			void* data = mode == 1 ? (void*)patch : (void*)orig;
			if (memWrite(ptr, data, size)) return true;
		}

	__EXCEPT("[memPatch] access violation")
		;
	__FINALLY
    
	return false;
}

static bool memScan(const char *sig, const char *mask, uintptr_t *pptr)
{
	if (pptr == 0 || *pptr == (uintptr_t)(-1)) return false;

	__TRY

		if (*pptr == 0) {

			*pptr = process_scan(sig, mask);
			if (!*pptr) *pptr = (uintptr_t)(-1);
			DBGPRINT(TEXT("[memScan('%s')=%p]"), mask, *pptr);
		}
    
	__EXCEPT("[memScan] access violation")
		;
	__FINALLY

	if (*pptr && *pptr != (uintptr_t)(-1))
		return true;
	return false;
}

uintptr_t TR_GetHPBarPtr()
{
	static uintptr_t ptr = 0;

	if (ptr && ptr != (uintptr_t)-1) return ptr;

	// Add HP to nameplates always (as if hovering) - players
	// 32 Bit:
	/*==========================================================================================
	disabled: 00868D9D | 8B 7D F0                 | mov edi,dword ptr ss:[ebp-10]
	enabled:  00868D9D | 8B 7D EF                 | mov edi,dword ptr ss:[ebp-11]
	============================================================================================
	00868D99 | EB 02                    | jmp gw2.868D9D                                       |
	00868D9B | 33 F6                    | xor esi,esi                                          |
	00868D9D | 8B 7D F0                 | mov edi,dword ptr ss:[ebp-10]                        |
	00868DA0 | 85 FF                    | test edi,edi                                         |
	00868DA2 | 75 32                    | jne gw2.868DD6                                       |
	00868DA4 | 39 7D F8                 | cmp dword ptr ss:[ebp-8],edi                         |
	00868DA7 | 74 10                    | je gw2.868DB9                                        |
	00868DA9 | E8 F2 B1 FE FF           | call gw2.853FA0                                      |
	00868DAE | 8B C8                    | mov ecx,eax                                          |
	00868DB0 | 8B 10                    | mov edx,dword ptr ds:[eax]                           |
	00868DB2 | FF 52 48                 | call dword ptr ds:[edx+48]                           |
	00868DB5 | 85 C0                    | test eax,eax                                         |
	00868DB7 | 75 1D                    | jne gw2.868DD6                                       |
	00868DB9 | 85 F6                    | test esi,esi                                         |
	00868DBB | 75 19                    | jne gw2.868DD6                                       |
	00868DBD | 8D 4B 54                 | lea ecx,dword ptr ds:[ebx+54]                        |
	00868DC0 | C7 01 CD CC 4C 3D        | mov dword ptr ds:[ecx],3D4CCCCD                      |
	00868DC6 | 51                       | push ecx                                             |
	00868DC7 | 89 34 24                 | mov dword ptr ss:[esp],esi                           |
	00868DCA | E8 71 41 F3 FF           | call gw2.79CF40                                      |
	============================================================================================
	// 64 Bit: "333?" the hit with the JE right above it (10th hit), scroll up
	// "C7 01 CD CC 4C 3D E9" (-0x2A)
	======================================================================================================
	disabled: 00007FF6B042E90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]
	disabled: 00007FF6B042E912 | 45 85 FF                            | test r15d,r15d
	disabled: 00007FF6B042E915 | 75 33                               | jne gw2-64.7FF6B042E94A
	enabled:  00007FF6B042E90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]
	enabled:  00007FF6B042E912 | 45 85 FF                            | test r15d,r15d
	enabled:  00007FF6B042E915 | 75 33                               | jne gw2-64.7FF6B042E94A
	=======================================================================================================
	00007FF6B042E8CD | FF 50 78                            | call qword ptr ds:[rax+78]                   |
	00007FF6B042E8D0 | 49 8B 16                            | mov rdx,qword ptr ds:[r14]                   |
	00007FF6B042E8D3 | 49 8B CE                            | mov rcx,r14                                  |
	00007FF6B042E8D6 | 48 8B D8                            | mov rbx,rax                                  |
	00007FF6B042E8D9 | FF 52 78                            | call qword ptr ds:[rdx+78]                   |
	00007FF6B042E8DC | 48 8B 13                            | mov rdx,qword ptr ds:[rbx]                   |
	00007FF6B042E8DF | 48 8B CB                            | mov rcx,rbx                                  |
	00007FF6B042E8E2 | 48 8B F8                            | mov rdi,rax                                  |
	00007FF6B042E8E5 | FF 52 10                            | call qword ptr ds:[rdx+10]                   |
	00007FF6B042E8E8 | 48 8B 17                            | mov rdx,qword ptr ds:[rdi]                   |
	00007FF6B042E8EB | 48 8B CF                            | mov rcx,rdi                                  |
	00007FF6B042E8EE | 0F 28 F0                            | movaps xmm6,xmm0                             |
	00007FF6B042E8F1 | FF 52 18                            | call qword ptr ds:[rdx+18]                   |
	00007FF6B042E8F4 | 0F 2F C6                            | comiss xmm0,xmm6                             |
	00007FF6B042E8F7 | 76 10                               | jbe gw2-64.7FF6B042E909                      |
	00007FF6B042E8F9 | 48 8B 8E 80 00 00 00                | mov rcx,qword ptr ds:[rsi+80]                |
	00007FF6B042E900 | E8 CB CE FF FF                      | call gw2-64.7FF6B042B7D0                     |
	00007FF6B042E905 | 85 C0                               | test eax,eax                                 |
	00007FF6B042E907 | 75 02                               | jne gw2-64.7FF6B042E90B                      |
	00007FF6B042E909 | 33 ED                               | xor ebp,ebp                                  |
	00007FF6B042E90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]                |
	00007FF6B042E912 | 45 85 FF                            | test r15d,r15d                               |
	00007FF6B042E915 | 75 33                               | jne gw2-64.7FF6B042E94A                      |
	...
	00007FF7DE1CE912 | 45 85 FF                            | test r15d,r15d                               |
	00007FF7DE1CE915 | 75 33                               | jne gw2-64.7FF7DE1CE94A                      |
	00007FF7DE1CE917 | 85 DB                               | test ebx,ebx                                 |
	00007FF7DE1CE919 | 74 15                               | je gw2-64.7FF7DE1CE930                       | <<
	00007FF7DE1CE91B | E8 F0 7A FE FF                      | call gw2-64.7FF7DE1B6410                     |
	00007FF7DE1CE920 | 48 8B C8                            | mov rcx,rax                                  |
	00007FF7DE1CE923 | 48 8B 10                            | mov rdx,qword ptr ds:[rax]                   |
	00007FF7DE1CE926 | FF 92 90 00 00 00                   | call qword ptr ds:[rdx+90]                   |
	00007FF7DE1CE92C | 85 C0                               | test eax,eax                                 |
	00007FF7DE1CE92E | 75 1A                               | jne gw2-64.7FF7DE1CE94A                      |
	00007FF7DE1CE930 | 85 ED                               | test ebp,ebp                                 |
	00007FF7DE1CE932 | 75 16                               | jne gw2-64.7FF7DE1CE94A                      |
	00007FF7DE1CE934 | 48 8D 8E A0 00 00 00                | lea rcx,qword ptr ds:[rsi+A0]                |
	00007FF7DE1CE93B | 41 0F 28 C8                         | movaps xmm1,xmm8                             |
	00007FF7DE1CE93F | C7 01 CD CC 4C 3D                   | mov dword ptr ds:[rcx],3D4CCCCD              |
	00007FF7DE1CE945 | E9 DF 00 00 00                      | jmp gw2-64.7FF7DE1CEA29                      |
	=====================================================================================================*/
	memScan(
		"\xC7\x01\xCD\xCC\x4C\x3D\xE9",
		"xxxxxxx",
		&ptr
	);
	return ptr;
}

bool TR_SetHPChars(int mode)
{
	/*====================================================================================================
	disabled: 00007FF7DE1CE90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]
	disabled: 00007FF7DE1CE912 | 45 85 FF                            | test r15d,r15d
	disabled: 00007FF7DE1CE915 | 75 33                               | jne gw2-64.7FF6B042E94A
	enabled:  00007FF7DE1CE90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]
	enabled:  00007FF7DE1CE912 | 45 85 FF                            | test r15d,r15d
	enabled:  00007FF7DE1CE915 | 75 33                               | jne gw2-64.7FF6B042E94A
	=======================================================================================================
	00007FF7DE1CE905 | 85 C0                               | test eax,eax                                 |
	00007FF7DE1CE907 | 75 02                               | jne gw2-64.7FF7DE1CE90B                      |
	00007FF7DE1CE909 | 33 ED                               | xor ebp,ebp                                  |
	00007FF7DE1CE90B | 8B 9C 24 E0 00 00 00                | mov ebx,dword ptr ss:[rsp+E0]                |
	00007FF7DE1CE912 | 45 85 FF                            | test r15d,r15d                               |
	00007FF7DE1CE915 | 75 33                               | jne gw2-64.7FF7DE1CE94A                      |
	00007FF7DE1CE917 | 85 DB                               | test ebx,ebx                                 |
	00007FF7DE1CE919 | 74 15                               | je gw2-64.7FF7DE1CE930                       |
	00007FF7DE1CE91B | E8 F0 7A FE FF                      | call gw2-64.7FF7DE1B6410                     |
	=====================================================================================================*/
	static const intptr_t offset = -0x31;
	static const uint8_t orig[8] = { 0xE0 ,0x00 ,0x00 ,0x00 ,0x45 ,0x85 ,0xFF ,0x75 };
	static const uint8_t patch[8] = { 0x00 ,0x00 ,0x00 ,0x00 ,0x45 ,0x85 ,0xFF ,0xEB };
	return memPatch(TR_GetHPBarPtr(), offset, orig, patch, sizeof(orig), mode);
}

bool TR_SetHPCharsBright(int mode)
{
	/*====================================================================================================
	disabled: 00007FF7DE1CE9BF | 85 C0                               | test eax,eax
	enabled:  00007FF7DE1CE9BF | 85 C8                               | test eax,ecx
	=======================================================================================================
	00007FF7DE1CE9BB | 41 FF 50 70                         | call qword ptr ds:[r8+70]                    |
	00007FF7DE1CE9BF | 85 C0                               | test eax,eax                                 |
	00007FF7DE1CE9C1 | 74 14                               | je gw2-64.7FF7DE1CE9D7                       |
	00007FF7DE1CE9C3 | F3 0F 59 35 9D A2 B2 00             | mulss xmm6,dword ptr ds:[7FF7DECF8C68]       | 7FF7DECF8C68:"333?"
	00007FF7DE1CE9CB | 85 ED                               | test ebp,ebp                                 |
	00007FF7DE1CE9CD | 74 08                               | je gw2-64.7FF7DE1CE9D7                       |
	00007FF7DE1CE9CF | F3 0F 59 35 BD D7 AC 00             | mulss xmm6,dword ptr ds:[7FF7DEC9C194]       |
	=====================================================================================================*/
	static const intptr_t offset = 0x80;
	static const uint8_t orig[3] = { 0x85 ,0xC0 ,0x74 };
	static const uint8_t patch[3] = { 0x85 ,0xC8 ,0x74 };
	return memPatch(TR_GetHPBarPtr(), offset, orig, patch, sizeof(orig), mode);
}

#ifdef __cplusplus
}
#endif
