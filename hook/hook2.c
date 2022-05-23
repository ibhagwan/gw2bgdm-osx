#include "hook2.h"
#include "mem.h"

#if !defined(_WIN32) && !defined (_WIN64)
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#endif


#if (defined _WIN64) || (defined TARGET_OS_MAC)
static const int JMPHOOKSIZE = 14;
#else
static const int JMPHOOKSIZE = 5;
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

// Initial capacity of the HOOK_ENTRY buffer.
#define INITIAL_HOOK_CAPACITY   32

// Initial capacity of the thread IDs buffer.
#define INITIAL_THREAD_CAPACITY 128

// Special hook position values.
#define INVALID_HOOK_POS UINT_MAX
#define ALL_HOOKS_POS    UINT_MAX


#if (defined _M_X64) || defined (TARGET_OS_MAC)
#include "hook/hde/hde64.h"
typedef hde64s HDE;
#define HDE_DISASM(code, hs) hde64_disasm(code, hs)
#else
#include "hook/hde/hde32.h"
typedef hde32s HDE;
#define HDE_DISASM(code, hs) hde32_disasm(code, hs)
#endif


// Spin lock flag for EnterSpinLock()/LeaveSpinLock().
#if defined(_WIN32) || defined (_WIN64)
static volatile LONG g_isLocked = FALSE;
#else
static pthread_mutex_t g_isLocked;
#endif


// Private heap handle. If not NULL, this library is initialized.
static uintptr_t g_dwPageSize = 0;
#if defined(_WIN32) || defined (_WIN64)
static Handle g_hHeap = NULL;
#endif

// Hook entries.
struct
{
	IHook*		pItems;     // Data heap
	uint32_t    capacity;   // Size of allocated data heap, items
	uint32_t    size;       // Actual number of data items
} g_iHooks;

#if defined(_WIN32) || defined (_WIN64)
//-------------------------------------------------------------------------
static VOID EnterSpinLock(volatile LONG *mutex)
{
	SIZE_T spinCount = 0;

	// Wait until the flag is FALSE.
	while (InterlockedCompareExchange(mutex, TRUE, FALSE) != FALSE)
	{
		// No need to generate a memory barrier here, since InterlockedCompareExchange()
		// generates a full memory barrier itself.

		// Prevent the loop from being too busy.
		if (spinCount < 32)
			Sleep(0);
		else
			Sleep(1);

		spinCount++;
	}
}
//-------------------------------------------------------------------------
static VOID LeaveSpinLock(volatile LONG *mutex)
{
	// No need to generate a memory barrier here, since InterlockedExchange()
	// generates a full memory barrier itself.

	InterlockedExchange(mutex, FALSE);
}
//-------------------------------------------------------------------------
#define ENTER_SPIN_LOCK(lock) EnterSpinLock(&lock);
#define EXIT_SPIN_LOCK(lock) LeaveSpinLock(&lock);
#else
#define ENTER_SPIN_LOCK(lock) pthread_mutex_lock(&lock);
#define EXIT_SPIN_LOCK(lock) pthread_mutex_unlock(&lock);
#endif



//-------------------------------------------------------------------------
// Returns INVALID_HOOK_POS if not found.
static uint32_t FindHookEntry(uintptr_t pTarget)
{
	uint32_t i;
	for (i = 0; i < g_iHooks.size; ++i)
	{
		if (pTarget == g_iHooks.pItems[i].location)
			return i;
	}

	return INVALID_HOOK_POS;
}

//-------------------------------------------------------------------------
static IHook* AddHookEntry(uintptr_t pTarget)
{
    void *pBuffer = PageAlloc(g_dwPageSize, PROTECTION_READ_WRITE_EXECUTE);
	if (!pBuffer)
		return NULL;

	if (g_iHooks.pItems == NULL)
	{
		g_iHooks.capacity = INITIAL_HOOK_CAPACITY;
#if defined(_WIN32) || defined (_WIN64)
		g_iHooks.pItems = (IHook*)HeapAlloc(g_hHeap, 0, g_iHooks.capacity * sizeof(IHook));
#else
        g_iHooks.pItems = (IHook*)malloc(g_iHooks.capacity * sizeof(IHook));
#endif
		if (g_iHooks.pItems == NULL)
			return NULL;
	}
	else if (g_iHooks.size >= g_iHooks.capacity)
	{
#if defined(_WIN32) || defined (_WIN64)
		IHook* p = (IHook*)HeapReAlloc(g_hHeap, 0, g_iHooks.pItems, (g_iHooks.capacity * 2) * sizeof(IHook));
#else
        IHook* p = (IHook*)realloc(g_iHooks.pItems, (g_iHooks.capacity * 2) * sizeof(IHook));
#endif
		if (p == NULL)
			return NULL;

		g_iHooks.capacity *= 2;
		g_iHooks.pItems = p;
	}

	g_iHooks.pItems[g_iHooks.size].wrapperCode = pBuffer;

	return &g_iHooks.pItems[g_iHooks.size++];
}

