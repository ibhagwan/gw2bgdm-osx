#include <algorithm>
#include <unordered_map>
#include <iterator>
#include <cstring>
#include <codecvt>
#include <sstream>
#include "bmsearch.hpp"
#include "core/TargetOS.h"
#include "core/segv.h"
#include "core/utf.hpp"
#include "osx.common/Logging.h"
#include "PatternScanner.h"

#if (defined _M_X64) || defined (TARGET_OS_MAC)
#include "hook/hde/hde64.h"
typedef hde64s HDE;
#define HDE_DISASM(code, hs) hde64_disasm(code, hs)
#else
#include "hook/hde/hde32.h"
typedef hde32s HDE;
#define HDE_DISASM(code, hs) hde32_disasm(code, hs)
#endif


static void except_handler(const char *exception)
{
    LogCrit("Unhandled exception in %s", exception);
}


using namespace hl;

// taken from: http://www.geeksforgeeks.org/pattern-searching-set-7-boyer-moore-algorithm-bad-character-heuristic/
/* Program for Bad Character Heuristic of Boyer Moore String Matching Algorithm */

#include <climits>
#include <cstdint>

#define NO_OF_CHARS 256

// The preprocessing function for Boyer Moore's bad character heuristic
template <typename T>
static void badCharHeuristic(const T *str, size_t size, int badchar[NO_OF_CHARS])
{
    size_t i;

    // Initialize all occurrences as -1
    for (i = 0; i < NO_OF_CHARS; i++)
        badchar[i] = -1;

    // Fill the actual value of last occurrence of a character
    for (i = 0; i < size; i++)
        badchar[(int)str[i]] = (int)i;
}

/* A pattern searching function that uses Bad Character Heuristic of
Boyer Moore Algorithm */
template <typename T>
static std::vector<size_t> boyermoore(const T *txt, const size_t n, const T *pat, const size_t m, bool firstOnly = true)
{
	uint32_t hit = 0;
	std::vector<size_t> ret;
    int badchar[NO_OF_CHARS];

	if (m > n || m < 1)
		return ret;

    /* Fill the bad character array by calling the preprocessing
    function badCharHeuristic() for given pattern */
    badCharHeuristic(pat, m, badchar);

    int s = 0;  // s is shift of the pattern with respect to text
    int end = (int)(n - m);
    while (s <= end)
    {
        int j = (int)m - 1;

        /* Keep reducing index j of pattern while characters of
        pattern and text are matching at this shift s */
        while (j >= 0 && pat[j] == txt[s + j])
            j--;

        /* If the pattern is present at current shift, then index j
        will become -1 after the above loop */
        if (j < 0)
        {
            // EDIT BEGIN
			ret.push_back(s);

			if (firstOnly)
				return ret;
			else ++hit;

            //printf("\n pattern occurs at shift = %d", s);

            /* Shift the pattern so that the next character in text
            aligns with the last occurrence of it in pattern.
            The condition s+m < n is necessary for the case when
            pattern occurs at the end of text */
            s += (s + m < n) ? (int)m-badchar[txt[s + m]] : 1;
            // EDIT END
        }
        else
        {
            /* Shift the pattern so that the bad character in text
            aligns with the last occurrence of it in pattern. The
            max function is used to make sure that we get a positive
            shift. We may get a negative shift if the last occurrence
            of bad character in pattern is on the right side of the
            current character. */
            s += std::max(1, j - badchar[txt[s + j]]);
        }
    }

    return ret;
}

// End of third party code.

PatternScanner::PatternScanner(int pid, const char *moduleName) {
    
	memoryMap = hl::GetMemoryMap(nullptr, pid);
	_pid = pid;
    if (moduleName) _module = moduleName;
    auto regions = memoryMap->regionsForModule(_module);
    if (regions.size() == 0)
        throw std::runtime_error("Unable to initialize scanner");
}

PatternScanner::~PatternScanner() {
}

std::vector<uintptr_t> PatternScanner::find(const std::vector<std::string>& strings)
{
    std::vector<uintptr_t> results(strings.size());
    int stringsFound = 0;

    int i = 0;
    for (const auto& str : strings) {
        uintptr_t found = 0;
        try {
            found = doFindString<char>((const char *)str.c_str(), 0, 0);
        } catch (...) { }

        if (found) {
            results[i] = found;
            stringsFound++;
            if (stringsFound == strings.size())
                break;
        }
        i++;
    }

    if (stringsFound != strings.size())
        throw std::runtime_error("one or more patterns not found");

    return results;
}

