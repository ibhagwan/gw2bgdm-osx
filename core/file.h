#pragma once
#include "core/TargetOS.h"
#include "core/types.h"

#ifdef TARGET_OS_WIN
#include <wchar.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
// Create a directory
bool create_directory(i8 const* path);

// Returns true if the file exists at the given path.
bool file_exists(i8 const* path);

// Returns the last modify time for the given path.
i64 file_get_time(i8 const* path);

// Reads a file from disk and returns an allocated buffer.
u8* file_read(i8 const* path, u32* size);

// Writes a buffer to disk.
bool file_write(i8 const* path, void const* src, u32 bytes);
    
// Copy a file
bool file_copy(i8 const* from, i8 const* to, bool overwrite);
    
// Enumerate files in a directory
bool directory_enum(char const* dir, void(*cb)(char const *));
    
// Windows UNICODE variant
#ifdef TARGET_OS_WIN
bool create_directoryW(const wchar_t *path);
bool file_existsW(const wchar_t *path);
bool file_writeW(const wchar_t *path, void const* src, u32 bytes);
#endif

#ifdef __cplusplus
}
#endif
