#include "core/TargetOS.h"
#include "core/debug.h"
#include "core/logging.h"
#include "core/message.h"
#include "core/time_.h"
#include "core/types.h"
#include "core/network.h"
#include "meter/dps.h"
#include "meter/logging.h"
#include "meter/process.h"
#include "meter/dps.h"
#include "meter/lru_cache.h"
#include "meter/localization.h"
#include "meter/imgui_bgdm.h"
#include "hook/hook2.h"

#define PATH_METER_LOG "bgdm/bgdm.log"

#if defined(_WIN32) || defined (_WIN64)
#include "meter/crypto.h"
#include "meter/updater.h"
#include "hook/hook.h"
#include "meter/d3dx9_hooks.h"
#include "loader/d3d9/d3d9_context.h"
#include <Windows.h>

// The path to the meter.
#define PATH_METER_DLL ".\\bin64\\bgdm.dll"

// Our module handke
HANDLE g_hInstance = 0;

logger_t s_logger;
static logger_filter_t  s_filter;
static logger_formatter_t s_formatter;
static logger_handler_t s_handler;


static bool logger_create(i8* filename)
{
	char path[1024] = {0};
	FILE *logfile;

	StringCchPrintfA(path, ARRAYSIZE(path), "%s\\%s", config_get_my_documentsA(), filename);

	memset(&s_logger, 0, sizeof(s_logger));
	s_logger.name = "bgdm-logger";

	s_filter.minlevel = g_state.log_minlevel;
	s_filter.maxlevel = LOGGING_MAX_LEVEL;
	s_filter.next = NULL;

	s_formatter.datefmt = "%Y-%m-%dT%H:%M:%SZ";
	s_formatter.fmt =
		"%(asctime)s	"
		"%(levelname)s	"
		"%(message)s";

	if (strcmp(g_state.log_filemode, "a") == 0)
		logfile = fopen(path, "a");
	else
		logfile = fopen(path, "w");

	s_handler.name = "bgdm-file";
	s_handler.file = logfile != NULL ? logfile : stdout;
	s_handler.filter = &s_filter;
	s_handler.formatter = &s_formatter;
	s_handler.next = NULL;

	s_logger.filter = &s_filter;
	s_logger.handler = &s_handler;

	logging_setlogger(&s_logger);

	if (!logfile) {
		DBGPRINT(TEXT("[logger] Unable to open %S for writing"), path);
	} else {
		DBGPRINT(TEXT("[logger] Successfully opened %S for writing [level=%d]"), path, g_state.log_minlevel);
	}

	return true;
}
#endif  // defined(_WIN32) || defined (_WIN64)

bool meter_init()
{
	static bool m_init = false;
	if (m_init) return false;

	config_get_state();
	logger_create(PATH_METER_LOG);
	localtext_init(g_state.lang_dir, g_state.lang_file);
	mumble_link_create();
	m_init = true;
	return true;
}

#if defined(TARGET_OS_WIN)
void meter_create(void* device, void* viewport, HWND hwnd)
{
	g_hWnd = hwnd;
	meter_init();

	// Initialize hooking.
	if (MH_Initialize() != MH_OK)
	{
		MessageBox(0, TEXT("hook lib failed to initialize."), TEXT("Error"), MB_OK);
		return;
	}
	if (hookInit() != ERROR_SUCCESS)
	{
		MessageBox(0, TEXT("hook lib failed to initialize."), TEXT("Error"), MB_OK);
		return;
	}

	time_create();
	crypto_create();
	lru_create();
	autolock_init();
	d3dx9_create((LPDIRECT3DDEVICE9)device, (D3DVIEWPORT9*)viewport, hwnd);
	renderer_create(device, viewport, hwnd);
	process_create("Guild Wars 2", "ArenaNet_Dx_Window_Class");
	network_create(g_state.network_addr, g_state.network_port);

	updater_create(PATH_METER_DLL, g_state.dbg_ver);
	if (g_state.dbg_ver == 0 && updater_get_cur_version() != 0) {
		g_state.dbg_ver = updater_get_cur_version();
		config_set_int(CONFIG_SECTION_DBG, "Version", g_state.dbg_ver);
	}

	dps_create();

	if (!ImGui_Init(hwnd, device, viewport, dps_get_game_ptrs())) {
		LOG_ERR("ImGui init failed");
		g_state.renderImgui = false;
	}

	LOG_INFO("BGDM loaded successfully [version=%x]", updater_get_cur_version());
}

