#include "core/debug.h"
#include "process.h"
#include <Windows.h>
#include <Strsafe.h>
#pragma warning (push)
#pragma warning (disable: 4201)
#include <winternl.h>
#pragma warning (pop)
#include <TlHelp32.h>
#include <stdlib.h>

static HANDLE g_process;
static HANDLE g_window;
static DWORD g_id;

void process_find_window(i8 const* wnd, i8 const* cls)
{
	static i8 swnd[256] = { 0 };
	static i8 scls[256] = { 0 };

	if (wnd) StringCchCopyA(swnd, sizeof(swnd), wnd);
	if (cls) StringCchCopyA(scls, sizeof(scls), cls);

	if (swnd[0] == 0 || scls[0] == 0)
		return;

	g_window = FindWindowA(scls, swnd);
	if (!g_window)
	{
		DBGPRINT(TEXT("FindWindow(%S, %S) failed, error 0x%08x"), swnd, scls, GetLastError());
	}
	else
	{
		DBGPRINT(TEXT("[hWindow=0x%llX]"), g_window, g_id);
	}
}

void process_create(i8 const* wnd, i8 const* cls)
{
	g_id = GetCurrentProcessId();
	g_process = GetCurrentProcess();
	DBGPRINT(TEXT("[process_id=%08d]"), g_id);

	// Get the game window handle
	process_find_window(wnd, cls);
}

u64 process_follow(i8 const* pattern, i8 const* mask, i32 off)
{
	u64 addr = process_scan(pattern, mask);
	if (addr == 0)
	{
		return 0;
	}

	u32 offset = process_read_u32(addr + off);
	return (addr + offset + off + 4);
}

void *process_get_handle(void)
{
	return g_process;
}

u32 process_get_id(void)
{
	return g_id;
}

void *process_get_window(void)
{
	if (!g_window)
		process_find_window(NULL, NULL);

	return g_window;
}

bool process_read(u64 base, void *dst, u32 bytes)
{
	if (!base || !dst)
		return false;

	if (ReadProcessMemory(g_process, (void*)base, dst, bytes, 0) == FALSE)
	{
		memset(dst, 0, bytes);
		return false;
	}

	return true;
}

u64 process_scan(void const* sig, i8 const* msk)
{
	u32 bytes = 0;
	while (msk[bytes])
	{
		++bytes;
	}

	u64 addr = 0;
	MEMORY_BASIC_INFORMATION mbi;

	while (VirtualQuery((void*)addr, &mbi, sizeof(mbi)))
	{
		addr += mbi.RegionSize;

		if (mbi.RegionSize < bytes ||
			mbi.State != MEM_COMMIT ||
			mbi.Protect != PAGE_EXECUTE_READ)
		{
			continue;
		}

		u8* s1 = (u8*)sig;
		u8* buf = (u8*)mbi.BaseAddress;
		u64 end = mbi.RegionSize - bytes;

		for (u64 i = 0; i < end; ++i)
		{
			bool match = true;

			for (u64 j = 0; j < bytes; ++j)
			{
				if (msk[j] == 'x' && buf[i + j] != s1[j])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				DBGPRINT(TEXT("pattern match [bytes=%d] [mask=%S]\r\n"), bytes, msk);
				return (u64)mbi.BaseAddress + i;
			}
		}
	}

	return 0;
}


// taken from: http://www.geeksforgeeks.org/pattern-searching-set-7-boyer-moore-algorithm-bad-character-heuristic/
/* Program for Bad Character Heuristic of Boyer Moore String Matching Algorithm */

#include <stdint.h>
//#include <climits>
//#include <cstdint>

#define NO_OF_CHARS 256

// The preprocessing function for Boyer Moore's bad character heuristic
static void badCharHeuristic(const uint8_t *str, size_t size, int badchar[NO_OF_CHARS])
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
static const uint8_t *boyermoore(const uint8_t *txt, const size_t n, const uint8_t *pat, const size_t m)
{
    if (m > n || m < 1)
        return NULL;

    int badchar[NO_OF_CHARS];

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
            // HACKLIB EDIT BEGIN
            // We only want the first occurence of the pattern, so return immediatly.
            return txt + s;

            //printf("\n pattern occurs at shift = %d", s);

            /* Shift the pattern so that the next character in text
            aligns with the last occurrence of it in pattern.
            The condition s+m < n is necessary for the case when
            pattern occurs at the end of text */
            //s += (s + m < n) ? m-badchar[txt[s + m]] : 1;
            // HACKLIB EDIT END
        }
        else
        {
            /* Shift the pattern so that the bad character in text
            aligns with the last occurrence of it in pattern. The
            max function is used to make sure that we get a positive
            shift. We may get a negative shift if the last occurrence
            of bad character in pattern is on the right side of the
            current character. */
            s += max(1, j - badchar[txt[s + j]]);
        }
    }

    return NULL;
}

// End of third party code.



uintptr_t process_follow_rel_addr(uintptr_t addr)
{
	// Hardcoded 32-bit dereference to make it work with 64-bit code.
	u32 offset = process_read_u32(addr);
	return (addr + offset + 4);
	//return *(int32_t*)addr + addr + 4;
}


uintptr_t process_read_rel_addr(uintptr_t adr)
{
	// Hardcoded 32-bit dereference to make it work with 64-bit code.
	u32 reladr = process_read_i32(adr);
	return  adr + reladr + 4;
}

u64 process_find_str_ref(void const* str)
{
	if (!str)
		return 0;

	size_t bytes = strlen(str);
	uintptr_t strFound = 0;
	uintptr_t refFound = 0;

	if (bytes) {

		u64 addr = 0;
		MEMORY_BASIC_INFORMATION mbi;
		while (VirtualQuery((void*)addr, &mbi, sizeof(mbi)))
		{
			addr += mbi.RegionSize;

			if (mbi.RegionSize < bytes ||
				mbi.Protect != PAGE_READONLY) {
				continue;
			}

			strFound = (uintptr_t)boyermoore((const uint8_t*)mbi.BaseAddress, mbi.RegionSize, (const uint8_t*)str, bytes+1);
			if (strFound)
				break;
		}
	}


	if (strFound) {

		DBGPRINT(TEXT("string match [str=%S] [str_addr=0x%016llX]\r\n"), str, strFound);

		u64 addr = 0;
		MEMORY_BASIC_INFORMATION mbi;
		while (VirtualQuery((void*)addr, &mbi, sizeof(mbi)))
		{
			addr += mbi.RegionSize;

			if ( /*!(mbi.Protect & PAGE_READONLY) &&*/
				 !(mbi.Protect & PAGE_EXECUTE_READ) ) {
				continue;
			}

			//uintptr_t sptr = (uintptr_t)strFound;
			//refFound = boyermoore((const uint8_t*)mbi.BaseAddress, mbi.RegionSize, (const uint8_t*)&sptr, sizeof(uintptr_t));

			uintptr_t endAdr = (uintptr_t)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
			for (uintptr_t adr = (uintptr_t)mbi.BaseAddress; adr < endAdr; adr++)
			{
				if (process_follow_rel_addr(adr) == strFound)
				{
					// Prevent false poritives by checking if the reference occurs in a LEA instruction.
					uint16_t opcode = *(uint16_t*)(adr - 3);
					if (opcode == 0x8D48 || opcode == 0x8D4C) {
						refFound = adr;
						break;
					}
				}
			}
		}
	}

	if (refFound) {
		DBGPRINT(TEXT("string ref match [str=%S] [ref_addr=0x%016llX]\r\n"), str, refFound);
	}

	return (u64)refFound;
}