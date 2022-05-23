#include "core/debug.h"
#include "renderer.h"
#pragma warning (push)
#pragma warning (disable: 4201)
#include "dxsdk\d3dx9.h"
#pragma warning (pop)
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <Strsafe.h>
#include "meter\dps.h"
#include "meter\resource.h"

// The name of the font to add to the system.
#define FONT_NAME_TFONT "tfont"
#define FONT_NAME_ENVYA "Envy Code A"

// The path to the font to add to the system.
//#define FONT_PATH ".\\bin64\\tfont.fon"

// D3D9 interface and state.
static IDirect3DDevice9* g_dev = NULL;
static D3DVIEWPORT9 *g_view = NULL;
static HANDLE g_hFontT = NULL;
static HANDLE g_hFontE = NULL;
static ID3DXFont* g_font_7;
static ID3DXFont* g_font_u;
static ID3DXLine* g_line;
static LPD3DXSPRITE g_sprite = NULL;
static bool g_in_sprite = false;
static bool g_in_scene = false;

// Renderer state.
static i32 g_font7_x;
static i32 g_font7_y;
static i32 g_fontu_x;
static i32 g_fontu_y;
static i32 g_res_x;
static i32 g_res_y;
static bool g_is_init;


// GetFontResourceInfoW
typedef BOOL(WINAPI *GETFONTRESOURCEINFOW)(LPCWSTR, LPDWORD, LPVOID, DWORD);

BOOL GetResourceInfo(LPCWSTR pFontPath, LPWSTR pOutFaceName, LPDWORD pdwSize)
{
	/*wchar_t fontPath[MAX_PATH] = { 0 };
	if (!GetFullPathNameW(pFontPath, ARRAYSIZE(fontPath), fontPath, NULL))
		return FALSE;*/

	BOOL bRet = FALSE;
	HINSTANCE hGdi32 = LoadLibraryA("gdi32.dll");
	if (!hGdi32)
		return FALSE;

	GETFONTRESOURCEINFOW GetFontResourceInfoW =
		(GETFONTRESOURCEINFOW)GetProcAddress(hGdi32, "GetFontResourceInfoW");
	if (!GetFontResourceInfoW)
		return FALSE;

	//EXTLOGFONTW elf = { 0 };
	//DWORD dwSize = sizeof(elf);
	//SecureZeroMemory(&elf, sizeof(elf));
	LOGFONTW alf[10] = { 0 };
	DWORD dwSize = sizeof(alf);
	SecureZeroMemory(&alf, dwSize);
	/*SecureZeroMemory(pOutFaceName, *pdwSize);
	if (GetFontResourceInfoW(pFontPath, pdwSize, pOutFaceName, 1))
	{
		bRet = TRUE;
	}*/
	if (GetFontResourceInfoW(pFontPath, &dwSize, &alf, 2))
	{
		for (INT i = 0; i < ARRAYSIZE(alf); ++i)
		{
			if (alf[i].lfFaceName[0] == 0)
				break;

			SecureZeroMemory(pOutFaceName, *pdwSize);
			StringCbCopyW(pOutFaceName, *pdwSize, alf[i].lfFaceName);
		}
	}
	else {
		DBGPRINT(TEXT("GetFontResourceInfoW(%s) failed, error 0x%08x"), pFontPath, GetLastError());
	}
	FreeLibrary(hGdi32);
	return bRet;
}

