//
//  mem.c
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include "core/mem.h"
#include <Windows.h>


static DWORD ToWindowsProt(Protection protection)
{
    DWORD windowsProt = 0;
    
    if (protection & PROTECTION_GUARD)
    {
        protection &= ~PROTECTION_GUARD;
        windowsProt |= PAGE_GUARD;
    }
    
    switch (protection)
    {
        case PROTECTION_NOACCESS:
            windowsProt |= PAGE_NOACCESS;
            break;
        case PROTECTION_READ:
            windowsProt |= PAGE_READONLY;
            break;
        case PROTECTION_READ_WRITE:
            windowsProt |= PAGE_READWRITE;
            break;
        case PROTECTION_READ_EXECUTE:
            windowsProt |= PAGE_EXECUTE_READ;
            break;
        case PROTECTION_READ_WRITE_EXECUTE:
            windowsProt |= PAGE_EXECUTE_READWRITE;
            break;
        default:
            windowsProt |= PROTECTION_INVALID;
    }
    
    return windowsProt;
}

static Protection FromWindowsProt(DWORD windowsProt)
{
    Protection protection = PROTECTION_NOACCESS;
    
    if (windowsProt & PAGE_GUARD)
    {
        protection |= PROTECTION_GUARD;
    }
    
    // Strip modifiers.
    windowsProt &= ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    
    switch (windowsProt)
    {
        case PAGE_NOACCESS:
            break;
        case PAGE_READONLY:
            protection |= PROTECTION_READ;
            break;
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
            protection |= PROTECTION_READ_WRITE;
            break;
        case PAGE_EXECUTE:
            protection |= PROTECTION_EXECUTE;
            break;
        case PAGE_EXECUTE_READ:
            protection |= PROTECTION_READ_EXECUTE;
            break;
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            protection |= PROTECTION_READ_WRITE_EXECUTE;
            break;
        default:
            throw std::runtime_error("unknown windows protection");
    }
    
    return protection;
}


static __inline Handle GetProcessHandle(int pid)
{
    if (pid == 0)
        return GetCurrentProcess();
    
    auto hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc)
    {
        throw std::runtime_error("OpenProcess failed");
    }
    return hProc;
}

uintptr_t GetPageSize()
{
    static uintptr_t pageSize = 0;
    
    if (!pageSize)
    {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        pageSize = (uintptr_t)sys_info.dwPageSize;
    }
    
    return pageSize;
}


void *PageAlloc(size_t n, Protection protection)
{
    return VirtualAlloc(NULL, n, MEM_COMMIT, ToWindowsProt(protection));
}

void PageFree(void *p, size_t n)
{
    VirtualFree(p, 0, MEM_RELEASE);
}

bool PageProtect(const void *p, size_t n, Protection protection)
{
    DWORD dwOldProt;
    
    // const_cast: The memory contents will remain unchanged, so this is fine.
    return VirtualProtect(const_cast<LPVOID>(p), n, ToWindowsProt(protection), &dwOldProt);
}
