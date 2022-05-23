#include "hook/hook2.h"
#include "meter/imgui_bgdm.h"
#include "meter/KeyBind.h"
#include "meter/dps.h"

EXTERN_C void __fastcall hkWndProc(CpuContext *ctx)
{

#ifdef _WIN64
	HWND &hWnd = (HWND&)(ctx->RCX);
	UINT &msg = (UINT&)(ctx->RDX);
	WPARAM &wParam = (WPARAM&)(ctx->R8);
	LPARAM &lParam = (LPARAM&)(ctx->R9);
#else
	HWND &hWnd = (HWND&)(ctx->RCX);
	UINT &msg = (UINT&)(ctx->RDX);
	WPARAM &wParam = (WPARAM&)(ctx->R8);
	LPARAM &lParam = (LPARAM&)(ctx->R9);
#endif

	bool coolOrig = ImGui_WndProcHandler(hWnd, msg, wParam, lParam);
	if (!coolOrig) {
		msg = WM_NULL;
		return;
	}

	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {

		for (u32 i = 0; i < MAX_PANELS; ++i)
		{
			Panel *panel = g_state.panel_arr[i];
			if (!panel)
				continue;

			KeyBind bind(&panel->bind);
			if (bind.isDown())
				msg = WM_NULL;
		}
	}

	if (dps_is_dragging())
	{
		KeyBind bind(&g_state.bind_wnd_drag);
		if (bind.isDown())
			msg = WM_NULL;
	}
}