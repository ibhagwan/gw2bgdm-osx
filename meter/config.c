#include "core/debug.h"
#include "meter/utf.h"
#include "config.h"
#include <math.h>
#include <Windows.h>
#include <Strsafe.h>
#include <Shlobj.h>

#pragma comment(lib, "shell32.lib")

// The path to the config file.
#define CONFIG_PATH L"bgdm\\bgdm.ini"
#define MAX_INIKEY_LEN 256

// Key-name mapping entry.
typedef struct KeyName
{
	wchar_t* key;
	u32 vk;
} KeyName;

// Key-name mapping table.
static KeyName CONFIG_KEYS[] = {
	{ L"BACKSPACE",	VK_BACK },
	{ L"CLEAR",		VK_CLEAR },
	{ L"DELETE",	VK_DELETE },
	{ L"DOWN",		VK_DOWN },
	{ L"END",		VK_END },
	{ L"ESC",		VK_ESCAPE },
	{ L"F1",		VK_F1 },
	{ L"F10",		VK_F10 },
	{ L"F11",		VK_F11 },
	{ L"F12",		VK_F12 },
	{ L"F2",		VK_F2 },
	{ L"F3",		VK_F3 },
	{ L"F4",		VK_F4 },
	{ L"F5",		VK_F5 },
	{ L"F6",		VK_F6 },
	{ L"F7",		VK_F7 },
	{ L"F8",		VK_F8 },
	{ L"F9",		VK_F9 },
	{ L"HOME",		VK_HOME },
	{ L"INSERT",	VK_INSERT },
	{ L"LEFT",		VK_LEFT },
	{ L"NUM0",		VK_NUMPAD0 },
	{ L"NUM1",		VK_NUMPAD1 },
	{ L"NUM2",		VK_NUMPAD2 },
	{ L"NUM3",		VK_NUMPAD3 },
	{ L"NUM4",		VK_NUMPAD4 },
	{ L"NUM5",		VK_NUMPAD5 },
	{ L"NUM6",		VK_NUMPAD6 },
	{ L"NUM7",		VK_NUMPAD7 },
	{ L"NUM8",		VK_NUMPAD8 },
	{ L"NUM9",		VK_NUMPAD9 },
	{ L"PAGEDOWN",	VK_NEXT },
	{ L"PAGEUP",	VK_PRIOR },
	{ L"PAUSE",		VK_PAUSE },
	{ L"PRINT",		VK_PRINT },
	{ L"RETURN",	VK_RETURN },
	{ L"RIGHT",		VK_RIGHT },
	{ L"SELECT",	VK_SELECT },
	{ L"SPACE",		VK_SPACE },
	{ L"TAB",		VK_TAB },
	{ L"UP",		VK_UP },
	{ L"LBUTTON",	VK_LBUTTON },
	{ L"RBUTTON",	VK_RBUTTON },
	{ L"MBUTTON",	VK_MBUTTON },
};

// Returns true if the given key is down.
bool is_key_down(u32 vk)
{
	return GetAsyncKeyState(vk) & 0x8000;
}

// Returns true if the given bind is down.
bool is_bind_down(Bind const* b)
{
	if (b->vk)
		return
		((b->ctrl == 0 && !is_key_down(VK_CONTROL)) || (b->ctrl == 1 && is_key_down(VK_CONTROL))) &&
		((b->alt == 0 && !is_key_down(VK_MENU)) || (b->alt == 1 && is_key_down(VK_MENU))) &&
		((b->shift == 0 && !is_key_down(VK_SHIFT)) || (b->shift == 1 && is_key_down(VK_SHIFT))) &&
		is_key_down(b->vk);
	else
		return
		(b->ctrl == 0 || is_key_down(VK_CONTROL)) &&
		(b->alt == 0 || is_key_down(VK_MENU)) &&
		(b->shift == 0 || is_key_down(VK_SHIFT));
}

Bind config_get_bindW(const wchar_t* sec, const wchar_t* key, const wchar_t* val)
{
	wchar_t buf[MAX_INIKEY_LEN];
	GetPrivateProfileStringW(sec, key, val, buf, ARRAYSIZE(buf), config_get_ini_file());
	_wcsupr_s(buf, ARRAYSIZE(buf));

	Bind b = { 0 };
	StringCchPrintfA(b.str, ARRAYSIZE(b.str), "%S", buf);

	wchar_t* ctx = buf;
	for (wchar_t* token = wcstok_s(buf, L" + ", &ctx); token; token = wcstok_s(NULL, L" + ", &ctx))
	{
		if (lstrlenW(token) == 1)
		{
			b.vk = token[0];
		}
		else if (lstrcmpW(token, L"ALT") == 0)
		{
			b.alt = true;
		}
		else if (lstrcmpW(token, L"CTRL") == 0)
		{
			b.ctrl = true;
		}
		else if (lstrcmpW(token, L"SHIFT") == 0)
		{
			b.shift = true;
		}
		else
		{
			for (u32 i = 0; i < ARRAYSIZE(CONFIG_KEYS); ++i)
			{
				if (lstrcmpW(token, CONFIG_KEYS[i].key) == 0)
				{
					b.vk = CONFIG_KEYS[i].vk;
					break;
				}
			}
		}
	}

	return b;
}

Bind config_get_bind(i8 const* sec, i8 const* key, i8 const* val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	StringCchPrintfW(args[2], ARRAYSIZE(args[2]), L"%S", val);
	return config_get_bindW(args[0], args[1], args[2]);
}

i32 config_get_intW(const wchar_t* sec, const wchar_t* key, i32 val)
{
	return (i32)GetPrivateProfileIntW(sec, key, val, config_get_ini_file());
}

