#ifndef HACKLIB_MEMORY_H
#define HACKLIB_MEMORY_H

#include "Handles.h"
#include <memory>
#include <vector>
#include <string>

namespace hl {


typedef int Protection;

static const Protection PROTECTION_NOACCESS = 0x0;
static const Protection PROTECTION_READ = 0x1;
static const Protection PROTECTION_WRITE = 0x2;
static const Protection PROTECTION_EXECUTE = 0x4;
static const Protection PROTECTION_GUARD = 0x8; // Only supported on Windows.
static const Protection PROTECTION_READ_WRITE = PROTECTION_READ|PROTECTION_WRITE;
static const Protection PROTECTION_READ_EXECUTE = PROTECTION_READ|PROTECTION_EXECUTE;
static const Protection PROTECTION_READ_WRITE_EXECUTE = PROTECTION_READ_WRITE|PROTECTION_EXECUTE;


// A region of memory on page boundaries with equal protection.
struct MemoryRegion
{
    enum class Status
    {
        // A valid committed memory region. All fields are valid.
        Valid,
        // A free memory region. Only the base and size fields are valid.
        Free,
        // An invalid memory region. This is usually kernel address space. No fields are valid.
        Invalid
    };

	~MemoryRegion() { clear(); }

	int pid = 0;
    Status status = Status::Invalid;
    uintptr_t base = 0;
    size_t size = 0;
    Protection protection = hl::PROTECTION_NOACCESS;
	hl::Handle hProcess = 0;
    hl::ModuleHandle hModule = 0;
    std::string name;
	std::vector<uint8_t> bytes;

	const uint8_t *readBytes();
    const uint8_t *readBytesNoCopy();
	void freeBytes();
	void clear();
};

struct MemoryMap
{
	MemoryMap(hl::Handle hProc = nullptr, int pid = 0);
	~MemoryMap();

	hl::Handle hProcess = 0;
	std::vector<MemoryRegion> regions;
    std::vector<MemoryRegion> regionsForModule(const std::string& name = "");

	bool readBytes(uintptr_t offset, uint8_t size, uint8_t* outData);
    
    uintptr_t followRelativeAddress(uintptr_t adr);
};


uintptr_t GetPageSize();

// Allocate memory on own pages. This is useful when the protection
// is adjusted to prevent other data from being affected.
// PageFree must be used to free the retrieved memory block.
void *PageAlloc(size_t n, Protection protection);
void PageFree(void *p, size_t n = 0);
void PageProtect(const void *p, size_t n, Protection protection);

template <typename T, typename A>
void PageProtectVec(const std::vector<T, A>& vec, Protection protection)
{
    PageProtect(vec.data(), vec.size()*sizeof(T), protection);
}


// An empty string requests the main module.
hl::ModuleHandle GetModuleByName(const std::string& name = "", hl::Handle hProc = nullptr, int pid = 0);

// Returns hl::NullModuleHandle when the address does not reside in a module.
hl::ModuleHandle GetModuleByAddress(uintptr_t adr, hl::Handle hProc = nullptr, int pid = 0);

std::string GetModulePath(hl::ModuleHandle hModule, hl::Handle hProc = nullptr, int pid = 0);

// On linux it is way more performant to use GetMemoryMap than to build
// one using this function.
MemoryRegion GetMemoryByAddress(uintptr_t adr, hl::Handle hProc = nullptr, int pid = 0);

// The resulting memory map only contains regions with Status::Valid.
// A pid of zero can be passed for the current process.
std::shared_ptr<hl::MemoryMap> GetMemoryMap(hl::Handle hProc = nullptr, int pid = 0);

}

#endif