//-------------------------------------------------------------------------
static void DeleteHookEntry(uint32_t pos)
{
	if (g_iHooks.pItems[pos].wrapperCode)
		PageFree((void*)g_iHooks.pItems[pos].wrapperCode, g_dwPageSize);

	if (pos < g_iHooks.size - 1)
		g_iHooks.pItems[pos] = g_iHooks.pItems[g_iHooks.size - 1];

	g_iHooks.size--;

	if (g_iHooks.capacity / 2 >= INITIAL_HOOK_CAPACITY && g_iHooks.capacity / 2 >= g_iHooks.size)
	{
#if defined(_WIN32) || defined (_WIN64)
		IHook* p = (IHook*)HeapReAlloc(g_hHeap, 0, g_iHooks.pItems, (g_iHooks.capacity / 2) * sizeof(IHook));
#else
        IHook* p = (IHook*)realloc(g_iHooks.pItems, (g_iHooks.capacity / 2) * sizeof(IHook));
#endif
		if (p == NULL)
			return;

		g_iHooks.capacity /= 2;
		g_iHooks.pItems = p;
	}
}

//-------------------------------------------------------------------------
int WINAPI hookEnableAllHooks(bool enable)
{
	int status = 0;
	uint32_t i, first = INVALID_HOOK_POS;

	for (i = 0; i < g_iHooks.size; ++i)
	{
		if (g_iHooks.pItems[i].isEnabled != enable)
		{
			first = i;
			break;
		}
	}

	if (first != INVALID_HOOK_POS)
	{
		//FROZEN_THREADS threads;
		//Freeze(&threads, ALL_HOOKS_POS, enable ? ACTION_ENABLE : ACTION_DISABLE);

		for (i = first; i < g_iHooks.size; ++i)
		{
			IHook* pHook = &g_iHooks.pItems[i];
			if (pHook->isEnabled && !enable)
			{
				if (pHook->type == HOOK_TYPE_JMP ||
					pHook->type == HOOK_TYPE_DETOUR)
				{
					if (PageProtect((void*)pHook->location, pHook->offset, PROTECTION_READ_WRITE_EXECUTE))
					{
						unsigned char *srcData = (pHook->type == HOOK_TYPE_DETOUR) ? pHook->originalCode : pHook->wrapperCode;
						memcpy((void*)pHook->location, srcData, pHook->offset);
						PageProtect((void*)pHook->location, pHook->offset, PROTECTION_READ_EXECUTE);

						if (pHook->wrapperCode)
							PageFree(pHook->wrapperCode, g_dwPageSize);
						memset(pHook, 0, sizeof(*pHook));
						pHook->isEnabled = false;
					}
					else
					{
#if defined(_WIN32) || defined (_WIN64)
						status = GetLastError();
#else
                        status = errno;
#endif
						break;
					}
				}
				else
				{
					// HOOK_TYPE_VT
				}
			}
		}

		//Unfreeze(&threads);
	}

	return status;
}


//-------------------------------------------------------------------------
int WINAPI hookInit(void)
{
	int status = 0;

	ENTER_SPIN_LOCK(g_isLocked);

#if defined(_WIN32) || defined (_WIN64)
	if (g_hHeap == NULL)
	{
		g_hHeap = HeapCreate(0, 0, 0);
		if (g_hHeap != NULL)
		{
			SYSTEM_INFO sys_info;
			GetSystemInfo(&sys_info);
			g_dwPageSize = sys_info.dwPageSize;
		}
		else
		{
			status = ERROR_NOT_ENOUGH_MEMORY;
		}
	}
	else
	{
		status = ERROR_ALREADY_INITIALIZED;
	}
#else
    if (g_dwPageSize == 0) {
        g_dwPageSize = GetPageSize();
    }
    else
    {
        status = EALREADY;
    }
#endif

	EXIT_SPIN_LOCK(g_isLocked);

	return status;
}

