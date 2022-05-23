#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include "Memory.h"
#include "Memory_OSX.h"
#include "osx.common/Logging.h"


static int ToUnixProt(hl::Protection protection)
{
    int unixProt = PROT_NONE;

    if (protection & hl::PROTECTION_READ)
        unixProt |= PROT_READ;
    if (protection & hl::PROTECTION_WRITE)
        unixProt |= PROT_WRITE;
    if (protection & hl::PROTECTION_EXECUTE)
        unixProt |= PROT_EXEC;
    if (protection & hl::PROTECTION_GUARD)
        throw std::runtime_error("protection not supported");

    return unixProt;
}

static hl::Protection FromUnixProt(int unixProt)
{
    hl::Protection protection = hl::PROTECTION_NOACCESS;

    if (unixProt & PROT_READ)
        protection |= hl::PROTECTION_READ;
    if (unixProt & PROT_WRITE)
        protection |= hl::PROTECTION_WRITE;
    if (unixProt & PROT_EXEC)
        protection |= hl::PROTECTION_EXECUTE;

    return protection;
}


static inline hl::Handle GetProcessHandle(int pid, hl::Handle *pHandle)
{
    if (pid == 0) {
        *pHandle = getCurrentProcess();
        return *pHandle;
        
    } else {
        
        openProcess(pid, pHandle);
    }
    
    if (!*pHandle)
    {
        throw std::runtime_error("OpenProcess failed");
    }
    return *pHandle;
}

uintptr_t hl::GetPageSize()
{
    static uintptr_t pageSize = 0;

    if (!pageSize)
    {
        pageSize = (uintptr_t)sysconf(_SC_PAGESIZE);
    }

    return pageSize;
}


void *hl::PageAlloc(size_t n, hl::Protection protection)
{
    return mmap(nullptr, n, ToUnixProt(protection), MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

void hl::PageFree(void *p, size_t n)
{
    if (!n)
        throw std::runtime_error("you must specify the free size on linux");

    munmap(p, n);
}

void hl::PageProtect(const void *p, size_t n, hl::Protection protection)
{
    // Align to page boundary.
    auto pAligned = (const void*)((uintptr_t)p & ~(hl::GetPageSize() - 1));
    size_t nAligned = (uintptr_t)p - (uintptr_t)pAligned + n;

    // const_cast: The memory contents will remain unchanged, so this is fine.
    mprotect(const_cast<void*>(pAligned), nAligned, ToUnixProt(protection));
}


hl::ModuleHandle hl::GetModuleByName(const std::string& name, hl::Handle hProc, int pid)
{
    return hProc;
    
    /*void *handle;
    if (name == "")
    {
        handle = dlopen(NULL, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    }
    else
    {
        handle = dlopen(name.c_str(), RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    }

    if (handle)
    {
        // This is undocumented, but works and is easy.
        auto hModule = *(hl::ModuleHandle*)handle;
        if (!hModule)
        {
            hModule = (hl::ModuleHandle)0x400000;
        }
        dlclose(handle);
        return hModule;
    }
    else
    {
        return hl::NullModuleHandle;
    }*/
}

hl::ModuleHandle hl::GetModuleByAddress(uintptr_t adr, hl::Handle hProc, int pid)
{
    Dl_info info = { 0 };
    dladdr((void*)adr, &info);
    return info.dli_fbase;
}


std::string hl::GetModulePath(hl::ModuleHandle hModule, hl::Handle hProc, int pid)
{
    Dl_info info = { 0 };
    if (dladdr((void*)hModule, &info) == 0)
    {
        throw std::runtime_error("dladdr failed");
    }
    return info.dli_fname;
}


hl::MemoryRegion hl::GetMemoryByAddress(uintptr_t adr, hl::Handle hProc, int pid)
{
    hl::MemoryRegion region;

    auto memoryMap = hl::GetMemoryMap(hProc, pid);
    auto itRegion = std::find_if(memoryMap->regions.begin(), memoryMap->regions.end(), [adr](const hl::MemoryRegion& r){
        return adr > r.base && adr < r.base + r.size;
    });
    if (itRegion != memoryMap->regions.end())
    {
        region = *itRegion;
    }

    return region;
}

std::shared_ptr<hl::MemoryMap> hl::GetMemoryMap(hl::Handle hProc, int pid)
{
    auto regions = std::make_shared<MemoryMap>(hProc, pid);
    
    auto addRegion = [](void *ctx, uintptr_t base, size_t size, int prot, const char *module) {
        
        hl::MemoryRegion region;
        region.status = hl::MemoryRegion::Status::Valid;
        region.base = base;
        region.size = size;
        region.protection = FromUnixProt(prot);
        region.hProcess = ((MemoryMap*)ctx)->hProcess;
        region.name = std::string(module ? module : "");
        //region.hModule = hl::GetModuleByAddress(region.base);
        ((MemoryMap*)ctx)->regions.push_back(region);
        //LogTrace("<module:%s> <addr:%llx> <size:%d> <prot:%d> ", module, base, size, prot);
    };
    
    
    getMemRegions(regions->hProcess, (void*)regions.get(), addRegion);
    
    return regions;
}

const uint8_t *hl::MemoryRegion::readBytes()
{
    bytes.clear();
    bytes.resize(size);
    if (readProcessMemory(hProcess, base, bytes.data(), size, (uint32_t *)&size)) {
        return bytes.data();
    }
    return nullptr;
}

const uint8_t *hl::MemoryRegion::readBytesNoCopy()
{
    return vmReadProcessMemory(hProcess, base, &size);
}


void hl::MemoryRegion::freeBytes()
{
    bytes.clear();
}

void hl::MemoryRegion::clear()
{
    status = Status::Invalid;
    base = 0;
    size = 0;
    protection = hl::PROTECTION_NOACCESS;
    hProcess = 0;
    hModule = 0;
    name.clear();
    bytes.clear();
}


hl::MemoryMap::MemoryMap(hl::Handle hProc, int pid)
{
    hProcess = hProc;
    if (!hProcess)
        GetProcessHandle(pid, &hProcess);
}

hl::MemoryMap::~MemoryMap() {
    if (hProcess)
        dlclose(hProcess);
    hProcess = nullptr;
}

std::vector<hl::MemoryRegion> hl::MemoryMap::regionsForModule(const std::string& name)
{
    auto ret = std::vector<hl::MemoryRegion>();
    
    for (auto& region : this->regions) {
        
        if (name.empty() || region.name == name) {
            ret.push_back(region);
        }
    }
    
    return ret;
}

bool hl::MemoryMap::readBytes(uintptr_t offset, uint8_t size, uint8_t* outData)
{
    if (size == 0 || !outData)
        return false;
    
    return readProcessMemory(hProcess, offset, outData, size, 0);
}

uintptr_t hl::MemoryMap::followRelativeAddress(uintptr_t adr)
{
    uintptr_t addr = 0;
    size_t size = sizeof(addr);
    unsigned char *data = vmReadProcessMemory(hProcess, adr, &size);
    if (data) {
        addr = (uintptr_t)*data;
        addr += adr + 4;
    }
    /*if ( readProcessMemory(hProcess, adr, (uint8_t*)&addr, sizeof(addr), 0) ) {
        addr += adr + 4;
    }*/
    return addr;
}
