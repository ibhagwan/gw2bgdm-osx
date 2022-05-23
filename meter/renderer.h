#pragma once
#include "core/types.h"
#pragma warning (push)
#pragma warning (disable: 4201)
#include "dxsdk\d3dx9.h"
#pragma warning (pop)

// Returns a 32-bit RGBA color.
#define RGBA(r, g, b, a) ((u32)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff)))

// Common colors.
#define COLOR_WHITE		RGBA(255, 255, 255, 255)
#define COLOR_GRAY		RGBA(255, 255, 255, 150)
#define COLOR_BLACK		RGBA(0, 0, 0, 255)
#define COLOR_RED		RGBA(255, 0, 0, 255)
#define COLOR_GREEN		RGBA(0, 255, 0, 255)
#define COLOR_BLUE		RGBA(0, 100, 255, 255)
#define COLOR_PINK		RGBA(255, 0, 255, 255)
#define COLOR_YELLOW	RGBA(255, 255, 0, 255)
#define COLOR_CYAN		RGBA(0, 255, 255, 255)
#define COLOR_ORANGE	RGBA(255, 175, 0, 255)
#define COLOR_PURPLE	RGBA(150, 0, 255, 255)
#define COLOR_LIGHT_BLUE	RGBA(65, 150, 190, 255)
#define COLOR_DARK_GREEN	RGBA(0, 175, 80, 255)
#define COLOR_LIGHT_PURPLE	RGBA(185, 95, 250, 255)
#define COLOR_BLACK_OVERLAY(a) D3DCOLOR_ARGB(a, 32, 32, 32)

#ifdef __cplusplus
extern "C" {
#endif

// Creates the renderer.
void renderer_create(void* device, void *viewport, HWND hwnd);

// Destroys the renderer.
void renderer_destroy(void);

bool renderer_sprite_begin();
bool renderer_sprite_end();

// Draws a filled rectangle.
void renderer_draw_rect(i32 x, i32 y, i32 w, i32 h, u32 color, RECT* outRect);

// Draws a rectangle.
void renderer_draw_frame(f32 x, f32 y, f32 w, f32 h, u32 color);

// Draws a circle.
void renderer_draw_circle(i32 x, i32 y, f32 r, u32 color);

// Draws text.
void renderer_draw_text(i32 x, i32 y, u32 color, i8 const* str, bool useFont7);
void renderer_draw_textW(i32 x, i32 y, u32 color, wchar_t const* str, bool useFont7);

// Returns the average width of a character in the font.
i32 renderer_get_font7_x(void);
i32 renderer_get_fontu_x(void);

// Returns the average height of a character in the font.
i32 renderer_get_font7_y(void);
i32 renderer_get_fontu_y(void);

// Returns the x-resolution of the backbuffer.
i32 renderer_get_res_x(void);

// Returns the y-resolution of the backbuffer.
i32 renderer_get_res_y(void);


//  IDirect3DDevice9*
IDirect3DDevice9* renderer_get_dev();

// IDirect3DDevice9::GetViewport
i32 renderer_get_vp(D3DVIEWPORT9 *vp);

// Draws a formatted string.
void renderer_printf(bool useFont7, i32 x, i32 y, u32 color, i8 const * fmt, ...);
void renderer_printfW(bool useFont7, i32 x, i32 y, u32 color, wchar_t const * fmt, ...);

// Performs pre-reset operations.
void renderer_reset_init(void);

// Performs post-reset operations.
void renderer_reset_post(void);

// Updates the renderer.
void renderer_update();

// Take screenshot
void renderer_screenshot();

#ifdef __cplusplus
}
#endif