template <typename T>
static inline std::string verifyUTF8(T str)
{
    std::string utf8;
    if (std::is_same<T, const wchar_t *>::value)
        utf8 = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes((const wchar_t *)str);
    else if (std::is_same<T, const char16_t *>::value)
        utf8 = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes((const char16_t *)str);
    else utf8 = (const char *)str;
    return utf8;
}

template <>
uintptr_t PatternScanner::findStringT(const char *data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
    return doFindString<char>(data, occurence, refOccurence, outRva);
}

template <>
uintptr_t PatternScanner::findStringT(const char16_t *data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
    return doFindString<char16_t>(data, occurence, refOccurence, outRva);
}

template <>
uintptr_t PatternScanner::findStringT(const wchar_t *data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
    return doFindString<wchar_t>(data, occurence, refOccurence, outRva);
}

template <typename T>
uintptr_t hl::PatternScanner::findStringT(const T* data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
    return 0;
}


template <typename T>
uintptr_t hl::PatternScanner::doFindString(const T* data, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
	// Clear the "found" memory region
	memoryRegion.clear();
    
    // We always return at least the first hit
    if (occurence == 0)
        occurence = 1;

    size_t hits = 0;
	uintptr_t addr = 0;
    std::basic_string<T> str = data;
    std::string utf8 = verifyUTF8(data);

    __TRY
    
	// Search all readonly sections for the strings.
    auto regions = memoryMap->regionsForModule(_module);
    if (regions.size() == 0)
        LogWarn("Unable to find module '%s'", _module.c_str());
    for (auto& region : regions) {
        
		if (region.protection & hl::PROTECTION_READ_EXECUTE) {

            const T* bytes = (const T*)region.readBytesNoCopy();
            if (bytes)
            {
#ifdef __APPLE2__
                std::vector<size_t> found = boyermoore(bytes, region.size, (const T*)str.data(), str.size() + 1, occurence > 0);
                if (found.size() > 0) {
                    size_t hits_total = hits + found.size();
                    if (hits_total >= occurence) {
                        addr = (uintptr_t)region.base + found[occurence-hits-1];
                        LogTrace("pattern '%s' found at addr:%#llx, region <addr:%#llx> <size:%lld> <prot:%#x> <module:%s>",
                                 utf8.c_str(), addr, region.base, region.size, region.protection, region.name.c_str());
                        region.freeBytes();
                        break;
                    }
                    hits = hits_total;
                }
#else
				std::vector<size_t> other_shifts;
				other_shifts.resize(str.length());
				bm_build<T>(str.data(), str.length(), other_shifts.data());
                
                size_t found = 0;
                size_t offset = -1;
                do {
                    found = bm_search((const T *)bytes + (offset+1), region.size/sizeof(T) - (offset+1), 0, str.length(), other_shifts.data());
                    if (found != -1) {
                        uintptr_t rva = (offset+1) * sizeof(T) + (uintptr_t)found * sizeof(T);
                        addr = (uintptr_t)region.base + rva;
                        LogTrace("pattern hit #%d '%s' found at rva %#llx, <addr:%#llx>, region <addr:%#llx> <size:%lld> <prot:%#x> <module:%s>",
                                 hits+1, utf8.c_str(), rva, addr, region.base, region.size, region.protection, region.name.c_str());
                        if (++hits >= occurence) {
                            bm_clean<T>();
                            region.freeBytes();
                            break;
                        }
                        else offset = (rva+1) / sizeof(T);
                    }
                } while (found != -1);
                
                bm_clean<T>();
#endif
                region.freeBytes();
            }
		}
	}
    
    __EXCEPT( "findString::wstring" )
    __FINALLY

	if (!addr) throw std::runtime_error("pattern not found");

	return findAddrRef(addr, refOccurence, outRva);
}