//-------------------------------------------------------------------------
int WINAPI hookFini(void)
{
	int status = 0;

	ENTER_SPIN_LOCK(g_isLocked);

#if defined(_WIN32) || defined (_WIN64)
	if (g_hHeap != NULL)
	{
		status = hookEnableAllHooks(FALSE);
		if (status == 0)
		{
			// Free the internal function buffer.

			// HeapFree is actually not required, but some tools detect a false
			// memory leak without HeapFree.

			if (g_iHooks.pItems)
				HeapFree(g_hHeap, 0, g_iHooks.pItems);
			HeapDestroy(g_hHeap);

			g_hHeap = NULL;

			g_iHooks.pItems = NULL;
			g_iHooks.capacity = 0;
			g_iHooks.size = 0;
		}
	}
	else
	{
		status = ERROR_ALREADY_INITIALIZED;
	}
#else
    if (g_dwPageSize != 0) {
        g_dwPageSize = 0;
    }
    else
    {
        status = EALREADY;
    }
#endif

	EXIT_SPIN_LOCK(g_isLocked);

	return status;
}


static void JMPHookLocker(IHook *pHook, CpuContext *ctx)
{
	ENTER_SPIN_LOCK(pHook->mutex);
	pHook->cbHook(ctx);
	EXIT_SPIN_LOCK(pHook->mutex);
}


#if (!defined _WIN64) && (!defined TARGET_OS_MAC)
static unsigned char * GenJumpOverwrite_x86(uintptr_t target, uintptr_t location, int nextInstructionOffset, OpcodeBuffer *opcodes)
{
	if (nextInstructionOffset > MAX_OPCODE_BUF)
		return 0;

	// Calculate delta.
	uintptr_t jmpFromLocToTarget = (uintptr_t)target - location - 5;

	// Generate patch. Fill with NOPs.
	for (int i = 0; i < nextInstructionOffset; ++i)
		opcodes->data[i] = 0x90;
	opcodes->data[0] = 0xe9; // JMP target
	*(uintptr_t*)&opcodes->data[1] = jmpFromLocToTarget;

	return opcodes->data;
}

static void GenWrapper_x86(IHook *pHook)
{
	unsigned char *buffer = pHook->wrapperCode;

	// Calculate delta.
	uintptr_t callFromWrapperToLocker = (uintptr_t)JMPHookLocker - (uintptr_t)buffer - 13 - 5;

	// Push context to the stack. General purpose, flags and instruction pointer.
	buffer[0] = 0x60; // PUSHAD
	buffer[1] = 0x9c; // PUSHFD
	buffer[2] = 0x68; // PUSH jumpBack
	*(uintptr_t*)&buffer[3] = pHook->location + pHook->offset;
	// Push stack pointer to stack as second argument to the locker function. Pointing to the context.
	buffer[7] = 0x54; // PUSH ESP
					  // Push pHook instance pointer as first argument to the locker function.
	buffer[8] = 0x68; // PUSH pHook
	*(uintptr_t*)&buffer[9] = (uintptr_t)pHook;
	// Call the hook callback.
	buffer[13] = 0xe8; // CALL cbHook
	*(uintptr_t*)&buffer[14] = callFromWrapperToLocker;
	// Cleanup parameters from cdecl call.
	buffer[18] = 0x58; // POP EAX
	buffer[19] = 0x58; // POP EAX
					   // Backup the instruction pointer that may have been modified by the callback.
	buffer[20] = 0x8f; // POP [ipBackup]
	buffer[21] = 0x05;
	*(uintptr_t**)&buffer[22] = &pHook->ipBackup;
	// Restore general purpose and flags registers.
	buffer[26] = 0x9d; // POPFD
	buffer[27] = 0x61; // POPAD

	buffer += 28;
	pHook->originalCode = buffer;

	// Copy originally overwritten code.
	memcpy(buffer, (void*)pHook->location, pHook->offset);

	buffer += pHook->offset;

	// Jump to the backed up instruction pointer.
	buffer[0] = 0xff; // JMP [ipBackup]
	buffer[1] = 0x25;
	*(uintptr_t**)&buffer[2] = &pHook->ipBackup;
}

