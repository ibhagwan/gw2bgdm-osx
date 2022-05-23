#ifndef HACKLIB_PATTERNSCANNER_H
#define HACKLIB_PATTERNSCANNER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include "Memory.h"
#ifdef TARGET_OS_WIN
#include <wchar.h>
#define char16_t wchar_t
#endif


namespace hl {
    
class PatternScanner
{
public:
    PatternScanner(int pid=0, const char *moduleName = NULL);
	~PatternScanner();
    
    // Searches for referenced strings in code of the module.
    std::vector<uintptr_t> find(const std::vector<std::string>& strings);
    
    // Search for a string
    template <typename T>
    uintptr_t findStringT(const T *str, uint32_t occurence = 0, uint32_t refOccurence = 0, uintptr_t *outRva = 0);
    
    template <typename T>
    uintptr_t doFindString(const T* data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva = 0);
    
	// More convenient than findPatternMask and less error prone alternative.
	// Example: "12 45 ?? 89 ?? ?? ?? cd ef"
    uintptr_t findPattern(const char *str, uint32_t occurence = 0, uint32_t refOccurence = 0, uintptr_t *outRva = 0);
    
    // Find references to address
    uintptr_t findAddrRef(uintptr_t addr, uint32_t occurence = 0, uintptr_t *outRva = 0);
    
	// Finds a binary pattern with mask in executable sections of a module.
	// The mask is a string containing 'x' to match and '?' to ignore.
	// If moduleName is nullptr the module of the main module is searched.
	uintptr_t findPatternMask(const std::string& str, const std::string& mask, uintptr_t *outRva = 0);
    std::map<std::string, uintptr_t> findMap(const std::vector<std::string>& strings);

	uintptr_t base();
	uintptr_t data(size_t offset);
	bool readData(size_t offset, uint8_t size, uint8_t* outData);
    
    const char* module() { return _module.c_str(); }

private:
    int _pid;
    std::string _module;
	std::shared_ptr<hl::MemoryMap> memoryMap;
	hl::MemoryRegion memoryRegion;
};


// Variant for arbitrary memory.
uintptr_t FindPattern(const std::string& pattern, uintptr_t address, size_t len);
uintptr_t FindPatternMask(const char *byteMask, const char *checkMask, uintptr_t address, size_t len);

// Helper to follow relative addresses in instructions.
// Instruction is assumed to end after the relative address. for example jmp, call
uintptr_t FollowRelativeAddress(uintptr_t adr);

// The above func always assumes the jump is 32 bit
// this one uses disassmbly to figure out the jmp size
uintptr_t FollowRelativeAddressAsm(uintptr_t adr, uintptr_t rva);
}

#endif
