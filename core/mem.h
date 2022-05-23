//
//  mem.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef mem_h
#define mem_h

#include <stdint.h>
#include <stdbool.h>
#include "core/TargetOS.h"

#ifdef __APPLE__
#include <sys/types.h>
#endif


#if defined(TARGET_OS_WIN)
#include <windows.h>
// Can interpreted as pointer to the module base address.
typedef HANDLE Handle;
typedef HINSTANCE ModuleHandle;
typedef HWND WindowHandle;
#else
// Analogous to Windows this represents the module base address, not a dlopen handle!
typedef void* Handle;
typedef void* ModuleHandle;
// Must be compatible with X11 Window type.
typedef long unsigned int WindowHandle;
#endif

typedef int Protection;

static const Protection PROTECTION_INVALID = -1;
static const Protection PROTECTION_NOACCESS = 0x0;
static const Protection PROTECTION_READ = 0x1;
static const Protection PROTECTION_WRITE = 0x2;
static const Protection PROTECTION_EXECUTE = 0x4;
static const Protection PROTECTION_GUARD = 0x8; // Only supported on Windows.
static const Protection PROTECTION_READ_WRITE = PROTECTION_READ|PROTECTION_WRITE;
static const Protection PROTECTION_READ_EXECUTE = PROTECTION_READ|PROTECTION_EXECUTE;
static const Protection PROTECTION_READ_WRITE_EXECUTE = PROTECTION_READ_WRITE|PROTECTION_EXECUTE;

typedef enum MemRegionStatus
{
    // An invalid memory region. This is usually kernel address space. No fields are valid.
    Invalid,
    // A valid committed memory region. All fields are valid.
    Valid,
    // A free memory region. Only the base and size fields are valid.
    Free
} MemRegionStatus;

typedef struct MemRegion
{
    MemRegionStatus status;
    uintptr_t base;
    size_t size;
    Protection protection;
    ModuleHandle hModule;
} MemRegion;


#ifdef __cplusplus
extern "C" {
#endif

    
uintptr_t GetPageSize();

// Allocate memory on own pages. This is useful when the protection
// is adjusted to prevent other data from being affected.
// PageFree must be used to free the retrieved memory block.
void *PageAlloc(size_t n, Protection protection);
void PageFree(void *p, size_t n);
bool PageProtect(const void *p, size_t n, Protection protection);
    
#ifdef __cplusplus
}
#endif


#endif /* mem_h */