#else

static unsigned char * GenJumpOverwrite_x86_64(uintptr_t target, int nextInstructionOffset, OpcodeBuffer *opcodes)
{
	if (nextInstructionOffset > MAX_OPCODE_BUF)
		return 0;

	uint32_t target_lo = (uint32_t)target;
	uint32_t target_hi = (uint32_t)(target >> 32);

	// Generate patch. Fill with NOPs.
	for (int i = 0; i < nextInstructionOffset; ++i)
		opcodes->data[i] = 0x90;

	opcodes->data[0] = 0x68; // PUSH cbHook@lo
	*(uint32_t*)&opcodes->data[1] = target_lo;
	opcodes->data[5] = 0xc7; // MOV [RSP+4], cbHook@hi
	opcodes->data[6] = 0x44;
	opcodes->data[7] = 0x24;
	opcodes->data[8] = 0x04;
	*(uint32_t*)&opcodes->data[9] = target_hi;
	opcodes->data[13] = 0xc3; // RETN

	return opcodes->data;
}

static void GenWrapper_x86_64(IHook *pHook)
{
	uintptr_t returnAdr = pHook->location + pHook->offset;
	uint32_t return_lo = (uint32_t)returnAdr;
	uint32_t return_hi = (uint32_t)(returnAdr >> 32);

	unsigned char *buffer = pHook->wrapperCode;

	// TODO: Respect red zone for System V ABI! 128 bytes above rsp may not be clobbered!
	// I think this must be done in the overwrite code
	// or use this weird trick for jumping: jmp [rip+0]; dq adr

	// Push context to the stack. General purpose, flags and instruction pointer.
	buffer[0] = 0x54; // PUSH RSP
	buffer[1] = 0x50; // PUSH RAX
	buffer[2] = 0x51; // PUSH RCX
	buffer[3] = 0x52; // PUSH RDX
	buffer[4] = 0x53; // PUSH RBX
	buffer[5] = 0x55; // PUSH RBP
	buffer[6] = 0x56; // PUSH RSI
	buffer[7] = 0x57; // PUSH RDI
	buffer[8] = 0x41; // PUSH R8
	buffer[9] = 0x50;
	buffer[10] = 0x41; // PUSH R9
	buffer[11] = 0x51;
	buffer[12] = 0x41; // PUSH R10
	buffer[13] = 0x52;
	buffer[14] = 0x41; // PUSH R11
	buffer[15] = 0x53;
	buffer[16] = 0x41; // PUSH R12
	buffer[17] = 0x54;
	buffer[18] = 0x41; // PUSH R13
	buffer[19] = 0x55;
	buffer[20] = 0x41; // PUSH R14
	buffer[21] = 0x56;
	buffer[22] = 0x41; // PUSH R15
	buffer[23] = 0x57;
	buffer[24] = 0x9c; // PUSHFQ
	buffer[25] = 0x68; // PUSH returnAdr@lo
	*(uint32_t*)&buffer[26] = return_lo;
	buffer[30] = 0xc7; // MOV [RSP+4], returnAdr@hi
	buffer[31] = 0x44;
	buffer[32] = 0x24;
	buffer[33] = 0x04;
	*(uint32_t*)&buffer[34] = return_hi;

	buffer += 38;

	// Backup RSP to RBX and align it on 16 byte boundary.
	buffer[0] = 0x48; // MOV RBX, RSP
	buffer[1] = 0x89;
	buffer[2] = 0xe3;
	buffer[3] = 0x48; // AND RSP, 0xffffffff_fffffff0
	buffer[4] = 0x83;
	buffer[5] = 0xe4;
	buffer[6] = 0xf0;

	buffer += 7;

	// Call the locker function.
#if defined(_WIN64) // Microsoft x64 calling convention
	// Second parameter: CpuContext*
	buffer[0] = 0x48; // MOV RDX, RBX
	buffer[1] = 0x89;
	buffer[2] = 0xda;
	// First parameter: DetourHook*
	buffer[3] = 0x48; // MOV RCX, pHook
	buffer[4] = 0xb9;
	*(uintptr_t*)&buffer[5] = (uintptr_t)pHook;
	// Shadow space for callee.
	buffer[13] = 0x48; // SUB RSP, 0x20
	buffer[14] = 0x83;
	buffer[15] = 0xec;
	buffer[16] = 0x20;
	buffer[17] = 0x48; // MOV RAX, JMPHookLocker
	buffer[18] = 0xb8;
	*(uintptr_t*)&buffer[19] = (uintptr_t)JMPHookLocker;
	buffer[27] = 0xff; // CALL RAX
	buffer[28] = 0xd0;

	buffer += 29;
#elif defined(unix) || defined(__unix__) || defined(__unix) || defined (__APPLE__) // System V AMD64 ABI
	// Second parameter: CpuContext*
	buffer[0] = 0x48; // MOV RSI, RBP
	buffer[1] = 0x89;
	buffer[2] = 0xde;
	// First parameter: DetourHook*
	buffer[3] = 0x48; // MOV RDI, pHook
	buffer[4] = 0xbf;
	*(uintptr_t*)&buffer[5] = (uintptr_t)pHook;
	buffer[13] = 0x48; // MOV RAX, JMPHookLocker
	buffer[14] = 0xb8;
	*(uintptr_t*)&buffer[15] = (uintptr_t)JMPHookLocker;
	buffer[23] = 0xff; // CALL RAX
	buffer[24] = 0xd0;

	buffer += 25;
#endif

	// Restore RSP.
	buffer[0] = 0x48; // MOV RSP, RBX
	buffer[1] = 0x89;
	buffer[2] = 0xdc;

	// Backup the instruction pointer that may have been modified by the callback.
	buffer[3] = 0x58; // POP EAX
	buffer[4] = 0x48; // MOV [ipBackup], RAX
	buffer[5] = 0xa3;
	*(uintptr_t**)&buffer[6] = &pHook->ipBackup;

	buffer += 14;

	// Restore general purpose and flags registers.
	buffer[0] = 0x9d; // POPFQ
	buffer[1] = 0x41; // POP R15
	buffer[2] = 0x5f;
	buffer[3] = 0x41; // POP R14
	buffer[4] = 0x5e;
	buffer[5] = 0x41; // POP R13
	buffer[6] = 0x5d;
	buffer[7] = 0x41; // POP R12
	buffer[8] = 0x5c;
	buffer[9] = 0x41; // POP R11
	buffer[10] = 0x5b;
	buffer[11] = 0x41; // POP R10
	buffer[12] = 0x5a;
	buffer[13] = 0x41; // POP R9
	buffer[14] = 0x59;
	buffer[15] = 0x41; // POP R8
	buffer[16] = 0x58;
	buffer[17] = 0x5f; // POP RDI
	buffer[18] = 0x5e; // POP RSI
	buffer[19] = 0x5d; // POP RBP
	buffer[20] = 0x5b; // POP RBX
	buffer[21] = 0x5a; // POP RDX
	buffer[22] = 0x59; // POP RCX
	buffer[23] = 0x58; // POP RAX
	buffer[24] = 0x5c; // POP RSP

	buffer += 25;

	pHook->originalCode = buffer;
	// Copy originally overwritten code.
	memcpy(buffer, (void*)pHook->location, pHook->offset);

	buffer += pHook->offset;

	// Jump to the backed up instruction pointer.
	buffer[0] = 0x50; // PUSH RAX
	buffer[1] = 0x48; // MOV RAX, [ipBackup]
	buffer[2] = 0xa1;
	*(uintptr_t**)&buffer[3] = &pHook->ipBackup;
	buffer[11] = 0x48; // XCHG [RSP], RAX
	buffer[12] = 0x87;
	buffer[13] = 0x04;
	buffer[14] = 0x24;
	buffer[15] = 0xc3; // RETN
}
#endif


