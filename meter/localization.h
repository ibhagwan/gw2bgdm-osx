#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include "core/TargetOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOCALTEXT(_idx, _text) _idx,
enum LocalizationStrings {

	LOCALIZED_TEXT_START = -1,

#include "meter/localization.inc"

	LOCALIZED_TEXT_END
};
#undef LOCALTEXT
    
#if (defined _UNICODE || defined __UNICODE__ || defined UNICODE) && defined TARGET_OS_WIN
#define LOCALUNI
#define LCHAR               wchar_t
#define LSTR                std::wstring
#define __file_write        file_writeW
#define __file_exists       file_existsW
#define __file_copy         file_copyW
#define __create_directory  create_directoryW
#ifndef __TEXT
#define __TEXT(quote)       L##quote
#define __tcslen            wcslen
#endif
#else
#undef LOCALUNI
#define LCHAR               char
#define LSTR                std::string
#define __file_write        file_write
#define __file_exists       file_exists
#define __file_copy         file_copy
#define __create_directory  create_directory
#ifndef __TEXT
#define __TEXT(quote)       quote
#define __tcslen            strlen
#endif
#endif

size_t localtext_size();
bool localtext_init(const LCHAR *directory, const LCHAR *filename
#ifdef __APPLE__
        , const LCHAR *cn_filename
#endif
                    );
void localtext_load_defaults();
bool localtext_load_file(const LCHAR *file);
bool localtext_save_file(const LCHAR *file);
const char *localtext_get(int idx);
const char *localtext_get_fmt(int idx, const char *fmt, ...);
const char *localtext_get_name(int idx);
bool localtext_set(int idx, const char *name, const char *text);
bool localtext_set_name(const char *name, const char *text);

#ifdef LOCALTEXT
#undef LOCALTEXT
#endif
#define LOCALTEXT		localtext_get
#define LOCALTEXT_FMT	localtext_get_fmt

#ifdef __cplusplus
}
#endif
