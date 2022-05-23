//
//  mem.c
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include "core/mem.h"
//#include <fstream>
//#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>

#ifdef __APPLE__
#include <sys/mman.h>
#endif


static int ToUnixProt(Protection protection)
{
    int unixProt = PROT_NONE;
    
    if (protection & PROTECTION_READ)
        unixProt |= PROT_READ;
    if (protection & PROTECTION_WRITE)
        unixProt |= PROT_WRITE;
    if (protection & PROTECTION_EXECUTE)
        unixProt |= PROT_EXEC;
    if (protection & PROTECTION_GUARD)
        unixProt = PROTECTION_INVALID;
    
    return unixProt;
}

/*
static Protection FromUnixProt(int unixProt)
{
    Protection protection = PROTECTION_NOACCESS;
    
    if (unixProt & PROT_READ)
        protection |= PROTECTION_READ;
    if (unixProt & PROT_WRITE)
        protection |= PROTECTION_WRITE;
    if (unixProt & PROT_EXEC)
        protection |= PROTECTION_EXECUTE;
    
    return protection;
}
*/

uintptr_t GetPageSize()
{
    static uintptr_t pageSize = 0;
    
    if (!pageSize)
    {
        pageSize = (uintptr_t)sysconf(_SC_PAGESIZE);
    }
    
    return pageSize;
}


void *PageAlloc(size_t n, Protection protection)
{
    return mmap(NULL, n, ToUnixProt(protection), MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

void PageFree(void *p, size_t n)
{
    if (!n)
        return;
        //throw std::runtime_error("you must specify the free size on linux");
    
    munmap(p, n);
}

bool PageProtect(const void *p, size_t n, Protection protection)
{
    // Align to page boundary.
    void * pAligned = (void*)((uintptr_t)p & ~(GetPageSize() - 1));
    size_t nAligned = (uintptr_t)p - (uintptr_t)pAligned + n;
    
    // const_cast: The memory contents will remain unchanged, so this is fine.
    return mprotect((void*)(pAligned), nAligned, ToUnixProt(protection)) == 0;
}