const IHook *hookVT(uintptr_t classInstance, int functionIndex, uintptr_t cbHook, int vtBackupSize)
{
	// Check for invalid parameters.
	if (!classInstance || functionIndex < 0 || functionIndex >= vtBackupSize || !cbHook)
		return NULL;

	IHook* pHook = AddHookEntry(0);
	if (pHook != NULL)
	{
		pHook->type = HOOK_TYPE_VT;
		pHook->cbHook = (HookCallback_t)cbHook;
	}

	return pHook;
}


const IHook *hookJMP(uintptr_t location, /*int nextInstructionOffset,*/ uintptr_t cbHook, uintptr_t *jmpBack)
{
	// Check for invalid parameters.
	if (!location || /*nextInstructionOffset < JMPHOOKSIZE ||*/ !cbHook)
		return NULL;

	uint32_t copySize = 0;
	while (copySize < (uint32_t)JMPHOOKSIZE) {
		HDE hs;
		copySize += HDE_DISASM((void*)(location + copySize), &hs);
		if (hs.flags & F_ERROR)
			return NULL;
	}
	int nextInstructionOffset = copySize;

	IHook* pHook = AddHookEntry(location);
	if (!pHook)
		return pHook;

	pHook->type = HOOK_TYPE_JMP;
	pHook->location = location;
	pHook->offset = nextInstructionOffset;
	pHook->cbHook = (HookCallback_t)cbHook;
	memcpy(pHook->wrapperCode, (void*)location, pHook->offset);

	OpcodeBuffer jmpPatch;
	OpcodeBuffer jmpBackPatch;

	// The jump back must only be written if used.
	if (jmpBack)
	{
#if (defined _WIN64) || (defined TARGET_OS_MAC)
		GenJumpOverwrite_x86_64(location + nextInstructionOffset, JMPHOOKSIZE, &jmpBackPatch);
#else
		GenJumpOverwrite_x86(location + nextInstructionOffset, (uintptr_t)pHook->wrapperCode.data + nextInstructionOffset, JMPHOOKSIZE);
#endif
		// It is save to write out of bounds here, because we allocated a whole page.
		memcpy(pHook->wrapperCode + nextInstructionOffset, jmpBackPatch.data, JMPHOOKSIZE);
		*jmpBack = (uintptr_t)pHook->wrapperCode;
	}

#if (defined _WIN64) || (defined TARGET_OS_MAC)
	GenJumpOverwrite_x86_64(cbHook, nextInstructionOffset, &jmpPatch);
#else
	GenJumpOverwrite_x86(cbHook, location, nextInstructionOffset, &jmpPatch);
#endif

	// Apply the hook by writing the jump.
	if (PageProtect((void*)location, nextInstructionOffset, PROTECTION_READ_WRITE_EXECUTE))
	{
		memcpy((void*)location, jmpPatch.data, nextInstructionOffset);
		PageProtect((void*)location, nextInstructionOffset, PROTECTION_READ_EXECUTE);
		pHook->isEnabled = true;
		return pHook;
	}

	uint32_t pos = FindHookEntry(pHook->location);
	DeleteHookEntry(pos);
	return NULL;
}