uintptr_t hl::PatternScanner::findAddrRef(uintptr_t addr, uint32_t occurence, uintptr_t *outRva)
{
	size_t hits = 0;
    uintptr_t ret = 0;

	// We always return at least the first hit
	if (occurence == 0)
		occurence = 1;
    
    __TRY

    // Search all code sections for references to the strings.
    auto regions = memoryMap->regionsForModule(_module);
    if (regions.size() == 0)
        LogWarn("Unable to find module '%s'", _module.c_str());
    for (auto& region : regions) {
        
        if (region.protection & hl::PROTECTION_READ_EXECUTE) {

			size_t regionSize = region.size;
			const uint8_t *baseAdr = region.readBytesNoCopy();
			if (!baseAdr) continue;
            
            // minus the FollowRelativeAddress ptr size
            uintptr_t endAdr = (uintptr_t)(baseAdr + regionSize) - sizeof(uint32_t);
            for (uintptr_t adr = (uintptr_t)baseAdr; adr < endAdr; adr++) {
                
                //uintptr_t reladdr = FollowRelativeAddress(adr);
                uintptr_t reladdr = FollowRelativeAddressAsm(adr, adr - (uintptr_t)baseAdr);
				reladdr = (uintptr_t)region.base + reladdr - (uintptr_t)baseAdr;
                if (reladdr == addr) {
                    // Prevent false poritives by checking if the reference occurs in a LEA instruction.
                    uintptr_t rva = adr - (uintptr_t)baseAdr;
                    uint8_t opcode8 = (rva > 1) ? *(uint8_t*)(adr - 1) : 0;
                    uint16_t opcode16 = (rva > 2) ? *(uint16_t*)(adr - 2) : 0;
                    uint16_t opcode32 = (rva > 3) ? *(uint16_t*)(adr - 3) : 0;
                    if (opcode32 == 0x8D48 || opcode32 == 0x8D4C ||
                        opcode16 == 0x830F || opcode16 == 0x840F ||
                        opcode16 == 0x850F || opcode16 == 0x860F ||
                        opcode16 == 0x870F || opcode16 == 0x8D0F ||
                        opcode8 == 0x72 || opcode8 == 0x73 || opcode8 == 0x74 ||
                        opcode8 == 0x75 || opcode8 == 0x76 || opcode8 == 0x7A ||
                        opcode8 == 0x7D || opcode8 == 0x7F || opcode8 == 0xE8 ||
                        opcode8 == 0xE9 || opcode8 == 0xEB
                        ) {
						if (++hits == occurence) {
							ret = (uintptr_t)region.base + adr - (uintptr_t)baseAdr;
							memoryRegion = region;
                            if (outRva) *outRva = rva;
                            LogTrace("addr ref #%d <%#llx> found at rva %#llx <addr:%#llx>, region <addr:%#llx> <size:%lld> <prot:%#x> <module:%s>",
                                     hits, addr, rva, ret, region.base, region.size, region.protection, region.name.c_str());
							region.freeBytes();
							break;
						}
                    }
                }
            }
            
			region.freeBytes();
            if (ret) break;
        }
    }
    
    __EXCEPT( "findAddrRef" )
    __FINALLY
    
    if (!ret) {
        LogError("addr ref not found <addr:%#llx>", addr);
        throw std::runtime_error("addr ref not found");
    }

    return ret;
}

uintptr_t PatternScanner::findPatternMask(const std::string & str, const std::string& mask, uintptr_t *outRva)
{
	// Clear the "found" memory region
	memoryRegion.clear();

	uintptr_t addr = 0;
    
    __TRY
    
    LogTrace("Searching for pattern '%s' mask '%s'", str.c_str(), mask.c_str());
    
    // Search all readonly sections for the strings.
    auto regions = memoryMap->regionsForModule(_module);
    if (regions.size() == 0)
        LogWarn("Unable to find module '%s'", _module.c_str());
	for (auto& region : regions) {
		if (region.protection & hl::PROTECTION_READ_EXECUTE) {

			uintptr_t bytes = (uintptr_t)region.readBytesNoCopy();
			if (bytes)
			{
				uintptr_t found = mask.empty() ? hl::FindPattern(str, bytes, region.size) : hl::FindPatternMask(str.c_str(), mask.c_str(), bytes, region.size);
				if (found) {
					addr = region.base + found - bytes;
                    if (outRva) *outRva = found - (uintptr_t)bytes;
                    LogTrace("Pattern '%s' found at rva %#llx <addr:%#llx>, region <addr:%#llx> <size:%lld> <prot:%#x> <module:%s>",
                             str.c_str(), found - (uintptr_t)bytes, addr, region.base, region.size, region.protection, region.name.c_str());
					memoryRegion = region;
					region.freeBytes();
					break;
				}
			}
			region.freeBytes();
		}
	}
    
    __EXCEPT( "findPatternMask" )
    __FINALLY

	if (!addr) throw std::runtime_error("pattern not found");
	return addr;
}


std::map<std::string, uintptr_t> PatternScanner::findMap(const std::vector<std::string>& strings)
{
    auto results = find(strings);

    std::map<std::string, uintptr_t> resultMap;
    for (size_t i = 0; i < results.size(); i++)
    {
        resultMap[strings[i]] = results[i];
    }
    return resultMap;
}

uintptr_t PatternScanner::base()
{
	return memoryRegion.base;
}