HANDLE AddResourceFont(LPCTSTR ResID, DWORD *Installed)
{
	if (Installed) *Installed = 0;

	HMODULE hMod = g_hInstance;
	DWORD nFonts = 0;

	HRSRC hRes = FindResource(hMod, ResID, RT_FONT);
	if (!hRes)
	{
		DBGPRINT(TEXT("FindResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return NULL;
	}

	DWORD len = SizeofResource(hMod, hRes);
	if ((len == 0) && (GetLastError() != 0))
	{
		DBGPRINT(TEXT("SizeofResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return NULL;
	}

	HGLOBAL hMem = LoadResource(hMod, hRes);
	if (!hMem)
	{
		DBGPRINT(TEXT("LoadResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return NULL;
	}

	PVOID pFontData = LockResource(hMem);
	if (!pFontData)
	{
		DBGPRINT(TEXT("LockResource(%d) failed, error 0x%08x"), ResID, GetLastError());
		return NULL;
	}

	HANDLE hFont = AddFontMemResourceEx(pFontData, len, NULL, &nFonts);
	if (!hFont) {
		DBGPRINT(TEXT("AddFontMemResourceEx(%p, %d) failed, error 0x%08x"), pFontData, len, GetLastError());
	}
	else {
		DBGPRINT(TEXT("AddFontMemResourceEx(%p, %d) success [resId=%d] [count=%d]"), pFontData, len, ResID, nFonts);
	}
	if (Installed) *Installed = nFonts;
	return hFont;
}


HRESULT load_font(
	LPDIRECT3DDEVICE9       pDevice,
	INT                     Height,
	UINT                    Width,
	/*UINT                    Weight,*/
	LPCSTR					pFaceName,
	LPCSTR					pFontPath,
	LPD3DXFONT*             ppFont)
{
	static wchar_t fontPath[MAX_PATH];
	static wchar_t fontName[MAX_PATH];

	SecureZeroMemory(fontName, sizeof(fontName));
	SecureZeroMemory(fontPath, sizeof(fontPath));

	size_t pathLen = pFontPath ? lstrlenA(pFontPath) : 0;
	size_t nameLen = pFaceName ? lstrlenA(pFaceName) : 0;

	if (!nameLen)
		pFaceName = FONT_NAME_TFONT;

	if (nameLen)
		StringCchPrintfW(fontName, ARRAYSIZE(fontName), L"%S", pFaceName);

	bool resourceLoaded = false;
	if (pathLen) {
		StringCchPrintfW(fontPath, ARRAYSIZE(fontPath), L"%S", pFontPath);
		if (AddFontResourceEx(fontPath, FR_PRIVATE, 0)) {
			resourceLoaded = true;
		}
		else {
			if (!resourceLoaded && !strchr(pFontPath, '.')) {
				StringCchPrintfW(fontPath, ARRAYSIZE(fontPath), L"%S.fon", pFontPath);
				if (AddFontResourceEx(fontPath, FR_PRIVATE, 0)) {
					resourceLoaded = true;
				}
			}
			if (!resourceLoaded && !strchr(pFontPath, '\\') && !strchr(pFontPath, '/')) {
				StringCchPrintfW(fontPath, ARRAYSIZE(fontPath), L".\\bin64\\%S", pFontPath);
				if (AddFontResourceEx(fontPath, FR_PRIVATE, 0)) {
					resourceLoaded = true;
				}
			}
			if (!resourceLoaded && !strchr(pFontPath, '.')) {
				StringCchPrintfW(fontPath, ARRAYSIZE(fontPath), L".\\bin64\\%S.fon", pFontPath);
				if (AddFontResourceEx(fontPath, FR_PRIVATE, 0)) {
					resourceLoaded = true;
				}
			}
		}
	}
	/*if (resourceLoaded) {
		static wchar_t tmpFontName[MAX_PATH];
		DWORD dwSize = sizeof(tmpFontName);
		if (GetResourceInfo(fontPath, tmpFontName, &dwSize)) {
			SecureZeroMemory(fontName, sizeof(fontName));
			StringCchCopyW(fontName, ARRAYSIZE(fontName), tmpFontName);
		}
	}*/
	
	HRESULT hr = D3DXCreateFont(
		pDevice, Height, Width, FW_NORMAL, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName, ppFont);

	if (FAILED(hr))
	{
		hr = D3DXCreateFont(
			pDevice, 7, 0, FW_NORMAL, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT(FONT_NAME_TFONT), ppFont);
	}

	return hr;
}


void renderer_create(void* device, void *viewport, HWND hwnd)
{
	g_dev = (IDirect3DDevice9*)device;

	DBGPRINT(TEXT("[device=0x%p] [vp=0x%p]"), device, viewport);

	// Load our default fonts into memory
	g_hFontT = AddResourceFont(MAKEINTRESOURCE(IDR_RT_FONT_TFONT), NULL);
	g_hFontE = AddResourceFont(MAKEINTRESOURCE(IDR_RT_FONT_ENVYA), NULL);

	if (g_state.font_path && lstrlenA(g_state.font_path) > 0 || g_state.font_width < 7) {
		if (FAILED(load_font(g_dev, g_state.font_height, g_state.font_width,
			g_state.font_name, g_state.font_path, &g_font_7)))
			return;
	}
	else {
		if (FAILED(load_font(g_dev, 7, 0, FONT_NAME_TFONT, NULL, &g_font_7)))
			return;
	}

	if (FAILED(load_font(g_dev, g_state.font_height, g_state.font_width,
		g_state.font_name, g_state.font_path, &g_font_u)))
		return;

	if (FAILED(D3DXCreateLine(g_dev, &g_line)))
	{
		DBGPRINT(TEXT("D3DXCreateLine() failed, error 0x%08x"), GetLastError());
		return;
	}

	if (FAILED(D3DXCreateSprite(g_dev, &g_sprite))) {
		DBGPRINT(TEXT("D3DXCreateSprite() failed, error 0x%08x"), GetLastError());
	}

	TEXTMETRIC tm;
	if (g_font_7->lpVtbl->GetTextMetrics(g_font_7, &tm))
	{
		g_font7_x = (i32)tm.tmAveCharWidth;
		g_font7_y = (i32)tm.tmHeight;
	}

	memset(&tm, 0, sizeof(tm));
	if (g_font_u->lpVtbl->GetTextMetrics(g_font_u, &tm))
	{
		g_fontu_x = (i32)tm.tmAveCharWidth;
		g_fontu_y = (i32)tm.tmHeight;
	}

	renderer_update();

	// On create only, overwrite the screen resolution values
	// this is a workaround to the F9 reload buggy resolution
	if (viewport) {
		D3DVIEWPORT9 *vp = (D3DVIEWPORT9*)viewport;
		g_res_x = (i32)vp->Width;
		g_res_y = (i32)vp->Height;
		g_view = (D3DVIEWPORT9*)viewport;
		DBGPRINT(TEXT("ViewPort(%d,%d)"), (i32)vp->Width, (i32)vp->Height);
	}

	g_is_init = true;
}

void renderer_destroy(void)
{
	if (g_sprite)
	{
		g_sprite->lpVtbl->Release(g_sprite);
	}

	if (g_line)
	{
		g_line->lpVtbl->Release(g_line);
	}

	if (g_font_7)
	{
		g_font_7->lpVtbl->Release(g_font_7);
	}

	if (g_font_u)
	{
		g_font_u->lpVtbl->Release(g_font_u);
	}

	if (g_hFontT)
		RemoveFontMemResourceEx(g_hFontT);

	if (g_hFontE)
		RemoveFontMemResourceEx(g_hFontE);
}


bool renderer_sprite_begin()
{
	LPD3DXSPRITE m_pSprite = g_sprite;
	//if (FAILED(D3DXCreateSprite(g_dev, &m_pSprite)))
	//	return false;
	if (m_pSprite) {
		if (SUCCEEDED(g_dev->lpVtbl->BeginScene(g_dev))) {
			g_in_scene = true;
			if (SUCCEEDED(m_pSprite->lpVtbl->Begin(m_pSprite, D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE | D3DXSPRITE_SORT_DEPTH_BACKTOFRONT))) {
				g_in_sprite = true;
				g_sprite = m_pSprite;
				return true;
			}
		}
	}
	return false;
}

bool renderer_sprite_end()
{
	bool bRet = false;
	LPD3DXSPRITE m_pSprite = g_sprite;

	assert(g_in_scene == true && g_in_sprite == true);

	if (m_pSprite && g_in_sprite) {
		m_pSprite->lpVtbl->End(m_pSprite);
		g_in_sprite = false;
		bRet = true;
	}
	if (g_in_scene) {
		g_dev->lpVtbl->EndScene(g_dev);
		g_in_scene = false;
	}

	return bRet;
}

void renderer_draw_text(i32 x, i32 y, u32 color, i8 const* str, bool useFont7)
{
	RECT r;
	ID3DXFont* font = useFont7 ? g_font_7 : g_font_u;

	r.left = x;
	r.top = y;

	font->lpVtbl->DrawTextA(font, g_in_sprite ? g_sprite : 0, str, -1, &r, DT_NOCLIP, color);
}

void renderer_draw_textW(i32 x, i32 y, u32 color, wchar_t const* str, bool useFont7)
{
	RECT r;
	ID3DXFont* font = useFont7 ? g_font_7 : g_font_u;

	r.left = x;
	r.top = y;

	font->lpVtbl->DrawTextW(font, g_in_sprite ? g_sprite : 0, str, -1, &r, DT_NOCLIP, color);
}

void renderer_draw_rect(i32 x, i32 y, i32 w, i32 h, u32 color, RECT* outRect)
{
	g_line->lpVtbl->SetWidth(g_line, (FLOAT)w);

	D3DXVECTOR2 line[2];
	line[0].x = (f32)(x + w / 2);
	line[0].y = (f32)y;
	line[1].x = line[0].x;
	line[1].y = (f32)y + h;

	if (outRect) {
		outRect->left = x;
		outRect->top = y;
		outRect->right = x + w;
		outRect->bottom = y + h;
	}

	g_line->lpVtbl->Begin(g_line);
	g_line->lpVtbl->Draw(g_line, line, 2, color);
	g_line->lpVtbl->End(g_line);
}

typedef struct _VERTEX_2D_DIF { // transformed colorized
	float x, y, z, rhw;
	D3DCOLOR color;
} VERTEX_2D_DIF;

void renderer_draw_frame(f32 x, f32 y, f32 w, f32 h, u32 color)
{
	static const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

#pragma warning( push )  
#pragma warning( disable : 4204 )  
	VERTEX_2D_DIF verts[] = {
		{ x, y, 0, 1, color },
		{ x + w, y, 0, 1, color },
		{ x + w, y + h, 0, 1, color },
		{ x, y + h, 0, 1, color },
		{ x, y, 0, 1, color }
	};
#pragma warning( pop )  

	g_dev->lpVtbl->SetFVF(g_dev, FVF);
	g_dev->lpVtbl->DrawPrimitiveUP(g_dev, D3DPT_LINESTRIP, 4, &verts, sizeof(VERTEX_2D_DIF));
}

void renderer_draw_circle(i32 x, i32 y, f32 r, u32 color)
{

#define CIRCLE_RESOLUTION 64

	static const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

	VERTEX_2D_DIF verts[CIRCLE_RESOLUTION + 1];

	for (int i = 0; i < CIRCLE_RESOLUTION + 1; i++)
	{
		verts[i].x = (f32)((f32)x + r*cos(D3DX_PI*(i / (CIRCLE_RESOLUTION / 2.0f))));
		verts[i].y = (f32)((f32)y + r*sin(D3DX_PI*(i / (CIRCLE_RESOLUTION / 2.0f))));
		verts[i].z = 0;
		verts[i].rhw = 1;
		verts[i].color = color;
	}

	g_dev->lpVtbl->SetFVF(g_dev, FVF);
	g_dev->lpVtbl->DrawPrimitiveUP(g_dev, D3DPT_TRIANGLEFAN, CIRCLE_RESOLUTION - 1, &verts, sizeof(VERTEX_2D_DIF));
}

i32 renderer_get_font7_x(void)
{
	return g_font7_x;
}

i32 renderer_get_font7_y(void)
{
	return g_font7_y;
}

i32 renderer_get_fontu_x(void)
{
	return g_fontu_x;
}

i32 renderer_get_fontu_y(void)
{
	return g_fontu_y;
}

i32 renderer_get_res_x(void)
{
	return g_res_x;
}

i32 renderer_get_res_y(void)
{
	return g_res_y;
}

IDirect3DDevice9* renderer_get_dev()
{
	return g_dev;
}

i32 renderer_get_vp(D3DVIEWPORT9* vp)
{
	if (!vp)
		return 1;

	// IDirect3DDevice9::GetViewport
	HRESULT hr = g_dev->lpVtbl->GetViewport(g_dev, vp);
	if (SUCCEEDED(hr))
	{
	}

	return hr;
}

void renderer_printf(bool useFont7, i32 x, i32 y, u32 color, i8 const * fmt, ...)
{
	i8 buf[1024];

	va_list arg;
	va_start(arg, fmt);
	StringCchVPrintfA(buf, sizeof(buf) - 1, fmt, arg);
	va_end(arg);

	renderer_draw_text(x, y, color, buf, useFont7);
}

void renderer_printfW(bool useFont7, i32 x, i32 y, u32 color, wchar_t const * fmt, ...)
{
	wchar_t buf[1024];

	va_list arg;
	va_start(arg, fmt);
	StringCchVPrintfW(buf, ARRAYSIZE(buf) - 1, fmt, arg);
	va_end(arg);

	renderer_draw_textW(x, y, color, buf, useFont7);
}

void renderer_reset_init(void)
{
	DBGPRINT(TEXT("reset_init in_sprite:%d"), g_in_sprite);
	if (g_is_init == false)
	{
		return;
	}

	g_sprite->lpVtbl->OnLostDevice(g_sprite);
	g_font_7->lpVtbl->OnLostDevice(g_font_7);
	g_font_u->lpVtbl->OnLostDevice(g_font_u);
	g_line->lpVtbl->OnLostDevice(g_line);
}

void renderer_reset_post(void)
{
	DBGPRINT(TEXT("reset_post in_sprite:%d"), g_in_sprite);
	if (g_is_init == false)
	{
		return;
	}

	g_sprite->lpVtbl->OnResetDevice(g_sprite);
	g_line->lpVtbl->OnResetDevice(g_line);
	g_font_7->lpVtbl->OnResetDevice(g_font_7);
	g_font_u->lpVtbl->OnResetDevice(g_font_u);
	renderer_update();

	// Update the loader DLL ViewPort
	if (g_view) {
		g_view->Width = g_res_x;
		g_view->Height = g_res_y;
	}
}

void renderer_update()
{
	g_font_u->lpVtbl->PreloadCharacters(g_font_u, 0, 255);
	g_font_7->lpVtbl->PreloadCharacters(g_font_7, 0, 255);

	D3DVIEWPORT9 vp;
	if (SUCCEEDED(g_dev->lpVtbl->GetViewport(g_dev, &vp)))
	{
		DBGPRINT(TEXT("ViewPort(%d,%d)"), (i32)vp.Width, (i32)vp.Height);
		g_res_x = (i32)vp.Width;
		g_res_y = (i32)vp.Height;
	}
}

#ifndef BGDM_NODXSCREENSHOT
void renderer_screenshot()
{

#define SAFE_RELEASE(p)          { if (p) { (p)->lpVtbl->Release(p); (p)=NULL; } }

	static wchar_t FileName[1024];

	// Check if we need to take a screenshot
	static bool is_toggled;

	bool key = is_bind_down(&g_state.bind_screenshot);
	if (key && is_toggled == false) {
		is_toggled = true;
	}
	else if (key == false) {
		is_toggled = false;
	}

	if (!is_toggled)
		return;

	SecureZeroMemory(FileName, sizeof(FileName));
	StringCchCopyW(FileName, ARRAYSIZE(FileName), config_get_my_documents());
	int len = lstrlenW(FileName);
	StringCchPrintfW(&FileName[len], ARRAYSIZE(FileName) - len, L"\\BGDM_Screenshot_");
	len = lstrlenW(FileName);

	time_t seconds;
	struct tm tm;
	//struct timeval tv;
	//gettimeofday(&tv, NULL);
	//seconds = tv.tv_sec;
	time(&seconds);
	localtime_s(&tm, &seconds);
	wcsftime(&FileName[len], ARRAYSIZE(FileName) - len, L"%Y-%m-%d %H.%M.%S", &tm);

	len = lstrlenW(FileName);
	StringCchPrintfW(&FileName[len], ARRAYSIZE(FileName) - len, L".jpg");

	LPDIRECT3DSURFACE9 pBackBuffer;
	HRESULT hr = g_dev->lpVtbl->GetBackBuffer(g_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	if (hr == D3D_OK) {
		D3DXSaveSurfaceToFile(FileName, D3DXIFF_JPG, pBackBuffer, NULL, NULL);
		SAFE_RELEASE(pBackBuffer);
	}
}
#endif