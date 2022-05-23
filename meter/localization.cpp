#include "string.hpp"
#include <codecvt>
#include <vector>
#include <unordered_map>
#include "core/TargetOS.h"
#include "core/file.h"
#include "core/utf.hpp"
#include "meter/config.h"
#include "meter/localization.h"
#include "simpleini/SimpleIni.h"

//#define INI_ANSIONLY
//#include "minIni.h"


#ifdef TARGET_OS_WIN
#include "meter/resource.h"
extern "C" bool GetEmbeddedResource(int ResID, LPVOID *ppData, LPDWORD pBytes);
#endif

#define LOCALIZATION_INI_SECT   "Localization"
#define LOCALIZATION_CN_PATH    "/Simplified Chinese"

typedef struct LocalText {

	LocalText() {};
	LocalText(const char *name, const char *text) {
		fieldName = name;
		fieldText = text;
	}

    std::string fieldName;
	std::string fieldText;

} LocalText;

typedef std::unordered_map<std::string, LocalText*> LocalTextMap;
static std::vector<LocalText> m_localTextArr;
static LocalTextMap m_localTextMap;

static LSTR m_directory;
static LSTR m_filename;

size_t localtext_size()
{
	return m_localTextArr.size();
}

bool localtext_init(const LCHAR *directory, const LCHAR *filename
#ifdef __APPLE__
                    , const LCHAR *cn_filename
#endif
)
{
	m_directory = directory;
	m_filename = filename;

	// Create the language directory (if needed)
	// And load the default language file
    __create_directory(m_directory.c_str());

#if !(defined BGDM_TOS_COMPLIANT)
	// Check if the chinese lang file exists
	// if not get the file from the resource and deploy
    LSTR cn_path;
    cn_path = m_directory;
    cn_path += __TEXT(LOCALIZATION_CN_PATH);
#ifdef TARGET_OS_WIN
    if (!__file_exists(cn_path.c_str())) {
		LPVOID pFile = NULL;
		DWORD dwBytes = 0;
		if (GetEmbeddedResource(IDR_RT_LANG_CHINESE, &pFile, &dwBytes)) {
			file_writeW(cn_path, pFile, dwBytes);
		}
    }
#elif defined __APPLE__
        __file_copy(cn_filename, cn_path.c_str(), false);
#endif
#endif	// !(defined BGDM_TOS_COMPLIANT)

	localtext_load_file(m_filename.c_str());
	return true;
}

void localtext_load_defaults()
{
	//m_localTextArr.RemoveAll();
	//m_localTextMap.RemoveAll();
	m_localTextArr.resize(LOCALIZED_TEXT_END);

#undef LOCALTEXT
#define LOCALTEXT(_idx, _text) \
	localtext_set(_idx, #_idx, _text);
#include "meter/localization.inc"
#undef LOCALTEXT
}

bool localtext_load_file(const LCHAR *file)
{
	static LCHAR buf[1024] = { 0 };

	localtext_load_defaults();

    if (__tcslen(file) <= 0) return false;

    LSTR path;
    path = m_directory;
    path += __TEXT("/");
    path += file;

    if (__file_exists(path.c_str())) {

        CSimpleIniA ini;
        ini.SetUnicode();
        ini.SetMultiLine();
        ini.LoadFile(path.c_str());
        
		for (int i = 0; i < m_localTextArr.size(); ++i)
		{
            std::string text;
			memset(buf, 0, sizeof(buf));
#ifdef LOCALUNI
            const std::wstring utf16 = std_utf8_to_utf16(m_localTextArr[i].fieldName);
			GetPrivateProfileStringW(LOCALIZATION_INI_SECT, utf16, NULL, buf, ARRAYSIZE(buf), path.c_str());
            if (wcslen(buf) > 0) text = std_utf16_to_utf8(buf, len);
#else
            //ini_gets(LOCALIZATION_INI_SECT, m_localTextArr[i].fieldName.c_str(), NULL, buf, ARRAYSIZE(buf), path.c_str());
            //if (strlen(buf) > 0) text = buf;
            text = ini.GetValue(LOCALIZATION_INI_SECT, m_localTextArr[i].fieldName.c_str(), "");
#endif
            if (!text.empty()) {
                /*if (text.find(__TEXT("\\r\\n")) != std::string::npos) {
                    text.replace(text.begin(), text.end(), __TEXT("\\r\\n"), __TEXT("\r\n"));
                }
                if (text.find(__TEXT("\\n")) != std::string::npos) {
                    text.replace(text.begin(), text.end(), __TEXT("\\n"), __TEXT("\n"));
                }*/
                m_localTextArr[i].fieldText = text;
            }
		}
		return true;
	}
	return false;
}

bool localtext_save_file(const LCHAR *file)
{
    
    if (__tcslen(file) <= 0) return false;

    LSTR path;
    path = m_directory;
    path += __TEXT("/");
    path += file;
    

    if (!__file_exists(path.c_str())) {
        __file_write(path.c_str(), "", 0);
    }
    if (__file_exists(path.c_str())) {

        CSimpleIniA ini;
        ini.SetUnicode();
        ini.SetMultiLine();
        
		for (int i = 0; i < m_localTextArr.size(); ++i)
		{
            std::string val = m_localTextArr[i].fieldText;
            /*if (val.find("\r\n") != std::string::npos) {
                val.replace(val.begin(), val.end(), "\r\n", "\\r\\n");
            }
            if (val.find("\n") != std::string::npos) {
                val.replace(val.begin(), val.end(), "\n", "\\n");
            }*/
#ifdef LOCALUNI
            const std::wstring wkey = std_utf8_to_utf16(m_localTextArr[i].fieldName);
            const std::wstring wval = std_utf8_to_utf16(val);
            WritePrivateProfileStringW(LOCALIZATION_INI_SECT, wkey.c_str(), wval.c_str(), path.c_str());
#else
            //ini_puts(LOCALIZATION_INI_SECT, m_localTextArr[i].fieldName.c_str(), val.c_str(), path.c_str());
            ini.SetValue(LOCALIZATION_INI_SECT, m_localTextArr[i].fieldName.c_str(), val.c_str());
#endif
		}
        
        ini.SaveFile(path.c_str());
		return true;
	}
	return false;
}

const char *localtext_get(int idx)
{
	static const char null_string[] = "(null)";
	if (idx < m_localTextArr.size())
		return m_localTextArr[idx].fieldText.c_str();
	return null_string;
}

const char *localtext_get_fmt(int idx, const char *fmt, ...)
{
    static std::string str;
	str.clear();
	str = localtext_get(idx);
	va_list args;
	va_start(args, fmt);
    str += toString(fmt, args);
	va_end(args);
	return str.c_str();
}

const char *localtext_get_name(int idx)
{
	static const char null_string[] = "(null)";
	if (idx < m_localTextArr.size())
		return m_localTextArr[idx].fieldName.c_str();
	return null_string;
}

bool localtext_set(int idx, const char* name, const char *text)
{
	if (idx < m_localTextArr.size()) {
		m_localTextArr[idx].fieldName = name;
		m_localTextArr[idx].fieldText = text;
		m_localTextMap[name] = &m_localTextArr[idx];
		return true;
	}
	return false;
}

bool localtext_set_name(const char *name, const char *text)
{
    LocalTextMap::iterator pair = m_localTextMap.find(name);
	if (pair != m_localTextMap.end()) {
		pair->second->fieldText = text;
		return true;
	}
	return false;
}