const IHook *hookDetour(uintptr_t location, /*int nextInstructionOffset,*/ HookCallback_t cbHook)
{
	// Check for invalid parameters.
	if (!location || /*nextInstructionOffset < JMPHOOKSIZE ||*/ !cbHook)
		return NULL;

	uint32_t copySize = 0;
	while (copySize < (uint32_t)JMPHOOKSIZE) {
		HDE hs;
		copySize += HDE_DISASM((void*)(location + copySize), &hs);
		if (hs.flags & F_ERROR)
			return NULL;
	}
	int nextInstructionOffset = copySize;

	IHook* pHook = AddHookEntry(location);
	if (!pHook)
		return pHook;

	pHook->type = HOOK_TYPE_DETOUR;
	pHook->location = location;
	pHook->offset = nextInstructionOffset;
	pHook->cbHook = (HookCallback_t)cbHook;

	OpcodeBuffer jmpPatch;

#if (defined _WIN64) || (defined TARGET_OS_MAC)
	GenWrapper_x86_64(pHook);
	GenJumpOverwrite_x86_64((uintptr_t)pHook->wrapperCode, nextInstructionOffset, &jmpPatch);
#else
	GenWrapper_x86(pHook);
	GenJumpOverwrite_x86((uintptr_t)pHook->wrapperCode, location, nextInstructionOffset, &jmpPatch);
#endif

	// Apply the hook by writing the jump.
	if (PageProtect((void*)location, nextInstructionOffset, PROTECTION_READ_WRITE_EXECUTE))
	{
		memcpy((void*)location, jmpPatch.data, nextInstructionOffset);
		PageProtect((void*)location, nextInstructionOffset, PROTECTION_READ_EXECUTE);
		pHook->isEnabled = true;
		return pHook;
	}

	uint32_t pos = FindHookEntry(pHook->location);
	DeleteHookEntry(pos);
	return NULL;
}