uintptr_t PatternScanner::data(size_t offset)
{
	if (memoryRegion.base == 0 || memoryRegion.size == 0)
		return 0;

	if (memoryRegion.bytes.size() == 0)
		memoryRegion.readBytes();

	size_t rva = (offset < memoryRegion.base) ? 0 : offset - memoryRegion.base;
	if (rva < memoryRegion.bytes.size()) {
		return (uintptr_t)memoryRegion.bytes.data() + rva;
	}

	return 0;
}

bool PatternScanner::readData(size_t offset, uint8_t size, uint8_t* outData)
{
	return memoryMap->readBytes(offset, size, outData);
}


static bool MatchMaskedPattern(uintptr_t address, const char *byteMask, const char *checkMask)
{
    for (; *checkMask; ++checkMask, ++address, ++byteMask)
        if (*checkMask == 'x' && *(char*)address != *byteMask)
            return false;
    return *checkMask == 0;
}

uintptr_t hl::FindPatternMask(const char *byteMask, const char *checkMask, uintptr_t address, size_t len)
{
    uintptr_t end = address + len - strlen(checkMask) + 1;
    for (uintptr_t i = address; i < end; i++) {
        if (MatchMaskedPattern(i, byteMask, checkMask)) {
            return i;
        }
    }
    return 0;
}

uintptr_t PatternScanner::findPattern(const char *str, uint32_t occurence, uint32_t refOccurence, uintptr_t *outRva)
{
    return findPatternMask(str, "", outRva);
}

uintptr_t hl::FindPattern(const std::string& pattern, uintptr_t address, size_t len)
{
    std::vector<char> byteMask;
    std::vector<char> checkMask;

    std::string lowPattern = pattern;
    std::transform(lowPattern.begin(), lowPattern.end(), lowPattern.begin(), ::tolower);
    lowPattern += " ";

    for (size_t i = 0; i < lowPattern.size()/3; i++)
    {
        if (lowPattern[3*i+2] == ' ' && lowPattern[3*i] == '?' && lowPattern[3*i+1] == '?')
        {
            byteMask.push_back(0);
            checkMask.push_back('?');
        }
        else if (lowPattern[3*i+2] == ' ' &&
            ((lowPattern[3*i] >= '0' && lowPattern[3*i] <= '9') ||
                (lowPattern[3*i] >= 'a' && lowPattern[3*i] <= 'f')) &&
            ((lowPattern[3*i+1] >= '0' && lowPattern[3*i+1] <= '9') ||
                (lowPattern[3*i+1] >= 'a' && lowPattern[3*i+1] <= 'f')))

        {
            auto value = strtol(lowPattern.data()+3*i, nullptr, 16);
            byteMask.push_back((char)value);
            checkMask.push_back('x');
        }
        else
        {
            throw std::runtime_error("invalid format of pattern string");
        }
    }

    // Terminate mask string, because it is used to determine length.
    checkMask.push_back('\0');

    return hl::FindPatternMask(byteMask.data(), checkMask.data(), address, len);
}


uintptr_t hl::FollowRelativeAddress(uintptr_t adr)
{
    // Hardcoded 32-bit dereference to make it work with 64-bit code.
    return *(int32_t*)adr + adr + 4;
}

uintptr_t hl::FollowRelativeAddressAsm(uintptr_t adr, uintptr_t rva)
{
    // Hardcoded 32-bit dereference to make it work with 64-bit code.
    uint8_t opcode = (rva > 0) ? *(int8_t*)(adr-1) : 0;
    switch (opcode) {
        case(0x72):     // JB
        case(0x73):     // JBE
        case(0x74):     // JE
        case(0x75):     // JNE
        case(0x76):     // JBE
        case(0x7A):     // JP
        case(0x7D):     // JGE
        case(0x7F):     // JG
        case(0xEB):     // JMP
            //if (rva==1) LogTrace("opcode: %#x <addr:%#llx> <rva:%#x> <val:%#x> <res:%#llx>", opcode, adr, rva, *(int8_t*)adr, *(int8_t*)adr + adr + 1);
            return *(int8_t*)adr + adr + 1;
    }
    return *(int32_t*)adr + adr + 4;
}

/*
uintptr_t hl::FollowRelativeAddressAsm(uintptr_t adr)
{
    HDE hs = { 0 };
    
    __TRY
    
    int instructionSize = HDE_DISASM((void*)adr, &hs);
    if ( !(hs.flags & F_ERROR) ) {
        if (instructionSize == 1)
            return 0;
        else if (instructionSize == 2)
            return *(uint8_t*)adr + adr + 1;
        else
            return *(int32_t*)adr + adr + 4;
    }
    else {
        return *(int32_t*)adr + adr + 4;
    }
    
    __EXCEPT( "FollowRelativeAddressAsm" )
    __FINALLY
    
    return 0;
}
 */
