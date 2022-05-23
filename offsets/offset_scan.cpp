#include <string>
#include <codecvt>
#include <type_traits>
#include "offsets/offset_scan.h"
#include "offsets/PatternScanner.h"
#include "core/segv.h"
#include "core/utf.hpp"
#include "meter/logging.h"

#ifdef __APPLE__
#include <errno.h>
#define STATUS_ACCESS_VIOLATION EFAULT
#endif

typedef uintptr_t(*__cbAddr)(void *ctx, uintptr_t addr);

#define ADDR_CB(x) cbArr.push_back( [](void *ctx, uintptr_t addr)->uintptr_t x )
#define ADDR_CBARR []( std::vector<__cbAddr>& cbArr )->void

#define VERIFY_CB [](uintptr_t o, uintptr_t val)->bool
#define POST_CB_VT(vt_off, offset) POST_CB { \
	o = val; val = 0; g_scanner->readData(o + vt_off, sizeof(uintptr_t), (uint8_t*)&val); \
	o = val; val = 0; g_scanner->readData(o + offset, (uint8_t)s, (uint8_t*)&val); \
	return 0; }

static int g_pid = 0;
static std::shared_ptr<hl::PatternScanner> g_scanner = nullptr;

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
/*
* DataScan():
*  pattern = the pattern to search for, passed to hl::FindPattern() or hl::PatternScanner::find()
*  shiftCb = custom func to call for shifting the offset, "shift" and "size" are ignored
*  shift = the relative offset from the pattern to the target value
*  size = number of bytes to extract from the target offset
*  isString = which scan type to use hl::FindPattern() or hl::PatternScanner::find()
*  verify = callback to verify if offset is correct
*/

template <typename T>
Offset MemoryScan(T pattern, bool isString, uint32_t occurence, uint32_t refOccurence, uint8_t size, void(*getCbArr)(std::vector<__cbAddr>& cbArray), bool(*verifyCb)(uintptr_t offset, uintptr_t value))
{
    //static std::unordered_map <T, uintptr_t> OffsetScanMap;
    uintptr_t offset = 0;
    uintptr_t rva = 0;
    uintptr_t val = 0;
    Offset retVal = { 0 };
    
    if (pattern == nullptr || g_scanner == nullptr)
        return retVal;
    
    std::string utf8pattern = verifyUTF8(pattern);
    if (utf8pattern.length() == 0)
        return retVal;
    
    /*if (OffsetScanMap.count(pattern)) {
     offset = OffsetScanMap[pattern];
     }
     else */{
         try {
             
             //offset = isString ? g_scanner->findString(pattern, occurence, refOccurence, moduleName) :
             //    g_scanner->findPattern(utf8pattern.c_str(), occurence, refOccurence, moduleName);
             
             if (isString) {
                 
                 if (std::is_same<T, const wchar_t *>::value)
                     offset = g_scanner->findStringT<wchar_t>((const wchar_t*)pattern, occurence, refOccurence, &rva);
                 else if (std::is_same<T, const char16_t *>::value)
                     offset = g_scanner->findStringT<char16_t>((const char16_t*)pattern, occurence, refOccurence, &rva);
                 else
                     offset = g_scanner->findStringT<char>((const char *)pattern, occurence, refOccurence, &rva);
             }
             else {
                 offset = g_scanner->findPattern(utf8pattern.c_str(), occurence, refOccurence, &rva);
             }
             
             //OffsetScanMap[pattern] = offset;
         }
         catch (...) {
             offset = 0;
         }
     }
    
    if (!offset) {
        LOG_ERR("[GW2LIB::DataScan] Unable to find pattern: \"%s\" in module '%s'", utf8pattern.c_str(), g_scanner->module());
        throw STATUS_ACCESS_VIOLATION;
    }
    else if (getCbArr) {
        
        try {
            std::vector<__cbAddr> cbArray;
            getCbArr(cbArray);
            for (int i=0; i<cbArray.size(); i++) {
                intptr_t old_offset = offset;
                offset = cbArray[i]((void*)g_scanner.get(), offset);
                rva += (offset-old_offset);
            }
        }
        catch (...) {
            LOG_ERR("[GW2LIB::DataScan] Exception occured in callback for pattern: \"%s\"", utf8pattern.c_str());
            throw STATUS_ACCESS_VIOLATION;
        }
        
    }
    
    try {
        if (offset && size) {
            g_scanner->readData(offset, size, (uint8_t*)&val);
        }
    } catch (...) {
        LOG_ERR("[GW2LIB::DataScan] Exception occured reading offset <%#llx> for pattern: \"%s\"", offset, utf8pattern.c_str());
        throw STATUS_ACCESS_VIOLATION;
    }
    
    if (verifyCb && !verifyCb(offset, val)) {
        LOG_ERR("[GW2LIB::DataScan] Verifying pattern failed (offset: %p, value: %#x, pattern: \"%s\")", offset, val, utf8pattern.c_str());
        throw STATUS_ACCESS_VIOLATION;
    }
        
    retVal.addr = offset;
    retVal.rva = rva;
    retVal.val = val;
    return retVal;
}

bool scanOffsets(int pid, const char *moduleName, bool print)
{
	g_pid = pid;
	GameOffsets &m = g_offsets;
	m = offsets_default;
    std::string module = std::string(moduleName ? moduleName : "");

	if (g_scanner == nullptr) {
        try { g_scanner = std::make_shared<hl::PatternScanner>(g_pid, moduleName);
        } catch (...) {
            LogError("Unable to initialize scanner for pid:%d", pid);
            return false;
        }
	}
    
    try {
        
#undef OFFSET
#define OFFSET(_type, _var, _default, _pattern, _is_str, _occurence, _ref_occur, _cbArr, _verify)               \
        m._var = MemoryScan(_pattern, _is_str, _occurence, _ref_occur, sizeof(_type), _cbArr, _verify);         \
        if (m._var.val == 0) m._var.val = offsets_default._var.val;                                             \
        if (print) LogDebug("%-24s <addr:%#016llx> <rva:%#llx> %s <val:%#llx> <def:%#llx>",                     \
            #_var, m._var.addr, m._var.rva,                                                                     \
            (offsets_default._var.val == 0 || m._var.val == offsets_default._var.val) ? "" : " UPDATE",         \
            m._var.val, offsets_default._var.val);
#ifdef __APPLE__
#include "offsets/offsets_osx.def"
#else
#include "offsets/offsets.def"
#endif
#undef OFFSET
        
    } catch (...) {
        
        LogError("Exception raised while scanning for offsets", pid);
        return false;
    }
    
    return true;
}
