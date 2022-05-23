#pragma once
#ifndef STRICT
#define STRICT
#endif
#if defined(_WIN32) || defined (_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <pthread.h>
#define WINAPI
#endif
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include "TargetOS.h"

typedef enum HookType
{
	HOOK_TYPE_VT,
	HOOK_TYPE_JMP,
	HOOK_TYPE_DETOUR
} HookType;

typedef struct CpuContext_x86
{
	uintptr_t EIP;
	uintptr_t EFLAGS;
	uintptr_t EDI;
	uintptr_t ESI;
	uintptr_t EBP;
	uintptr_t ESP;
	uintptr_t EBX;
	uintptr_t EDX;
	uintptr_t ECX;
	uintptr_t EAX;
} CpuContext_x86;

typedef struct CpuContext_x86_64
{
	uintptr_t RIP;
	uintptr_t RFLAGS;
	uintptr_t R15;
	uintptr_t R14;
	uintptr_t R13;
	uintptr_t R12;
	uintptr_t R11;
	uintptr_t R10;
	uintptr_t R9;
	uintptr_t R8;
	uintptr_t RDI;
	uintptr_t RSI;
	uintptr_t RBP;
	uintptr_t RBX;
	uintptr_t RDX;
	uintptr_t RCX;
	uintptr_t RAX;
	uintptr_t RSP;
} CpuContext_x86_64;

#if defined(_M_X64) || defined(__x86_64__)
typedef CpuContext_x86_64 CpuContext;
#else
typedef CpuContext_x86 CpuContext;
#endif

// The hook callback
typedef void(*HookCallback_t)(CpuContext *);


#define MAX_OPCODE_BUF 1024

typedef struct OpcodeBuffer
{
	unsigned int size;
	unsigned char data[MAX_OPCODE_BUF];
} OpcodeBuffer;

typedef struct IHook
{
	int type;
	int offset;
	uintptr_t location;
	uintptr_t ipBackup;
	unsigned char *originalCode;
	unsigned char *wrapperCode;
	HookCallback_t cbHook;
#if defined(_WIN32) || defined (_WIN64)
    volatile LONG mutex;
#else
    pthread_mutex_t mutex;
#endif
	bool isEnabled;
} IHook;


#ifdef __cplusplus
extern "C" {
#endif


int WINAPI hookInit(void);
int WINAPI hookFini(void);

// Hook by replacing an object instances virtual table pointer.
// This method can only target virtual functions. It should always
// be preferred if possible as it is almost impossible to detect.
// No read-only target memory is modified.
// param classInstance: The class instance to modify.
// param functionIndex: Zero based ordinal number of the targeted virtual function.
// param cbHook: The hook target location.
// param vtBackupSize: Amount of memory to use for backing up the original virtual table.
const IHook *hookVT(uintptr_t classInstance, int functionIndex, uintptr_t cbHook, int vtBackupSize);// = 1024);

// Hook by patching the target location with a jump instruction.
// Simple but has maximum flexibility. Your code has the responsibility
// to resume execution somehow. A simple return will likely crash the target.
// param location: The location to hook.
// param nextInstructionOffset: The offset from location to the next instruction after
//     the jump that will be written. Currently 5 bytes on 32-bit and 14 bytes on 64-bit.
// param jmpBack: Optional output parameter to receive the address of wrapper code that
//     executes the overwritten code and jumps back. Do a jump to this address
//     at the end of your hook to resume execution.
const IHook *hookJMP(uintptr_t location, /*int nextInstructionOffset,*/ uintptr_t cbHook, uintptr_t *jmpBack);// = NULL);

// Hook by patching the location with a jump like hookJMP, but jumps to
// wrapper code that preserves registers, calls the given hook callback and
// executes the overwritten instructions for maximum convenience.
const IHook *hookDetour(uintptr_t location, /*int nextInstructionOffset,*/ HookCallback_t cbHook);

// Hook by using memory protection and a global exception handler.
// This method is very slow.
// No memory in the target is modified at all.
const IHook *hookVEH(uintptr_t location, HookCallback_t cbHook);


#ifdef __cplusplus
}
#endif