void meter_create_ex(void* context)
{
	if (!context) return;
	PD3D9Context pD3Dcontext = (PD3D9Context)context;
	meter_init();
	d3dx9_create_ex(pD3Dcontext);
	meter_create(pD3Dcontext->device_proxy, pD3Dcontext->p_viewport, pD3Dcontext->hWnd);
	imgui_has_colorblind_mode = 1;
}
#endif

void meter_destroy(void)
{
#if defined(TARGET_OS_WIN)
	LOG_INFO("BGDM unload [version=%x]", updater_get_cur_version());

	// Release our hooks.
	d3dx9_destroy();
	MH_Uninitialize();
#endif
    hookFini();
	ImGui_Shutdown();
	dps_destroy();
#if defined(TARGET_OS_WIN)
	updater_destroy();
    crypto_destroy();
#endif
    network_destroy();
	autolock_free();
	lru_destroy();
	mumble_link_destroy();
}

bool meter_present(bool check_for_update, bool use_sprite)
{
	// Update timing.
	i64 now = time_get();

	for (;;)
	{
		// Receive queued network messages.
		static i8 buf[UDP_MAX_PKT_SIZE];
		i32 len = network_recv(buf, sizeof(buf), NULL, NULL);
		if (len <= 0)
		{
			break;
		}

		u32 type = *(u32*)buf;
		switch (type)
		{

			case MSG_TYPE_SERVER_DAMAGE_UTF8:
			{
				// Received a damage update message from the server.
				if (len != sizeof(MsgServerDamageUTF8))
				{
					break;
				}

				dps_handle_msg_damage_utf8((MsgServerDamageUTF8*)buf);
			} break;

#if defined(TARGET_OS_WIN)
			case MSG_TYPE_SERVER_UPDATE_BEGIN:
			{
				// Received an update notification from the server.
				if (len != sizeof(MsgServerUpdateBegin))
				{
					break;
				}

				updater_handle_msg_begin((MsgServerUpdateBegin*)buf);
			} break;

			case MSG_TYPE_SERVER_UPDATE_PIECE:
			{
				LOG(9, "[server] MSG_TYPE_SERVER_UPDATE_PIECE [len=%d]", len);
				// Received an update piece from the server.
				if (len != sizeof(MsgServerUpdatePiece) &&
					len != sizeof(MsgServerUpdatePieceNoFrag))
				{
					break;
				}

				updater_handle_msg_piece((MsgServerUpdatePiece*)buf);
			} break;
#endif
		}
	}

	// Start a new Imgui Frame
	ImGui_NewFrame();


	// Update the meter.
	dps_update(now);

	// Draw timing and version information.
	f32 ms = (time_get() - now) / 1000.0f;

	if (!g_state.renderImgui)
		dps_version(ms);

	// Render experimental ImGui
	ImGui_Render(ms, now);


#if defined(TARGET_OS_WIN)
	if (!check_for_update)
		return false;
    
	// Check and handle updates.
	bool ret = updater_update(now);
	if (ret && g_state.dbg_ver && updater_get_srv_version()) {
		config_set_int(CONFIG_SECTION_DBG, "Version", updater_get_srv_version());
	}
	return ret;
#else
    return false;
#endif
}

bool meter_update(void)
{
	return meter_present(true, imgui_has_colorblind_mode ? false : true);
}

bool meter_update_ex(void)
{
	return meter_present(true, false);
}

#if defined(TARGET_OS_WIN)
bool meter_was_updated()
{
	i64 now = time_get();
	return updater_update(now);
}

void meter_reset_init(void)
{
    ImGui_ImplDX9_ResetInit();
    d3dx9_unload();
}

void meter_reset_post(void)
{
    ImGui_ImplDX9_ResetPost();
    d3dx9_reset();
}

BOOL WINAPI DllMain(HANDLE instance, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
	{
		DBGPRINT(TEXT("DLL_PROCESS_ATTACH"));
		g_hInstance = instance;
	} break;

	case DLL_PROCESS_DETACH:
	{
		DBGPRINT(TEXT("DLL_PROCESS_DETACH"));
	} break;
	}

	return TRUE;
}
#endif  // defined(_WIN32) || defined (_WIN64)