i32 config_get_int(i8 const* sec, i8 const* key, i32 val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	return config_get_intW(args[0], args[1], val);
}

f32 config_get_floatW(const wchar_t* sec, const wchar_t* key, f32 val)
{
	wchar_t buf[MAX_INIKEY_LEN] = { 0 };
	f32 fRet = val;
	GetPrivateProfileStringW(sec, key, NULL, buf, ARRAYSIZE(buf), config_get_ini_file());
	if (lstrlenW(buf) > 0)
		fRet = (f32)_wtof(buf);
	return fRet;
}

f32 config_get_float(i8 const* sec, i8 const* key, f32 val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	return config_get_floatW(args[0], args[1], val);
}

bool config_set_intW(const wchar_t* sec, const wchar_t* key, i32 val)
{
	wchar_t buff[256] = { 0 };
	StringCchPrintfW(buff, ARRAYSIZE(buff), L"%d", val);
	return WritePrivateProfileStringW(sec, key, buff, config_get_ini_file());
}

bool config_set_int(i8 const* sec, i8 const* key, i32 val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	return config_set_intW(args[0], args[1], val);
}

bool config_set_floatW(const wchar_t* sec, const wchar_t* key, f32 val)
{
	wchar_t buff[256] = { 0 };
	StringCchPrintfW(buff, ARRAYSIZE(buff), L"%.2f", val);
	return WritePrivateProfileStringW(sec, key, buff, config_get_ini_file());
}

bool config_set_float(i8 const* sec, i8 const* key, f32 val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	return config_set_floatW(args[0], args[1], val);
}

wchar_t* config_get_strW(const wchar_t* sec, const wchar_t* key, const wchar_t* val)
{
	wchar_t buf[256];
	GetPrivateProfileStringW(sec, key, val, buf, ARRAYSIZE(buf), config_get_ini_file());
	return _wcsdup(buf);
}

i8* config_get_str(i8 const* sec, i8 const* key, i8 const* val)
{
	i8 buf[256] = { 0 };
	GetPrivateProfileStringA(sec, key, val, buf, ARRAYSIZE(buf), config_get_ini_fileA());
	return _strdup(buf);
}

bool config_set_strW(const wchar_t* sec, const wchar_t* key, const wchar_t* val)
{
	return WritePrivateProfileStringW(sec, key, val, config_get_ini_file());
}

bool config_set_str(i8 const* sec, i8 const* key, i8 const *val)
{
	wchar_t args[3][MAX_INIKEY_LEN] = { 0 };
	StringCchPrintfW(args[0], ARRAYSIZE(args[0]), L"%S", sec);
	StringCchPrintfW(args[1], ARRAYSIZE(args[1]), L"%S", key);
	StringCchPrintfW(args[2], ARRAYSIZE(args[2]), L"%S", val);
	return config_set_strW(args[0], args[1], args[2]);
}


const i8* config_get_my_documentsA()
{
	static char MyDocuments[1024] = { 0 };
	if (MyDocuments[0] == 0) {
		int size = WideCharToMultiByte(CP_UTF8, 0, config_get_my_documents(), -1, NULL, 0, NULL, NULL);
		size = min(size, sizeof(MyDocuments));
		WideCharToMultiByte(CP_UTF8, 0, config_get_my_documents(), -1, MyDocuments, size, NULL, NULL);
	}
	return MyDocuments;
}

const wchar_t* config_get_my_documents()
{
	static wchar_t MyDocuments[1024] = { 0 };
	if (MyDocuments[0] == 0) {
		SecureZeroMemory(MyDocuments, sizeof(MyDocuments));
		HRESULT hr = SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, CSIDL_PERSONAL, MyDocuments);
		if (FAILED(hr)) {
			StringCchPrintfW(&MyDocuments[0], ARRAYSIZE(MyDocuments), L"bin64");
		}
	}
	return MyDocuments;
}


const i8* config_get_ini_fileA()
{
	static char IniFilename[1024] = { 0 };
	if (IniFilename[0] == 0) {
		int size = WideCharToMultiByte(CP_UTF8, 0, config_get_ini_file(), -1, NULL, 0, NULL, NULL);
		size = min(size, sizeof(IniFilename));
		WideCharToMultiByte(CP_UTF8, 0, config_get_ini_file(), -1, IniFilename, size, NULL, NULL);
	}
	return IniFilename;
}

const wchar_t* config_get_ini_file()
{
	static wchar_t IniFilename[1024] = { 0 };
	if (IniFilename[0] == 0) {
		SecureZeroMemory(IniFilename, sizeof(IniFilename));
		StringCchPrintfW(&IniFilename[0], ARRAYSIZE(IniFilename), L"%s\\%s", config_get_my_documents(), CONFIG_PATH);
	}
	return IniFilename;
}



/*const i8* config_get_dbg1()
{
	static char sect[128];
	/*char buff[128] = { 0 };
	utf8_to_base64("NonTosCompliant", buff, sizeof(buff));
	DBGPRINT(L"BASE64-1: %S", buff);
	base64_to_utf8("Tm9uVG9zQ29tcGxpYW50", sect, sizeof(sect));
	return sect;
}*/


const i8* config_get_dbg2()
{
	static char sect[128];
	/*char buff[128] = { 0 };
	utf8_to_base64("MyNameIsArcAndISukDick", buff, sizeof(buff));
	DBGPRINT(L"BASE64-2: %S", buff);*/
	base64_to_utf8("TXlOYW1lSXNBcmNBbmRJU3VrRGljaw==", sect, sizeof(sect));
	return sect;
}