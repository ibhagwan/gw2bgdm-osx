#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h> // for GetCurrentWindow()->TitleBarRect9)
#include <string>
#include <vector>
#include <unordered_map>
#include <float.h>  // FLT_MAX
#include "core/TargetOS.h"
#include "core/types.h"
#include "core/debug.h"
#include "core/file.h"
#include "core/segv.h"
#include "core/input.h"
#include "core/helpers.h"
#include "core/utf.h"
#include "core/utf.hpp"
#include "core/string.hpp"
#ifdef TARGET_OS_WIN
#include <Windows.h>
#include <Strsafe.h>
#include <intrin.h>
#include "dxsdk/d3dx9.h"
#include "imgui_impl_dx9.h"
#include "meter/resource.h"
#elif defined __APPLE__
#include "entry_osx.h"
#include "imgui_osx.h"
#endif
#include "meter/imgui_bgdm.h"
#include "meter/imgui_bgdm_ext.h"
#include "imgui_extras/imgui_colorpicker.hpp"
#include "imgui_extras/imgui_memory_editor.hpp"
#include "offsets/offset_scan.h"
#include "meter/ForeignClass.h"
#include "meter/offsets.h"
#include "meter/updater.h"
#include "meter/process.h"
#include "meter/localdb.h"
#include "meter/localization.h"
#include "meter/lru_cache.h"
#include "meter/autolock.h"
#include "meter/game.h"
#include "meter/game_data.h"
#include "meter/dps.h"
#include "meter/gfx.h"
#include "meter/game_data.h"
#include "simpleini/SimpleIni.h"

#pragma intrinsic(__movsb)

#define IMGUI_WHITE			IM_COL32(255, 255, 255, 255)
#define IMGUI_GRAY			IM_COL32(255, 255, 255, 185)
#define IMGUI_BLACK			IM_COL32(0, 0, 0, 255)
#define IMGUI_RED			IM_COL32(255, 0, 0, 255)
#define IMGUI_GREEN			IM_COL32(0, 255, 0, 255)
#define IMGUI_BLUE			IM_COL32(0, 100, 255, 255)
#define IMGUI_PINK			IM_COL32(255, 0, 255, 255)
#define IMGUI_YELLOW		IM_COL32(255, 255, 0, 255)
#define IMGUI_CYAN			IM_COL32(0, 255, 255, 255)
#define IMGUI_ORANGE		IM_COL32(255, 175, 0, 255)
#define IMGUI_PURPLE		IM_COL32(150, 0, 255, 255)
#define IMGUI_LIGHT_BLUE	IM_COL32(65, 150, 190, 255)
#define IMGUI_DARK_GREEN	IM_COL32(0, 175, 80, 255)
#define IMGUI_DARK_RED		IM_COL32(150, 50, 35, 255)
#define IMGUI_LIGHT_PURPLE	IM_COL32(185, 95, 250, 255)

// ImGui has a bug where if you have noinput specified
// it ignores the size setting of the window
#define IMGUI_FIX_NOINPUT_BUG {		\
	static bool INIT = false;		\
	if (!INIT) {					\
		flags &= !NO_INPUT_FLAG;	\
		INIT = true;				\
	}}


#ifdef __APPLE__
#include <inttypes.h>
#include "osx.common/Logging.h"

#if !(defined BGDM_TOS_COMPLIANT)
static void except_handler(const char *exception)
{
    LogCrit("Unhandled exception in %s", exception);
}
#endif
#endif

static int64_t g_now = 0;

// Color-blind mode input to shader
bool imgui_has_colorblind_mode = 0;
int imgui_colorblind_mode = -1;

#ifdef TARGET_OS_WIN
static LPDIRECT3DDEVICE9  g_pD3D = NULL;
static D3DVIEWPORT9 g_viewPort = { 0 };
static D3DVIEWPORT9* g_pViewPort = NULL;	// d3d9.dll ViewPort

static const int PROF_IMAGES_COUNT = IDB_PNG_PROF_END - IDB_PNG_PROF_START;
static LPDIRECT3DTEXTURE9 PROF_IMAGES[PROF_IMAGES_COUNT] = { 0 };

static const int BUFF_ICONS_COUNT = IDB_PNG_BUFF_END - IDB_PNG_BUFF_START;
static const int STD_BUFF_ICONS_COUNT = IDB_PNG_BUFF_END - IDB_PNG_BUFF_ALACRITY;
static LPDIRECT3DTEXTURE9 BUFF_ICONS[BUFF_ICONS_COUNT] = { 0 };
static LPDIRECT3DTEXTURE9 STD_BUFF_ICONS_C[STD_BUFF_ICONS_COUNT] = { 0 };
static LPDIRECT3DTEXTURE9 STD_BUFF_ICONS_D[STD_BUFF_ICONS_COUNT] = { 0 };

static LPDIRECT3DTEXTURE9 UP_ARROW = NULL;
static LPDIRECT3DTEXTURE9 DOWN_ARROW = NULL;

IMGUI_API LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

static ImFont* defaultFont = NULL;
static ImFont* tinyFont = NULL;
static ImFont* proggyTiny = NULL;
static ImFont* proggyClean = NULL;
static MemoryEditor mem_edit("Memory Editor");

//static const float DEFAULT_ALPHA = 0.4f;
static const int MIN_PANEL_FLAGS = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
static const int NO_INPUT_FLAG = ImGuiWindowFlags_NoInputs;

static bool disable_input = false;
static bool minimal_panels = false;
static bool global_use_tinyfont = false;
#if (defined _DEBUG) || (defined DEBUG)
static bool show_debug = false;
#endif
static bool show_lang_editor = false;
static bool localized_text_reloaded = false;

#if !(defined BGDM_TOS_COMPLIANT)
static const int TINY_FONT_WIDTH = 10;
#endif

static const char* ProfessionName(uint32_t profession, uint32_t hasElite);
static void* ProfessionImageFromPlayer(const struct Character* player);
static ImColor ColorFromCharacter(const Character *c, bool bUseColor);

#if !(defined BGDM_TOS_COMPLIANT)
static void ShowFloatBarMenu();
#endif

static ImU32 prof_colors[3][GW2::PROFESSION_END] = { 0 };

typedef struct BGDMStyle
{
	float global_alpha = 1.0f;
	int col_edit_mode = 0;
	int round_windows = 1;
	int compass_font = 0;
	int hp_percent_mode = 0;
	int hp_fixed_mode = 0;
	int hp_comma_mode = 0;
	int hp_precision_mode = 0;
	ImU32 hp_colors[HP_ROW_END] = { 0 };
} BGDMStyle;

typedef struct BGDMColor
{
	ImU32 col;
	int idx;
	const char *cfg;
	//const char *name;
} BGDMColor;

static BGDMStyle bgdm_style;

enum BGDMColors
{
	BGDM_COL_TEXT1 = 2,
	BGDM_COL_TEXT2,
	BGDM_COL_TEXT3,
	BGDM_COL_TEXT_PLAYER,
	BGDM_COL_TEXT_PLAYER_SELF,
	BGDM_COL_TEXT_PLAYER_DEAD,
	BGDM_COL_TEXT_TARGET,
	BGDM_COL_GRAD_START,
	BGDM_COL_GRAD_END,
	BGDM_COL_TARGET_LOCK_BG,
};

static std::vector<BGDMColor> bgdm_colors_default;
static BGDMColor bgdm_colors[] = {
	{ 0, ImGuiCol_Text,					"ColText" },			//		"Text"},
	{ 0, ImGuiCol_TextDisabled,			"ColTextDisabled" },	//		"Text disabled" },
	{ 0, -1,							"ColText1" },			//		"Text Panel #1" },
	{ 0, -1,							"ColText2" },			//		"Text Panel #2" },
	{ 0, -1,							"ColText3" },			//		"Text Panel #3" },
	{ 0, -1,							"ColTextPlayer" },		//		"Text Player" },
	{ 0, -1,							"ColTextPlayerSelf" },	//		"Text Player (self)" },
	{ 0, -1,							"ColTextPlayerDead" },	//		"Text Player (dead)" },
	{ 0, -1,							"ColTextTarget" },		//		"Text Target" },
	{ 0, -1,							"ColGradStart" },
	{ 0, -1,							"ColGradEnd" },
	{ 0, -1,							"ColBorderTLock" },     //		"Border (target-lock)" },
	{ 0, ImGuiCol_Border,				"ColBorder" },			//		"Border (lines)" },
	{ 0, ImGuiCol_WindowBg,				"ColWindowBG" },		//		"Window BG" },
	{ 0, ImGuiCol_PopupBg,				"ColPopupBG" },         //		"Popup BG" },
	{ 0, ImGuiCol_FrameBg,				"ColFrameBG" },         //		"Frame BG" },
	{ 0, ImGuiCol_FrameBgActive,		"ColFrameBGActive" },	//		"Frame BG (active)" },
	{ 0, ImGuiCol_FrameBgHovered,		"ColFrameBGHover" },	//		"Frame BG (hover)" },
	{ 0, ImGuiCol_TitleBg,				"ColTitleBG" },         //		"Title BG" },
	{ 0, ImGuiCol_TitleBgCollapsed,		"ColTitleBGCollapsed" }, //	"Title BG (collapsed)" },
	{ 0, ImGuiCol_TitleBgActive,		"ColTitleBGActive" },	//		"Title BG (active)" },
	{ 0, ImGuiCol_MenuBarBg,			"ColMenuBarBG" },		//		"Menubar BG" },
	{ 0, ImGuiCol_ComboBg,				"ColComboBG" },         //		"Combobox BG" },
	{ 0, ImGuiCol_ScrollbarBg,			"ColScrollBG" },		//		"Scrollbar BG" },
	{ 0, ImGuiCol_ScrollbarGrab,		"ColScrollGrab" },		//		"Scrollbar grab" },
	{ 0, ImGuiCol_ScrollbarGrabHovered,	"ColScrollGrabHover" }, //		"Scrollbar grab (hover)" },
	{ 0, ImGuiCol_ScrollbarGrabActive,	"ColScrollGrabActive" },//		"Scrollbar grab (active)" },
	{ 0, ImGuiCol_SliderGrab,			"ColSliderGrab" },		//		"Slider grab" },
	{ 0, ImGuiCol_SliderGrabActive,		"ColSliderGrabActive" },//		"Slider grab (active)" },
	{ 0, ImGuiCol_Button,				"ColButton" },			//		"Button" },
	{ 0, ImGuiCol_ButtonActive,			"ColButtonActive" },	//		"Button (active)" },
	{ 0, ImGuiCol_ButtonHovered,		"ColButtonHover" },     //		"Button (hover)" },
	{ 0, ImGuiCol_Header,				"ColHeader" },			//		"Header" },
	{ 0, ImGuiCol_HeaderActive,			"ColHeaderActive" },	//		"Header (active)" },
	{ 0, ImGuiCol_HeaderHovered,		"ColHeaderHover" },     //		"Header (hover)" },
	{ 0, ImGuiCol_Column,				"ColColumn" },			//		"Column" },
	{ 0, ImGuiCol_ColumnActive,			"ColColumnActive" },	//		"Column (active)" },
	{ 0, ImGuiCol_ColumnHovered,		"ColColumnHover" },     //		"Column (hover)" },
	{ 0, ImGuiCol_ResizeGrip,			"ColResize" },			//		"Resize Grip" },
	{ 0, ImGuiCol_ResizeGripActive,		"ColResizeActive" },	//		"Resize Grip (active)" },
	{ 0, ImGuiCol_ResizeGripHovered,	"ColResizeHover" },     //		"Resize Grip (hover)" },
	{ 0, ImGuiCol_CloseButton,			"ColCloseBtn" },		//		"Close Button" },
	{ 0, ImGuiCol_CloseButtonActive,	"ColCloseBtnActive" },  //		"Close Button (active)" },
	{ 0, ImGuiCol_CloseButtonHovered,	"ColCloseBtnHover" },	//		"Close Button (hover)" },
	{ 0, ImGuiCol_PlotLines,			"ColPlotLines" },       //		"Plot Lines" },
	{ 0, ImGuiCol_PlotLinesHovered,		"ColPlotLinesHover" },  //		"Plot Lines (hover)" },
};

int IsActionCam() {
	if (g_offsets.pActionCam.addr)
		return *(int*)g_offsets.pActionCam.addr;
	return false;
}

bool IsDisableInputOrActionCam()
{
	if (disable_input || IsActionCam())
		return true;
	return false;
}
#ifdef TARGET_OS_WIN
EXTERN_C bool ImGui_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//bool capture_input = cap_keyboard || cap_mouse || cap_input;
	//bool process_imgui = true; // (IsInterfaceHidden() || IsMapOpen() || IsInCutscene());
	//bool ret = !(process_imgui && capture_input);

	if (ImGui_ImplDX9_WndProcHandler(hWnd, msg, wParam, lParam)) {

		switch (msg) {
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEMOVE:
			return cap_mouse ? false : true;
		};

		// Only capture mouse in BGDM options
		// when cap input is disabled
		if (IsDisableInputOrActionCam())
			return true;

		switch (msg) {
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
			return (cap_keyboard || cap_input) ? false : true;
		}

		return true;
	}

	switch (msg) {
	case WM_SIZE:
		if (g_pD3D) {
			ImGui_ImplDX9_InvalidateDeviceObjects();
		}
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			break;
	}

	return true;
}

EXTERN_C void ImGui_Shutdown()
{
	ImGui_ImplDX9_Shutdown();
	if (hp_bars_enable) TR_SetHPChars(0);
	if (hp_bars_brighten) TR_SetHPCharsBright(0);
}

EXTERN_C void ImGui_ImplDX9_ResetInit()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

EXTERN_C bool ImGui_ImplDX9_ResetPost()
{
	if (!g_pD3D) return false;
	if (SUCCEEDED(g_pD3D->GetViewport(&g_viewPort))) {
		// Update d3d9.dll's ViewPort
		// so we can use it on "hot" reload
		if (g_pViewPort) *g_pViewPort = g_viewPort;
	}
	return ImGui_ImplDX9_CreateDeviceObjects();
}

EXTERN_C void ImGui_NewFrame() {
    if (!g_pD3D || !g_state.renderImgui) return;
    ImGui_ImplDX9_NewFrame((void*)&g_viewPort);
}
#endif  // TARGET_OS_WIN

extern "C" const char* GetProggyTinyCompressedFontDataTTFBase85();
bool ImGui_InitResources()
{
    static char IniFilename[PATH_MAX] = { 0 };
    char FontFilename[PATH_MAX] = { 0 };
    
    // Set default addr for our memory editor
    mem_edit.SetBaseAddr((unsigned char*)TlsContext, 0x1000);
    
    char * imgui_ini = config_get_str(CONFIG_SECTION_GLOBAL, "ImGuiIniFilename", "bgdm/bgdmUI.ini");
    if (imgui_ini) {
        snprintf(IniFilename, ARRAYSIZE(IniFilename), "%s/%s", config_get_my_documentsA(), imgui_ini);
        free(imgui_ini);
    }
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = IniFilename;
    
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 1; //or 2 is the same
    font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true;
    
    proggyClean = io.Fonts->AddFontDefault();
    
    if (proggyTiny == NULL) {
        memset(font_cfg.Name, 0, sizeof(font_cfg.Name));
        strncpy(font_cfg.Name, "Proggy Tiny", ARRAYSIZE(font_cfg.Name));
        const char* ttf_compressed_base85 = GetProggyTinyCompressedFontDataTTFBase85();
        proggyTiny = io.Fonts->AddFontFromMemoryCompressedBase85TTF(ttf_compressed_base85, 10.0f, &font_cfg);
    }
    
    char *default_font_path = config_get_str(CONFIG_SECTION_FONT, "DefaultFontPath", NULL);
    int default_font_size = config_get_int(CONFIG_SECTION_FONT, "DefaultFontSize", 13);
    char * tiny_font_path = config_get_str(CONFIG_SECTION_FONT, "TinyFontPath", NULL);
    int tiny_font_size = config_get_int(CONFIG_SECTION_FONT, "TinyFontSize", 10);
    
    if (default_font_path &&
        default_font_size > 0 &&
        strlen(default_font_path) > 0 &&
        file_exists(default_font_path)) {
        
        memset(FontFilename, 0, sizeof(FontFilename));
        memset(font_cfg.Name, 0, sizeof(font_cfg.Name));
        strncpy(font_cfg.Name, "Default Font", ARRAYSIZE(font_cfg.Name));
        strncpy(&FontFilename[0], default_font_path, ARRAYSIZE(FontFilename));
        defaultFont = io.Fonts->AddFontFromFileTTF(FontFilename, (float)default_font_size, &font_cfg, io.Fonts->GetGlyphRangesChinese());
    }
    
    if (tiny_font_path &&
        tiny_font_size > 0 &&
        strlen(tiny_font_path) > 0 &&
        file_exists(tiny_font_path)) {
        
        memset(FontFilename, 0, sizeof(FontFilename));
        memset(font_cfg.Name, 0, sizeof(font_cfg.Name));
        strncpy(font_cfg.Name, "Tiny Font", ARRAYSIZE(font_cfg.Name));
        strncpy(&FontFilename[0], tiny_font_path, ARRAYSIZE(FontFilename));
        tinyFont = io.Fonts->AddFontFromFileTTF(FontFilename, (float)tiny_font_size, &font_cfg, io.Fonts->GetGlyphRangesChinese());
    }
    
    if (default_font_path) free(default_font_path);
    if (tiny_font_path) free(tiny_font_path);
    
    if (tinyFont == NULL) tinyFont = proggyTiny;
    if (defaultFont == NULL) defaultFont = io.FontDefault ? io.FontDefault : io.Fonts->Fonts[0];
    
    
#ifdef TARGET_OS_WIN
    // Load Profession icons
    for (int i = 0; i < PROF_IMAGES_COUNT; i++) {
        HRESULT hr = D3DXCreateTextureFromResource(device, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDB_PNG_PROF_START+i), &PROF_IMAGES[i]);
        if (FAILED(hr)) {
            DBGPRINT(TEXT("D3DXCreateTextureFromResource(%d) failed, error 0x%08x"), i, GetLastError());
        }
    }
    
    for (int i = 0; i < BUFF_ICONS_COUNT; i++) {
        HRESULT hr = D3DXCreateTextureFromResource(device, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDB_PNG_BUFF_START + i), &BUFF_ICONS[i]);
        if (FAILED(hr)) {
            DBGPRINT(TEXT("D3DXCreateTextureFromResource(%d) failed, error 0x%08x"), i, GetLastError());
        }
    }
    
    for (int j = 0; j < 2; j++) {
        int start = (j == 0) ? IDB_PNG_BUFF_C_START : IDB_PNG_BUFF_D_START;
        LPDIRECT3DTEXTURE9 *arr = (j == 0) ? STD_BUFF_ICONS_C : STD_BUFF_ICONS_D;
        for (int i = 0; i < STD_BUFF_ICONS_COUNT; i++) {
            HRESULT hr = D3DXCreateTextureFromResource(device, (HMODULE)g_hInstance, MAKEINTRESOURCE(start + i), &arr[i]);
            if (FAILED(hr)) {
                //DBGPRINT(TEXT("D3DXCreateTextureFromResource(%d) failed, error 0x%08x"), i, GetLastError());
            }
        }
    }
    
    // Arrows
    D3DXCreateTextureFromResource(device, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDB_PNG_UP_ARROW), &UP_ARROW);
    D3DXCreateTextureFromResource(device, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDB_PNG_DOWN_ARROW), &DOWN_ARROW);
    
    hp_bars_enable = config_get_int(CONFIG_SECTION_GLOBAL, "HPBarsEnable", hp_bars_enable) > 0;
    hp_bars_brighten = config_get_int(CONFIG_SECTION_GLOBAL, "HPBarsBrighten", hp_bars_brighten) > 0;
    
    if (hp_bars_enable) TR_SetHPChars(hp_bars_enable);
    if (hp_bars_brighten) TR_SetHPCharsBright(hp_bars_brighten);
#else
    ImGui_LoadImagesPlatform();
#endif
    
    return true;
}

//void func();
//void ShowExampleAppCustomNodeGraph(bool* opened);

static void PanelSaveColumnConfig(const struct Panel* panel)
{
	static char config_sec[32] = { 0 };
	static char config_str[256] = { 0 };

	config_set_int(panel->section, "Enabled", panel->enabled);
	config_set_int(panel->section, "TinyFont", panel->tinyFont);
	config_set_int(panel->section, "AutoResize", panel->autoResize);

	if (panel->cfg.col_str == 0) return;

	int len = 0;
	int count = 0;
	memset(config_sec, 0, sizeof(config_sec));
	memset(config_str, 0, sizeof(config_str));
	for (i32 i = 0; i < panel->cfg.maxCol; ++i)
	{
		if (panel->cfg.col_visible[i]) {
			if (count > 0)
				len += snprintf(&config_str[len], sizeof(config_str) - len, "|");
			len += snprintf(&config_str[len], sizeof(config_str) - len, panel->cfg.col_str[i]);
			++count;
		}
	}
	if (strlen(config_str) > 0) {
		config_set_str(panel->section, "Columns", config_str);
		config_set_int(panel->section, "Sort", panel->cfg.sortCol);
		config_set_int(panel->section, "SortAsc", panel->cfg.asc);
	}
}


void ShowMetricsWindow(bool* p_open, float alpha, struct State* state, float ms)
{
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_AlwaysAutoResize /*|
		ImGuiWindowFlags_NoSavedSettings*/;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;

	float spacing = 80;
	ImGui::PushFont(tinyFont);
	if (ImGui::Begin("BGDM Metrics", p_open, ImVec2(0, 0), alpha, flags))
	{
		ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
		ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT1].col));

		ImGui::TextColored(white, LOCALTEXT(TEXT_METRICS_GW2_PING)); ImGui::SameLine(spacing);
		ImGui::Text("%dms", currentPing());

		ImGui::TextColored(white, LOCALTEXT(TEXT_METRICS_GW2_FPS)); ImGui::SameLine(spacing);
		ImGui::Text("%d", currentFPS());

		if (g_state.show_metrics_bgdm) {
			ImGui::TextColored(white, LOCALTEXT(TEXT_METRICS_BGDM_FPS)); ImGui::SameLine(spacing);
			ImGui::Text("%.1f", ImGui::GetIO().Framerate);
			ImGui::TextColored(white, LOCALTEXT(TEXT_METRICS_BGDM_APP)); ImGui::SameLine(spacing);
			//ImGui::Text("%.3fms", 1000.0f / ImGui::GetIO().Framerate);
			ImGui::Text("%.2fms", ms);
		}

		ImGui::PopStyleColor();
	}
	ImGui::End();
	ImGui::PopFont();
}

void ShowServerWindow(bool* p_open, float alpha, struct State* state)
{
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_AlwaysAutoResize /*|
		ImGuiWindowFlags_NoSavedSettings*/;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;

	ImGui::PushFont(tinyFont);
	if (ImGui::Begin("Server Status", p_open, ImVec2(0, 0), alpha, flags))
	{
		ImColor color(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
        
		ImGui::PushStyleColor(ImGuiCol_Text, color);

#ifdef __APPLE__
        ImGui::Text(LOCALTEXT(TEXT_SERVEROL_AA_NOTAVAIL));
#else
		if (state->autoUpdate_disable) {
			ImGui::Text(LOCALTEXT(TEXT_SERVEROL_AA_DISABLED));
		}
		else {
			ImGui::Text(LOCALTEXT(TEXT_SERVEROL_AA_ENABLED));
		}
#endif

#ifdef TARGET_OS_WIN
        static bool update_init = 0;
        u32 update_offset, update_size;
		if (updater_is_updating(&update_offset, &update_size))
		{
			ImGui::Text("%s: %.1f/%.1fKB",
				LOCALTEXT(TEXT_SERVEROL_AA_UPDATING),
				update_offset / 1024.0f, update_size / 1024.0f);
			update_init = 1;
		}
		else
		{
			if (updater_get_srv_version() != 0)
			{
				if (updater_get_srv_version() == updater_get_cur_version())
				{
					ImGui::Text(update_init ? LOCALTEXT(TEXT_SERVEROL_AA_UPDATE_OK) : LOCALTEXT(TEXT_SERVEROL_AA_UPDATED));
				}
				else
				{
					ImGui::Text("%s %x",
						update_init ? LOCALTEXT(TEXT_SERVEROL_AA_UPDATE_FAIL) : LOCALTEXT(TEXT_SERVEROL_AA_UPDATE_REQ),
						updater_get_srv_version()
					);
				}
			}
			else
			{
				ImGui::Text(LOCALTEXT(TEXT_SERVEROL_SRV_NO_CONN));
			}
		}
#endif
		ImGui::PopStyleColor();

        time_t srv_time = currentContext()->serverPing;
		if (srv_time != 0)
		{
			char buff[32] = { 0 };
			struct tm tm = { 0 };
			localtime_r(&srv_time, &tm);
			strftime(&buff[0], sizeof(buff), "%Y-%m-%dT%H:%M:%SZ", &tm);
			ImGui::TextColored(ImColor(bgdm_colors[BGDM_COL_TEXT2].col), "%s", buff);
		}
#ifdef __APPLE__
        else ImGui::Text(LOCALTEXT(TEXT_SERVEROL_SRV_NO_CONN));
#endif
	}
	ImGui::End();
	ImGui::PopFont();
}

void ShowUserGuide()
{
    static char buff[64];
    ImGui::BulletText("'%s' %s", keyBindToString(g_state.panel_version.bind, buff, sizeof(buff)), LOCALTEXT(TEXT_HELP_USERGUIDE_BIND));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE1));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE2));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE3));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE4));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE5));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE6));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE7));
	ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE8));
	if (ImGui::GetIO().FontAllowUserScaling)
		ImGui::BulletText(LOCALTEXT(TEXT_HELP_USERGUIDE9));
}

void ShowKeybinds()
{
    static char buff[64];
	ImGui::Columns(2, "KeyBinds", false);
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_BGDM_MAIN)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_version.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_GLOBAL_ONOFF)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_on_off, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_INPUTCAP_ONOFF)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_input, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_HEADER_ONOFF)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_minPanels, buff, sizeof(buff))); ImGui::NextColumn();
#ifdef TARGET_OS_WIN
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_BGDM_RELOAD)); ImGui::NextColumn();
	ImGui::Text("CTRL + SHIFT + F9"); ImGui::NextColumn();
#endif
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_SCREENSHOT)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_screenshot, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_LOCK_TARGET)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_dps_lock, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_DATA_RESET)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.bind_dps_reset, buff, sizeof(buff))); ImGui::NextColumn();
#if !(defined BGDM_TOS_COMPLIANT)
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_HP)); ImGui::NextColumn();
#endif
	ImGui::Text(keyBindToString(g_state.panel_hp.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_COMPASS)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_compass.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_TARGET)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_dps_self.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_GROUPDPS)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_dps_group.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_BUFFUPTIME)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_buff_uptime.bind, buff, sizeof(buff))); ImGui::NextColumn();
	ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_SKILLS)); ImGui::NextColumn();
	ImGui::Text(keyBindToString(g_state.panel_skills.bind, buff, sizeof(buff))); ImGui::NextColumn();

#if !(defined BGDM_TOS_COMPLIANT)
		ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_FLOATBARS)); ImGui::NextColumn();
		ImGui::Text(keyBindToString(g_state.panel_float.bind, buff, sizeof(buff))); ImGui::NextColumn();
		ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_INSPECT_SELF)); ImGui::NextColumn();
		ImGui::Text(keyBindToString(g_state.panel_gear_self.bind, buff, sizeof(buff))); ImGui::NextColumn();
		ImGui::BulletText(LOCALTEXT(TEXT_BIND_PANEL_INSPECT_TARGET)); ImGui::NextColumn();
		ImGui::Text(keyBindToString(g_state.panel_gear_target.bind, buff, sizeof(buff))); ImGui::NextColumn();
#endif
}

void ShowAppHelp(bool *p_open, float alpha, bool use_tinyfont)
{
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;
	if (ImGui::Begin(LOCALTEXT(TEXT_HELP_TITLE), p_open, ImVec2(460, 250), alpha, flags)) {
		ImGui::Text(LOCALTEXT(TEXT_HELP_TITLE));
		ImGui::Spacing();
		if (ImGui::CollapsingHeader(LOCALTEXT(TEXT_HELP_BASICS_HEADER), ImGuiTreeNodeFlags_Selected|ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextWrapped(LOCALTEXT(TEXT_HELP_BASICS_TEXT));
			ShowUserGuide();
		}
		if (ImGui::CollapsingHeader(LOCALTEXT(TEXT_HELP_BINDS_HEADER)))
		{
			ImGui::TextWrapped(LOCALTEXT(TEXT_HELP_BINDS_TEXT));
			ShowKeybinds();
		}
	}
	ImGui::End();
	ImGui::PopFont();
}


void ShowAppAbout(bool *p_open, float alpha, bool use_tinyfont)
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	if (ImGui::Begin(LOCALTEXT(TEXT_ABUOT_TITLE), p_open, ImVec2(0, 0), alpha, flags)) {
		ImGui::Text("%s %s", LOCALTEXT(TEXT_ABOUT_VERSION), versionStr());
		ImGui::Separator();
		ImGui::Text(LOCALTEXT(TEXT_ABOUT_WHAT));
		ImGui::Text("Copyright © 2017 Bhagwan Software. All rights reserved.");
		ImGui::Text("http://gw2bgdm.blogspot.com");
	}
	ImGui::End();
	ImGui::PopFont();
}

void ShowUnicodeTest(bool *p_open, float alpha, bool use_tinyfont)
{
	ImGuiWindowFlags flags = 0;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);
	ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin("Unicode Test", p_open, flags)) {
        utf16string unicode = u"中日友好";
        std::string utf8 = std_utf16_to_utf8(unicode);
		ImGui::Text(u8"UTF8-1: %s", utf8.c_str());
		ImGui::Text(u8"UTF8-2: 我是中文");
		ImGui::Text("Kanjis: \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e (nihongo)");
	}
	ImGui::End();
	ImGui::PopFont();
}

static void ShowHelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(450.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static inline bool ShowColorPickerMenu(const char *label, const char *btn_text, float offset, int mode, float width, ImU32 &color, float offset_help = -1.0f, ImU32 color_default = 0)
{
	bool dirty = false;
	ImGuiColorEditFlags flags = ImGuiColorEditFlags_Alpha;

	if (mode == 1) flags |= ImGuiColorEditFlags_HSV;
	else if (mode == 2) flags |= ImGuiColorEditFlags_HEX;
	else flags |= ImGuiColorEditFlags_RGB;

	if (label && label[0] != '#') {
		ImGui::Text(label); ImGui::SameLine(offset);
	}

	ImColor col(color);
    std::string str = toString("%s##btn_%s", btn_text, label);
	ImGui::PushStyleColor(ImGuiCol_Button, col);
	if (ImGui::Button(str.c_str(), ImVec2(width, 0))) {
	}
	ImGui::PopStyleColor(1);

	str = toString("##pop_%s", label);
	if (ImGui::BeginPopupContextItem(str.c_str())) {

		str = toString("##col_%s", label);
		if (ImGui::ColorPicker4(str.c_str(), reinterpret_cast<float*>(&col.Value), flags)) {
			color = col;
			dirty = true;
		}
		ImGui::EndPopup();
	}

	if (color_default != 0 && color != color_default) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
		ImGui::SameLine();
		str = toString("%s##revert_%s", LOCALTEXT(TEXT_GLOBAL_REVERT), label);
		if (ImGui::Button(str.c_str())) {
			color = color_default;
			dirty = true;
		}
		ImGui::PopStyleVar();
	}


	if (offset_help >= 0.0f) {
		ImGui::SameLine(offset_help);  ShowHelpMarker(LOCALTEXT(TEXT_GLOBAL_COLORPICK_HELP));
	}

	return dirty;
}

bool ShowNpcIDMenuItem(const struct AutoLockTarget* target, const char* name)
{
    std::string utf8 = name;
	if (utf8.length() == 0) utf8 = toString("%d", target->npc_id);

	bool is_selected = autolock_get(target->npc_id);
	if (ImGui::MenuItemNoPopupClose(utf8.c_str(), "", &is_selected)) {
		autolock_set(target->npc_id, is_selected);
		return true;
	}
	return false;
}

static void ShowCombatMenu()
{
	static char buf[128] = { 0 };
    static char buff[128] = { 0 };

	if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_CBT_TITLE)))
	{
		ImGui::Text(LOCALTEXT(TEXT_MENU_CBT_OOCGRACE));
		ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_MENU_CBT_OOCGRACE_HELP));
		ImGui::PushItemWidth(100);
		if (ImGui::InputInt("##OOCGracePeriod", &g_state.ooc_grace_period)) {
			if (g_state.ooc_grace_period < 0) g_state.ooc_grace_period = 0;
			config_set_int(CONFIG_SECTION_GLOBAL, "OOCGracePeriod", g_state.ooc_grace_period);
		}
		ImGui::Separator();


		ImGui::Text(LOCALTEXT(TEXT_MENU_CBT_RETENTION));
		ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_MENU_CBT_RETENTION_HELP));

		int retention_time = (g_state.target_retention_time == INT_MAX) ? (-1) : (g_state.target_retention_time / 1000000);
		ImGui::PushItemWidth(100);
		if (ImGui::InputInt("##TargetRetention", &retention_time)) {
			if (retention_time < -1) retention_time = -1;
			// Adjsut to seconds
			g_state.target_retention_time = (retention_time >= 0) ? (retention_time * 1000000) : INT_MAX;
			config_set_int(CONFIG_SECTION_GLOBAL, "TargetRetention", retention_time);
		}

		ImGui::EndMenu();
	}


    DPSTargetEx* target = &currentContext()->currentDpsTarget;

	if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_TLOCK_TITLE)))
	{
		if (!target || target->t.agent_id == 0 || target->t.aptr == 0)
			ImGui::TextDisabled(LOCALTEXT(TEXT_MENU_TLOCK_INVALID));
		else if (!target->locked) {
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_MENU_TLOCK_LOCK), keyBindToString(g_state.bind_dps_lock, buff, sizeof(buff)))) {
				target->locked = true;
			}
		}
		else {
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_MENU_TLOCK_UNLOCK), keyBindToString(g_state.bind_dps_lock, buff, sizeof(buff)))) {
				target->locked = false;
			}
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_TITLE)))
	{
		bool needsUpdate = false;
		if (target && target->t.npc_id) {
            std::string utf8;
			bool autolock = autolock_get(target->t.npc_id);
			utf8 = toString("%s %s", LOCALTEXT(TEXT_MENU_AUTOLOCK_CURRENT), autolock ? LOCALTEXT(TEXT_GLOBAL_REMOVE) : LOCALTEXT(TEXT_GLOBAL_ADD));
			if (ImGui::SelectableNoPopupClose(utf8.c_str())) {
				autolock_set(target->t.npc_id, !autolock);
				target->locked = !autolock;
				needsUpdate = true;
			}
			ImGui::TextDisabled("%s %d", LOCALTEXT(TEXT_MENU_AUTOLOCK_ID), target->t.npc_id);
		}
		else {
			ImGui::TextDisabled(LOCALTEXT(TEXT_MENU_TLOCK_INVALID));
		}
		ImGui::Separator();
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_ALL))) {
			autolock_allraid();
			needsUpdate = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_CLEAR))) {
			autolock_freeraid();
			needsUpdate = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_MENU_AUTOLOCK_CLEAR_CUSTOM))) {
			autolock_freecustom();
			needsUpdate = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_MENU_AUTOLOCK_CLEAR_ALL))) {
			autolock_free();
			needsUpdate = true;
		}
		ImGui::Separator();
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING1))) {
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[0], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING1B1));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[1], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING1B2));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[2], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING1B3));
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2))) {
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[3], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2B1));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[4], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2B21));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[5], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2B22));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[6], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2B23));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[7], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING2B3));
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING3))) {
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[8], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING3B2));
			// Xera has 2 diff IDs make sure we tick/untick both
			if (ShowNpcIDMenuItem(&g_raid_bosses[9], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING3B3))) {
				bool is_selected = autolock_get(g_raid_bosses[9].npc_id);
				autolock_set(g_raid_bosses[10].npc_id, is_selected);
				needsUpdate = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING4))) {
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[11], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING4B1));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[12], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING4B2));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[13], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING4B3));
			needsUpdate |= ShowNpcIDMenuItem(&g_raid_bosses[14], LOCALTEXT(TEXT_MENU_AUTOLOCK_RAID_WING4B4));
			ImGui::EndMenu();
		}

		typedef struct KeyVal {
			i32 key;
			bool value;
		} KeyVal, *PKeyVal;

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_AUTOLOCK_CUSTOM))) {
			ImGui::BeginChild("CustomIDs", ImVec2(0, 80), true);
			autolock_iter([](void *data) {
				PKeyVal entry = (PKeyVal)data;
				std::string id = toString("%d", entry->key);
				if (ImGui::Selectable(id.c_str(), entry->value))
					entry->value ^= 1;
			}, true);
			ImGui::EndChild();
			if (ImGui::Button(LOCALTEXT(TEXT_GLOBAL_REMOVE_SEL), ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
				autolock_iter([](void *data) {
					PKeyVal entry = (PKeyVal)data;
					if (entry->value)
						autolock_set(entry->key, false);
				}, true);
				needsUpdate = true;
			}
			ImGui::PushItemWidth(100);
			ImGui::InputText("##ID", buf, sizeof(buf), ImGuiInputTextFlags_CharsDecimal);
			ImGui::SameLine();
			if (ImGui::Button(LOCALTEXT(TEXT_GLOBAL_ADD))) {
				autolock_set(atoi(buf), true);
				needsUpdate = true;
			}
			ImGui::EndMenu();
		}

		if (needsUpdate) {
			static char config_str_raid[256] = { 0 };
			static char config_str_custom[256] = { 0 };
			static char *buff;
			static size_t size;

			for (int i = 0; i < 2; i++) {
				buff = (i == 0) ? config_str_raid : config_str_custom;
				size = (i == 0) ? sizeof(config_str_raid) : sizeof(config_str_custom);
				memset(buff, 0, size);
				autolock_iter([](void *data) {
					PKeyVal entry = (PKeyVal)data;
					size_t len = strlen(buff);
					snprintf(&buff[len], size - len, len > 0 ? "|%d" : "%d", entry->key);
				}, (i>0));
			}

			config_set_str(CONFIG_SECTION_GLOBAL, "AutoLockRaid", config_str_raid);
			config_set_str(CONFIG_SECTION_GLOBAL, "AutoLockCustom", config_str_custom);
		}

		ImGui::EndMenu();
	}
}

static bool ShowWindowOptionsMenu(struct Panel* panel, int *outFlags)
{
	bool bRet = false;
	if (ImGui::MenuItem(LOCALTEXT(TEXT_MENU_OPT_TINY), "", &panel->tinyFont)) {
		if (outFlags) *outFlags |= ImGuiWindowFlags_AlwaysAutoResize;
		config_set_int(panel->section, "TinyFont", panel->tinyFont);
		bRet = true;
	}
	if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_MENU_OPT_AUTO_RESIZE), "", &panel->autoResize)) {
		if (outFlags) *outFlags |= ImGuiWindowFlags_AlwaysAutoResize;
		config_set_int(panel->section, "AutoResize", panel->autoResize);
		bRet = true;
	}
	ImGui::PushAllowKeyboardFocus(false);
	if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_OPT_ALPHA))) {
		if (ImGui::SliderFloat("##Alpha", &panel->fAlpha, 0.0f, 1.0f, "%.2f")) {
			config_set_float(panel->section, "Alpha", panel->fAlpha);
			bRet = true;
		}
		ImGui::EndMenu();
	}
	ImGui::PopAllowKeyboardFocus();
	ImGui::Separator();
	return bRet;
}

static bool ShowStyleMenu(struct Panel* panel, ImGuiStyle &style, int *outFlags)
{
	static const float color_width = 60.0f;
	static const float offset_help = 160.0f;
	static const float offset = offset_help + 25.0f;
	static const float offset_radio = offset + 60.0f;

	bool dirty = false;

	if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_STYLE_TITLE)))
	{
		ShowWindowOptionsMenu(panel, outFlags);

		if (ImGui::Button(LOCALTEXT(TEXT_GLOBAL_RESTORE_DEF))) {
			ImGui_StyleSetDefaults();
			dirty = true;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Text(LOCALTEXT(TEXT_GLOBAL_COLORPICK_MODE));
		ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_GLOBAL_COLORPICK_MODE_HELP));
		ImGui::SameLine();
		ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_RGB, "##col_edit"), &bgdm_style.col_edit_mode, 0); ImGui::SameLine();
		ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_HSV, "##col_edit"), &bgdm_style.col_edit_mode, 1); ImGui::SameLine();
		ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_HEX, "##col_edit"), &bgdm_style.col_edit_mode, 2);
		ImGui::Separator();


		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_STYLE_GLOBAL)))
		{
			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_WND_ROUND));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_WND_ROUND_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##round"), &bgdm_style.round_windows, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##round"), &bgdm_style.round_windows, 0);

			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_ALPHA));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_ALPHA_HELP));
			ImGui::SameLine(offset);
			ImGui::PushItemWidth(-1);
			// Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
			// But application code could have a toggle to switch between zero and non-zero.
			dirty |= ImGui::SliderFloat("##Alpha", &bgdm_style.global_alpha, 0.25f, 1.0f, "%.2f");
			//dirty |= ImGui::DragFloat("##Alpha", &bgdm_style.global_alpha, 0.005f, 0.20f, 1.0f, "%.2f");
			ImGui::PopItemWidth();

			ImGui::Separator();

			for (int i=0; i < ARRAYSIZE(bgdm_colors); i++)
				dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_COL_TEXT+i), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_colors[i].col, offset_help, bgdm_colors_default[i].col);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_STYLE_METRICS)))
		{
			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_METRICS_BGDM));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_METRICS_BGDM_HELP));
			ImGui::SameLine(offset);
			bool needUpdate = false;
			int mode = g_state.show_metrics_bgdm == 1;
			needUpdate |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##metrics"), &mode, 1); ImGui::SameLine(offset_radio);
			needUpdate |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##metrics"), &mode, 0);
			if (needUpdate) {
				g_state.show_metrics_bgdm = mode == 1;
				dirty = true;
			}
			ImGui::EndMenu();
		}

		ImGui::PopStyleVar();

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_STYLE_PROFCOL)))
		{
			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_PROFCOL_HELP));
			ImGui::Separator();
			for (int i = 0; i < 3; i++) {
				ImGui::BeginGroup();
				for (int j = GW2::PROFESSION_GUARDIAN; j < GW2::PROFESSION_END; j++) {

					void* img = ImGui_GetProfImage(j, i);
					if (img) {
						ImGui::Image(img, ImVec2(14.0f, 14.0f));
						ImGui::SameLine();
					}
                    
                    std::string str = toString("##prof_%d%d", i, j);
					dirty |= ShowColorPickerMenu(str.c_str(), ProfessionName(j, i), 0.0f, bgdm_style.col_edit_mode, 100.0f, prof_colors[i][j]);
				}
				ImGui::EndGroup();
                if (i == 0) ImGui::SameLine(150);
                if (i == 1) ImGui::SameLine(300);
			}
			ImGui::EndMenu();
		}

#if !(defined BGDM_TOS_COMPLIANT)
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_MENU_STYLE_HPBAR)))
		{
			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_HP_COMMA));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_HP_COMMA_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##comma"), &bgdm_style.hp_comma_mode, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##comma"), &bgdm_style.hp_comma_mode, 0);

			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_HP_PER));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_HP_PER_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##percent"), &bgdm_style.hp_percent_mode, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##percent"), &bgdm_style.hp_percent_mode, 0);

			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_HP_PER_MODE));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_HP_PER_MODE_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_INT), &bgdm_style.hp_precision_mode, 0); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_FLOAT), &bgdm_style.hp_precision_mode, 1);

			ImGui::Text(LOCALTEXT(TEXT_MENU_STYLE_HP_PER_WIDTH));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_MENU_STYLE_HP_PER_WIDTH_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_FIXED), &bgdm_style.hp_fixed_mode, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_DYNAMIC), &bgdm_style.hp_fixed_mode, 0);


			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_PLAYERHP), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_PLAYER_HP], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_TARGETHP), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_TARGET_HP], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_TARGETDST), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_TARGET_DIST], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_TARGETBB), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_TARGET_BB], offset_help);
#if !(defined BGDM_TOS_COMPLIANT)
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_PORT_DIST), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_PORT_DIST], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_MENU_STYLE_COL_PORT_DIST_BG), "", offset, bgdm_style.col_edit_mode, color_width, bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG], offset_help);
#endif		
			ImGui::EndMenu();
		}

		ImGui::PopStyleVar();
#endif  // !(defined BGDM_TOS_COMPLIANT)

		
#if !(defined BGDM_TOS_COMPLIANT)
		ImGui::Separator();
		ShowFloatBarMenu();
#endif

		ImGui::Separator();
        static std::vector<std::string> itemStrings;
        static std::vector<const char *> items;
		//if (items.IsEmpty()) {
			itemStrings.clear();
			itemStrings.push_back(LOCALTEXT(TEXT_GLOBAL_FONT_SMALL));
			itemStrings.push_back(LOCALTEXT(TEXT_GLOBAL_FONT_NORMAL));
			if (tinyFont != proggyTiny) itemStrings.push_back(LOCALTEXT_FMT(TEXT_GLOBAL_FONT_SMALL, " (custom)"));
			if (defaultFont != proggyClean) itemStrings.push_back(LOCALTEXT_FMT(TEXT_GLOBAL_FONT_NORMAL, " (custom)"));
			items.clear();
			items.resize(itemStrings.size());
			for (size_t i = 0; i < itemStrings.size(); ++i)
				items[i] = itemStrings[i].c_str();
			if (bgdm_style.compass_font < 0 || bgdm_style.compass_font >= (int)items.size()) bgdm_style.compass_font = 0;

		//}
		ImGui::Text(LOCALTEXT_FMT(TEXT_GLOBAL_COMPASS, " %s", LOCALTEXT(TEXT_GLOBAL_FONT)));
		ImGui::SameLine(offset_help - 30.0f);
		ImGui::PushItemWidth(-1);
		dirty |= ImGui::Combo("##combo", &bgdm_style.compass_font, items.data(), (int)items.size());
		ImGui::PopItemWidth();

		ImGui::EndMenu();
	}

	if (dirty) ImGui_StyleConfigSave();

	return dirty;
}


static bool ShowLanguageButtons(float offset, float width, bool &rescan_directory, bool &reload_file)
{
	rescan_directory = ImGui::Button(LOCALTEXT(TEXT_LOCALIZATION_RESCAN), ImVec2(width, 0));
    const std::string utf8 = g_state.lang_dir;
    std::string str = toString("%s\n%s", LOCALTEXT(TEXT_LOCALIZATION_RESCAN_HELP), utf8.c_str());
	ImGui::SameLine(offset); ShowHelpMarker(str.c_str());

	reload_file |= ImGui::Button(LOCALTEXT(TEXT_LOCALIZATION_RELOAD), ImVec2(width, 0));
	ImGui::SameLine(offset); ShowHelpMarker(LOCALTEXT(TEXT_LOCALIZATION_RELOAD_HELP));
	
	if (reload_file) return true;
	return false;
}

static bool ShowLanguageEditor(bool use_tiny_font, float offset = 160.0f);
static bool ShowLangaugeControls(bool use_tiny_font, bool isWindow = false)
{
	static char file[1024] = { 0 };
	static const float btn_width = 120.0f;
	static const float offset = 160.0f;
	static int selected_file = 0;
	static bool reload_file = false;
	static bool rescan_directory = true;
    static std::vector<std::string> files;
    static std::vector<const char *> items;
    
    
	if (reload_file) {
		reload_file = false;
		rescan_directory = true;
		localized_text_reloaded = true;
		if (selected_file > 0 && selected_file < items.size()) {
            const char * selectedFile = items[selected_file];
			localtext_load_file(selectedFile);
			strncpy(g_state.lang_file, selectedFile, ARRAYSIZE(g_state.lang_file));
			config_set_str(CONFIG_SECTION_LANG, "Default", g_state.lang_file);
		}
		else {
			localtext_load_defaults();
			memset(g_state.lang_file, 0, sizeof(g_state.lang_file));
			config_set_str(CONFIG_SECTION_LANG, "Default", "");
		}
		strncpy(file, g_state.lang_file, sizeof(file));
	}

	if (rescan_directory) {
		rescan_directory = false;
		localized_text_reloaded = true;
		items.clear();
		files.clear();
		items.push_back(LOCALTEXT(TEXT_LOCALIZATION_DEFAULT));
        directory_enum(g_state.lang_dir, [](const char *filename){
            files.push_back(filename);
        });
        
        int i = 1;
        for (auto& file : files) {
            items.push_back(file.c_str());
            if (strcmp(file.c_str(), g_state.lang_file) == 0)
                selected_file = i;
            ++i;
        }
	}

	if (!isWindow) {

		ShowLanguageButtons(offset, btn_width, rescan_directory, reload_file);

		if (ImGui::Button(LOCALTEXT(TEXT_LOCALIZATION_EDITOR_OPEN), ImVec2(btn_width, 0))) {
			show_lang_editor = 1;
			ShowLanguageEditor(use_tiny_font);
		}
		ImGui::SameLine(offset); ShowHelpMarker(LOCALTEXT(TEXT_LOCALIZATION_EDITOR_HELP));
		ImGui::Separator();
	}

	ImGui::Text(LOCALTEXT(TEXT_LOCALIZATION_CURR_LANG)); ImGui::SameLine(offset);
	ImGui::Text("%s", LOCALTEXT(TEXT_LANGUAGE));

	ImGui::Text(LOCALTEXT(TEXT_LOCALIZATION_CURR_FILE)); ImGui::SameLine(offset);
	ImGui::PushItemWidth(150);
	reload_file |= ImGui::Combo("##combo", &selected_file, items.data(), (int)items.size());
	ImGui::PopItemWidth();

	if (isWindow) {

		ImGui::Separator();
		ShowLanguageButtons(offset, btn_width, rescan_directory, reload_file);

		if (ImGui::Button(LOCALTEXT(TEXT_LOCALIZATION_SAVE), ImVec2(btn_width, 0))) {
			localtext_save_file(file);
			rescan_directory = true;
		}
		ImGui::SameLine(offset);
		ImGui::PushItemWidth(200.0f);
		ImGui::InputText("##file", file, sizeof(file));
		ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_LOCALIZATION_SAVE_HELP));
		ImGui::Separator();
	}

	return false;
}

static bool ShowLanguageEditor(bool use_tiny_font, float offset)
{
	static bool m_init = false;
	bool bRet = false;

	if (global_use_tinyfont || use_tiny_font) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);
	if (ImGui::Begin(LOCALTEXT(TEXT_LOCALIZATION_EDITOR_TITLE), &show_lang_editor)) {
		bRet = ShowLangaugeControls(use_tiny_font, true);

		ImGui::Separator();

		ImGui::Text(LOCALTEXT(TEXT_LOCALIZATION_EDITOR_FILTER)); ImGui::SameLine(offset);
		static ImGuiTextFilter filter;
		filter.Draw("##filter", 200.0f);
		ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_LOCALIZATION_EDITOR_FILTER_HELP));
		ImGui::BeginChild("#strings", ImVec2(0, -1), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
		//ImGui::PushItemWidth(-160);

		ImGui::Columns(2);
		if (!m_init) {
			m_init = true;
			ImGui::SetColumnOffset(1, 200.0f);
		}
		for (int i = 0; i < (int)localtext_size(); ++i)
		{
			const char* name = localtext_get_name(i);
			static char buff[2048] = { 0 };
			memset(buff, 0, sizeof(buff));
			strncpy(buff, LOCALTEXT(i), sizeof(buff));
			if (!filter.PassFilter(name) && !filter.PassFilter(buff))
				continue;

			ImGui::Text(name); ImGui::NextColumn();
			ImGuiInputTextFlags flags = 0;
			if (ImGui::CalcTextSize(buff).y > ImGui::GetTextLineHeight()) flags |= ImGuiInputTextFlags_Multiline|ImGuiInputTextFlags_CtrlEnterForNewLine;
			ImGui::PushID(i);
			if (ImGui::InputTextEx("##text", buff, sizeof(buff), ImVec2(-1.0, 0.0f), flags)) {
				localtext_set(i, name, buff);
			}
			ImGui::PopID();
			ImGui::NextColumn();
		}
		//ImGui::PopItemWidth();
		ImGui::EndChild();
	}
	ImGui::End();
	ImGui::PopFont();

	return bRet;
}

static bool ShowLanguageMenu(bool use_tiny_font)
{
	bool bRet = false;
	if (ImGui::BeginMenu(LOCALTEXT(TEXT_LOCALIZATION_MENU_TITLE)))
	{
		bRet = ShowLangaugeControls(use_tiny_font);
		ImGui::EndMenu();
	}
	return bRet;
}

void ImGui_Main(float ms, int64_t now) {

    static char buff[128];
#if (defined _DEBUG) || (defined DEBUG)
    static bool show_test_window = false;
	static bool show_demo_window = false;
#endif
	static bool show_unicode_test = false;
	static bool show_app_about = false;
	static bool show_app_help = false;
	bool show_app_metrics = g_state.show_metrics;
	bool show_server_status = g_state.show_server;

	g_now = now;
	Panel* panel = &g_state.panel_version;
	ImGuiStyle& style = ImGui::GetStyle();
    
	disable_input = !g_state.global_cap_input;
    minimal_panels = g_state.minPanels_enable;

	//if (IsInterfaceHidden() || IsMapOpen() || IsInCutscene()) return;

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	if (g_state.global_on_off) {
		if (show_app_about) ShowAppAbout(&show_app_about, panel->fAlpha, use_tinyfont);
		if (show_app_help) ShowAppHelp(&show_app_help, panel->fAlpha, use_tinyfont);
		if (show_app_metrics) ShowMetricsWindow(&show_app_metrics, panel->fAlpha, &g_state, ms);
		if (show_server_status) ShowServerWindow(&show_server_status, panel->fAlpha, &g_state);
		if (show_lang_editor) { ShowLanguageEditor(use_tinyfont); }
		if (show_unicode_test) ShowUnicodeTest(&show_unicode_test, panel->fAlpha, use_tinyfont);
#if (defined _DEBUG) || defined (DEBUG)
		if (show_demo_window) {
			ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
			ImGui::ShowTestWindow(&show_demo_window);
		}
        if (show_test_window) ImGui_Test(&show_test_window, &show_demo_window);
#endif
		if (mem_edit.IsOpen) {
			if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(proggyTiny);
			else ImGui::PushFont(proggyClean);
			mem_edit.Draw();
			ImGui::PopFont();
		}
	}

	if (panel->enabled) {

		static ImGuiWindowFlags temp_flags = 0;
		static int temp_fr_count = 0;
		ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;

		if (!panel->autoResize) temp_flags = 0;
		else if (temp_flags) {
			if (temp_fr_count++ > 1) {
				flags |= temp_flags;
				temp_flags = 0;
				temp_fr_count = 0;
			}
		}

		ImGui::SetNextWindowPos(ImVec2(300, 5), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(LOCALTEXT(TEXT_BGDM_OPTS), (bool*)&panel->enabled, ImVec2(450, 130), panel->fAlpha, flags)) {

			// Menu
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu(LOCALTEXT(TEXT_BGDM_HELP)))
				{
					ImGui::MenuItem(LOCALTEXT(TEXT_BGDM_HELP_VIEW), NULL, &show_app_help);
					if (ImGui::MenuItem(LOCALTEXT(TEXT_BGDM_HELP_METRICS), NULL, &show_app_metrics)) {
						g_state.show_metrics = show_app_metrics;
						config_set_int(CONFIG_SECTION_OPTIONS, "ShowMetricsWindow", show_app_metrics);
					}
					if (ImGui::MenuItem(LOCALTEXT(TEXT_BGDM_HELP_SRV_STATUS), NULL, &show_server_status)) {
						g_state.show_server = show_server_status;
						config_set_int(CONFIG_SECTION_OPTIONS, "ShowServerStatus", show_server_status);
					}
					ImGui::MenuItem(LOCALTEXT(TEXT_BGDM_HELP_ABOUT), NULL, &show_app_about);
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
				{
					if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_ENABLED), keyBindToString(g_state.bind_on_off, buff, sizeof(buff)), &g_state.global_on_off)) {
						config_set_int(CONFIG_SECTION_GLOBAL, "Enabled", g_state.global_on_off);
					}
					bool temp_bool = !IsDisableInputOrActionCam();
					if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_INPUTCAP), keyBindToString(g_state.bind_input, buff, sizeof(buff)), &temp_bool)) {
						disable_input = !temp_bool;
                        setDisableCap(disable_input);
						g_state.global_cap_input = temp_bool;
						config_set_int(CONFIG_SECTION_GLOBAL, "CaptureInput", g_state.global_cap_input);
					}
					if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_HEADERS), keyBindToString(g_state.bind_minPanels, buff, sizeof(buff)), &minimal_panels)) {
                        g_state.minPanels_enable = minimal_panels;
						config_set_int(CONFIG_SECTION_GLOBAL, "MinimalPanels", minimal_panels);
					}

					if (ImGui::BeginMenu(LOCALTEXT(TEXT_BGDM_OPTS_PROX)))
					{
                        ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_MAX_PL));
                        ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_MAX_PL_HELP));
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        if (ImGui::DragInt("##players", &g_state.max_players, 0.8f, 0, SQUAD_SIZE_MAX, "%.0f")) {
                            if (g_state.max_players < 0) g_state.max_players = 0;
                            if (g_state.max_players > SQUAD_SIZE_MAX) g_state.max_players = SQUAD_SIZE_MAX;
                            config_set_int(CONFIG_SECTION_GLOBAL, "MaxPlayers", g_state.max_players);
                        }
                        ImGui::PopStyleVar();
                        ImGui::Separator();
                        
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_PROX_SQUAD_PRIORITY), "", &g_state.priortizeSquad, false)) {
							config_set_int(CONFIG_SECTION_GLOBAL, "PrioritizeSquadMembers", g_state.priortizeSquad);
						}
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_PROX_NON_SQUAD_HIDE), "", &g_state.hideNonSquad)) {
							config_set_int(CONFIG_SECTION_GLOBAL, "HideNonSquadMembers", g_state.hideNonSquad);
						}
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_PROX_NON_PARTY_HIDE), "", &g_state.hideNonParty)) {
							config_set_int(CONFIG_SECTION_GLOBAL, "HideNonPartyMembers", g_state.hideNonParty);
						}
						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu(LOCALTEXT(TEXT_BGDM_OPTS_SRV)))
					{
						char buf[128] = { 0 };
						snprintf(buf, ARRAYSIZE(buf), "%s:%d", g_state.network_addr, g_state.network_port);
						ImGui::MenuItem(buf, "", false, false);
						ImGui::Separator();
						temp_bool = !g_state.autoUpdate_disable;
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_SRV_AUTO_UP), "", &temp_bool)) {
							g_state.autoUpdate_disable = !temp_bool;
							config_set_int(CONFIG_SECTION_SRV, "AutoUpdateDisable", g_state.autoUpdate_disable);
						}
						temp_bool = !g_state.netDps_disable;
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_SRV_NET_SHARE), "", &temp_bool)) {
							g_state.netDps_disable = !temp_bool;
							config_set_int(CONFIG_SECTION_SRV, "NetworkDpsDisable", g_state.netDps_disable);
						}
						ImGui::EndMenu();
					}

					ShowCombatMenu();
                    
#if !(defined BGDM_TOS_COMPLIANT)
					if (ImGui::BeginMenu(LOCALTEXT(TEXT_BGDM_OPTS_HPBARS), (gameGetHPBarsVisibleMode() != -1 && gameGetHPBarsHighlightMode() != -1)))
					{
						bool hp_bars_enable = gameGetHPBarsVisibleMode() == 1;
						bool hp_bars_brighten = gameGetHPBarsHighlightMode() == 1;
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_HPBARS_ENABLED), "", &hp_bars_enable, gameGetHPBarsVisibleMode() >= 0)) {
                            
                            gameSetHPBarsVisibleMode(hp_bars_enable);
						}
						if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BGDM_OPTS_HPBARS_BRIGHT), "", &hp_bars_brighten, gameGetHPBarsHighlightMode() >= 0)) {
                            
                            gameSetHPBarsHighlightMode(hp_bars_brighten);
						}
						ImGui::EndMenu();
					}

					ShowFloatBarMenu();
#endif
					ImGui::EndMenu();
				}

				if (ShowStyleMenu(panel, style, &temp_flags)) {
					const ImGuiStyle default_style;
					if (bgdm_style.round_windows == 1) style.WindowRounding = default_style.WindowRounding;
					else style.WindowRounding = 0.0f;

					for (int i = 0; i < ARRAYSIZE(bgdm_colors); i++) {
						int idx = bgdm_colors[i].idx;
						if (idx >= 0 && idx < ImGuiCol_COUNT)
							style.Colors[bgdm_colors[i].idx] = ImColor(bgdm_colors[i].col);
					}

					style.Alpha = bgdm_style.global_alpha;
				}

				if (ShowLanguageMenu(use_tinyfont)) {}

#if (defined _DEBUG) || (defined DEBUG)
				if (g_state.panel_debug.enabled) {
					if (ImGui::BeginMenu("Debug"))
					{
						ImGui::MenuItem("BGDM Debug", NULL, &show_debug);
						ImGui::MenuItem("Unicode Test", NULL, &show_unicode_test);
						ImGui::MenuItem("Memory Editor", NULL, &mem_edit.IsOpen);
						ImGui::MenuItem("ImGui Demo", NULL, &show_demo_window);
                        ImGui::MenuItem("ImGui Test", NULL, &show_test_window);
						ImGui::EndMenu();
					}
				}
#endif
				ImGui::EndMenuBar();
			}

			//ImGuiStyle& style = ImGui::GetStyle();
			const ImVec4 colorActive = style.Colors[ImGuiCol_SliderGrabActive];
			const ImVec4 colorButton = style.Colors[ImGuiCol_Button];
			ImGui::PushStyleColor(ImGuiCol_SliderGrab, colorActive);
			ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, colorButton);
			ImGui::PushAllowKeyboardFocus(false);

			ImGui::Text(LOCALTEXT(TEXT_BGDM_MAIN_ONOFF)); ImGui::SameLine();
			int temp_int = g_state.global_on_off;
			ImGui::PushItemWidth(30);
			if (ImGui::SliderInt("##GlobalOnOff", &temp_int, 0, 1, NULL)) {
				g_state.global_on_off = temp_int ? true : false;
				config_set_int(CONFIG_SECTION_GLOBAL, "Enabled", g_state.global_on_off);
			}
			ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_BGDM_MAIN_ONOFF_HELP));
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text(LOCALTEXT(TEXT_BGDM_MAIN_INPUTCAP_ONOFF)); ImGui::SameLine();
			temp_int = !IsDisableInputOrActionCam();
			ImGui::PushItemWidth(30);
			if (ImGui::SliderInt("##GlobalInput", &temp_int, 0, 1, NULL)) {
				disable_input = temp_int ? false : true;
                setDisableCap(disable_input);
				g_state.global_cap_input = !disable_input;
				config_set_int(CONFIG_SECTION_GLOBAL, "CaptureInput", g_state.global_cap_input);
			}
			ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_BGDM_MAIN_INPUTCAP_ONOFF_HELP));

			ImGui::PopAllowKeyboardFocus();
			ImGui::PopStyleColor(2);

#if !(defined BGDM_TOS_COMPLIANT)
			bool needSaveFloat = false;
#endif
			bool needSaveHP = false;
			bool needSaveCompass = false;
			bool needSaveTarget = false;
			bool needSaveGroup = false;
			bool needSaveBuff = false;
			bool needSaveSkills = false;
			bool needSaveGearSelf = false;
			bool needSaveGearTarget = false;
  
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::Separator();
            
#if !(defined BGDM_TOS_COMPLIANT)
			bool tmp_bool = false;
			tmp_bool = g_state.panel_hp.enabled && g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_HP];
			if (ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_TARGET_HP), &tmp_bool)) {
				needSaveHP = true;
				if (!g_state.panel_hp.enabled) g_state.panel_hp.enabled = tmp_bool;
				g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_HP] = tmp_bool;
			}
			ImGui::SameLine(150);
			tmp_bool = g_state.panel_hp.enabled && g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_DIST];
			if (ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_TARGET_DIST), &tmp_bool)) {
				needSaveHP = true;
				if (!g_state.panel_hp.enabled) g_state.panel_hp.enabled = tmp_bool;
				g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_DIST] = tmp_bool;
			}
			ImGui::SameLine(300);
			tmp_bool = g_state.panel_hp.enabled && g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_BB];
			if (ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_TARGET_BB), &tmp_bool)) {
				needSaveHP = true;
				if (!g_state.panel_hp.enabled) g_state.panel_hp.enabled = tmp_bool;
				g_state.panel_hp.cfg.col_visible[HP_ROW_TARGET_BB] = tmp_bool;
			}
			tmp_bool = g_state.panel_hp.enabled && g_state.panel_hp.cfg.col_visible[HP_ROW_PLAYER_HP];
			if (ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_PLAYER_HP), &tmp_bool)) {
				needSaveHP = true;
				if (!g_state.panel_hp.enabled) g_state.panel_hp.enabled = tmp_bool;
				g_state.panel_hp.cfg.col_visible[HP_ROW_PLAYER_HP] = tmp_bool;
			}
			ImGui::SameLine(150);
#endif  // !(defined BGDM_TOS_COMPLIANT)
#if !(defined BGDM_TOS_COMPLIANT)
			tmp_bool = g_state.panel_hp.enabled && g_state.panel_hp.cfg.col_visible[HP_ROW_PORT_DIST];
			if (ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_PORT_DIST), &tmp_bool)) {
				needSaveHP = true;
				if (!g_state.panel_hp.enabled) g_state.panel_hp.enabled = tmp_bool;
				g_state.panel_hp.cfg.col_visible[HP_ROW_PORT_DIST] = tmp_bool;
			}
			ImGui::SameLine(300);
			needSaveFloat = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_FLOAT_BARS), (bool*)&g_state.panel_float.enabled);
			needSaveCompass = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_COMPASS), (bool*)&g_state.panel_compass.enabled);
			ImGui::Separator();
#endif
			needSaveTarget = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_TARGET_STATS), (bool*)&g_state.panel_dps_self.enabled); ImGui::SameLine(150);
			needSaveGroup = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_GROUP_STATS), (bool*)&g_state.panel_dps_group.enabled); ImGui::SameLine(300);
			needSaveBuff = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_BUFF_UPTIME), (bool*)&g_state.panel_buff_uptime.enabled);
			needSaveSkills = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_SKILLS), (bool*)&g_state.panel_skills.enabled);

#if !(defined BGDM_TOS_COMPLIANT)
				ImGui::SameLine(150);
				needSaveGearSelf = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_INSPECT_SELF), (bool*)&g_state.panel_gear_self.enabled); ImGui::SameLine(300);
				needSaveGearTarget = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_INSPECT_TARGET), (bool*)&g_state.panel_gear_target.enabled);
#else
            ImGui::SameLine(150);
            needSaveCompass = ImGui::Checkbox(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_COMPASS), (bool*)&g_state.panel_compass.enabled);
#endif
			ImGui::Separator();
			ImGui::PopStyleVar();

			if (imgui_has_colorblind_mode) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                static std::vector<const char *> items;
				if (items.empty() || localized_text_reloaded) {
					localized_text_reloaded = false;
					items.clear();
					items.push_back(LOCALTEXT(TEXT_GLOBAL_NONE));
					items.push_back(LOCALTEXT(TEXT_GLOBAL_COLORBLIND_MODE1));
					items.push_back(LOCALTEXT(TEXT_GLOBAL_COLORBLIND_MODE2));
					items.push_back(LOCALTEXT(TEXT_GLOBAL_COLORBLIND_MODE3));
				}
				ImGui::Text(LOCALTEXT(TEXT_GLOBAL_COLORBLIND));
				ImGui::SameLine(); ShowHelpMarker(LOCALTEXT(TEXT_GLOBAL_COLORBLIND_HELP));
				ImGui::SameLine();
				ImGui::PushItemWidth(120);
				int mode = imgui_colorblind_mode + 1;
				if (ImGui::Combo("##combo", &mode, items.data(), (int)items.size())) {
					imgui_colorblind_mode = mode - 1;
					config_set_int(CONFIG_SECTION_GLOBAL, "ColorblindMode", imgui_colorblind_mode);
				}
				ImGui::PopItemWidth();
				ImGui::PopStyleVar();
			}

#if !(defined BGDM_TOS_COMPLIANT)
			if (needSaveFloat) ImGui_FloatBarConfigSave();
#endif
			if (needSaveHP) PanelSaveColumnConfig(&g_state.panel_hp);
			if (needSaveCompass) PanelSaveColumnConfig(&g_state.panel_compass);
			if (needSaveTarget) PanelSaveColumnConfig(&g_state.panel_dps_self);
			if (needSaveGroup) PanelSaveColumnConfig(&g_state.panel_dps_group);
			if (needSaveBuff) PanelSaveColumnConfig(&g_state.panel_buff_uptime);
			if (needSaveSkills) PanelSaveColumnConfig(&g_state.panel_skills);
			if (needSaveGearSelf) PanelSaveColumnConfig(&g_state.panel_gear_self);
			if (needSaveGearTarget) PanelSaveColumnConfig(&g_state.panel_gear_target);
		}
		ImGui::End();
	}
	ImGui::PopFont();
}

static ImColor ColorFromCharacter(const Character *c, bool bUseColor)
{
    assert(c != NULL);
    if (!c) return ImColor(bgdm_colors[BGDM_COL_TEXT_PLAYER].col);
    
    if (!bUseColor) {
        bool isPlayerDead = !c->is_alive && !c->is_downed;
        if (isPlayerDead)
            return ImColor(bgdm_colors[BGDM_COL_TEXT_PLAYER_DEAD].col);
        else if (controlledCharacter()->cptr == c->cptr)
            return ImColor(bgdm_colors[BGDM_COL_TEXT_PLAYER_SELF].col);
        else
            return ImColor(bgdm_colors[BGDM_COL_TEXT_PLAYER].col);
    }
    
    ImColor col = ImColor(bgdm_colors[BGDM_COL_TEXT_PLAYER].col);
    if (c->profession > GW2::PROFESSION_NONE && c->profession < GW2::PROFESSION_END)
        col = ImColor(prof_colors[c->is_elite][c->profession]);
    return col;
}

#if !(defined BGDM_TOS_COMPLIANT)
static inline void DrawTextAndFrame(float per, ImVec2 pos, ImVec2 size, ImU32 col_text, ImU32 col_frame, float rounding = 0.0f, float thickness = 1.0f, bool draw_per = true, float dist = 0.0f)
{
	float y = (float)pos.y;
	float x = (float)pos.x - size.x / 2;
	if (per < 0.0f) per = 0.0f;
	if (per > 1.0f) per = 1.0f;

	uint32_t hp = (uint32_t)ceilf(per * 100.0f);
	hp = ImMin(hp, 100);

	uint32_t num = 0;
	num += (hp > 9);
	num += (hp > 99);

    std::string str = toString("%u%%", hp);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	float text_height = ImGui::CalcTextSize("100%%", NULL, true).y;
	float text_y = (text_height < size.y) ? pos.y + (size.y-text_height)/2 : pos.y + 1.0f;

	if (draw_per) {
		if (dist == 0.0f)
			draw_list->AddText(ImVec2(pos.x - num*TINY_FONT_WIDTH / 2, text_y), col_text, str.c_str());
		else
			draw_list->AddText(ImVec2(x + ImGui::CalcTextSize("1").x, text_y), col_text, str.c_str());
	}
	if (thickness > 0.0f) draw_list->AddRect(ImVec2(x, y), ImVec2(x, y) + size, col_frame, rounding, -1, thickness);
	if (dist > 0.0f) {
		str = toString("%.0f ", dist);
		x = pos.x + size.x / 2 - ImGui::CalcTextSize(str.c_str()).x;
		draw_list->AddText(ImVec2(x, text_y), col_text, str.c_str());
	}
}

static void DrawRectPercentage(float per, ImVec2 pos, ImVec2 size, ImU32 col_left, ImU32 col_right, ImU32 col_frame, float rounding = 0.0f, bool draw_frame = true)
{
	
	float y = (float)pos.y;
	float x = (float)pos.x - size.x / 2;
	if (per < 0.0f) per = 0.0f;
	if (per > 1.0f) per = 1.0f;

	ImVec2 rect_size_left(size.x * per, size.y);
	ImVec2 rect_size_right(size.x * (1.0f - per), size.y);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	if (rect_size_left.x > 0.0f)
		draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x, y) + rect_size_left, col_left, rounding, -1);

	if (rect_size_right.x > 0.0f)
			draw_list->AddRectFilled(
				ImVec2(x + rect_size_left.x, y),
				ImVec2(x + rect_size_left.x, y) + rect_size_right,
				col_right, rounding, -1);

	if (draw_frame)
		draw_list->AddRect(ImVec2(x, y), ImVec2(x, y) + size, col_frame, rounding, -1);
}


enum FloatBarColors {
	FLOAT_COL_BG,
	FLOAT_COL_TEXT,
	FLOAT_COL_FRAME,
	FLOAT_COL_HP_LEFT,
	FLOAT_COL_HP_LEFT90,
	FLOAT_COL_HP_RIGHT,
	FLOAT_COL_DOWN_LEFT,
	FLOAT_COL_DOWN_RIGHT,
	FLOAT_COL_SHROUD_LEFT,
	FLOAT_COL_SHROUD_RIGHT,
	FLOAT_COL_GOTL_LEFT,
	FLOAT_COL_GOTL_RIGHT,
	FLOAT_COL_HP_END
};

typedef struct FloatBarConfig
{
	bool draw_hp;
	bool draw_cls;
	bool draw_gotl;
	int mode_self;
	int disp_mode;
	int dist_mode;
	int per_mode;
	int text_mode;
	int cls_mode;
	int col_mode;
	//int closest_max;
	int frame_round_mode;
	float frame_round_default;
	float frame_round;
	float frame_thick;
	float vert_adjust;
	float icon_size;
	float rect_width;
	float rect_height_hp;
	float rect_height_gotl;
	uint32_t colors[FLOAT_COL_HP_END];
} FloatBarConfig;

static FloatBarConfig flt_config;

static inline void FloatBarSetDefaults(FloatBarConfig &flt)
{
	flt.draw_hp = true;
	flt.draw_cls = true;
	flt.draw_gotl = true;
	flt.mode_self = 0;
	flt.disp_mode = 0;
	flt.dist_mode = 0;
	flt.per_mode = 0;
	flt.text_mode = 0;
	flt.cls_mode = 0;
	flt.col_mode = 1;
	//flt.closest_max = 9;
	flt.frame_round_mode = 1;
	flt.frame_round_default = 2.0f;
	flt.frame_round = flt.frame_round_default;
	flt.frame_thick = 2.0f;
	flt.vert_adjust = 0.0f;
	flt.rect_width = 80.0f;
	flt.rect_height_hp = 13.0f;
	flt.rect_height_gotl = 9.0f;
	flt.icon_size = flt.rect_height_hp + flt.rect_height_gotl - 1.0f;

	int default_bg = -1272568280;
	flt.colors[FLOAT_COL_BG] = default_bg;
	flt.colors[FLOAT_COL_TEXT] = ImColor(IMGUI_BLACK);
	flt.colors[FLOAT_COL_FRAME] = ImColor(IMGUI_BLACK);
	flt.colors[FLOAT_COL_HP_LEFT] = ImColor(IMGUI_GREEN);
	flt.colors[FLOAT_COL_HP_LEFT90] = flt.colors[FLOAT_COL_HP_LEFT];
	flt.colors[FLOAT_COL_HP_RIGHT] = ImColor(IMGUI_RED);
	flt.colors[FLOAT_COL_DOWN_LEFT] = ImColor(IMGUI_DARK_RED);
	flt.colors[FLOAT_COL_DOWN_RIGHT] = default_bg;
	flt.colors[FLOAT_COL_SHROUD_LEFT] = ImColor(IMGUI_DARK_GREEN);
	flt.colors[FLOAT_COL_SHROUD_RIGHT] = default_bg;
	flt.colors[FLOAT_COL_GOTL_LEFT] = ImColor(IMGUI_CYAN);
	flt.colors[FLOAT_COL_GOTL_RIGHT] = default_bg;
}

FloatBarConfig* ImGui_FloatBarGet()
{
	return &flt_config;
}

void ImGui_FloatBarSetDefaults()
{
	FloatBarSetDefaults(flt_config);
}

void ImGui_FloatBarConfigLoad()
{
	const struct Panel* panel = &g_state.panel_float;
	ImGui_FloatBarSetDefaults();

	FloatBarConfig &flt = flt_config;
	flt.draw_hp = config_get_int(panel->section, "DrawHP", flt.draw_hp) > 0;
	flt.draw_cls = config_get_int(panel->section, "DrawClass", flt.draw_cls) > 0;
	flt.draw_gotl = config_get_int(panel->section, "DrawGOTL", flt.draw_gotl) > 0;
	flt.mode_self = config_get_int(panel->section, "DrawSelf", flt.mode_self);
	flt.text_mode = config_get_int(panel->section, "TextMode", flt.text_mode);
	flt.cls_mode = config_get_int(panel->section, "ClassMode", flt.cls_mode);
	flt.col_mode = config_get_int(panel->section, "ColorPickerMode", flt.col_mode);
	flt.frame_round_mode = config_get_int(panel->section, "RoundMode", flt.frame_round_mode);
	flt.per_mode = config_get_int(panel->section, "PercentMode", flt.per_mode);
	//flt.closest_max = config_get_int(panel->section, "PlayersMax", flt.closest_max);
	flt.frame_thick = config_get_float(panel->section, "FrameThickness", flt.frame_thick);
	flt.vert_adjust = config_get_float(panel->section, "VerticalAdjust", flt.vert_adjust);
	flt.rect_width = config_get_float(panel->section, "RectWidth", flt.rect_width);
	flt.rect_height_hp = config_get_float(panel->section, "RectHeightHP", flt.rect_height_hp);
	flt.rect_height_gotl = config_get_float(panel->section, "RectHeightGOTL", flt.rect_height_gotl);
	flt.icon_size = config_get_float(panel->section, "ClassRectSize", flt.icon_size);

	flt.colors[FLOAT_COL_BG] = config_get_int(panel->section, "ColClassBG", flt.colors[FLOAT_COL_BG]);
	flt.colors[FLOAT_COL_TEXT] = config_get_int(panel->section, "ColText", flt.colors[FLOAT_COL_TEXT]);
	flt.colors[FLOAT_COL_FRAME] = config_get_int(panel->section, "ColFrame", flt.colors[FLOAT_COL_FRAME]);
	flt.colors[FLOAT_COL_HP_LEFT] = config_get_int(panel->section, "ColHPLeft", flt.colors[FLOAT_COL_HP_LEFT]);
	flt.colors[FLOAT_COL_HP_LEFT90] = config_get_int(panel->section, "ColHPLeft90", flt.colors[FLOAT_COL_HP_LEFT]);
	flt.colors[FLOAT_COL_HP_RIGHT] = config_get_int(panel->section, "ColHPRight", flt.colors[FLOAT_COL_HP_RIGHT]);
	flt.colors[FLOAT_COL_DOWN_LEFT] = config_get_int(panel->section, "ColDownLeft", flt.colors[FLOAT_COL_DOWN_LEFT]);
	flt.colors[FLOAT_COL_DOWN_RIGHT] = config_get_int(panel->section, "ColDownRight", flt.colors[FLOAT_COL_DOWN_RIGHT]);
	flt.colors[FLOAT_COL_SHROUD_LEFT] = config_get_int(panel->section, "ColShroudLeft", flt.colors[FLOAT_COL_SHROUD_LEFT]);
	flt.colors[FLOAT_COL_SHROUD_RIGHT] = config_get_int(panel->section, "ColShroudRight", flt.colors[FLOAT_COL_SHROUD_RIGHT]);
	flt.colors[FLOAT_COL_GOTL_LEFT] = config_get_int(panel->section, "ColGOTLLeft", flt.colors[FLOAT_COL_GOTL_LEFT]);
	flt.colors[FLOAT_COL_GOTL_RIGHT] = config_get_int(panel->section, "ColGOTLRight", flt.colors[FLOAT_COL_GOTL_RIGHT]);
}

void ImGui_FloatBarConfigSave()
{
	const struct Panel* panel = &g_state.panel_float;
	config_set_int(panel->section, "Enabled", panel->enabled);

	FloatBarConfig &flt = flt_config;
	config_set_int(panel->section, "DrawHP", flt.draw_hp);
	config_set_int(panel->section, "DrawClass", flt.draw_cls);
	config_set_int(panel->section, "DrawGOTL", flt.draw_gotl);
	config_set_int(panel->section, "DrawSelf", flt.mode_self);
	config_set_int(panel->section, "TextMode", flt.text_mode);
	config_set_int(panel->section, "ClassMode", flt.cls_mode);
	config_set_int(panel->section, "ColorPickerMode", flt.col_mode);
	config_set_int(panel->section, "RoundMode", flt.frame_round_mode);
	config_set_int(panel->section, "PercentMode", flt.per_mode);
	//config_set_int(panel->section, "PlayersMax", flt.closest_max);
	config_set_float(panel->section, "FrameThickness", flt.frame_thick);
	config_set_float(panel->section, "VerticalAdjust", flt.vert_adjust);
	config_set_float(panel->section, "RectWidth", flt.rect_width);
	config_set_float(panel->section, "RectHeightHP", flt.rect_height_hp);
	config_set_float(panel->section, "RectHeightGOTL", flt.rect_height_gotl);
	config_set_float(panel->section, "ClassRectSize", flt.icon_size);

	config_set_int(panel->section, "ColClassBG", flt.colors[FLOAT_COL_BG]);
	config_set_int(panel->section, "ColText", flt.colors[FLOAT_COL_TEXT]);
	config_set_int(panel->section, "ColFrame", flt.colors[FLOAT_COL_FRAME]);
	config_set_int(panel->section, "ColHPLeft", flt.colors[FLOAT_COL_HP_LEFT]);
	config_set_int(panel->section, "ColHPLeft90", flt.colors[FLOAT_COL_HP_LEFT90]);
	config_set_int(panel->section, "ColHPRight", flt.colors[FLOAT_COL_HP_RIGHT]);
	config_set_int(panel->section, "ColDownLeft", flt.colors[FLOAT_COL_DOWN_LEFT]);
	config_set_int(panel->section, "ColDownRight", flt.colors[FLOAT_COL_DOWN_RIGHT]);
	config_set_int(panel->section, "ColShroudLeft", flt.colors[FLOAT_COL_SHROUD_LEFT]);
	config_set_int(panel->section, "ColShroudRight", flt.colors[FLOAT_COL_SHROUD_RIGHT]);
	config_set_int(panel->section, "ColGOTLLeft", flt.colors[FLOAT_COL_GOTL_LEFT]);
	config_set_int(panel->section, "ColGOTLRight", flt.colors[FLOAT_COL_GOTL_RIGHT]);
}


static void DrawFloatBar(struct Character& c, struct Player& player)
{
	float y_adjust = 0.0f;
	FloatBarConfig &flt = flt_config;

	if (!flt.draw_hp && !flt.draw_cls && !flt.draw_gotl) return;
	if (c.aptr == 0 || c.cptr == 0) return;

	// Adjust the frame rouding based on user request
	if (flt.frame_round_mode == 0) flt.frame_round = 0.0f;
	else flt.frame_round = flt.frame_round_default;

	ImVec2 rect_hp = ImVec2(flt.rect_width, flt.rect_height_hp);
	ImVec2 rect_gotl = ImVec2(flt.rect_width, flt.rect_height_gotl);

    vec2 p = { 0 };
	if (!AgentPosProject2D(c.apos, &p))
		return;	// pt is out of screen bounds

    
	if (flt.vert_adjust > 0.0f)
		p.y -= (int)flt.vert_adjust;

	bool isDowned = c.is_downed;
	bool isDead = !c.is_alive && !isDowned;
	if (isDead) return;

	if (flt.draw_hp) {

		y_adjust = rect_hp.y - 1.0f;
		float hp_per = (float)c.hp_val / c.hp_max;
		hp_per = ImClamp(hp_per, 0.0f, 100.0f);

		float dist = 0.0f;
		ImU32 col_hp_left = (hp_per < 0.90f) ? flt.colors[FLOAT_COL_HP_LEFT90] : flt.colors[FLOAT_COL_HP_LEFT];
		ImU32 col_hp_right = flt.colors[FLOAT_COL_HP_RIGHT];

		if (isDowned) {
			DrawRectPercentage(hp_per, ImVec2(p.x, p.y), rect_hp,
				flt.colors[FLOAT_COL_DOWN_LEFT], flt.colors[FLOAT_COL_DOWN_RIGHT], flt.colors[FLOAT_COL_FRAME], flt.frame_round, flt.frame_thick > 0.0f);
		}
		else if (c.profession == GW2::PROFESSION_NECROMANCER && c.stance == GW2::STANCE_NECRO_SHROUD) {
			float per = hp_per;
            if (c.energy_max != 0)
                per = c.energy_cur / c.energy_max;
			ImVec2 rect_size(rect_hp.x, rect_hp.y / 2.0f);
			DrawRectPercentage(per, ImVec2(p.x, p.y), rect_hp,
				flt.colors[FLOAT_COL_SHROUD_LEFT], flt.colors[FLOAT_COL_SHROUD_RIGHT], flt.colors[FLOAT_COL_FRAME], flt.frame_round, flt.frame_thick > 0.0f);
			DrawRectPercentage(hp_per, ImVec2(p.x, p.y), rect_size,
				col_hp_left, col_hp_right, flt.colors[FLOAT_COL_FRAME], flt.frame_round, false);
			hp_per = per;
		}
		else {
			DrawRectPercentage(hp_per, ImVec2(p.x, p.y), rect_hp,
				col_hp_left, col_hp_right, flt.colors[FLOAT_COL_FRAME], flt.frame_round, flt.frame_thick > 0.0f);
		}

		if (flt.text_mode == 0) ImGui::PushFont(proggyTiny);
		else ImGui::PushFont(proggyClean);
		DrawTextAndFrame(hp_per, ImVec2(p.x, p.y), rect_hp,
			flt.colors[FLOAT_COL_TEXT], flt.colors[FLOAT_COL_FRAME],
			flt.frame_round, flt.frame_thick, flt.per_mode == 0, dist);
		ImGui::PopFont();
	}

	if (flt.draw_gotl) {

		static const int gotl_max = 5;
        uint32_t gotl_stacks = player.buffs.GOTL;
		float y = p.y + y_adjust;
		float per = (float)gotl_stacks / gotl_max;
		DrawRectPercentage(per, ImVec2(p.x, y), rect_gotl, flt.colors[FLOAT_COL_GOTL_LEFT],
			flt.colors[FLOAT_COL_GOTL_RIGHT], flt.colors[FLOAT_COL_FRAME], flt.frame_round);

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		float fraction = rect_gotl.x / gotl_max;
		float x = p.x - rect_gotl.x / 2;
		for (int j = 0; j < gotl_max; j++) {
			draw_list->AddRect(ImVec2(x, y), ImVec2(x + fraction, y + rect_gotl.y), flt.colors[FLOAT_COL_FRAME], flt.frame_round, -1, flt.frame_thick);
			x += fraction;
		}

		y_adjust += rect_gotl.y;
	}

	if (flt.draw_cls) {

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 icon_size(flt.icon_size, flt.icon_size);
		float x = (flt.draw_hp || flt.draw_gotl) ? p.x - rect_gotl.x / 2 - icon_size.x : p.x - icon_size.x / 2;
		float y = (y_adjust < flt.icon_size) ? p.y - (flt.icon_size- y_adjust)/2 : p.y + (y_adjust - flt.icon_size)/2;
		ImVec2 pos(flt.frame_thick > 0.0f ? x+1.0f : x, y);

		draw_list->AddRectFilled(pos, pos + icon_size,
			flt.cls_mode == 1 ? ColorFromCharacter(&c, true) : (ImColor)flt.colors[FLOAT_COL_BG],
			flt.frame_round);

		if (flt.frame_thick > 0.0f)
			draw_list->AddRect(pos, pos + icon_size, flt.colors[FLOAT_COL_FRAME], flt.frame_round, -1, flt.frame_thick);

		if (flt.cls_mode != 1) {
			void* img = ProfessionImageFromPlayer(&c);
			if (img) draw_list->AddImage(img, pos + ImVec2(1, 1), pos + icon_size - ImVec2(2, 2));
		}
	}
}

static void ShowFloatBarMenu()
{
    static char buff[128];
	const struct Panel* panel = &g_state.panel_float;
	FloatBarConfig &flt = flt_config;
	bool dirty = false;

	if (ImGui::BeginMenu(LOCALTEXT(TEXT_FLOATBAR_TITLE)))
	{
		static const float offset_help = 160.0f;
		static const float offset = offset_help + 25.0f;
		static const float offset_radio = offset + 60.0f;
		dirty |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_GLOBAL_ENABLED), keyBindToString(panel->bind, buff, sizeof(buff)), (bool*)&panel->enabled);
		ImGui::Separator();
		dirty |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_FLOATBAR_HP), "", &flt.draw_hp);
		dirty |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_FLOATBAR_GOTL), "", &flt.draw_gotl);
		dirty |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_FLOATBAR_CLASS), "", &flt.draw_cls);
		ImGui::Separator();

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{

			if (ImGui::Button(LOCALTEXT(TEXT_GLOBAL_RESTORE_DEF))) {
				ImGui_FloatBarSetDefaults();
				dirty = true;
			}
			ImGui::Separator();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_INC_SELF));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_INC_SELF_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##self"), &flt.mode_self, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##self"), &flt.mode_self, 0);
			ImGui::Separator();

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_BARVERT));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_BARVERT_HELP));
			ImGui::SameLine(offset);
			float slider_min = 0.0f;
			float slider_max = 500.0f;
			if (ImGui::DragFloat("##vert", &flt.vert_adjust, 1.00f, slider_min, slider_max, "%.0f")) {
				if (flt.vert_adjust <= slider_min) flt.vert_adjust = slider_min;
				if (flt.vert_adjust > slider_max) flt.vert_adjust = slider_max;
				dirty = true;
			}

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_BARWIDTH));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_BARWIDTH_HELP));
			ImGui::SameLine(offset);
			slider_min = 30.0f;
			slider_max = 200.0f;
			if (ImGui::DragFloat("##width", &flt.rect_width, 0.8f, slider_min, slider_max, "%.0f")) {
				if (flt.rect_width <= slider_min) flt.rect_width = slider_min;
				if (flt.rect_width > slider_max) flt.rect_width = slider_max;
				dirty = true;
			}

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_BARHEIGHT_HP));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_BARHEIGHT_HP_HELP));
			ImGui::SameLine(offset);
			slider_min = 5.0f;
			slider_max = 50.0f;
			if (ImGui::DragFloat("##hp", &flt.rect_height_hp, 0.2f, slider_min, slider_max, "%.0f")) {
				if (flt.rect_height_hp <= slider_min) flt.rect_height_hp = slider_min;
				if (flt.rect_height_hp > slider_max) flt.rect_height_hp = slider_max;
				dirty = true;
			}
			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_BARHEIGHT_GOTL));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_BARHEIGHT_GOTL_HELP));
			ImGui::SameLine(offset);
			if (ImGui::DragFloat("##gotl", &flt.rect_height_gotl, 0.2f, slider_min, slider_max, "%.0f")) {
				if (flt.rect_height_gotl <= slider_min) flt.rect_height_gotl = slider_min;
				if (flt.rect_height_gotl > slider_max) flt.rect_height_gotl = slider_max;
				dirty = true;
			}

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_CLASS_RECT));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_CLASS_RECT_HELP));
			ImGui::SameLine(offset);
			slider_min = 5.0f;
			slider_max = 50.0f;
			if (ImGui::DragFloat("##icon", &flt.icon_size, 0.2f, slider_min, slider_max, "%.0f")) {
				if (flt.icon_size <= slider_min) flt.icon_size = slider_min;
				if (flt.icon_size > slider_max) flt.icon_size = slider_max;
				dirty = true;
			}

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_CLASS_IND));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_CLASS_IND_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_ICON), &flt.cls_mode, 0); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_COLOR), &flt.cls_mode, 1);

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_SIZE));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_SIZE_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_FONT_SMALL), &flt.text_mode, 0); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT(TEXT_GLOBAL_FONT_NORMAL), &flt.text_mode, 1);

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_DRAW));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_DRAW_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##per"), &flt.per_mode, 0); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##per"), &flt.per_mode, 1);

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_FR_ROUND));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_FR_ROUND_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_YES, "##round"), &flt.frame_round_mode, 1); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_NO, "##round"), &flt.frame_round_mode, 0);

			ImGui::Text(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_FR_THICK));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_FLOATBAR_TEXTPER_FR_THICK_HELP));
			ImGui::SameLine(offset);
			if (ImGui::SliderFloat("##thick", &flt.frame_thick, 0.0f, 5.0f, "%.0f")) dirty = true;

			ImGui::Separator();

			ImGui::Text(LOCALTEXT(TEXT_GLOBAL_COLORPICK_MODE));
			ImGui::SameLine(offset_help); ShowHelpMarker(LOCALTEXT(TEXT_GLOBAL_COLORPICK_MODE_HELP));
			ImGui::SameLine(offset);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_RGB, "##coledit"), &flt.col_mode, 0); ImGui::SameLine(offset_radio);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_HSV, "##coledit"), &flt.col_mode, 1); ImGui::SameLine(offset_radio + 60.0f);
			dirty |= ImGui::RadioButton(LOCALTEXT_FMT(TEXT_GLOBAL_COLORPICK_HEX, "##coledit"), &flt.col_mode, 2);

			static float btn_width = 60.0f;
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_FRAME), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_FRAME], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_TEXTPER), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_TEXT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_CLASS_BG), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_BG], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_HP_LEFT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_HP_LEFT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_HP_90), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_HP_LEFT90], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_HP_RIGHT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_HP_RIGHT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_DOWN_LEFT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_DOWN_LEFT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_DOWN_RIGHT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_DOWN_RIGHT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_SHROUD_LEFT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_SHROUD_LEFT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_SHROUD_RIGHT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_SHROUD_RIGHT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_GOTL_LEFT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_GOTL_LEFT], offset_help);
			dirty |= ShowColorPickerMenu(LOCALTEXT(TEXT_FLOATBAR_COL_GOTL_RIGHT), "", offset, flt.col_mode, btn_width, flt.colors[FLOAT_COL_GOTL_RIGHT], offset_help);

			ImGui::PopStyleVar();
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

	if (dirty) ImGui_FloatBarConfigSave();
}

void ImGui_FloatBars(struct Panel* panel, struct Player *player)
{
	if (!panel->enabled) return;

	FloatBarConfig &flt = flt_config;
	if (flt.draw_hp || flt.draw_gotl || flt.draw_cls) {

		static bool open = true;

		ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

        int display_w, display_h;
        ImGui_GetDisplaySize(&display_w, &display_h);
        ImVec2 windowSize(display_w, display_h);
        ImGui::SetNextWindowSize(windowSize, ImGuiSetCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
		if (ImGui::Begin("##DummyWindow", &open, windowSize, 0.0f, fl)) {

			if (flt.mode_self) DrawFloatBar(player->c, *player);
            
            currentContext()->closestPlayers.iter_lock([](struct CacheEntry<Player>* entry) {
                if (flt_config.disp_mode == 1 && entry->value.data.c.attitude != CHAR_ATTITUDE_FRIENDLY) return;
                if (flt_config.disp_mode == 2 && entry->value.data.c.attitude == CHAR_ATTITUDE_FRIENDLY) return;
                DrawFloatBar(entry->value.data.c, entry->value.data);
            });
		}
		ImGui::End();
	}
}

void ImGui_HpAndDistance(struct Panel* panel,
                         struct Character *player,
                         struct Character *target)
{
    static bool open = true;
	if (!panel->enabled) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
    
    int display_w, display_h;
    ImGui_GetDisplaySize(&display_w, &display_h);
    ImVec2 fullScreen(display_w, display_h);
    
    int center = display_w / 2;
    int ui_scale = uiInterfaceSize();
    ui_scale = ui_scale > 3 ? 3 : ui_scale;
    ui_scale = ui_scale < 0 ? 0 : ui_scale;
    struct GFX* ui = g_gfx + ui_scale;
    struct GFXTarget set = ui->normal;
    
    if (target && target->agent_id)
    {
        if (target->type == AGENT_TYPE_ATTACK) {
            set = ui->attack;
            if (ui_scale < 3) set.hp.y -= 1;
        }
        else if (target->type == AGENT_TYPE_GADGET) {
            set = ui->gadget;
            if (ui_scale < 3) set.hp.y -= 1;
        }
    }
    
    if (currentMapType() >= MAP_TYPE_WVW_EB && currentMapType() <= MAP_TYPE_WVW_EOTM2)
    {
        static const i32 wvw_offset[4] = { 27, 30, 33, 36 };;
        set.hp.y += wvw_offset[ui_scale];
        set.bb.y += wvw_offset[ui_scale];
    }
    

	ImGui::PushFont(proggyTiny);
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(IMGUI_WHITE));
	if (panel->cfg.col_visible[HP_ROW_PLAYER_HP]) {

        uint32_t hp = (player->hp_max > 0) ? (uint32_t)ceilf(player->hp_val / (float)player->hp_max * 100.0f) : 0;
		hp = ImMin(hp, 100);

		uint32_t num = 0;
		num += (hp > 9);
		num += (hp > 99);

        float x = (float)center - num * TINY_FONT_WIDTH / 2;
        float y = (float)(display_h - ui->hp);

        ImGui::SetNextWindowSize(fullScreen, ImGuiSetCond_Always);
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
		if (ImGui::Begin("Player HP", &open, fullScreen, 0.0f, flags)) {
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            std::string str = toString("%u%%", hp);
            draw_list->AddText(ImVec2(x, y), ImColor(bgdm_style.hp_colors[HP_ROW_PLAYER_HP]), str.c_str());
		}
		ImGui::End();
	}

#if !(defined BGDM_TOS_COMPLIANT)
	if (panel->cfg.col_visible[HP_ROW_PORT_DIST] &&
        currentContext()->portalData.is_weave &&
        currentContext()->portalData.map_id == currentMapId()) {

		float dist = CalcDistance(currentContext()->portalData.apos, player->apos, false);
		dist = ImMin(dist, 99999.0f);

		float x = (float)center + ui->utils.x;
		float y = (float)(display_h - ui->utils.y);

        ImGui::SetNextWindowSize(fullScreen, ImGuiSetCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
		if (ImGui::Begin("Portal Distance", &open, fullScreen, 0.0f, flags)) {

            std::string str = toString("%s %d (%.2fs)", LOCALTEXT(TEXT_PORTAL_DISTANCE), (uint32_t)dist,
				60.01f - (g_now - currentContext()->portalData.time) / 1000000.0f);

			float xpadding = 12.0f;
			float ypadding = 4.0f;
			//ImGuiStyle &style = ImGui::GetStyle();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 size = ImGui::CalcTextSize(str.c_str());
			size.x += xpadding;
			size.y += ypadding;
			x -=  xpadding/2;
			y -=  ypadding/2;
			draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + size.x, y + size.y),
				ImColor(bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG]), 0.0f/*style.WindowRounding*/);
            draw_list->AddText(ImVec2(x + 7, y + 2), ImColor(bgdm_style.hp_colors[HP_ROW_PORT_DIST]), str.c_str());
		}
		ImGui::End();
	}
#endif

	bool drawTarget = true;
	if (!target->aptr || !target->agent_id)
		drawTarget = false;

	if (drawTarget && (target->is_player || target->is_clone)) {
#if (defined BGDM_TOS_COMPLIANT)
		drawTarget = false;
#else
		if (isCurrentMapPvPorWvW() && target->attitude != CHAR_ATTITUDE_FRIENDLY) {

			drawTarget = false;
		}
#endif
	}

	if (!drawTarget) {
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

    ImGui::SetNextWindowSize(fullScreen, ImGuiSetCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
    if (ImGui::Begin("##DummyTarget", &open, fullScreen, 0.0f, flags)) {
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        if (panel->cfg.col_visible[HP_ROW_TARGET_HP]) {

            float x = (float)center + set.hp.x;
            float y = (float)(set.hp.y);
            float hp = target->hp_max ? (float)target->hp_val / (float)target->hp_max * 100.f : 0.0f;
            ImClamp(hp, 0.0f, 100.0f);

            std::string hpstr;
            if (bgdm_style.hp_percent_mode) {
                if (bgdm_style.hp_precision_mode == 1) {

                    if (bgdm_style.hp_fixed_mode == 1) {
                        if (hp < 10.0f) hpstr = toString("[  %.2f%%] ", hp);
                        else if (hp < 100.0f) hpstr = toString("[ %.2f%%] ", hp);
                        else hpstr = toString("[%.2f%%] ", hp);
                    }
                    else hpstr = toString("[%.2f%%] ", hp);
                }
                else {
                    if (bgdm_style.hp_fixed_mode == 1) hpstr = toString("[%3d%%] ", (int)roundf(hp));
                    else hpstr = toString("[%d%%] ", (int)roundf(hp));
                }
            }

            if (bgdm_style.hp_comma_mode == 1) {
                char strHpVal[32] = { 0 };
                char strHpMax[32] = { 0 };
                hpstr += toString("%s / %s",
                    sprintf_num_commas(target->hp_val, strHpVal, sizeof(strHpVal)),
                    sprintf_num_commas(target->hp_max, strHpMax, sizeof(strHpMax)));
            }
            else {
                hpstr += toString("%d / %d", target->hp_val, target->hp_max);
            }

            draw_list->AddText(ImVec2(x, y), ImColor(bgdm_style.hp_colors[HP_ROW_TARGET_HP]), hpstr.c_str());
        }

        if (panel->cfg.col_visible[HP_ROW_TARGET_DIST]) {

            u32 dist = CalcDistance(player->apos, target->apos, false);
            dist = ImMin(dist, 9999);

            u32 num = 2;
            num += (dist > 9);
            num += (dist > 99);
            num += (dist > 999);

            float x = (float)center + set.dst - num * TINY_FONT_WIDTH / 2;
            float y = (float)(set.hp.y);

            std::string str = toString("%d", dist);
            draw_list->AddText(ImVec2(x, y), ImColor(bgdm_style.hp_colors[HP_ROW_TARGET_DIST]), str.c_str());
        }


        if (panel->cfg.col_visible[HP_ROW_TARGET_BB]) {

            if (target->bptr == 0 || (target->bb_state != BREAKBAR_READY && target->bb_state != BREAKBAR_REGEN))
            {
                //return;
            }
            else
            {

                u32 bb = target->bb_value;

                u32 num = 2;
                num += (bb > 9);
                num += (bb > 99);

                float x = (float)center + set.bb.x - num * TINY_FONT_WIDTH / 2;
                float y = (float)(set.bb.y);

                std::string str = toString("%u%%", bb);
                draw_list->AddText(ImVec2(x, y), ImColor(bgdm_style.hp_colors[HP_ROW_TARGET_BB]), str.c_str());
            }
        }
    }

    ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopFont();
}
#endif  // !BGDM_TOS_COMPLIANT

static inline float round_up_to_multiple_of(f32 f, f32 multiple)
{
	f32 remainder = (f32)fmod(fabs(f), multiple);
	if (remainder == 0)
		return f;

	if (f < 0)
		return -((float)fabs(f) - remainder);
	else
		return f + multiple - remainder;
}

float vec_angle_normalplane(vec3 vA, vec3 vB, vec3 vN)
{
    float dot = vA.x*vB.x + vA.y*vB.y + vA.z*vB.z;
    float det = vA.x*vB.y*vN.z + vB.x*vN.y*vA.z + vN.x*vA.y*vB.z -
    vA.z*vB.y*vN.x - vB.z*vN.y*vA.x - vN.z*vA.y*vB.x;
    float angle = (float)atan2(det, dot) * (float)(180 / M_PI);
    return angle;
}


float get_cam_direction(vec3 cam_dir)
{
    vec3 north = { 0 ,0, 1 };
    vec3 normal = { 0 ,1, 1 };
    float angle = vec_angle_normalplane(cam_dir, north, normal);
    bool to_north = 0;
    bool is_east = 0;
    if (abs((int)angle) <= 90) to_north = true;
    if (angle > 0) is_east = true;
    return (-angle);
}

void ImGui_Compass(struct Panel* panel, int x, int y, int w)
{
#define COMPASS_STR_LEN	38
	static char cardinals_str[COMPASS_STR_LEN + 1];
	static char indicator_long_str[COMPASS_STR_LEN + 1];
	static char indicator_short_str[COMPASS_STR_LEN + 1];
	static const f32 compass_inc_size = 5.0f;
	static float last_seen_angle = 181.0f;
    
    if (!panel->enabled) return;
    //if (!currentCam()->valid) return;

	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor gray(ImColor(bgdm_colors[BGDM_COL_TEXT2].col));
	ImColor cyan(ImColor(bgdm_colors[BGDM_COL_TEXT3].col));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;

	switch (bgdm_style.compass_font) {
	case(1):
		ImGui::PushFont(proggyClean);
		break;
	case(2):
		ImGui::PushFont(tinyFont != proggyTiny ? tinyFont : defaultFont);
		break;
	case(3):
		ImGui::PushFont(defaultFont);
		break;
	default:
		ImGui::PushFont(proggyTiny);
		break;
	}

	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin("Compass", (bool*)&panel->enabled, ImVec2(0, 0), panel->fAlpha, flags))
	{

        vec3 camDirection;
        camDirection.x = currentCam()->viewVec.x;
        camDirection.y = -currentCam()->viewVec.z;
        camDirection.z = currentCam()->viewVec.y;
		f32 angle = get_cam_direction(camDirection);
		f32 start_angle = angle - (COMPASS_STR_LEN / 2) * compass_inc_size;
		if (start_angle < -180) {
			start_angle += 360;
		}
		f32 rounded_angle = round_up_to_multiple_of(start_angle, compass_inc_size);
		//cur_x += (i32)(font_x / 2 + font_x * ((rounded_angle - start_angle) / compass_inc_size));

		// redraw the strings only if cam changed
		if (last_seen_angle == 181.00f ||
			last_seen_angle != angle) {

			// save angle
			last_seen_angle = angle;

			// reset the current compass strings
			memset(&cardinals_str[0], ' ', COMPASS_STR_LEN); cardinals_str[COMPASS_STR_LEN] = 0;
			memset(&indicator_long_str[0], ' ', COMPASS_STR_LEN); indicator_long_str[COMPASS_STR_LEN] = 0;
			memset(&indicator_short_str[0], ' ', COMPASS_STR_LEN); indicator_short_str[COMPASS_STR_LEN] = 0;

			// round up to nearest compass inc multiple
			// and adjust the cursor relative to starting position
			f32 cur_angle = rounded_angle;
			for (int i = 0; i < COMPASS_STR_LEN; i++)
			{
				f32 next_angle = cur_angle + compass_inc_size;
				if (next_angle > 180) {
					next_angle -= 360;
				}

				if (fabs(cur_angle) == 180.00) {
					cardinals_str[i] = 'S';
				}
				else if (cur_angle == -90.00) {
					cardinals_str[i] = 'W';
				}
				else if (cur_angle == 0.00) {
					cardinals_str[i] = 'N';
				}
				else if (cur_angle == 90.00) {
					cardinals_str[i] = 'E';
				}
				else if (fmod(cur_angle, compass_inc_size * 3) == 0) {
					indicator_long_str[i] = '|';
				}
				else {
					indicator_short_str[i] = '|';
				}

				cur_angle = next_angle;
			}
		}

		ImGui::SameLine(1);
		ImGui::TextColored(cyan, cardinals_str); ImGui::SameLine(1);
		ImGui::TextColored(white, indicator_long_str); ImGui::SameLine(1);
		ImGui::TextColored(gray, indicator_short_str);
	}

	ImGui::End();
	ImGui::PopFont();
}

static inline const char* GdpsAcronymFromColumn(int col)
{
	switch (col)
	{
	case(GDPS_COL_NAME): return LOCALTEXT(TEXT_GDPS_COLAC_NAME);
	case(GDPS_COL_CLS): return LOCALTEXT(TEXT_GDPS_COLAC_CLASS);
	case(GDPS_COL_DPS): return LOCALTEXT(TEXT_GDPS_COLAC_DPS);
	case(GDPS_COL_PER): return LOCALTEXT(TEXT_GDPS_COLAC_DMG_PER);
	case(GDPS_COL_DMGOUT): return LOCALTEXT(TEXT_GDPS_COLAC_DMG_OUT);
	case(GDPS_COL_DMGIN): return LOCALTEXT(TEXT_GDPS_COLAC_DNG_IN);
	case(GDPS_COL_HPS): return LOCALTEXT(TEXT_GDPS_COLAC_HPS);
	case(GDPS_COL_HEAL): return LOCALTEXT(TEXT_GDPS_COLAC_HEAL_OUT);
	case(GDPS_COL_TIME): return LOCALTEXT(TEXT_GDPS_COLAC_TIME);
	};
	return NULL;
}
static const char * GdpsTextFromColumn(int col)
{
	switch (col)
	{
	case(GDPS_COL_NAME): return LOCALTEXT(TEXT_GDPS_COL_NAME);
	case(GDPS_COL_CLS): return LOCALTEXT(TEXT_GDPS_COL_CLASS);
	case(GDPS_COL_DPS): return LOCALTEXT(TEXT_GDPS_COL_DPS);
	case(GDPS_COL_PER): return LOCALTEXT(TEXT_GDPS_COL_DMG_PER);
	case(GDPS_COL_DMGOUT): return LOCALTEXT(TEXT_GDPS_COL_DMG_OUT);
	case(GDPS_COL_DMGIN): return LOCALTEXT(TEXT_GDPS_COL_DNG_IN);
	case(GDPS_COL_HPS): return LOCALTEXT(TEXT_GDPS_COL_HPS);
	case(GDPS_COL_HEAL): return LOCALTEXT(TEXT_GDPS_COL_HEAL_OUT);
	case(GDPS_COL_TIME): return LOCALTEXT(TEXT_GDPS_COL_TIME);
	};
	return NULL;
}

static const char * BuffTextFromColumn(int col)
{
#define BUFF_COL_LOCALTEXT(x)	case(x): return LOCALTEXT(TEXT_##x)
	switch (col)
	{
		BUFF_COL_LOCALTEXT(BUFF_COL_NAME);
		BUFF_COL_LOCALTEXT(BUFF_COL_CLS);
		BUFF_COL_LOCALTEXT(BUFF_COL_SUB);
		BUFF_COL_LOCALTEXT(BUFF_COL_DOWN);
		BUFF_COL_LOCALTEXT(BUFF_COL_SCLR);
		BUFF_COL_LOCALTEXT(BUFF_COL_SEAW);
		BUFF_COL_LOCALTEXT(BUFF_COL_PROT);
		BUFF_COL_LOCALTEXT(BUFF_COL_QUIK);
		BUFF_COL_LOCALTEXT(BUFF_COL_ALAC);
		BUFF_COL_LOCALTEXT(BUFF_COL_FURY);
		BUFF_COL_LOCALTEXT(BUFF_COL_MIGHT);
		BUFF_COL_LOCALTEXT(BUFF_COL_GOTL);
		BUFF_COL_LOCALTEXT(BUFF_COL_GLEM);
		BUFF_COL_LOCALTEXT(BUFF_COL_VIGOR);
		BUFF_COL_LOCALTEXT(BUFF_COL_SWIFT);
		BUFF_COL_LOCALTEXT(BUFF_COL_STAB);
		BUFF_COL_LOCALTEXT(BUFF_COL_RETAL);
		BUFF_COL_LOCALTEXT(BUFF_COL_RESIST);
		BUFF_COL_LOCALTEXT(BUFF_COL_REGEN);
		BUFF_COL_LOCALTEXT(BUFF_COL_AEGIS);
		BUFF_COL_LOCALTEXT(BUFF_COL_BANS);
		BUFF_COL_LOCALTEXT(BUFF_COL_BAND);
		BUFF_COL_LOCALTEXT(BUFF_COL_BANT);
		BUFF_COL_LOCALTEXT(BUFF_COL_EA);
		BUFF_COL_LOCALTEXT(BUFF_COL_SPOTTER);
		BUFF_COL_LOCALTEXT(BUFF_COL_FROST);
		BUFF_COL_LOCALTEXT(BUFF_COL_SUN);
		BUFF_COL_LOCALTEXT(BUFF_COL_STORM);
		BUFF_COL_LOCALTEXT(BUFF_COL_STONE);
		BUFF_COL_LOCALTEXT(BUFF_COL_ENGPP);
		BUFF_COL_LOCALTEXT(BUFF_COL_REVNR);
		BUFF_COL_LOCALTEXT(BUFF_COL_REVAP);
		BUFF_COL_LOCALTEXT(BUFF_COL_REVRD);
		BUFF_COL_LOCALTEXT(BUFF_COL_ELESM);
		BUFF_COL_LOCALTEXT(BUFF_COL_GRDSN);
		BUFF_COL_LOCALTEXT(BUFF_COL_NECVA);
		BUFF_COL_LOCALTEXT(BUFF_COL_TIME);
	}
	return NULL;
}

static inline void* BuffIconFromColumn(int col)
{
    return ImGui_GetBuffColImage(col);
}

static const char* ProfessionName(uint32_t profession, uint32_t hasElite)
{
    const char* ret = NULL;
    switch (profession)
    {
        case(GW2::PROFESSION_GUARDIAN):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_FIREBRAND) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_DRAGONHUNTER) : LOCALTEXT(TEXT_PROF_GUARDIAN);
            break;
        case(GW2::PROFESSION_WARRIOR):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_SPELLBREAKER) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_BERSERKER) : LOCALTEXT(TEXT_PROF_WARRIOR);
            break;
        case(GW2::PROFESSION_ENGINEER):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_HOLOSMITH) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_SCRAPPER) : LOCALTEXT(TEXT_PROF_ENGINEER);
            break;
        case(GW2::PROFESSION_RANGER):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_SOULBEAST) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_DRUID) : LOCALTEXT(TEXT_PROF_RANGER);
            break;
        case(GW2::PROFESSION_THIEF):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_DEADEYE) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_DAREDEVIL) : LOCALTEXT(TEXT_PROF_THIEF);
            break;
        case(GW2::PROFESSION_ELEMENTALIST):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_WEAVER) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_TEMPEST) : LOCALTEXT(TEXT_PROF_ELEMENTALIST);
            break;
        case(GW2::PROFESSION_MESMER):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_MIRAGE) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_CHRONO) : LOCALTEXT(TEXT_PROF_MESMER);
            break;
        case(GW2::PROFESSION_NECROMANCER):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_SCOURGE) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_REAPER) : LOCALTEXT(TEXT_PROF_NECROMANCER);
            break;
        case(GW2::PROFESSION_REVENANT):
            ret = hasElite == 2 ? LOCALTEXT(TEXT_PROF_RENEGADE) : hasElite == 1 ? LOCALTEXT(TEXT_PROF_HERALD) : LOCALTEXT(TEXT_PROF_REVENANT);
            break;
        default:
            ret = LOCALTEXT(TEXT_GLOBAL_NONE);
    }
    return ret;
}
static inline void* ProfessionImageFromPlayer(const struct Character* player)
{
	if (!player) return NULL;
	return ImGui_GetProfImage(player->profession, player->is_elite);
}

void ImGui_StyleSetDefaults()
{
	// Defaults
	bgdm_style.global_alpha = 1.0f;
	bgdm_style.col_edit_mode = 2;
	bgdm_style.round_windows = 1;
	bgdm_style.compass_font = 0;
	bgdm_style.hp_comma_mode = !g_state.hpCommas_disable;
	bgdm_style.hp_fixed_mode = 0;
	bgdm_style.hp_precision_mode = 0;
	bgdm_style.hp_percent_mode = 1;

	ImColor white(IMGUI_WHITE);
	bgdm_style.hp_colors[HP_ROW_PLAYER_HP] = white;
	bgdm_style.hp_colors[HP_ROW_TARGET_HP] = white;
	bgdm_style.hp_colors[HP_ROW_TARGET_DIST] = white;
	bgdm_style.hp_colors[HP_ROW_TARGET_BB] = white;
	bgdm_style.hp_colors[HP_ROW_PORT_DIST] = white;
	bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG] = ImColor(0.0f, 0.0f, 0.0f, 0.75f);

	ImGuiStyle default_style;
	for (int i = 0; i < ARRAYSIZE(bgdm_colors); i++) {
		int idx = bgdm_colors[i].idx;
		if (idx >= 0 && idx < ImGuiCol_COUNT) {
			bgdm_colors[i].col = ImColor(default_style.Colors[idx]);
		}
	}

	bgdm_colors[BGDM_COL_TEXT1].col = ImColor(IMGUI_WHITE);
	bgdm_colors[BGDM_COL_TEXT2].col = ImColor(IMGUI_GRAY);
	bgdm_colors[BGDM_COL_TEXT3].col = ImColor(IMGUI_CYAN);
	bgdm_colors[BGDM_COL_TEXT_PLAYER].col = ImColor(IMGUI_WHITE);
	bgdm_colors[BGDM_COL_TEXT_PLAYER_SELF].col = ImColor(IMGUI_CYAN);
	bgdm_colors[BGDM_COL_TEXT_PLAYER_DEAD].col = ImColor(IMGUI_RED);
	bgdm_colors[BGDM_COL_TEXT_TARGET].col = ImColor(IMGUI_RED);
	bgdm_colors[BGDM_COL_GRAD_START].col = ImColor(IMGUI_CYAN);
	bgdm_colors[BGDM_COL_GRAD_END].col = ImColor(IMGUI_BLACK);
	bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col = ImColor(IMGUI_RED);

	bgdm_colors_default.clear();
	bgdm_colors_default.resize(ARRAYSIZE(bgdm_colors));
	for (int i = 0; i < ARRAYSIZE(bgdm_colors); i++) {
		bgdm_colors_default[i] = bgdm_colors[i];
	}

	for (int j = 0; j < GW2::PROFESSION_END; j++) {
		switch (j)
		{
		case (GW2::PROFESSION_GUARDIAN):
			prof_colors[0][j] = ImColor(IMGUI_CYAN);
			break;
		case (GW2::PROFESSION_WARRIOR):
			prof_colors[0][j] = ImColor(IMGUI_ORANGE);
			break;
		case (GW2::PROFESSION_ENGINEER):
			prof_colors[0][j] = ImColor(IMGUI_LIGHT_PURPLE);
			break;
		case (GW2::PROFESSION_RANGER):
			prof_colors[0][j] = ImColor(IMGUI_GREEN);
			break;
		case (GW2::PROFESSION_THIEF):
			prof_colors[0][j] = ImColor(IMGUI_RED);
			break;
		case (GW2::PROFESSION_ELEMENTALIST):
			prof_colors[0][j] = ImColor(IMGUI_YELLOW);
			break;
		case (GW2::PROFESSION_MESMER):
			prof_colors[0][j] = ImColor(IMGUI_PINK);
			break;
		case (GW2::PROFESSION_NECROMANCER):
			prof_colors[0][j] = ImColor(IMGUI_DARK_GREEN);
			break;
		case (GW2::PROFESSION_REVENANT):
			prof_colors[0][j] = ImColor(IMGUI_LIGHT_BLUE);
			break;
		case (GW2::PROFESSION_NONE):
		case (GW2::PROFESSION_END):
		default:
			break;
		}
		prof_colors[1][j] = prof_colors[0][j];
        prof_colors[2][j] = prof_colors[0][j];
	}
}

void ImGui_StyleConfigLoad()
{
	const ImGuiStyle default_style;
	ImGuiStyle &style = ImGui::GetStyle();
	ImGui_StyleSetDefaults();

	g_state.show_metrics_bgdm = config_get_int(CONFIG_SECTION_OPTIONS, "ShowMetricsBGDM", 0) == 1;
	imgui_colorblind_mode = config_get_int(CONFIG_SECTION_GLOBAL, "ColorblindMode", -1);

	style.Alpha = bgdm_style.global_alpha = config_get_float(CONFIG_SECTION_STYLE, "GlobalAlpha", bgdm_style.global_alpha);
	bgdm_style.col_edit_mode = config_get_int(CONFIG_SECTION_STYLE, "ColorEditMode", bgdm_style.col_edit_mode);
	bgdm_style.round_windows = config_get_int(CONFIG_SECTION_STYLE, "WindowRounding", bgdm_style.round_windows);
	if (bgdm_style.round_windows == 1) style.WindowRounding = default_style.WindowRounding;
	else style.WindowRounding = 0.0f;

	bgdm_style.compass_font = config_get_int(CONFIG_SECTION_STYLE, "CompassFont", bgdm_style.compass_font);

	bgdm_style.hp_comma_mode = config_get_int(CONFIG_SECTION_STYLE, "TargetHPComma", bgdm_style.hp_comma_mode);
	bgdm_style.hp_fixed_mode = config_get_int(CONFIG_SECTION_STYLE, "TargetHPPerFixed", bgdm_style.hp_fixed_mode);
	bgdm_style.hp_percent_mode = config_get_int(CONFIG_SECTION_STYLE, "TargetHPPerOnOff", bgdm_style.hp_percent_mode);
	bgdm_style.hp_precision_mode = config_get_int(CONFIG_SECTION_STYLE, "TargetHPPrecision", bgdm_style.hp_precision_mode);

	bgdm_style.hp_colors[HP_ROW_PLAYER_HP] = config_get_int(CONFIG_SECTION_STYLE, "PlayerHPColor", bgdm_style.hp_colors[HP_ROW_PLAYER_HP]);
	bgdm_style.hp_colors[HP_ROW_TARGET_HP] = config_get_int(CONFIG_SECTION_STYLE, "TargetHPColor", bgdm_style.hp_colors[HP_ROW_TARGET_HP]);
	bgdm_style.hp_colors[HP_ROW_TARGET_DIST] = config_get_int(CONFIG_SECTION_STYLE, "TargetBBColor", bgdm_style.hp_colors[HP_ROW_TARGET_DIST]);
	bgdm_style.hp_colors[HP_ROW_TARGET_BB] = config_get_int(CONFIG_SECTION_STYLE, "TargetDistColor", bgdm_style.hp_colors[HP_ROW_TARGET_BB]);
	bgdm_style.hp_colors[HP_ROW_PORT_DIST] = config_get_int(CONFIG_SECTION_STYLE, "PortDistColor", bgdm_style.hp_colors[HP_ROW_PORT_DIST]);
	bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG] = config_get_int(CONFIG_SECTION_STYLE, "PortDistColorBG", bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG]);

	for (int i = 0; i < ARRAYSIZE(bgdm_colors); i++) {
		bgdm_colors[i].col = config_get_int(CONFIG_SECTION_STYLE, bgdm_colors[i].cfg, bgdm_colors[i].col);
        CLogTrace("%s: %d", bgdm_colors[i].cfg, bgdm_colors[i].col);
		int idx = bgdm_colors[i].idx;
		if (idx >= 0 && idx < ImGuiCol_COUNT)
			style.Colors[bgdm_colors[i].idx] = ImColor(bgdm_colors[i].col);
	}

	for (int i = 0; i < 3; i++) {
		for (int j = GW2::PROFESSION_GUARDIAN; j < GW2::PROFESSION_END; j++) {
            std::string str = toString("ProfColor_%d%d", i, j);
			prof_colors[i][j] = config_get_int(CONFIG_SECTION_STYLE, str.c_str(), prof_colors[i][j]);
		}
	}
}

void ImGui_StyleConfigSave()
{
	config_set_int(CONFIG_SECTION_OPTIONS, "ShowMetricsBGDM", g_state.show_metrics_bgdm);

	config_set_float(CONFIG_SECTION_STYLE, "GlobalAlpha", bgdm_style.global_alpha);
	config_set_int(CONFIG_SECTION_STYLE, "ColorEditMode", bgdm_style.col_edit_mode);
	config_set_int(CONFIG_SECTION_STYLE, "WindowRounding", bgdm_style.round_windows);

	config_set_int(CONFIG_SECTION_STYLE, "CompassFont", bgdm_style.compass_font);

	config_set_int(CONFIG_SECTION_STYLE, "TargetHPComma", bgdm_style.hp_comma_mode);
	config_set_int(CONFIG_SECTION_STYLE, "TargetHPPerFixed", bgdm_style.hp_fixed_mode);
	config_set_int(CONFIG_SECTION_STYLE, "TargetHPPerOnOff", bgdm_style.hp_percent_mode);
	config_set_int(CONFIG_SECTION_STYLE, "TargetHPPrecision", bgdm_style.hp_precision_mode);

	config_set_int(CONFIG_SECTION_STYLE, "PlayerHPColor", bgdm_style.hp_colors[HP_ROW_PLAYER_HP]);
	config_set_int(CONFIG_SECTION_STYLE, "TargetHPColor", bgdm_style.hp_colors[HP_ROW_TARGET_HP]);
	config_set_int(CONFIG_SECTION_STYLE, "TargetBBColor", bgdm_style.hp_colors[HP_ROW_TARGET_DIST]);
	config_set_int(CONFIG_SECTION_STYLE, "TargetDistColor", bgdm_style.hp_colors[HP_ROW_TARGET_BB]);
	config_set_int(CONFIG_SECTION_STYLE, "PortDistColor", bgdm_style.hp_colors[HP_ROW_PORT_DIST]);
	config_set_int(CONFIG_SECTION_STYLE, "PortDistColorBG", bgdm_style.hp_colors[HP_ROW_PORT_DIST_BG]);

	for (int i = 0; i < ARRAYSIZE(bgdm_colors); i++)
		config_set_int(CONFIG_SECTION_STYLE, bgdm_colors[i].cfg, bgdm_colors[i].col);

	for (int i = 0; i < 3; i++) {
		for (int j = GW2::PROFESSION_GUARDIAN; j < GW2::PROFESSION_END; j++) {
            std::string str = toString("ProfColor_%d%d", i, j);
			config_set_int(CONFIG_SECTION_STYLE, str.c_str(), prof_colors[i][j]);
		}
	}
}

typedef struct GraphData
{
	uint32_t id;
	bool isDead;
	bool isOOC;
	bool locked;
	float last_time;
	int last_second;
	uint32_t min_val;
	uint32_t max_val;
    std::vector<float> values;

	GraphData() : id(0), isDead(false), isOOC(false), locked(false), min_val(0), max_val(0), last_second(0), last_time(0.0f) {}
	GraphData(const GraphData& gd) {
		Copy(gd);
	}
	GraphData operator=(const GraphData& gd) {
		Copy(gd);
		return *this;
	}
	void Copy(const GraphData& gd) {
		id = gd.id;
		isDead = gd.isDead;
		isOOC = gd.isOOC;
		locked = gd.locked;
		min_val = gd.min_val;
		max_val = gd.max_val;
		last_time = gd.last_time;
		last_second = gd.last_second;
        values = gd.values;
	}
	void RemoveAll() {
		values.clear();
	}
	void Reset() {
		id = 0;
		isDead = false;
		isOOC = false;
		locked = false;
		last_time = 0.0f;
		last_second = 0;
		min_val = max_val = 0;
		RemoveAll();
	}
	void AddValue(uint32_t value)
	{
		values.push_back((float)value);
		max_val = max(max_val, value);
		if (min_val == 0) min_val = value;
		else min_val = min(min_val, value);
	}
} GraphData;
typedef std::unordered_map<uint32_t, GraphData> GraphDataMap;
static GraphData graph_data;
static GraphData graph_data_cleave;
static GraphDataMap graph_data_map;

typedef struct SkillData
{
	SkillData() : id(0), hash_id(0), dmg(0) {}

	uint32_t id;
	uint32_t hash_id;
	uint64_t dmg;
} SkillData;

//typedef ATL::CAtlList<SkillData> SkillDataList;
typedef std::vector<SkillData> SkillDataList;
typedef std::unordered_map<uint32_t, SkillData> SkillDataMap;

static int SkillDataCompare(void const* pItem1, void const* pItem2)
{
	const SkillData* sd1 = (SkillData*)pItem1;
	const SkillData* sd2 = (SkillData*)pItem2;

	if (sd1->dmg < sd2->dmg) return 1;
	else if (sd1->dmg > sd2->dmg) return -1;
	else return 0;
}

typedef struct SkillBreakdown
{
	SkillBreakdown() { Reset(); }

	float duration;
	uint32_t tid;
	uint32_t hits;
	uint64_t dmg_tot;
	uint64_t dmg_high;
	SkillDataList sdlist;

	void Reset(bool removeAll = true) {
		tid = 0;
		hits = 0;
		duration = 0;
		dmg_tot = 0;
		dmg_high = 0;
		if (removeAll) sdlist.clear();
	}

	void RemoveAll() {
		sdlist.clear();
	}

	void AddValue(const SkillData &sd)
	{
		dmg_tot += sd.dmg;
		dmg_high = max(dmg_high, sd.dmg);

		//POSITION pos = sdlist.AddTail();
		//sdlist.GetAt(pos) = sd;
		sdlist.push_back(sd);
	}

	void Sort()
	{
		qsort(sdlist.data(), sdlist.size(), sizeof(sdlist[0]), SkillDataCompare);

		//for (POSITION pos = sdlist.GetHeadPosition(); pos != sdlist.GetTailPosition(); sdlist.GetNext(pos)) {
		//	for (int j = 0; j < sdlist.GetCount() - 1 - i; j++) {
		//		if (array[d] > array[d + 1])
		//		{
		//			sdlist.SwapElements()
		//			swap = array[d];
		//			array[d] = array[d + 1];
		//			array[d + 1] = swap;
		//		}
		//	}
		//}
	}

} SkillBreakdown;

static SkillBreakdown skills_target;
static SkillBreakdown skills_cleave;

void ImGui_RemoveGraphData(int32_t id)
{
	if (id <= 0) return;
	graph_data_map.erase(id);
}

void ImGui_ResetGraphData(int32_t exclude_id)
{
	skills_target.Reset();
	skills_cleave.Reset();

	if (graph_data_map.empty()) return;

    GraphDataMap::iterator it;
	if (exclude_id <= 0 || (it = graph_data_map.find(exclude_id)) == graph_data_map.end()) {
		graph_data_map.clear();
		return;
	}
	else {
		uint32_t key = it->first;
		GraphData value = it->second;
		graph_data_map.clear();
		graph_data_map[key] = value;
	}
}

static const GraphData* BuildGraphDataTarget(const struct DPSTargetEx* dps_target)
{
	static const float ONE_SECOND = 1000000.0f;

	if (!dps_target || dps_target->t.agent_id == 0 || dps_target->t.num_hits == 0 ||
		(graph_data.id != 0 && dps_target->t.agent_id != graph_data.id))
	{ graph_data.Reset(); return &graph_data; }
	if (graph_data.isDead) return &graph_data;

	GraphData &gd = graph_data_map[dps_target->t.agent_id];
	gd.id = dps_target->t.agent_id;
	gd.locked = dps_target->locked;
	gd.isDead = dps_target->t.isDead;

	int duration_secs = (int)(dps_target->t.duration / ONE_SECOND);
	if (dps_target->t.isDead) duration_secs++;
	if (duration_secs < gd.last_second) gd.last_second = duration_secs;	// F9 reset
	if (!dps_target->t.isDead && (duration_secs == 0 || duration_secs == gd.last_second)) { return &graph_data; }
	else gd.last_second = duration_secs;

	gd.RemoveAll();

	uint32_t damage = 0;
	uint32_t next_hit = 0;
	for (int i = 0; i < duration_secs; i++) {

		int64_t second_start = dps_target->t.hits[0].time + i*(int64_t)ONE_SECOND;
		int64_t second_end = min(second_start + (int64_t)ONE_SECOND, dps_target->t.hits[0].time + dps_target->t.duration);
		for (; next_hit < dps_target->t.num_hits &&
			dps_target->t.hits[next_hit].time >= second_start &&
			dps_target->t.hits[next_hit].time < second_end;
			++next_hit) {

			damage += dps_target->t.hits[next_hit].dmg;
		}

		float duration = (float)(second_end - dps_target->t.hits[0].time) / ONE_SECOND;
		uint32_t dps = (uint32_t)((float)damage / duration);
		gd.AddValue(dps);
		//DBGPRINT(L"dps[%d]: %d  <damage=%d> <secs: %.2f> <duration=%.2f> <isDead=%d>",
		//	i, dps, damage, duration, (float)dps_target->t.duration / ONE_SECOND, dps_target->t.isDead);
	}

	graph_data = gd;
	return &graph_data;
}

static const GraphData* BuildGraphDataCleave(const struct Player* player)
{
	static const float ONE_SECOND = 1000.0f;

	GraphData &gd = graph_data_cleave;

	if (!player || player->dps.time == 0.0f || player->dps.time < gd.last_time) { gd.Reset(); return &gd; }

    bool isOOC = controlledPlayer()->cd.lastKnownCombatState == 0;
	if (isOOC && gd.isOOC) return &gd;
	gd.isOOC = isOOC;

	if (gd.last_time == player->dps.time) return &gd;
	if (!isOOC && player->dps.time - gd.last_time < ONE_SECOND) return &gd;

	float duration = player->dps.time / ONE_SECOND;
	gd.AddValue((uint32_t)(player->dps.damage / duration));
	gd.last_time = player->dps.time;

	return &gd;
}

static void PlotGraph(int ui_mode, const struct DPSTargetEx* dps_target, const struct Player* player)
{
	static char str_num[32] = { 0 };
	static const GraphData GdEmpty;
	const GraphData *pgd = &GdEmpty;

	int requested_mode = 0;
	if (player != NULL) pgd = BuildGraphDataCleave(player);
	else if (dps_target) {
		requested_mode = 1;
		pgd = BuildGraphDataTarget(dps_target);
	}

    std::string overlay;
	sprintf_num(pgd->min_val, str_num, sizeof(str_num));
	overlay =  toString("%s:%s\n", LOCALTEXT(TEXT_GLOBAL_MIN), str_num);
	sprintf_num(pgd->max_val, str_num, sizeof(str_num));
	overlay += toString("%s:%s", LOCALTEXT(TEXT_GLOBAL_MAX), str_num);

	if (ui_mode != requested_mode) return;

	ImGui::Columns(1);
	ImGui::Separator();
	ImGui::PushItemWidth(-1);
	ImGui::PlotLines("##DPSGraph", pgd->values.data(), (int)pgd->values.size(), 0, overlay.c_str(), 0.0f, FLT_MAX, ImVec2(0, 80));
	ImGui::PopItemWidth();
}

void ImGui_PersonalDps(struct Panel *panel, int x, int y)
{
	static char str_num[32] = { 0 };
	static ImGuiWindowFlags temp_flags = 0;
	static int temp_fr_count = 0;
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;
    
    if (!panel->enabled) return;
    
    const struct DPSTargetEx* dps_target = &currentContext()->currentDpsTarget;
    if (dps_target->t.agent_id == 0 || dps_target->t.aptr == 0) return;

	ImGuiStyle &style = ImGui::GetStyle();

	if (!panel->autoResize) temp_flags = 0;
	else if (temp_flags) {
		if (temp_fr_count++ > 1) {
			flags |= temp_flags;
			temp_flags = 0;
			temp_fr_count = 0;
		}
	}

	if (dps_target && dps_target->locked)
		flags |= ImGuiWindowFlags_ShowBorders;

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	uint32_t old_enabled = panel->enabled;
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor old_border = style.Colors[ImGuiCol_Border];
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col));
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_TARGET_STATS), (bool*)&panel->enabled, ImVec2(245, 150), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

	// was close button pressed 
	// uint32_t old_enabled = panel->enabled;
	if (old_enabled != panel->enabled)
		PanelSaveColumnConfig(panel);

	ImGui::PushStyleColor(ImGuiCol_Border, old_border);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		bool needUpdate = false;
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{
			ShowWindowOptionsMenu(panel, &temp_flags);

			needUpdate |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_TARGET_SHOW_DMG), "", &panel->cfg.col_visible[TDPS_COL_DMG]);
			needUpdate |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_TARGET_SHOW_TTK), "", &panel->cfg.col_visible[TDPS_COL_TTK]);
			needUpdate |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_TARGET_SHOW_GRAPH), "", &panel->cfg.col_visible[TDPS_COL_GRAPH]);

			ImGui::EndMenu();
		}

		ShowCombatMenu();

		if (g_state.panel_debug.enabled) {
			if (ImGui::BeginMenu("Debug")) {
				needUpdate |= ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_TARGET_SHOW_NPC), "", &panel->cfg.col_visible[TDPS_COL_SPECID]);
				ImGui::EndMenu();
			}
		}

		if (needUpdate) {
			temp_flags = ImGuiWindowFlags_AlwaysAutoResize;
			PanelSaveColumnConfig(panel);
		}

		ImGui::EndPopup();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(ImColor(bgdm_colors[BGDM_COL_TEXT2].col)));

    std::string utf8 = "<unresolved>";
    if (dps_target->t.decodedName)
        utf8 = std_utf16_to_utf8((const utf16str)dps_target->t.decodedName);
    ImGui::TextColored(ImColor(bgdm_colors[BGDM_COL_TEXT_TARGET].col), u8"%s", utf8.c_str());

    std::string str = toString("%sXXX", LOCALTEXT(TEXT_PANEL_TARGET_TIME));
	ImGui::Columns(4, "TargetDPS", true);
	ImGui::SetColumnOffset(1, ImGui::CalcTextSize(str.c_str(), NULL, true).x);
	ImGui::Separator();

    float tot_dur = (float)((uint32_t)(dps_target->t.duration / 1000.0f) / 1000.0f);
    float l1_dur = std::min(tot_dur, (DPS_INTERVAL_1 / 1000000.0f));
    float l2_dur = std::min(tot_dur, (DPS_INTERVAL_2 / 1000000.0f));
    
	ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_TIME)); ImGui::NextColumn();
	ImGui::Text("%dm %.2fs\n",
		(int)tot_dur / 60,
		fmod(tot_dur, 60));
	ImGui::NextColumn();

	ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_L10S));  ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_L30S));  ImGui::NextColumn();

	ImGui::Separator();
	ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_PDPS));  ImGui::NextColumn();
	u32 pdps = tot_dur ? (u32)(dps_target->t.tdmg / tot_dur) : 0;
	sprintf_num(pdps, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	sprintf_num(l1_dur ? (i32)(dps_target->t.c1dmg / l1_dur) : 0, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	sprintf_num(l2_dur ? (i32)(dps_target->t.c2dmg / l2_dur) : 0, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_TDPS));  ImGui::NextColumn();
	u32 tdps = tot_dur ? (u32)(dps_target->t.hp_lost / tot_dur) : 0;
	sprintf_num(tdps, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	sprintf_num(l1_dur ? (i32)(dps_target->t.c1dmg_team / l1_dur) : 0, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	sprintf_num(l2_dur ? (i32)(dps_target->t.c2dmg_team / l2_dur) : 0, str_num, sizeof(str_num));
	ImGui::Text(str_num);  ImGui::NextColumn();

	if (panel->cfg.col_visible[TDPS_COL_DMG]) {

		ImGui::Separator();
		ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_PDMG));  ImGui::NextColumn();
		sprintf_num(dps_target->t.tdmg, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();
		sprintf_num(dps_target->t.c1dmg, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();
		sprintf_num(dps_target->t.c2dmg, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();

		ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_TDMG));  ImGui::NextColumn();
		sprintf_num(dps_target->t.hp_lost, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();
		sprintf_num(dps_target->t.c1dmg_team, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();
		sprintf_num(dps_target->t.c2dmg_team, str_num, sizeof(str_num));
		ImGui::Text(str_num);  ImGui::NextColumn();
	}

	if (panel->cfg.col_visible[TDPS_COL_TTK]) {
		u32 ttk = (dps_target && tdps > 0) ? dps_target->t.hp_val / tdps : 0;
		ImGui::Separator();
		ImGui::TextColored(white, LOCALTEXT(TEXT_PANEL_TARGET_TTK)); ImGui::NextColumn();
		ImGui::TextColored(ImColor(bgdm_colors[BGDM_COL_TEXT_TARGET].col), "%dm %ds", ttk / 60, ttk % 60);
		ImGui::NextColumn();
	}

	if (panel->cfg.col_visible[TDPS_COL_GRAPH]) {

		PlotGraph(1, dps_target, NULL);
	}

	if (g_state.panel_debug.enabled && panel->cfg.col_visible[TDPS_COL_SPECID]) {
		ImGui::Columns(2, "", false);
		ImGui::SetColumnOffset(1, 70.0f);
		int hashID = dps_target ? dps_target->t.npc_id : 0;
		int speciesID = dps_target ? dps_target->t.species_id : 0;
		ImGui::Separator();
		ImGui::TextDisabled("NPC ID"); ImGui::NextColumn();
		ImGui::TextDisabled("%d (0x%X)", hashID, hashID);  ImGui::NextColumn();
		ImGui::TextDisabled("NPC HASH"); ImGui::NextColumn();
		ImGui::TextDisabled("%d (0x%X)", speciesID, speciesID); ImGui::NextColumn();
		if (speciesID) {
            std::string utf8 = "<unresolved>";
			utf16str unicode = (utf16str)lru_find(GW2::CLASS_TYPE_SPECIESID, speciesID, NULL, 0);
            if (unicode) utf8 = std_utf16_to_utf8(unicode);
			ImGui::TextDisabled("NPC Name"); ImGui::NextColumn();
			ImGui::TextDisabled("%s", utf8.c_str());
		}
	}

	ImGui::PopStyleColor(2); // Text/Separator
	ImGui::End();
	ImGui::PopStyleColor(); // Border
	ImGui::PopFont();
	
}

static void SetColumnOffsets(const struct Panel *panel, int type)
{
    std::string str("XX"); // Spacing
	int col = 0;
	float offset = 0;
	float width = 0;
	float spacing = ImGui::CalcTextSize(str.c_str(), NULL, true).x * 0.6f;
	int size = ImGui::GetColumnsDataSize();
	int nCol = panel->cfg.maxCol;
	if (type == 0) nCol = GDPS_COL_END;
	for (int i = 0; i < panel->cfg.maxCol; ++i) {
		if (!panel->cfg.col_visible[i])
			continue;

		if (i<2) {
			switch (i) {
			case(GDPS_COL_NAME):
				str = toString("XXXXXXXXXXXXXXXXXXXXX");
				if (panel->cfg.lineNumbering) str += toString("99.");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			case(GDPS_COL_CLS):
				str = toString("XXXX");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			}
		}
		else if ((type == 0 && i == GDPS_COL_TIME) ||
				(type == 1 && i == BUFF_COL_TIME)) {
			// Time
			str = toString("99m 99.99s");
			width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
		}
		else if (type == 0) {
			switch (i) {
			case(GDPS_COL_PER):
				str = toString("100%%");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			default:
				str = toString("XXXXXXXXX");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			}
		}
		else if (type == 1) {
			switch (i) {
			case(BUFF_COL_MIGHT):
				str = toString("25.0");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			default:
				str= toString("100%%");
				width = ImGui::CalcTextSize(str.c_str(), NULL, true).x;
				break;
			}
		}

		offset += width + spacing;
		if (++col < size) ImGui::SetColumnOffset(col, offset);
	}
}

static inline const void SkillAdd(SkillDataMap &sdmap, const DPSHit &hit)
{
	if (hit.eff_id == 0) return;
	SkillData &sd = sdmap[hit.eff_id];
	sd.dmg += hit.dmg;
	sd.id = hit.eff_id;
	sd.hash_id = hit.eff_hash;
}

static const void BuildSkillDataLists(const struct DPSTargetEx* dps_target)
{
	static bool last_ooc = false;
	static bool last_dead = false;
    bool isOOC = controlledPlayer()->cd.lastKnownCombatState == 0;
	if (last_ooc && isOOC) return;
	last_ooc = isOOC;

	bool build_target = true;
	bool build_cleave = true;

	if (!dps_target || dps_target->t.agent_id == 0 || dps_target->t.num_hits == 0 ||
		(last_dead && dps_target && dps_target->t.isDead)) {
		build_target = false;
	}
	last_dead = dps_target ? dps_target->t.isDead : 0;

	if ((dps_target && dps_target->t.num_hits == 0 /*&& skills_target.hits > 0*/) ||
		(dps_target && dps_target->t.agent_id != skills_target.tid))
		skills_target.Reset();

	if (build_target) {
		
		skills_target.Reset(false);
		skills_target.duration = (float)((uint32_t)dps_target->t.duration / 1000000.0f);

		if (skills_target.tid != dps_target->t.agent_id || skills_target.hits != dps_target->t.num_hits) {

			skills_target.tid = dps_target->t.agent_id;
			skills_target.hits = dps_target->t.num_hits;

			SkillDataMap sdmap;
			for (uint32_t i = 0; i < dps_target->t.num_hits; i++) {
				SkillAdd(sdmap, dps_target->t.hits[i]);
			}

			skills_target.RemoveAll();
            for ( auto it = sdmap.begin(); it != sdmap.end(); ++it ) {
				skills_target.AddValue(it->second);
			}
			skills_target.Sort();
		}
	}

	if (build_cleave) {

		skills_cleave.Reset(false);
		skills_cleave.duration = controlledPlayer()->cd.duration / 1000.0f;

		static uint32_t num_hits;
		static SkillDataMap sdmap;
        
        num_hits = 0;
        sdmap.clear();
        
        currentContext()->targets.iter_lock([](struct CacheEntry<DPSTarget>* entry) {
            DPSTarget* t = &entry->value.data;
            if (t->agent_id == 0 || t->num_hits == 0) return;
            for (uint32_t j = 0; j < t->num_hits; j++) {
                if (t->hits[j].time >= controlledPlayer()->cd.begin) {
                    SkillAdd(sdmap, t->hits[j]);
                    num_hits++;
                }
            }
        });

		if (num_hits == 0 || skills_cleave.hits != num_hits) {
			skills_cleave.RemoveAll();
			skills_cleave.hits = num_hits;
            for ( auto it = sdmap.begin(); it != sdmap.end(); ++it ) {
				skills_cleave.AddValue(it->second);
			}
			skills_cleave.Sort();
		}
	}
}

void ImGui_SkillBreakdown(struct Panel *panel, int x, int y)
{
	static char str_num1[32] = { 0 };
	static char str_num2[32] = { 0 };
	static utf16chr skillName[60] = { 0 };
	static ImGuiWindowFlags temp_flags = 0;
	static int temp_fr_count = 0;
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;
    
    if (!panel->enabled) return;
    
    const struct DPSTargetEx* dps_target = &currentContext()->currentDpsTarget;
    
	ImGuiStyle &style = ImGui::GetStyle();

	if (!panel->autoResize) temp_flags = 0;
	else if (temp_flags) {
		if (temp_fr_count++ > 1) {
			flags |= temp_flags;
			temp_flags = 0;
			temp_fr_count = 0;
		}
	}

	int mode = panel->mode;
	if (mode == 1 && dps_target && dps_target->locked)
		flags |= ImGuiWindowFlags_ShowBorders;

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	uint32_t old_enabled = panel->enabled;
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor old_border = style.Colors[ImGuiCol_Border];
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col));
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_SKILLS), (bool*)&panel->enabled, ImVec2(425, 225), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

	// was close button pressed 
	// uint32_t old_enabled = panel->enabled;
	if (old_enabled != panel->enabled)
		PanelSaveColumnConfig(panel);

	ImGui::PushStyleColor(ImGuiCol_Border, old_border);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		bool needUpdate = false;
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{
			ShowWindowOptionsMenu(panel, &temp_flags);

			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_OPTS_LINENO), "", &panel->cfg.lineNumbering)) {
				config_set_int(panel->section, "LineNumbering", panel->cfg.lineNumbering);
			}

			ImGui::EndMenu();
		}

		if (needUpdate) {
			temp_flags = ImGuiWindowFlags_AlwaysAutoResize;
			PanelSaveColumnConfig(panel);
		}

		ImGui::EndPopup();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(ImColor(bgdm_colors[BGDM_COL_TEXT2].col)));

	{
		bool needUpdate = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_PANEL_GDPS_CLEAVE), &mode, 0); ImGui::SameLine();
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_PANEL_GDPS_TARGET), &mode, 1);
		ImGui::PopStyleVar();
		if (needUpdate) {
			panel->mode = mode;
			config_set_int(panel->section, "Mode", panel->mode);
		}
	}

	if (mode == 1 && dps_target) {

        std::string utf8 = "<unresolved>";
        if (dps_target->t.decodedName)
            utf8 = std_utf16_to_utf8((const utf16str)dps_target->t.decodedName);
		ImGui::TextColored(ImColor(bgdm_colors[BGDM_COL_TEXT_TARGET].col), u8"%s", utf8.c_str());
	}

	ImGui::Separator();

	// Build the skill data
	BuildSkillDataLists(dps_target);

	SkillBreakdown &skills = mode ? skills_target : skills_cleave;
	//int i = 0;
	//for (POSITION pos = skills.sdlist.GetTailPosition(); pos != NULL; skills.sdlist.GetPrev(pos), i++) {
	for (int i = 0; i < skills.sdlist.size(); i++) {

		const SkillData &sd = skills.sdlist[i];
		if (sd.id == 0) continue;

		float rect_per = skills.dmg_high ? (float)sd.dmg / skills.dmg_high : 0.0f;
		if (rect_per > 0.0f) {
			ImVec2 p = ImGui::GetCursorScreenPos();
			//ImGuiStyle &style = ImGui::GetStyle();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			float width = ImGui::GetContentRegionAvailWidth() * rect_per;
			float height = ImGui::GetTextLineHeight() + style.WindowPadding.y/2;
			p.y -= style.WindowPadding.y / 4;
			draw_list->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + width, p.y + height),
				ImColor(bgdm_colors[BGDM_COL_GRAD_START].col),
				ImColor(bgdm_colors[BGDM_COL_GRAD_END].col),
				ImColor(bgdm_colors[BGDM_COL_GRAD_END].col),
				ImColor(bgdm_colors[BGDM_COL_GRAD_START].col));
		}

        std::string str;
		if (sd.hash_id) {
			memset(skillName, 0, sizeof(skillName));
			if (lru_find(GW2::CLASS_TYPE_SKILLID, sd.hash_id, (unsigned char*)skillName, sizeof(skillName))) {
                str = std_utf16_to_utf8(skillName);
			}
		}
		if (str.empty()) str = toString("ID:%d", sd.id);
		if (panel->cfg.lineNumbering) {
            std::string saved = str;
			str = toString((i<9) ? "%d. %s" : "%d.%s", i + 1, saved.c_str());
		}
		ImGui::TextColored(white, str.c_str());

		memset(str_num1, 0, sizeof(str_num1));
		memset(str_num2, 0, sizeof(str_num2));
		sprintf_num((double)sd.dmg, str_num1, sizeof(str_num1));
		sprintf_num(skills.duration ? (double)sd.dmg / skills.duration : 0, str_num2, sizeof(str_num2));
		float per = skills.dmg_tot ? (float)sd.dmg / skills.dmg_tot : 0.0f;
		if (per > 100.0f) per = 100.0f;

		str = toString("%s (%s, %.2f%%%%)", str_num2, str_num1, per*100.0f);
		float maxw = ImGui::GetContentRegionAvailWidth();
		float textw = ImGui::CalcTextSize(str.c_str()).x;
		ImGui::SameLine(max(150.0f, maxw-textw));
		ImGui::TextColored(white, str.c_str());
	}

	ImGui::PopStyleColor(2); // Text/Separator
	ImGui::End();
	ImGui::PopStyleColor(); // Border
	ImGui::PopFont();

}
static const char * nameFromProfession(u32 prof, int is_elite)
{
    static char const* prof_strings[][3] = {
        { "###", "###", "###" },
        { "GRD", "DHR", "FBR" },
        { "WAR", "BSR", "SBR" },
        { "ENG", "SCR", "HOL" },
        { "RGR", "DRU", "SBT" },
        { "THF", "DDV", "DDI" },
        { "ELE", "TMP", "WVR" },
        { "MES", "CHR", "MRG" },
        { "NEC", "RPR", "SCR" },
        { "REV", "HRL", "REN" },
        { "###", "###", "###" },
    };
    
    if (prof > GW2::PROFESSION_END || is_elite > 2)
        return prof_strings[GW2::PROFESSION_END][is_elite];
    else
        return prof_strings[prof][is_elite];
}

static int DpsCompare(void* pCtx, void const* pItem1, void const* pItem2)
{
#define LOBYTE(w)           ((uint8_t)(((uint64_t)(w)) & 0xff))
#define HIBYTE(w)           ((uint8_t)((((uint64_t)(w)) >> 8) & 0xff))
    
    i32 retVal = 0;
    u32 sortBy = LOBYTE(*(u32*)pCtx);
    bool isAsc = HIBYTE(*(u32*)pCtx);
    const Player* p1 = *(const Player**)pItem1;
    const Player* p2 = *(const Player**)pItem2;
    
#define DPS_COMPARE(x) \
    if (p1->dps.x < p2->dps.x) retVal = -1; \
    else if (p1->dps.x > p2->dps.x) retVal = 1; \
    else retVal = 0;
    
    u32 p1time = (u32)(p1->dps.target_time / 1000.0f);
    u32 p2time = (u32)(p2->dps.target_time / 1000.0f);
    u32 p1dmg = p1->dps.target_dmg;
    u32 p2dmg = p2->dps.target_dmg;
    u32 p1dps = p1time == 0 ? 0 : (u32)(p1dmg / p1time);
    u32 p2dps = p2time == 0 ? 0 : (u32)(p2dmg / p2time);
    u32 p1hps = p1->dps.time == 0 ? 0 : (u32)(p1->dps.heal_out / (p1->dps.time / 1000.0f));
    u32 p2hps = p2->dps.time == 0 ? 0 : (u32)(p2->dps.heal_out / (p2->dps.time / 1000.0f));
    
    switch (sortBy)
    {
        case(GDPS_COL_NAME):
            if (!p1->c.name[0] && p2->c.name[0])
                retVal = -1;
            else if (!p2->c.name[0] && p1->c.name[0])
                retVal = 1;
            else if (!p1->c.name[0] && !p2->c.name[0])
                retVal = 0;
            else {
                std::string utf1 = std_utf16_to_utf8((const utf16str)p1->c.name);
                std::string utf2 = std_utf16_to_utf8((const utf16str)p2->c.name);
                retVal = strcmp(utf1.c_str(), utf2.c_str());
            }
            break;
        case(GDPS_COL_CLS): {
                const char * profName1 = nameFromProfession(p1->c.profession, p1->c.is_elite);
                const char * profName2 = nameFromProfession(p2->c.profession, p2->c.is_elite);
                retVal = strcmp(profName1, profName2);
            }
            break;
        case(GDPS_COL_DPS):
            if (p1dps < p2dps) retVal = -1;
            else if (p1dps > p2dps) retVal = 1;
            else retVal = 0;
            break;
        case(GDPS_COL_HPS):
            if (p1hps < p2hps) retVal = -1;
            else if (p1hps > p2hps) retVal = 1;
            else retVal = 0;
            break;
        case(GDPS_COL_PER):
        case(GDPS_COL_DMGOUT):
            if (p1dmg < p2dmg) retVal = -1;
            else if (p1dmg > p2dmg) retVal = 1;
            else retVal = 0;
            break;
        case(GDPS_COL_DMGIN):
            DPS_COMPARE(damage_in);
            break;
        case(GDPS_COL_HEAL):
            DPS_COMPARE(heal_out);
            break;
        case(GDPS_COL_TIME):
        {
            if (p1time == 0 && p2time != 0) retVal = -1;
            else if (p2time == 0 && p1time != 0) retVal = 1;
            else if (p1time < p2time) retVal = -1;
            else if (p1time > p2time) retVal = 1;
            else retVal = 0;
        }
            break;
    };
    
    if (isAsc)
        return retVal;
    else
        return -(retVal);
}

void ImGui_GroupDps(struct Panel *panel, int x, int y)
{
	static char str_num[32] = { 0 };
	static ImGuiWindowFlags temp_flags = 0;
	static int temp_fr_count = 0;
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;
    
    if (!panel->enabled) return;
    
    const struct DPSTargetEx* target = &currentContext()->currentDpsTarget;

	ImGuiStyle &style = ImGui::GetStyle();
	
	if (!panel->autoResize) temp_flags = 0;
	else if (temp_flags) {
		if (temp_fr_count++ > 1) {
			flags |= temp_flags;
			temp_flags = 0;
			temp_fr_count = 0;
		}
	}

	int mode = panel->mode;
	if (mode == 1 && target->t.agent_id &&target->locked)
		flags |= ImGuiWindowFlags_ShowBorders;

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	uint32_t old_enabled = panel->enabled;
	ImVec2 ICON_SIZE(ImGui::GetFont()->FontSize + 2, ImGui::GetFont()->FontSize);
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor old_border = style.Colors[ImGuiCol_Border];
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col));
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_GROUP_STATS), (bool*)&panel->enabled, ImVec2(400, 175), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

	// was close button pressed 
	// uint32_t old_enabled = panel->enabled;
	if (old_enabled != panel->enabled)
		PanelSaveColumnConfig(panel);

	ImGui::PushStyleColor(ImGuiCol_Border, old_border);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		bool saveConfig = false;
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{
			ShowWindowOptionsMenu(panel, &temp_flags);

			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_GDPS_GRAPH), "", &panel->cfg.col_visible[GDPS_COL_GRAPH])) {
				saveConfig = true;
			}
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_OPTS_LINENO), "", &panel->cfg.lineNumbering)) {
				config_set_int(panel->section, "LineNumbering", panel->cfg.lineNumbering);
			}
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_OPTS_PROFCOL), "", &panel->cfg.useProfColoring)) {
				config_set_int(panel->section, "ProfessionColoring", panel->cfg.useProfColoring);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_COLUMNS)))
		{
			if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_ALL))) {
				for (int i = 1; i < GDPS_COL_END; i++)
					panel->cfg.col_visible[i] = true;
				saveConfig = true;
			}
			if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_NONE))) {
				for (int i = 1; i < GDPS_COL_END; i++)
					panel->cfg.col_visible[i] = false;
				saveConfig = true;
			}
			if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_DEFAULT))) {
				for (int i = 1; i < GDPS_COL_END; i++)
					panel->cfg.col_visible[i] = false;
				for (int i = 1; i < GDPS_COL_DMGIN; i++)
					panel->cfg.col_visible[i] = true;
				panel->cfg.col_visible[GDPS_COL_TIME] = true;
				saveConfig = true;
			}

			ImGui::Separator();
			for (int j = 1; j < GDPS_COL_END; ++j)
				if (ImGui::MenuItemNoPopupClose(GdpsTextFromColumn(j), "", &panel->cfg.col_visible[j]))
					saveConfig = true;
			ImGui::EndMenu();
		}
		ShowCombatMenu();
		ImGui::EndPopup();

		if (saveConfig) {
			PanelSaveColumnConfig(panel);
			temp_flags = ImGuiWindowFlags_AlwaysAutoResize;
		}
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT2].col));

	{
		bool needUpdate = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_PANEL_GDPS_CLEAVE), &mode, 0); ImGui::SameLine();
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_PANEL_GDPS_TARGET), &mode, 1);
		ImGui::PopStyleVar();
		if (needUpdate) {
			panel->mode = mode;
			config_set_int(panel->section, "Mode", panel->mode);
		}
	}

	if (mode == 1 && target) {

		ImGui::TextColored(white, "<%s>",
			target->t.invuln ? LOCALTEXT(TEXT_PANEL_GDPS_INVULN) :
			(target->locked ? LOCALTEXT(TEXT_PANEL_GDPS_LOCKED) : LOCALTEXT(TEXT_PANEL_GDPS_UNLOCKED)));

		ImGui::SameLine(85);
        std::string utf8 = "<unresolved>";
        if (target->t.decodedName)
            utf8 = std_utf16_to_utf8((const utf16str)target->t.decodedName);
		ImGui::TextColored(ImColor(bgdm_colors[BGDM_COL_TEXT_TARGET].col), u8"%s", utf8.c_str());
		ImGui::Separator();
	}

	int nCol = 1;
	for (int i = 1; i < GDPS_COL_END; ++i) {
		if (panel->cfg.col_visible[i])
			nCol++;
	}

	ImGui::Columns(nCol, NULL, true);
	SetColumnOffsets(panel, 0);
	bool sortFound = false;
	for (int i = 0; i < GDPS_COL_END; ++i) {
		if (!panel->cfg.col_visible[i])
			continue;
		if (i == panel->cfg.sortCol) {
			sortFound = true;
            void *img = ImGui_GetMiscImage(panel->cfg.asc ? 0 : 1);
            if (img) {
                ImGui::Image(img, ICON_SIZE);
            }
            else {
				ImGui::PushFont(tinyFont);
				ImGui::TextColored(white, panel->cfg.asc ? ">" : "<");
				ImGui::PopFont();
			}
		}
		ImGui::NextColumn();
	}
	if (!sortFound) ImGui::NewLine();

#define MAKEWORD(a, b)      ((uint16_t)(((uint8_t)(((uint64_t)(a)) & 0xff)) | ((uint16_t)((uint8_t)(((uint64_t)(b)) & 0xff))) << 8))
    uint32_t sortBy = MAKEWORD(panel->cfg.sortCol, panel->cfg.asc);
    
    u32 tot_dmg = 0;
	ImGui::Columns(nCol, "GroupDPS", true);
	SetColumnOffsets(panel, 0);
	for (int i = 0; i < GDPS_COL_END; ++i) {
		if (!panel->cfg.col_visible[i])
			continue;

		ImGui::PushStyleColor(ImGuiCol_Text, white);
		if (ImGui::Selectable(GdpsAcronymFromColumn(i))) {}
		if (i > 0 && ImGui::BeginPopupContextItem(gdps_col_str[i])) {
			ImGui::Text(GdpsTextFromColumn(i));
			ImGui::Separator();
			if (ImGui::Selectable(LOCALTEXT(TEXT_GLOBAL_HIDE))) {
				panel->cfg.col_visible[i] = false;
				PanelSaveColumnConfig(panel);
				temp_flags = ImGuiWindowFlags_AlwaysAutoResize;
				ImGui::EndPopup();
				ImGui::PopStyleColor();
				goto abort;
			}
			ImGui::EndPopup();
		}
		if (ImGui::IsItemClicked()) {
			panel->cfg.sortCol = i;
			if (panel->cfg.sortColLast != GDPS_COL_END &&
				panel->cfg.sortColLast == panel->cfg.sortCol)
				panel->cfg.asc ^= 1;
			else
				panel->cfg.asc = 0;
			panel->cfg.sortColLast = i;
			PanelSaveColumnConfig(panel);
		}
		ImGui::PopStyleColor();
		ImGui::NextColumn();
	}
    
    static std::vector<Player*> closePlayers;
    closePlayers.clear();
    if (controlledCharacter()->aptr != 0)
        closePlayers.push_back(controlledPlayer());
    currentContext()->closestPlayers.lock();
    currentContext()->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
        if (entry->value.data.dps.is_connected)
            closePlayers.push_back(&entry->value.data);
    });
    
    if ( closePlayers.size() > 0 )
        qsort_r(closePlayers.data(), closePlayers.size(), sizeof( closePlayers[0] ), (void*)&sortBy, DpsCompare);

	// Calculate total damage for the % calc
	if (mode == 1 && target && target->t.agent_id) {
		//if (target->t.isDead) { assert(target->t.hp_max == target->c.hp_max); }
		tot_dmg = target->t.hp_max;
	}
	else {
		for (size_t i = 0; i < closePlayers.size(); ++i)
			tot_dmg += closePlayers[i]->dps.damage;
	}
    
	ImGui::Separator();
    for (int i=0; i<closePlayers.size(); ++i) {
        
        Player *player = closePlayers[i];
		uint32_t dmg = (mode == 1) ? player->dps.target_dmg : player->dps.damage;
		float time = (mode == 1) ? player->dps.target_time : player->dps.time;
		float dur = time ? time / 1000.0f : 0;
		float dur_cleave = player->dps.time ? player->dps.time / 1000.0f : 0;

		for (int j = 0; j < GDPS_COL_END; ++j) {
			if (!panel->cfg.col_visible[j])
				continue;

			if (j > GDPS_COL_CLS && time == 0) {
				ImGui::NextColumn();
				continue;
			}

			switch (j) {
			case(GDPS_COL_NAME):
			{
                const std::string utf8 = std_utf16_to_utf8(player->c.decodedName ? (utf16str)player->c.decodedName : (utf16str)player->c.name);
                std::string no("");
				if (panel->cfg.lineNumbering) no = toString((i<9) ? "%d. " : "%d.", i + 1);
				ImColor plColor = ColorFromCharacter(&player->c, panel->cfg.useProfColoring);
				//float per = dmg / (f32)tot_dmg;
				//if (per > 0.0f) {
				//	ImVec2 p = ImGui::GetCursorScreenPos();
				//	ImGuiStyle &style = ImGui::GetStyle();
				//	ImDrawList* draw_list = ImGui::GetWindowDrawList();
				//	float width = ImGui::GetContentRegionAvailWidth() * per;
				//	float height = ImGui::GetTextLineHeight() + style.WindowPadding.y/2;
				//	p.y -= style.WindowPadding.y / 4;
				//	draw_list->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + width, p.y + height), plColor, ImColor(0, 0, 0), ImColor(0, 0, 0), plColor);
				//}
				ImGui::TextColored(plColor, u8"%s%s", no.c_str(), utf8.c_str());
			} break;
			case(GDPS_COL_CLS):
			{
				void* img = ProfessionImageFromPlayer(&player->c);
				if (img) ImGui::Image(img, ICON_SIZE);
			} break;
			case(GDPS_COL_DPS):
			{
				i32 pdps = dur ? (i32)(dmg / dur) : 0;
				ImGui::Text("%s", sprintf_num(pdps, str_num, sizeof(str_num)));
			} break;
			case(GDPS_COL_PER):
			{
				u32 per = (u32)roundf(dmg / (f32)tot_dmg * 100.0f);
				per = per > 100 ? 100 : per;
				if (per>99) ImGui::SameLine(3);
				ImGui::Text("%u%%", per);
			} break;
			case (GDPS_COL_DMGOUT):
			{
				ImGui::Text("%s", sprintf_num(dmg, str_num, sizeof(str_num)));
			} break;
			case (GDPS_COL_DMGIN):
			{
				ImGui::Text("%s", sprintf_num(player->dps.damage_in, str_num, sizeof(str_num)));
			} break;
			case (GDPS_COL_HPS):
			{
				i32 phps = dur_cleave ? (i32)(player->dps.heal_out / dur_cleave) : 0;
				ImGui::Text("%s", sprintf_num(phps, str_num, sizeof(str_num)));
			} break;
			case (GDPS_COL_HEAL):
			{
				ImGui::Text("%s", sprintf_num(player->dps.heal_out, str_num, sizeof(str_num)));
			} break;
			case (GDPS_COL_TIME):
			{
				ImGui::Text("%dm %.2fs", (int)dur / 60, fmod(dur, 60));
			} break;

			}
			ImGui::NextColumn();
		}
	}
    currentContext()->closestPlayers.unlock();

	if (panel->cfg.col_visible[GDPS_COL_GRAPH]) {

		// Always build cleave graph
        for (size_t i = 0; i < closePlayers.size(); ++i) {
			Player *player = closePlayers[i];
			if (player->c.aptr == controlledCharacter()->aptr) {
				PlotGraph(mode, NULL, player);
			}
		}
		if (mode == 1) PlotGraph(mode, target, NULL);
	}

abort:
	ImGui::PopStyleColor(2); // Text/Separator
	ImGui::End();
	ImGui::PopStyleColor(); // border
	ImGui::PopFont();
}

static bool inline BuildBuffColumnsMenu(struct Panel *panel)
{
	bool saveConfig = false;
	if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_COLUMNS)))
	{
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_ALL))) {
			for (int i = 1; i < BUFF_COL_END; i++)
				panel->cfg.col_visible[i] = true;
			saveConfig = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_NONE))) {
			for (int i = 1; i < BUFF_COL_END; i++)
				panel->cfg.col_visible[i] = false;
			saveConfig = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_DEFAULT))) {
			for (int i = 1; i < BUFF_COL_END; i++)
				panel->cfg.col_visible[i] = false;
			for (int i = 1; i < BUFF_COL_VIGOR; i++)
				panel->cfg.col_visible[i] = true;
			panel->cfg.col_visible[BUFF_COL_GOTL] = true;
			panel->cfg.col_visible[BUFF_COL_GLEM] = true;
			panel->cfg.col_visible[BUFF_COL_TIME] = true;
			saveConfig = true;
		}
		if (ImGui::SelectableNoPopupClose(LOCALTEXT(TEXT_GLOBAL_WVW))) {
			for (int i = 1; i < BUFF_COL_END; i++)
				panel->cfg.col_visible[i] = false;
			panel->cfg.col_visible[BUFF_COL_CLS] = true;
			panel->cfg.col_visible[BUFF_COL_SUB] = true;
			for (int i = BUFF_COL_PROT; i < BUFF_COL_AEGIS; i++)
				panel->cfg.col_visible[i] = true;
			panel->cfg.col_visible[BUFF_COL_ALAC] = false;
			panel->cfg.col_visible[BUFF_COL_RETAL] = false;
			panel->cfg.col_visible[BUFF_COL_REVNR] = true;
			panel->cfg.col_visible[BUFF_COL_REVRD] = true;
			panel->cfg.col_visible[BUFF_COL_ELESM] = true;
			panel->cfg.col_visible[BUFF_COL_TIME] = true;
			saveConfig = true;
		}
		ImGui::Separator();
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_CLS), "", &panel->cfg.col_visible[BUFF_COL_CLS]);
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SUB), "", &panel->cfg.col_visible[BUFF_COL_SUB]);
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_DOWN), "", &panel->cfg.col_visible[BUFF_COL_DOWN]);
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SCLR), "", &panel->cfg.col_visible[BUFF_COL_SCLR]);
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SEAW), "", &panel->cfg.col_visible[BUFF_COL_SEAW]);
		saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_TIME), "", &panel->cfg.col_visible[BUFF_COL_TIME]);

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_BUFF_STD)))
		{
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_PROT), "", &panel->cfg.col_visible[BUFF_COL_PROT]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_QUIK), "", &panel->cfg.col_visible[BUFF_COL_QUIK]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_ALAC), "", &panel->cfg.col_visible[BUFF_COL_ALAC]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_FURY), "", &panel->cfg.col_visible[BUFF_COL_FURY]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_MIGHT), "", &panel->cfg.col_visible[BUFF_COL_MIGHT]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_VIGOR), "", &panel->cfg.col_visible[BUFF_COL_VIGOR]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SWIFT), "", &panel->cfg.col_visible[BUFF_COL_SWIFT]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_STAB), "", &panel->cfg.col_visible[BUFF_COL_STAB]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_RETAL), "", &panel->cfg.col_visible[BUFF_COL_RETAL]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_RESIST), "", &panel->cfg.col_visible[BUFF_COL_RESIST]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_REGEN), "", &panel->cfg.col_visible[BUFF_COL_REGEN]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_AEGIS), "", &panel->cfg.col_visible[BUFF_COL_AEGIS]);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_BUFF_PROF)))
		{
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_GOTL), "", &panel->cfg.col_visible[BUFF_COL_GOTL]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_GLEM), "", &panel->cfg.col_visible[BUFF_COL_GLEM]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_BANS), "", &panel->cfg.col_visible[BUFF_COL_BANS]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_BAND), "", &panel->cfg.col_visible[BUFF_COL_BAND]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_BANT), "", &panel->cfg.col_visible[BUFF_COL_BANT]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_EA), "", &panel->cfg.col_visible[BUFF_COL_EA]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SPOTTER), "", &panel->cfg.col_visible[BUFF_COL_SPOTTER]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_FROST), "", &panel->cfg.col_visible[BUFF_COL_FROST]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_SUN), "", &panel->cfg.col_visible[BUFF_COL_SUN]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_STORM), "", &panel->cfg.col_visible[BUFF_COL_STORM]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_STONE), "", &panel->cfg.col_visible[BUFF_COL_STONE]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_ENGPP), "", &panel->cfg.col_visible[BUFF_COL_ENGPP]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_REVNR), "", &panel->cfg.col_visible[BUFF_COL_REVNR]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_REVAP), "", &panel->cfg.col_visible[BUFF_COL_REVAP]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_REVRD), "", &panel->cfg.col_visible[BUFF_COL_REVRD]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_ELESM), "", &panel->cfg.col_visible[BUFF_COL_ELESM]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_GRDSN), "", &panel->cfg.col_visible[BUFF_COL_GRDSN]);
			saveConfig |= ImGui::MenuItemNoPopupClose(BuffTextFromColumn(BUFF_COL_NECVA), "", &panel->cfg.col_visible[BUFF_COL_NECVA]);
			ImGui::EndMenu();
		}
		if (saveConfig) PanelSaveColumnConfig(panel);
		ImGui::EndMenu();
	}
	return saveConfig;
}

// Compare buff player
static int PlayerCompare(void* pCtx, void const* pItem1, void const* pItem2)
{
#define LOBYTE(w)           ((uint8_t)(((uint64_t)(w)) & 0xff))
#define HIBYTE(w)           ((uint8_t)((((uint64_t)(w)) >> 8) & 0xff))
    
    i32 retVal = 0;
    u32 sortBy = LOBYTE(*(u32*)pCtx);
    bool isAsc = HIBYTE(*(u32*)pCtx);
    const Player* p1 = *(const Player**)pItem1;
    const Player* p2 = *(const Player**)pItem2;
    
#define BUFF_PERCENT(x, y) ((u32)(x->cd.y / (f32)(x->cd.duration) * 100.0f))
#define BUFF_COMPARE(x) \
    if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1; \
    else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1; \
    else if (BUFF_PERCENT(p1, x) < BUFF_PERCENT(p2, x)) retVal = -1; \
    else if (BUFF_PERCENT(p1, x) > BUFF_PERCENT(p2, x)) retVal = 1; \
    else retVal = 0;
    
#define BUFF_COMPARE_NOPCT(x) \
    if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1; \
    else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1; \
    else if (p1->cd.x < p2->cd.x) retVal = -1; \
    else if (p1->cd.x > p2->cd.x) retVal = 1; \
    else retVal = 0;
    
    switch (sortBy)
    {
        case(BUFF_COL_NAME):
            if (!p1->c.name[0] && p2->c.name[0])
                retVal = -1;
            else if (!p2->c.name[0] && p1->c.name[0])
                retVal = 1;
            else if (!p1->c.name[0] && !p2->c.name[0])
                retVal = 0;
            else {
                std::string utf1 = std_utf16_to_utf8((const utf16str)p1->c.name);
                std::string utf2 = std_utf16_to_utf8((const utf16str)p2->c.name);
                retVal = strcmp(utf1.c_str(), utf2.c_str());
            }
            break;
        case(BUFF_COL_CLS): {
                const char * profName1 = nameFromProfession(p1->c.profession, p1->c.is_elite);
                const char * profName2 = nameFromProfession(p2->c.profession, p2->c.is_elite);
                retVal = strcmp(profName1, profName2);
            }
            break;
        case(BUFF_COL_SUB):
            if (p1->c.subgroup <= 0 && p2->c.subgroup > 0) retVal = -1;
            else if (p2->c.subgroup <= 0 && p1->c.subgroup > 0) retVal = 1;
            else if (p1->c.subgroup < p2->c.subgroup) retVal = -1;
            else if (p1->c.subgroup > p2->c.subgroup) retVal = 1;
            else retVal = 0;
            break;
        case(BUFF_COL_DOWN):
            if (p1->cd.duration == 0 && p2->cd.duration != 0) retVal = -1;
            else if (p2->cd.duration == 0 && p1->cd.duration != 0) retVal = 1;
            else if (p1->cd.noDowned < p2->cd.noDowned) retVal = -1;
            else if (p1->cd.noDowned > p2->cd.noDowned) retVal = 1;
            else retVal = 0;
            break;
        case(BUFF_COL_SCLR):
            BUFF_COMPARE(sclr_dura);
            break;
        case(BUFF_COL_SEAW):
            BUFF_COMPARE(seaw_dura);
            break;
        case(BUFF_COL_PROT):
            BUFF_COMPARE(prot_dura);
            break;
        case(BUFF_COL_QUIK):
            BUFF_COMPARE(quik_dura);
            break;
        case(BUFF_COL_ALAC):
            BUFF_COMPARE(alac_dura);
            break;
        case(BUFF_COL_FURY):
            BUFF_COMPARE(fury_dura);
            break;
        case(BUFF_COL_MIGHT):
            BUFF_COMPARE_NOPCT(might_avg);
            break;
        case(BUFF_COL_GOTL):
            BUFF_COMPARE_NOPCT(gotl_avg);
            break;
        case(BUFF_COL_GLEM):
            BUFF_COMPARE(glem_dura);
            break;
        case(BUFF_COL_VIGOR):
            BUFF_COMPARE(vigor_dura);
            break;
        case(BUFF_COL_SWIFT):
            BUFF_COMPARE(swift_dura);
            break;
        case(BUFF_COL_STAB):
            BUFF_COMPARE(stab_dura);
            break;
        case(BUFF_COL_RETAL):
            BUFF_COMPARE(retal_dura);
            break;
        case(BUFF_COL_RESIST):
            BUFF_COMPARE(resist_dura);
            break;
        case(BUFF_COL_REGEN):
            BUFF_COMPARE(regen_dura);
            break;
        case(BUFF_COL_AEGIS):
            BUFF_COMPARE(aegis_dura);
            break;
        case(BUFF_COL_BANS):
            BUFF_COMPARE(bans_dura);
            break;
        case(BUFF_COL_BAND):
            BUFF_COMPARE(band_dura);
            break;
        case(BUFF_COL_BANT):
            BUFF_COMPARE(bant_dura);
            break;
        case(BUFF_COL_EA):
            BUFF_COMPARE(warEA_dura);
            break;
        case(BUFF_COL_SPOTTER):
            BUFF_COMPARE(spot_dura);
            break;
        case(BUFF_COL_FROST):
            BUFF_COMPARE(frost_dura);
            break;
        case(BUFF_COL_SUN):
            BUFF_COMPARE(sun_dura);
            break;
        case(BUFF_COL_STORM):
            BUFF_COMPARE(storm_dura);
            break;
        case(BUFF_COL_STONE):
            BUFF_COMPARE(stone_dura);
            break;
        case(BUFF_COL_ENGPP):
            BUFF_COMPARE(engPPD_dura);
            break;
        case(BUFF_COL_REVNR):
            BUFF_COMPARE(revNR_dura);
            break;
        case(BUFF_COL_REVAP):
            BUFF_COMPARE(revAP_dura);
            break;
        case(BUFF_COL_NECVA):
            BUFF_COMPARE(necVA_dura);
            break;
        case(BUFF_COL_TIME):
        {
            u32 p1time = (u32)(p1->cd.duration / 1000.0f);
            u32 p2time = (u32)(p2->cd.duration / 1000.0f);
            if (p1time == 0 && p2time != 0) retVal = -1;
            else if (p2time == 0 && p1time != 0) retVal = 1;
            else if (p1time < p2time) retVal = -1;
            else if (p1time > p2time) retVal = 1;
            else retVal = 0;
        }
            break;
    };
    
    if (isAsc)
        return retVal;
    else
        return -(retVal);
}

void ImGui_BuffUptime(struct Panel *panel, int x, int y)
{
	static ImGuiWindowFlags temp_flags = 0;
	static int temp_fr_count = 0;
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;
	IMGUI_FIX_NOINPUT_BUG;

    if (!panel->enabled) return;

	ImGuiStyle &style = ImGui::GetStyle();

	if (!panel->autoResize) temp_flags = 0;
	else if (temp_flags) {
		if (temp_fr_count++ > 1) {
			flags |= temp_flags;
			temp_flags = 0;
			temp_fr_count = 0;
		}
	}

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	uint32_t old_enabled = panel->enabled;
	ImVec2 ICON_SIZE(ImGui::GetFont()->FontSize+2, ImGui::GetFont()->FontSize);
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor old_border = style.Colors[ImGuiCol_Border];
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col));
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(LOCALTEXT(TEXT_BGDM_MAIN_PANEL_BUFF_UPTIME), (bool*)&panel->enabled, ImVec2(575, 215), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

	// was close button pressed 
	// uint32_t old_enabled = panel->enabled;
	if (old_enabled != panel->enabled)
		PanelSaveColumnConfig(panel);

	ImGui::PushStyleColor(ImGuiCol_Border, old_border);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{
			ShowWindowOptionsMenu(panel, &temp_flags);

			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_OPTS_LINENO), "", &panel->cfg.lineNumbering)) {
				config_set_int(panel->section, "LineNumbering", panel->cfg.lineNumbering);
			}
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_PANEL_OPTS_PROFCOL), "", &panel->cfg.useProfColoring)) {
				config_set_int(panel->section, "ProfessionColoring", panel->cfg.useProfColoring);
			}
			ImGui::EndMenu();
		}
		if (BuildBuffColumnsMenu(panel))
			temp_flags = ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::BeginMenu(LOCALTEXT(TEXT_BUFF_OPT_ICONS)))
		{
			bool needsUpdate = false;
			bool tmp_bool = (g_state.icon_pack_no == 0);
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BUFF_ICONS_STD), "", &tmp_bool)) {
				needsUpdate = true;
				g_state.icon_pack_no = 0;
			}
			tmp_bool = (g_state.icon_pack_no == 1);
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BUFF_ICONS_CONTRAST), "", &tmp_bool)) {
				needsUpdate = true;
				g_state.icon_pack_no = 1;
			}
			tmp_bool = (g_state.icon_pack_no == 2);
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BUFF_ICONS_DARK), "", &tmp_bool)) {
				needsUpdate = true;
				g_state.icon_pack_no = 2;
			}
			if (needsUpdate) {
				config_set_int(CONFIG_SECTION_BUFF_UPTIME, "IconPackNo", g_state.icon_pack_no);
			}
			ImGui::Separator();

			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BUFF_ICONS_DOWN), "", &g_state.use_downed_enemy_icon)) {
				config_set_int(CONFIG_SECTION_BUFF_UPTIME, "UseDownedEnemyIcon", g_state.use_downed_enemy_icon);
			}
			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_BUFF_ICONS_SEAWEED), "", &g_state.use_seaweed_icon)) {
				config_set_int(CONFIG_SECTION_BUFF_UPTIME, "UseSeaweedSaladIcon", g_state.use_seaweed_icon);
			}
			ImGui::EndMenu();
		}
		ShowCombatMenu();
		ImGui::EndPopup();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT2].col));

	int nCol = 1;
	for (int i = 1; i < BUFF_COL_END; ++i) {
		if (panel->cfg.col_visible[i])
			nCol++;
	}

	ImGui::Columns(nCol, NULL, true);
	SetColumnOffsets(panel, 1);
	bool sortFound = false;
	for (int i = 0; i < BUFF_COL_END; ++i) {
		if (!panel->cfg.col_visible[i])
			continue;
		if (i == panel->cfg.sortCol) {
			sortFound = true;
            void *img = ImGui_GetMiscImage(panel->cfg.asc ? 0 : 1);
			if (img) {
				ImGui::Image(img, ICON_SIZE);
			}
            else {
				ImGui::PushFont(tinyFont);
                
				ImGui::TextColored(white, panel->cfg.asc ? ">" : "<");
				ImGui::PopFont();
			}
		}
		ImGui::NextColumn();
	}
	if (!sortFound) ImGui::NewLine();

#define MAKEWORD(a, b)      ((uint16_t)(((uint8_t)(((uint64_t)(a)) & 0xff)) | ((uint16_t)((uint8_t)(((uint64_t)(b)) & 0xff))) << 8))
    uint32_t sortBy = MAKEWORD(panel->cfg.sortCol, panel->cfg.asc);
    
	ImGui::Columns(nCol, "BuffUptime", true);
	SetColumnOffsets(panel, 1);
	const ImVec4 colorActive = style.Colors[ImGuiCol_HeaderActive];
	const ImVec4 colorHover = style.Colors[ImGuiCol_HeaderHovered];
	for (int i = 0; i < BUFF_COL_END; ++i) {
		if (!panel->cfg.col_visible[i])
			continue;

		bool isClicked = false;
		void* img = BuffIconFromColumn(i);
		ImGui::PushStyleColor(ImGuiCol_Text, white);
		if (img) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorHover);
			int frame_padding = 0;
			ImGui::SameLine(6);
			if (ImGui::ImageButton(img, use_tinyfont ? ImVec2(18, 18) : ImVec2(24, 24), ImVec2(0, 0), ImVec2(1, 1), frame_padding, ImColor(0, 0, 0, 0))) {
				isClicked = true;
			}
			ImGui::PopStyleColor(3);
		}
		else if (i == 0) { ImGui::Selectable(LOCALTEXT(TEXT_GDPS_COLAC_NAME)); }
		else if (ImGui::Selectable(buff_col_str[i])) {}
		if (i > 0 && ImGui::BeginPopupContextItem(buff_col_str[i])) {
			ImGui::Text(BuffTextFromColumn(i));
			ImGui::Separator();
			if (ImGui::Selectable(LOCALTEXT(TEXT_GLOBAL_HIDE))) {
				panel->cfg.col_visible[i] = false;
				PanelSaveColumnConfig(panel);
				temp_flags = ImGuiWindowFlags_AlwaysAutoResize;
				ImGui::EndPopup();
				ImGui::PopStyleColor();
				goto abort;
			}
			ImGui::EndPopup();
		}
		isClicked = img ? isClicked : ImGui::IsItemClicked();
		if (isClicked) {
			panel->cfg.sortCol = i;
			if (panel->cfg.sortColLast != BUFF_COL_END &&
				panel->cfg.sortColLast == panel->cfg.sortCol)
				panel->cfg.asc ^= 1;
			else
				panel->cfg.asc = 0;
			panel->cfg.sortColLast = i;
			PanelSaveColumnConfig(panel);
		}
		ImGui::PopStyleColor();
		ImGui::NextColumn();
	}

    static std::vector<Player*> closePlayers;
    closePlayers.clear();
    if (controlledCharacter()->aptr != 0)
        closePlayers.push_back(controlledPlayer());
    currentContext()->closestPlayers.lock();
    currentContext()->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
        closePlayers.push_back(&entry->value.data);
    });
    
    if ( closePlayers.size() > 0 )
        qsort_r(closePlayers.data(), closePlayers.size(), sizeof( closePlayers[0] ), (void*)&sortBy, PlayerCompare);

    
    ImGui::Separator();
    for (int i=0; i<closePlayers.size(); ++i) {
        
		Player *player = closePlayers[i];
		float dur = player->cd.duration ? player->cd.duration / 1000.0f : 0;

		for (int j = 0; j < BUFF_COL_END; ++j) {
			if (!panel->cfg.col_visible[j])
				continue;

			if (j > BUFF_COL_SUB && dur == 0) {
				ImGui::NextColumn();
				continue;
			}
			
			switch (j) {
			case(BUFF_COL_NAME):
			{
                const std::string utf8 = std_utf16_to_utf8(player->c.decodedName ? (utf16str)player->c.decodedName : (utf16str)player->c.name);
                std::string no("");
				if (panel->cfg.lineNumbering) no = toString((i<9) ? "%d. " : "%d.", i+1);
				ImGui::TextColored(ColorFromCharacter(&player->c, panel->cfg.useProfColoring), u8"%s%s", no.c_str(), utf8.c_str());
			} break;
			case(BUFF_COL_CLS):
			{
				void* img = ProfessionImageFromPlayer(&player->c);
				if (img) ImGui::Image(img, ICON_SIZE);
			} break;
			case(BUFF_COL_SUB):
			{
				if (player->c.subgroup) ImGui::Text("%d", player->c.subgroup);
			} break;
			case (BUFF_COL_DOWN):
			{
				ImGui::Text("%d", player->cd.noDowned);
			} break;

#define PERCENT_STR(x, dura) {							\
		u32 per = (u32)(x / (f32)(dura) * 100.0f);		\
		per = per > 100 ? 100 : per;					\
		if (per>99) ImGui::SameLine(3);					\
		ImGui::Text("%u%%", per);						\
		}

			case(BUFF_COL_SCLR):
				PERCENT_STR(player->cd.sclr_dura, player->cd.duration);
				break;
			case(BUFF_COL_SEAW):
				PERCENT_STR(player->cd.seaw_dura, player->cd.duration500);
				break;
			case(BUFF_COL_PROT):
				PERCENT_STR(player->cd.prot_dura, player->cd.duration);
				break;
			case(BUFF_COL_QUIK):
				PERCENT_STR(player->cd.quik_dura, player->cd.duration);
				break;
			case(BUFF_COL_ALAC):
				PERCENT_STR(player->cd.alac_dura, player->cd.duration);
				break;
			case(BUFF_COL_FURY):
				PERCENT_STR(player->cd.fury_dura, player->cd.duration);
				break;
			case(BUFF_COL_MIGHT):
			{
				if (player->cd.might_avg >= 10.0f) ImGui::SameLine(3);
				ImGui::Text("%.1f", player->cd.might_avg);
			} break;
			case(BUFF_COL_GOTL):
			{
				ImGui::Text("%.1f", player->cd.gotl_avg);
			}break;

			case(BUFF_COL_GLEM):
				PERCENT_STR(player->cd.glem_dura, player->cd.duration);
				break;
			case(BUFF_COL_VIGOR):
				PERCENT_STR(player->cd.vigor_dura, player->cd.duration);
				break;
			case(BUFF_COL_SWIFT):
				PERCENT_STR(player->cd.swift_dura, player->cd.duration);
				break;
			case(BUFF_COL_STAB):
				PERCENT_STR(player->cd.stab_dura, player->cd.duration);
				break;
			case(BUFF_COL_RETAL):
				PERCENT_STR(player->cd.retal_dura, player->cd.duration);
				break;
			case(BUFF_COL_RESIST):
				PERCENT_STR(player->cd.resist_dura, player->cd.duration);
				break;
			case(BUFF_COL_REGEN):
				PERCENT_STR(player->cd.regen_dura, player->cd.duration);
				break;
			case(BUFF_COL_AEGIS):
				PERCENT_STR(player->cd.aegis_dura, player->cd.duration);
				break;
			case(BUFF_COL_BANS):
				PERCENT_STR(player->cd.bans_dura, player->cd.duration);
				break;
			case(BUFF_COL_BAND):
				PERCENT_STR(player->cd.band_dura, player->cd.duration);
				break;
			case(BUFF_COL_BANT):
				PERCENT_STR(player->cd.bant_dura, player->cd.duration);
				break;
			case(BUFF_COL_EA):
				PERCENT_STR(player->cd.warEA_dura, player->cd.duration);
				break;
			case(BUFF_COL_SPOTTER):
				PERCENT_STR(player->cd.spot_dura, player->cd.duration);
				break;
			case(BUFF_COL_FROST):
				PERCENT_STR(player->cd.frost_dura, player->cd.duration);
				break;
			case(BUFF_COL_SUN):
				PERCENT_STR(player->cd.sun_dura, player->cd.duration);
				break;
			case(BUFF_COL_STORM):
				PERCENT_STR(player->cd.storm_dura, player->cd.duration);
				break;
			case(BUFF_COL_STONE):
				PERCENT_STR(player->cd.stone_dura, player->cd.duration);
				break;
			case(BUFF_COL_ENGPP):
				PERCENT_STR(player->cd.engPPD_dura, player->cd.duration);
				break;
			case(BUFF_COL_REVNR):
				PERCENT_STR(player->cd.revNR_dura, player->cd.duration);
				break;
			case(BUFF_COL_REVAP):
				PERCENT_STR(player->cd.revAP_dura, player->cd.duration);
				break;
			case(BUFF_COL_REVRD):
				PERCENT_STR(player->cd.revRD_dura, player->cd.duration);
				break;
			case(BUFF_COL_ELESM):
				PERCENT_STR(player->cd.eleSM_dura, player->cd.duration);
				break;
			case(BUFF_COL_GRDSN):
				PERCENT_STR(player->cd.grdSIN_dura, player->cd.duration);
				break;
			case(BUFF_COL_NECVA):
				PERCENT_STR(player->cd.necVA_dura, player->cd.duration);
				break;
			case (BUFF_COL_TIME):
			{
				ImGui::Text("%dm %.2fs", (int)dur / 60, fmod(dur, 60));
			} break;

			}
			ImGui::NextColumn();
		}
    }
    currentContext()->closestPlayers.unlock();

abort:
	ImGui::PopStyleColor(2); // Text/Separator
	ImGui::End();
	ImGui::PopStyleColor(); // border
	ImGui::PopFont();
}

#if !(defined BGDM_TOS_COMPLIANT)
void DrawSkin(EquipItem *item)
{
	const utf16str unicode = (item->ptr && item->skin_def.name) ? (utf16str)item->skin_def.name : u"";
    const std::string utf8 = (item->ptr && item->skin_def.name) ? std_utf16_to_utf8(unicode) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
	ImGui::Text(utf8.c_str());
}

void DrawFashion(struct Character *c, EquipItems* eq, bool bLocalizedText)
{
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));

	ImGui::Columns(2, "Skins", false);
	ImGui::SetColumnOffset(1, 70);
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_BACK)); ImGui::NextColumn();
	DrawSkin(&eq->back); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_HEAD)); ImGui::NextColumn();
	DrawSkin(&eq->head); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_SHOULDER)); ImGui::NextColumn();
	DrawSkin(&eq->shoulder); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_CHEST)); ImGui::NextColumn();
	DrawSkin(&eq->chest); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_GLOVES)); ImGui::NextColumn();
	DrawSkin(&eq->gloves); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_LEGS)); ImGui::NextColumn();
	DrawSkin(&eq->leggings); ImGui::NextColumn();
	ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_BOOTS)); ImGui::NextColumn();
	DrawSkin(&eq->boots); ImGui::NextColumn();
	ImGui::Separator();
	if (eq->wep1_main.ptr) {
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_WEP1M)); ImGui::NextColumn();
		DrawSkin(&eq->wep1_main); ImGui::NextColumn();
	}
	if (eq->wep1_off.ptr) {
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_WEP1O)); ImGui::NextColumn();
		DrawSkin(&eq->wep1_off); ImGui::NextColumn();
	}
	if (eq->wep2_main.ptr) {
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_WEP2M)); ImGui::NextColumn();
		DrawSkin(&eq->wep2_main); ImGui::NextColumn();
	}
	if (eq->wep2_off.ptr) {
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_WEP2O)); ImGui::NextColumn();
		DrawSkin(&eq->wep2_off); ImGui::NextColumn();
	}
}

void DrawSpec(struct Character *c, Spec* spec, bool bLocalizedText)
{
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
    
	EquipItems eq = { 0 };
	if (Ch_GetInventory(c->inventory, &eq)) {

		ImGui::Columns(3, "Weapons", false);
		ImGui::SetColumnOffset(1, 100);
		ImGui::SetColumnOffset(2, 160);
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_WEPS));
		ImGui::NextColumn();
		if (eq.wep1_main.ptr) ImGui::Text(wep_name_from_id(eq.wep1_main.wep_type));
		if (eq.wep1_off.ptr) ImGui::Text(wep_name_from_id(eq.wep1_off.wep_type));
		ImGui::NextColumn();
		if (eq.wep2_main.ptr) ImGui::Text(wep_name_from_id(eq.wep2_main.wep_type));
		if (eq.wep2_off.ptr) ImGui::Text(wep_name_from_id(eq.wep2_off.wep_type));
		ImGui::NextColumn();
		ImGui::Separator();
	}

	ImGui::Columns(2, "Spec", false);
	ImGui::SetColumnOffset(1, 100);

	for (u32 i = 0; i < GW2::SPEC_SLOT_END; ++i)
	{
		if (bLocalizedText) {
            //CLogTrace("DrawSpec[%i] %p", i, spec->specs[i].name);
            const std::string utf8 = (spec->specs[i].name) ? std_utf16_to_utf8((utf16str)spec->specs[i].name) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
			ImGui::TextColored(white, utf8.c_str());
		}
		else {
			ImGui::TextColored(white, spec_name_from_id(spec->specs[i].id));
		}
		ImGui::NextColumn();

		for (u32 j = 0; j < GW2::TRAIT_SLOT_END; ++j) {

			if (bLocalizedText) {
                //CLogTrace("DrawSpec[%d][%d] %p", i, j, spec->traits[i][j].name);
                const std::string utf8 = (spec->traits[i][j].name) ? std_utf16_to_utf8((utf16str)spec->traits[i][j].name) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
				ImGui::Text(utf8.c_str());
			}
			else {
				ImGui::Text(trait_name_from_id(spec->specs[i].id, spec->traits[i][j].id));
			}
		}
		ImGui::NextColumn();
	}
    
}


static inline bool WeaponIs2H(uint32_t type)
{
	switch (type)
	{
	case (GW2::WEP_TYPE_HAMMER):
	case (GW2::WEP_TYPE_LONGBOW):
	case (GW2::WEP_TYPE_SHORTBOW):
	case (GW2::WEP_TYPE_GREATSWORD):
	case (GW2::WEP_TYPE_RIFLE):
	case (GW2::WEP_TYPE_STAFF):
		return true;
	default:
		return false;
	}
}

static inline ImColor ImColorFromRarity(u32 rarity)
{
	ImColor color(bgdm_colors[BGDM_COL_TEXT2].col);

	switch (rarity) {

	case (ITEM_RARITY_NONE):
		color = IMGUI_WHITE;
		break;
	case (ITEM_RARITY_FINE):
		color = IMGUI_LIGHT_BLUE;
		break;
	case (ITEM_RARITY_MASTER):
		color = IMGUI_GREEN;
		break;
	case (ITEM_RARITY_RARE):
		color = IMGUI_YELLOW;
		break;
	case (ITEM_RARITY_EXOTIC):
		color = IMGUI_ORANGE;
		break;
	case (ITEM_RARITY_ASCENDED):
		color = IMGUI_PINK;
		break;
	case (ITEM_RARITY_LEGENDARY):
		color = IMGUI_PURPLE;
		break;
	default:
		break;
	}
	return color;
}

static inline const utf16str last2Words(const utf16str wstr)
{
	if (!wstr)
		return NULL;
    
    utf16string utf16 = wstr;

    size_t len = utf16.length();
	int w = 0;
	while (--len > 0) {
		if (wstr[len] == ' ')
			if (++w == 2)
				return &wstr[len + 1];
	}
	return wstr;
}

void DrawItem(const char *slot, const EquipItem *eqItem, u32* ar, bool bLocalizedText, bool use_tinyFont)
{
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImGui::TextColored(white, slot);
	ImGui::NextColumn();

	if (!bLocalizedText) {
		ImGui::TextColored(ImColorFromRarity(eqItem->item_def.rarity), stat_name_from_id(eqItem->stat_def.id));
	}
	else {
		const utf16str unicode = eqItem->ptr ? (eqItem->stat_def.name ? (utf16str)eqItem->stat_def.name : u"") : u"";
		const std::string utf8 = eqItem->ptr ? (eqItem->stat_def.name ? std_utf16_to_utf8(unicode) : LOCALTEXT(TEXT_GEAR_NOSTATS)) : LOCALTEXT(TEXT_GEAR_NOEQUIP);
		ImGui::TextColored(ImColorFromRarity(eqItem->item_def.rarity), utf8.c_str());
	}

	bool isArmor = false;
	bool isWeapon = false;
	bool isAccessory = false;

	if (eqItem->type < GW2::EQUIP_SLOT_ACCESSORY1) {
		isArmor = true;
	}
	else if (eqItem->type > GW2::EQUIP_SLOT_AMULET) {
		isWeapon = true;
	}
	else {
		isAccessory = true;
	}

	if (isWeapon) ImGui::TextColored(ImColorFromRarity(eqItem->item_def.rarity), wep_name_from_id(eqItem->wep_type));

	if (isWeapon || isArmor ||
		(isAccessory && (eqItem->item_def.rarity < GW2::RARITY_ASCENDED || eqItem->infus_len == 0)))
	{
		// Upgrade #1, skip it for ascended accessories
		// as they can't have an upgrade only infusions
		if (!bLocalizedText) {
			const char *name = isWeapon ? sigil_name_from_id(eqItem->upgrade1.id) : rune_name_from_id(eqItem->upgrade1.id);
			ImGui::TextColored(ImColorFromRarity(eqItem->upgrade1.rarity), name);
		}
		else {
			const utf16str unicode = eqItem->upgrade1.name ? (utf16str)last2Words((utf16str)eqItem->upgrade1.name) : u"";
            const std::string utf8 = eqItem->upgrade1.name ? std_utf16_to_utf8(unicode) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
			ImGui::TextColored(ImColorFromRarity(eqItem->upgrade1.rarity), utf8.c_str());
		}
	}

	if (isWeapon && WeaponIs2H(eqItem->wep_type)) {

		// Upgrade #2 only valid for 2h weapons
		if (!bLocalizedText) {
			ImGui::TextColored(ImColorFromRarity(eqItem->upgrade2.rarity), sigil_name_from_id(eqItem->upgrade2.id));
		}
		else {
			const utf16str unicode = eqItem->upgrade2.name ? (utf16str)last2Words((utf16str)eqItem->upgrade2.name) : u"";
            const std::string utf8 = eqItem->upgrade2.name ? std_utf16_to_utf8(unicode) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
			ImGui::TextColored(ImColorFromRarity(eqItem->upgrade2.rarity), utf8.c_str());
		}
	}

	u32 items_per_line = (isWeapon || bLocalizedText) ? 1 : 2;
	for (int i = 0; i < eqItem->infus_len; i++) {

		if (i%items_per_line == 1)
		{
			ImGui::SameLine();
			ImGui::Text("|");
			ImGui::SameLine();
		}

		if (!bLocalizedText) {
			const char* name = infusion_name_from_id(eqItem->infus_arr[i].id, ar);
			ImGui::TextColored(ImColorFromRarity(eqItem->infus_arr[i].rarity), name);
		}
		else {
			const utf16str unicode = eqItem->infus_arr[i].name ? (utf16str)eqItem->infus_arr[i].name : u"";
            const std::string utf8 = eqItem->infus_arr[i].name ? std_utf16_to_utf8(unicode) : LOCALTEXT(TEXT_GLOBAL_NONE_CAPS);
			ImGui::TextColored(ImColorFromRarity(eqItem->infus_arr[i].rarity), utf8.c_str());
		}
	}

	ImGui::NextColumn();
}

void DrawInventory(struct Character *c, const EquipItems* eq, bool bLocalizedText, bool use_tinyFont)
{
	u32 ar_armor = 0;
	u32 ar_wep1m = 0;
	u32 ar_wep1o = 0;
	u32 ar_wep2m = 0;
	u32 ar_wep2o = 0;
    
	ImGui::Spacing(); ImGui::Spacing();

	ImGui::Columns(4, "Inventory", false);

	if (!bLocalizedText) {
		ImGui::SetColumnOffset(1, 70);
		ImGui::SetColumnOffset(2, 170);
		ImGui::SetColumnOffset(3, 240);
	}
	else {
		ImGui::SetColumnOffset(1, 70);
		ImGui::SetColumnOffset(2, 210);
		ImGui::SetColumnOffset(3, 280);
	}

	DrawItem(LOCALTEXT(TEXT_GEAR_HEAD), &eq->head, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_BACK), &eq->back, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_SHOULDER), &eq->shoulder, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_EAR1), &eq->acc_ear1, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_CHEST), &eq->chest, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_EAR2), &eq->acc_ear2, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_GLOVES), &eq->gloves, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_RING1), &eq->acc_ring1, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_LEGS), &eq->leggings, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_RING2), &eq->acc_ring2, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_BOOTS), &eq->boots, &ar_armor, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_AMULET), &eq->acc_amulet, &ar_armor, bLocalizedText, use_tinyFont);

	DrawItem(LOCALTEXT(TEXT_GEAR_WEP1M), &eq->wep1_main, &ar_wep1m, bLocalizedText, use_tinyFont);
	DrawItem(LOCALTEXT(TEXT_GEAR_WEP2M), &eq->wep2_main, &ar_wep2m, bLocalizedText, use_tinyFont);

	if (!WeaponIs2H(eq->wep1_main.wep_type))
		DrawItem(LOCALTEXT(TEXT_GEAR_WEP1O), &eq->wep1_off, &ar_wep1o, bLocalizedText, use_tinyFont);
    else { ImGui::NextColumn(); ImGui::NextColumn(); }
	if (!WeaponIs2H(eq->wep2_main.wep_type))
		DrawItem(LOCALTEXT(TEXT_GEAR_WEP2O), &eq->wep2_off, &ar_wep2o, bLocalizedText, use_tinyFont);

	u32 ar_main = ar_armor;
	u32 ar_alt = ar_armor;

	if (WeaponIs2H(eq->wep1_main.wep_type)) {
		ar_main += ar_wep1m;
	}
	else {
		if (eq->wep1_main.ptr)
			ar_main += ar_wep1m;
		else if (!WeaponIs2H(eq->wep2_main.wep_type) && eq->wep2_main.ptr)
			ar_main += ar_wep2m;
		if (eq->wep1_off.ptr)
			ar_main += ar_wep1o;
		else if (!WeaponIs2H(eq->wep2_main.wep_type) && eq->wep2_off.ptr)
			ar_main += ar_wep2o;
	}
	if (WeaponIs2H(eq->wep2_main.wep_type)) {
		ar_alt += ar_wep2m;
	}
	else {
		if (eq->wep2_main.ptr)
			ar_alt += ar_wep2m;
		else if (!WeaponIs2H(eq->wep1_main.wep_type) && eq->wep1_main.ptr)
			ar_alt += ar_wep1m;
		if (eq->wep2_off.ptr)
			ar_alt += ar_wep2o;
		else if (!WeaponIs2H(eq->wep1_main.wep_type) && eq->wep1_off.ptr)
			ar_alt += ar_wep1o;
	}

	if (ar_main > 0 || ar_alt > 0) {
		ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
		ImColor pink(IMGUI_PINK);
		ImGui::Spacing();
		ImGui::Columns(2, NULL, false);
		ImGui::Separator();
		ImGui::SetColumnOffset(1, 70);
		ImGui::TextColored(white, LOCALTEXT(TEXT_GEAR_AR_TOTAL));
		ImGui::NextColumn();
		ImGui::TextColored(pink, "%s %d", LOCALTEXT(TEXT_GEAR_AR_MAIN), ar_main);
		ImGui::TextColored(pink, "%s %d", LOCALTEXT(TEXT_GEAR_AR_ALT), ar_alt);
	}
    
}

void ImGui_CharInspect(struct Panel* panel, struct Character *c)
{
	ImGuiWindowFlags flags = 0;
	if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;

	if (!panel->enabled || c->aptr == 0 || !c->is_player) return;
    if (isCurrentMapPvPorWvW() && c->attitude != CHAR_ATTITUDE_FRIENDLY) return;

	ImGuiStyle &style = ImGui::GetStyle();

	if (!panel->init) {
			flags &= !NO_INPUT_FLAG;
			panel->init = true;
	}

	if (!panel->autoResize) panel->tmp_fl = 0;
	else if (panel->tmp_fl) {
		if (panel->tmp_fr++ > 1) {
			flags |= panel->tmp_fl;
			panel->tmp_fl = 0;
			panel->tmp_fr = 0;
		}
	}

	int mode = panel->mode;

	const char *TITLE_SELF = LOCALTEXT(TEXT_GEAR_TITLE_SELF);
	const char *TITLE_TARGET = LOCALTEXT(TEXT_GEAR_TITLE_TARGET);
	bool isControlledPlayer = controlledCharacter()->cptr == c->cptr;


	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(tinyFont);
	else ImGui::PushFont(defaultFont);

	uint32_t old_enabled = panel->enabled;
	ImVec2 ICON_SIZE = (global_use_tinyfont || use_tinyfont) ? ImVec2(18, 18) : ImVec2(22, 22);
	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImColor old_border = style.Colors[ImGuiCol_Border];
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(bgdm_colors[BGDM_COL_TARGET_LOCK_BG].col));
	//ImGui::SetNextWindowPos(ImVec2((float)panel->pos.x, (float)panel->pos.y), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(isControlledPlayer ? TITLE_SELF : TITLE_TARGET, (bool*)&panel->enabled, ImVec2(350, 360), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopFont();
		return;
	}

	// was close button pressed 
	// uint32_t old_enabled = panel->enabled;
	if (old_enabled != panel->enabled)
		PanelSaveColumnConfig(panel);

	ImGui::PushStyleColor(ImGuiCol_Border, old_border);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		if (ImGui::BeginMenu(LOCALTEXT(TEXT_GLOBAL_OPTIONS)))
		{
			ShowWindowOptionsMenu(panel, &panel->tmp_fl);

			if (ImGui::MenuItemNoPopupClose(LOCALTEXT(TEXT_GEAR_OPT_LOCALTEXT), "", &panel->cfg.useLocalizedText)) {
				panel->tmp_fl = ImGuiWindowFlags_AlwaysAutoResize;
				config_set_int(panel->section, "LocalizedText", panel->cfg.useLocalizedText);
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT2].col));

	{
		bool needUpdate = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_GEAR_TAB_GEAR), &mode, 0); ImGui::SameLine();
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_GEAR_TAB_TRAITS), &mode, 1); ImGui::SameLine();
		needUpdate |= ImGui::RadioButton(LOCALTEXT(TEXT_GEAR_TAB_SKINS), &mode, 2);
		ImGui::PopStyleVar();
		if (needUpdate) {
			panel->mode = mode;
			panel->tmp_fl = ImGuiWindowFlags_AlwaysAutoResize;
			config_set_int(panel->section, "Mode", panel->mode);
		}
	}

    __TRY
    
	ImGui::Spacing(); ImGui::Spacing();
	{
		void* img = ProfessionImageFromPlayer(c);
		if (img) ImGui::Image(img, ICON_SIZE); ImGui::SameLine();

		const utf16str unicode = c->name[0] ? (utf16str)c->name : (utf16str)c->decodedName;
        const std::string utf8 = std_utf16_to_utf8(unicode);
		ImGui::TextColored(white, u8"%s", utf8.c_str());
	}
    
	if (mode == 2) {
		EquipItems eq = { 0 };
        if (Ch_GetInventory(c->inventory, &eq)) {
			DrawFashion(c, &eq, panel->cfg.useLocalizedText);
            
        }
	}
	else if (mode == 1) {
		Spec spec = { 0 };
        if (Pl_GetSpec(c->pptr, &spec)) {
			DrawSpec(c, &spec, panel->cfg.useLocalizedText);
        }
	}
	else {
		EquipItems eq = { 0 };
        if (Ch_GetInventory(c->inventory, &eq)) {
			DrawInventory(c, &eq, panel->cfg.useLocalizedText, global_use_tinyfont || use_tinyfont);
        }
	}
    
    __EXCEPT("[ImGui_CharInspect] access violation")
    __FINALLY
    
	ImGui::PopStyleColor(2); // Text/Separator
	ImGui::End();
	ImGui::PopStyleColor(); // border
	ImGui::PopFont();
}
#endif


///////////////////////////
// DEBUG FUNCTIONS START //
///////////////////////////

#if (defined _DEBUG) || (defined DEBUG)
static char	BaseAddrInput[32] = { 0 };

static inline void CopyToClipboard(const char *str)
{
	strncpy(BaseAddrInput, str, sizeof(BaseAddrInput));

#ifdef TARGET_OS_WIN
	// Commented out as it causes delays on EmptyClipboard()
	// seems to works with NULL ptr for OpenClipboard()
	//ImGuiIO& io = ImGui::GetIO();
	if (OpenClipboard((HWND)NULL))//io.ImeWindowHandle))
	{
		ATL::CAtlStringA source = str;
		HGLOBAL clipbuffer;
		char * buffer;
		EmptyClipboard();
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, source.GetLength() + 1);
		buffer = (char*)GlobalLock(clipbuffer);
		StringCchCopyA(buffer, source.GetLength() + 1, LPCSTR(source));
		GlobalUnlock(clipbuffer);
		SetClipboardData(CF_TEXT, clipbuffer);
		CloseClipboard();
	}
#endif
}

static inline void SelectableWithCopyHandler(const char *str)
{
	if (ImGui::Selectable(str, false, ImGuiSelectableFlags_AllowDoubleClick)) {
		if (ImGui::IsMouseDoubleClicked(0))
			CopyToClipboard(str);
	}
}

static inline void ColumnWithCopyHandler(const char *str)
{
	SelectableWithCopyHandler(str);
	ImGui::NextColumn();
}

/*static inline void PopupCtxCopy(const char *str)
{
	if (ImGui::BeginPopupContextItem("CtxClipboard"))
	{
		if (ImGui::Selectable("Copy")) {
			CopyToClipboard(str);
		}
		ImGui::EndPopup();
	}
}*/

static inline void PrintGamePtr(const char *var, uintptr_t ptr, const char* fmt, ...)
{
	ColumnWithCopyHandler(var);

    std::string str;
	str = toString("%p", ptr);
	if (ptr == 0) ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT3].col));
	ColumnWithCopyHandler(str.c_str());
	if (ptr == 0) ImGui::PopStyleColor();

    if (fmt && strlen(fmt)>0) {
        va_list args;
        va_start(args, fmt);
        str = toString(fmt, args);
        va_end(args);
        ColumnWithCopyHandler(str.c_str());
    }
    else ImGui::NextColumn();
}

static inline void StrWithCopyHandler(const char* fmt, ...)
{
    std::string str;
	va_list args;
	va_start(args, fmt);
	str = toString(fmt, args);
	va_end(args);
	SelectableWithCopyHandler(str.c_str());
}

static inline void Str2WithCopyHandler(float spacing, const char *name, const char* fmt, ...)
{
    ImGui::Text(name);
    //SelectableWithCopyHandler(name);
    
    if (fmt && strlen(fmt)>0) {
        std::string str;
        va_list args;
        va_start(args, fmt);
        str = toString(fmt, args);
        va_end(args);
        
        ImGui::SameLine(spacing);
        SelectableWithCopyHandler(str.c_str());
    }
}

#ifdef TARGET_OS_WIN
static inline void TreeNodeTextWrapped(float wrap_width, const char* name, const char *data)
{
	if (ImGui::TreeNode(name)) {

		if (wrap_width) {
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
			ImGui::Text(data);
			//StrWithCopyHandler("%s", utf8.GetString());
			ImGui::PopTextWrapPos();
		} else {
			ImGui::TextWrapped(data);
		}
		ImGui::TreePop();
	}
}
#endif

static inline void Vec3WithCopyHandler(float spacing, const char *name, const vec3* pos)
{
	Str2WithCopyHandler(spacing, name,
		"{%03.2f, %03.2f, %03.2f}",
		pos->x, pos->y, pos->z);
}

static inline void PrintCharacter(const char* title, struct Character* c, bool open = 1)
{
	if (!c || c->aptr == 0) {
		ImGui::NextColumn();
		return;
	}

    if (ImGui::TreeNodeEx(title, open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {

        
        float spacing = 130;
        std::string utf8 = std_utf16_to_utf8((utf16str)c->name);
		Str2WithCopyHandler(spacing, "name:", utf8.c_str());

        utf8.clear();
		if (c->decodedName) utf8 = std_utf16_to_utf8((utf16str)c->decodedName);
		Str2WithCopyHandler(spacing, "decodedName:", utf8.c_str());

        Str2WithCopyHandler(spacing, "agent_id:", "%d", c->agent_id);
        Str2WithCopyHandler(spacing, "player_id:", "%d", c->player_id);
        
		Str2WithCopyHandler(spacing, "aptr:", "%#" PRIxPTR, c->aptr);
		Str2WithCopyHandler(spacing, "cptr:", "%#" PRIxPTR, c->cptr);
		Str2WithCopyHandler(spacing, "pptr:", "%#" PRIxPTR, c->pptr);
		Str2WithCopyHandler(spacing, "bptr:", "%#" PRIxPTR, c->bptr);
		Str2WithCopyHandler(spacing, "tptr:", "%#" PRIxPTR, c->tptr);
        Str2WithCopyHandler(spacing, "hptr:", "%#" PRIxPTR, c->hptr);
		Str2WithCopyHandler(spacing, "wmptr:", "%#" PRIxPTR, c->wmptr);
		Str2WithCopyHandler(spacing, "cbptr:", "%#" PRIxPTR, c->cbptr);
		Str2WithCopyHandler(spacing, "ctptr:", "%#" PRIxPTR, c->ctptr);
        Str2WithCopyHandler(spacing, "gdptr:", "%#" PRIxPTR, c->gdptr);
        Str2WithCopyHandler(spacing, "atptr:", "%#" PRIxPTR, c->atptr);
        Str2WithCopyHandler(spacing, "spdef:", "%#" PRIxPTR, c->spdef);
		Str2WithCopyHandler(spacing, "invptr:", "%#" PRIxPTR, c->inventory);
        Str2WithCopyHandler(spacing, "uuid:", "%#" PRIx64, c->uuid);
        
        utf8.clear();
        const utf16str acccountNameUTF16 = (const utf16str)Contact_GetAccountName(c->ctptr);
        if (acccountNameUTF16)
            utf8 = std_utf16_to_utf8(acccountNameUTF16+1);
        Str2WithCopyHandler(spacing, "acct:", utf8.c_str());

		Str2WithCopyHandler(spacing, "type:", "%d", c->type);
		Str2WithCopyHandler(spacing, "attitude:", "%d", c->attitude);
		Str2WithCopyHandler(spacing, "profession:", "%d", c->profession);
		Str2WithCopyHandler(spacing, "stance:", "%d", c->stance);
		Str2WithCopyHandler(spacing, "race:", "%d", c->race);
		Str2WithCopyHandler(spacing, "gender:", "%d", c->gender);
		Str2WithCopyHandler(spacing, "is_player:", "%d", c->is_player);
		Str2WithCopyHandler(spacing, "is_clone:", "%d", c->is_clone);
		Str2WithCopyHandler(spacing, "is_alive:", "%d", c->is_alive);
		Str2WithCopyHandler(spacing, "is_downed:", "%d", c->is_downed);
		Str2WithCopyHandler(spacing, "in_combat:", "%d", c->in_combat);
        Str2WithCopyHandler(spacing, "is_elite:", "%d", c->is_elite);
        Str2WithCopyHandler(spacing, "is_party:", "%d", c->is_party);
        Str2WithCopyHandler(spacing, "subgroup:", "%d", c->subgroup);
		Str2WithCopyHandler(spacing, "hp_max:", "%d", c->hp_max);
		Str2WithCopyHandler(spacing, "hp_val:", "%d", c->hp_val);
		Str2WithCopyHandler(spacing, "bb_state:", "%d", c->bb_state);
		Str2WithCopyHandler(spacing, "bb_value:", "%d", c->bb_value);
        Str2WithCopyHandler(spacing, "stance:", "%d", c->stance);
        Str2WithCopyHandler(spacing, "energy_cur:", "%.0f", c->energy_cur);
        Str2WithCopyHandler(spacing, "energy_max:", "%.0f", c->energy_max);

        vec2 pAgent = {0};
        vec2 pTrans = {0};
        TransportPosProject2D(c->tpos, &pAgent);
        AgentPosProject2D(c->apos, &pTrans);
        
		Str2WithCopyHandler(spacing, "pos (trans):", "{%03.2f, %03.2f, %03.2f} (%.0f, %.0f)", c->tpos.x, c->tpos.y, c->tpos.z, pTrans.x, pTrans.y);
		Str2WithCopyHandler(spacing, "pos (agent):", "{%03.2f, %03.2f, %03.2f} (%.0f, %.0f)", c->apos.x, c->apos.y, c->apos.z, pAgent.x, pAgent.y);
        
        Str2WithCopyHandler(spacing, "distance:", "%.0f", CalcDistance(controlledCharacter()->apos, c->apos, false));

		Spec spec = { 0 };
		if (Pl_GetSpec(c->pptr, &spec))
		{
			if (ImGui::TreeNode("spec")) {

				for (u32 i = 0; i < GW2::SPEC_SLOT_END; ++i)
				{
					StrWithCopyHandler("Line[%d]:%d", i, spec.specs[i].id);
					for (u32 j = 0; j < GW2::TRAIT_SLOT_END; ++j)
					{
						ImGui::SameLine();
						StrWithCopyHandler("Col[%d]:%d", j, spec.traits[i][j].id);
					}
				}

				ImGui::TreePop();
			}
		}

		bool needpop = false;
		
        __TRY
            
			if (c->cbptr) {
				hl::ForeignClass combatant = (void*)(c->cbptr);
				if (combatant) {
					hl::ForeignClass buffBar = combatant.call<void*>(g_offsets.combatantVtGetBuffbar.val);
					if (buffBar) {
						auto buffs = buffBar.get<ANet::Collection<BuffEntry, false>>(OFF_BUFFBAR_BUFF_ARR);
						if (buffs.isValid()) {

							if (ImGui::TreeNode("buffs")) {

								needpop = true;
                                Str2WithCopyHandler(spacing, "buffptr:", "%p", buffBar);
								Str2WithCopyHandler(spacing, "capacity:", "%d", buffs.capacity());
								Str2WithCopyHandler(spacing, "count:", "%d", buffs.count());
								Str2WithCopyHandler(spacing, "data:", "%#" PRIxPTR, buffs.data());

                                std::string str;
								for (uint32_t i = 0; i < buffs.capacity(); i++) {
									BuffEntry be = buffs[i];
									hl::ForeignClass pBuff = be.pBuff;
									if (pBuff) {
										i32 efType = pBuff.get<i32>(OFF_BUFF_EF_TYPE);
										if (i > 0) str += toString("  ");
										str += toString("%d:%d", i, efType);
									}
								}

								//static float wrap_width = 350.0f;
								//ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
								//ImGui::InputTextMultiline("##buffs", (char*)str.GetString(), str.GetLength(),
								//	ImVec2(wrap_width, ImGui::GetTextLineHeight() * 10), ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_ReadOnly);
								ImGui::TextWrapped(str.c_str());
								//ImGui::PopTextWrapPos();

								ImGui::TreePop();
								needpop = false;
							}
						}
					}
				}
			}
		
        
		__EXCEPT("[PrintCharacter] access violation")
			if (needpop) ImGui::TreePop();
		__FINALLY

		ImGui::TreePop();
	} ImGui::NextColumn();
}

static inline void PrintTarget(int idx, DPSTarget* t)
{

	if (!t || t->aptr == 0) {
		ImGui::NextColumn(); ImGui::NextColumn(); ImGui::NextColumn();
		return;
	}

	StrWithCopyHandler("%d", idx); ImGui::NextColumn();

    std::string str = toString("%d: ", t->agent_id);
    if (t->decodedName) str += std_utf16_to_utf8((utf16str)t->decodedName);
    else str += toString("%#" PRIxPTR, t->aptr);

	if (ImGui::TreeNode(str.c_str())) {

		float spacing = 130;

        std::string utf8 = "<unresolved>";
		if (t->decodedName) utf8 = std_utf16_to_utf8((utf16str)t->decodedName);
        
        Str2WithCopyHandler(spacing, "name:", "%s", utf8.c_str());
		Str2WithCopyHandler(spacing, "aptr:", "%#" PRIxPTR, t->aptr);
        Str2WithCopyHandler(spacing, "wmptr:", "%#" PRIxPTR, t->wmptr);
        Str2WithCopyHandler(spacing, "type:", "%d", t->type);
		Str2WithCopyHandler(spacing, "agent_id:", "%d", t->agent_id);
        Str2WithCopyHandler(spacing, "shard_id:", "%d", t->shard_id);
		Str2WithCopyHandler(spacing, "npc_id:", "%d", t->npc_id);
		Str2WithCopyHandler(spacing, "species_id:", "%d", t->species_id);
		Str2WithCopyHandler(spacing, "isDead:", "%d", t->isDead);
		Str2WithCopyHandler(spacing, "invuln:", "%d", t->invuln);
		Str2WithCopyHandler(spacing, "duration:", "%lld", t->duration);
		Str2WithCopyHandler(spacing, "time_begin:", "%lld", t->time_begin);
		Str2WithCopyHandler(spacing, "time_hit:", "%lld", t->time_hit);
		Str2WithCopyHandler(spacing, "time_update:", "%lld", t->time_update);
		Str2WithCopyHandler(spacing, "hp_max:", "%d", t->hp_max);
        Str2WithCopyHandler(spacing, "hp_val:", "%d", t->hp_val);
        Str2WithCopyHandler(spacing, "hp_last:", "%d", t->hp_last);
		Str2WithCopyHandler(spacing, "hp_lost:", "%d", t->hp_lost);
		Str2WithCopyHandler(spacing, "tdmg:", "%d", t->tdmg);
		Str2WithCopyHandler(spacing, "c1dmg:", "%d", t->c1dmg);
		Str2WithCopyHandler(spacing, "c2dmg:", "%d", t->c2dmg);
		Str2WithCopyHandler(spacing, "c1dmg_team:", "%d", t->c1dmg_team);
		Str2WithCopyHandler(spacing, "c2dmg_team:", "%d", t->c2dmg_team);
		Str2WithCopyHandler(spacing, "num_hits:", "%d", t->num_hits);
		Str2WithCopyHandler(spacing, "num_hits_team:", "%d", t->num_hits_team);

		ImGui::TreePop();
	} ImGui::NextColumn();

	StrWithCopyHandler("%lld", t->time_update); ImGui::NextColumn();
}

static inline void PrintServerPlayer(int idx, const Player* p)
{
    
    if (!p) {
        ImGui::NextColumn(); ImGui::NextColumn();
        return;
    }
    
    StrWithCopyHandler("%d", idx); ImGui::NextColumn();
    std::string utf8 = toString("%d: ", p->c.agent_id);
    utf8 += std_utf16_to_utf8(p->c.decodedName ? (utf16str)p->c.decodedName : (utf16str)p->c.name);
    
    if (ImGui::TreeNode(utf8.c_str())) {
        
        float spacing = 130;
        
        utf8.clear();
        utf8 = std_utf16_to_utf8((utf16str)p->c.name);
        Str2WithCopyHandler(spacing, "name:", utf8.c_str());
        
        utf8.clear();
        if (p->c.decodedName) utf8 = std_utf16_to_utf8((utf16str)p->c.decodedName);
        Str2WithCopyHandler(spacing, "decodedName:", utf8.c_str());
        
        Str2WithCopyHandler(spacing, "agent_id:", "%d", p->c.agent_id);
        Str2WithCopyHandler(spacing, "profession:", "%d", p->dps.profession);
        Str2WithCopyHandler(spacing, "in_combat:", "%d", p->dps.in_combat);
        Str2WithCopyHandler(spacing, "time:", "%.0f", p->dps.time);
        Str2WithCopyHandler(spacing, "damage:", "%d", p->dps.damage);
        Str2WithCopyHandler(spacing, "damage_in:", "%d", p->dps.damage_in);
        Str2WithCopyHandler(spacing, "heal_out:", "%d", p->dps.heal_out);
        Str2WithCopyHandler(spacing, "target_time:", "%.0f", p->dps.target_time);
        Str2WithCopyHandler(spacing, "target_dmg:", "%d", p->dps.target_dmg);
        
        if (ImGui::TreeNode("stats")) {
            
            Str2WithCopyHandler(spacing, "pow:", "%d", p->dps.stats.pow);
            Str2WithCopyHandler(spacing, "pre:", "%d", p->dps.stats.pre);
            Str2WithCopyHandler(spacing, "tuf:", "%d", p->dps.stats.tuf);
            Str2WithCopyHandler(spacing, "vit:", "%d", p->dps.stats.vit);
            Str2WithCopyHandler(spacing, "fer:", "%d", p->dps.stats.fer);
            Str2WithCopyHandler(spacing, "hlp:", "%d", p->dps.stats.hlp);
            Str2WithCopyHandler(spacing, "cnd:", "%d", p->dps.stats.cnd);
            Str2WithCopyHandler(spacing, "con:", "%d", p->dps.stats.con);
            Str2WithCopyHandler(spacing, "exp:", "%d", p->dps.stats.exp);
            
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode("targets")) {
            
            for (int i = 0; i<MSG_TARGETS; ++i) {
                if (p->dps.targets[i].id) {
                    const ClientTarget &target = p->dps.targets[i];
                    utf8 = toString("%d", target.id);
                    if (ImGui::TreeNode(utf8.c_str())) {
                        
                        Str2WithCopyHandler(spacing, "id:", "%d", target.id);
                        Str2WithCopyHandler(spacing, "time:", "%d", target.time);
                        Str2WithCopyHandler(spacing, "tdmg:", "%d", target.tdmg);
                        Str2WithCopyHandler(spacing, "invuln:", "%d", target.invuln);
                        Str2WithCopyHandler(spacing, "reserved1:", "%d", target.reserved1);
                        
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::TreePop();
        }
        
        ImGui::TreePop();
    } ImGui::NextColumn();
}

void ImGui_ToggleDebug()
{
    show_debug ^= 1;
}
#endif  // (defined _DEBUG) || (defined DEBUG)

void ImGui_ToggleInput()
{
    g_state.global_cap_input ^= 1;
    config_set_int(CONFIG_SECTION_GLOBAL, "CaptureInput", g_state.global_cap_input);
}

void ImGui_ToggleMinPanels()
{
    minimal_panels ^= 1;
    g_state.minPanels_enable = minimal_panels;
    config_set_int(CONFIG_SECTION_GLOBAL, "MinimalPanels", minimal_panels);
}

#if (defined _DEBUG) || (defined DEBUG)
void ImGui_Debug(struct Panel* panel,
                 struct __GameContext *chCliCtx,
                 struct Character *player,
                 struct Character *target)
{

	ImGuiWindowFlags flags = 0;

	if (!panel->enabled) return;
	//if (minimal_panels) flags |= MIN_PANEL_FLAGS;
	if (IsDisableInputOrActionCam()) flags |= NO_INPUT_FLAG;


	if (!show_debug) return;

	if (!panel->init) {
		flags &= !NO_INPUT_FLAG;
		panel->init = true;
	}

	if (!panel->autoResize) panel->tmp_fl = 0;
	else if (panel->tmp_fl) {
		if (panel->tmp_fr++ > 1) {
			flags |= panel->tmp_fl;
			panel->tmp_fl = 0;
			panel->tmp_fr = 0;
		}
	}

	bool use_tinyfont = panel->tinyFont;
	if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(proggyTiny);
	else ImGui::PushFont(proggyClean);

	ImGui::SetNextWindowPos(ImVec2(720, 5), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin("BGDM Debug", &show_debug, ImVec2(750, 400), panel->fAlpha, flags))
	{
		ImGui::End();
		ImGui::PopFont();
		return;
	}

    ImGui::PushAllowKeyboardFocus(false);
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
	{
		ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if (ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("CtxMenu");
	}
	if (ImGui::BeginPopup("CtxMenu"))
	{
		if (ImGui::BeginMenu("Options"))
		{
			ShowWindowOptionsMenu(panel, &panel->tmp_fl);

			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	ImColor white(ImColor(bgdm_colors[BGDM_COL_TEXT1].col));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(bgdm_colors[BGDM_COL_TEXT2].col));

	ImGui::PushItemWidth(140);
	if (ImGui::InputText("##baseaddr", BaseAddrInput, sizeof(BaseAddrInput), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) { }
	ImGui::SameLine();
	if (ImGui::Button("Open in Memory Editor", ImVec2(0, 0))) {

		uintptr_t base_addr;
		if (sscanf(BaseAddrInput, "%" SCNxPTR, &base_addr) == 1)
		{
			if (global_use_tinyfont || use_tinyfont) ImGui::PushFont(proggyTiny);
			else ImGui::PushFont(proggyClean);
			mem_edit.Open((unsigned char*)base_addr);
			ImGui::PopFont();
		}
	}

	if (ImGui::CollapsingHeader("Game Pointers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Columns(3, "Offsets", true);
		ImGui::Separator();
		ImGui::TextColored(white, "VAR"); ImGui::NextColumn();
		ImGui::TextColored(white, "ADDR"); ImGui::NextColumn();
		ImGui::TextColored(white, "VALUE"); ImGui::NextColumn();
		ImGui::Separator();

        GameOffsets &m = g_offsets;
        
        PrintGamePtr("tlsCtx()", TlsContext(), "");
        PrintGamePtr("CharClientCtx()", CharClientCtx(), "");
        PrintGamePtr("WmAgentArray()", WmAgentArray(), "%d:%d", currentContext()->wmAgentArray.size(), currentContext()->wmAgentArray.capacity());
        PrintGamePtr("GadgetCtx()", GadgetCtx(), "");
        PrintGamePtr("ContactCtx()", ContactCtx(), "");
        PrintGamePtr("SquadCtx()", SquadCtx(), "");
        PrintGamePtr("ChatCtx()", ChatCtx(), "");
        PrintGamePtr("WorldViewCtx()", WorldViewCtx(), "status:%d", WorldViewCtx() ? *(int*)(WorldViewCtx()+OFF_WVCTX_STATUS) : 0);
        PrintGamePtr("buildId", m.buildId.addr, "%lu", m.buildId.val);
        
#undef OFFSET
#define OFFSET(_type, _var, _default, _pattern, _is_str, _occurence, _ref_occur, _cbArr, _verify)               \
        PrintGamePtr(#_var, m._var.addr, "%#llx", m._var.val);
#ifdef __APPLE__
#include "offsets/offsets_osx.def"
#else
#include "offsets/offsets.def"
#endif
#undef OFFSET
        
		ImGui::Columns(1);
		ImGui::Separator();
	}

	if (ImGui::CollapsingHeader("Camera & Misc Data"))
	{
		ImGui::Columns(2, "MumbleCam");
		ImGui::Separator();

		float spacing = 130;
#ifdef TARGET_OS_WIN
		LinkedMem* pMum = (LinkedMem*)g_offsets.pMumble;
#else
        LinkedMem* pMum = NULL;
#endif
        CamData* pCam = currentCam();
		CompassData compassData = { 0 };
		bool hasCompassData = GetCompassData(&compassData);

        if (!pCam && !hasCompassData) {
            ImGui::NextColumn();
        }
        else {
            
            if (ImGui::TreeNodeEx("Cam Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                
                Str2WithCopyHandler(spacing, "p_cam:", "%p", pCam);
                Str2WithCopyHandler(spacing, "valid:", "%d", pCam->valid);
                
                Vec3WithCopyHandler(spacing, "camPos:", (vec3*)&pCam->camPos);
                Vec3WithCopyHandler(spacing, "upVec:", (vec3*)&pCam->upVec);
                Vec3WithCopyHandler(spacing, "lookAt:", (vec3*)&pCam->lookAt);
                Vec3WithCopyHandler(spacing, "viewVec:", (vec3*)&pCam->viewVec);
                
                Str2WithCopyHandler(spacing, "fovy:", "%f", pCam->fovy);
                Str2WithCopyHandler(spacing, "curZoom:", "%f", pCam->curZoom);
                Str2WithCopyHandler(spacing, "minZoom:", "%f", pCam->minZoom);
                Str2WithCopyHandler(spacing, "maxZoom:", "%f", pCam->maxZoom);
                
                ImGui::TreePop();
            }
            
        } ImGui::NextColumn();
        
        {
            if (ImGui::TreeNodeEx("Compass Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                
                Str2WithCopyHandler(spacing, "pComp:", "%p", compassData.pComp);
                Str2WithCopyHandler(spacing, "width:", "%f", compassData.width);
                Str2WithCopyHandler(spacing, "height:", "%f", compassData.height);
                Str2WithCopyHandler(spacing, "maxWidth:", "%f", compassData.maxWidth);
                Str2WithCopyHandler(spacing, "maxHeight:", "%f", compassData.maxHeight);
                Str2WithCopyHandler(spacing, "zoom:", "%d", compassData.zoom);
                Str2WithCopyHandler(spacing, "mouseOver:", "%d", compassData.flags.mouseOver);
                Str2WithCopyHandler(spacing, "position:", "%d", compassData.flags.position);
                Str2WithCopyHandler(spacing, "rotation:", "%d", compassData.flags.rotation);
                
                ImGui::TreePop();
            }
        } ImGui::NextColumn();
        
        {
            if (ImGui::TreeNodeEx("Misc Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                
                Str2WithCopyHandler(spacing, "accountName", "%s", accountName());
                Str2WithCopyHandler(spacing, "uiInterfaceSize", "%d", uiInterfaceSize());
                Str2WithCopyHandler(spacing, "mapId", "%d", currentMapId());
                Str2WithCopyHandler(spacing, "mapType", "%d", currentMapType());
                Str2WithCopyHandler(spacing, "shardId", "%d", currentShardId());
                Str2WithCopyHandler(spacing, "isMapOpen", "%d", isMapOpen());
                Str2WithCopyHandler(spacing, "isActionCam", "%d", isActionCam());
                Str2WithCopyHandler(spacing, "isInCutscene", "%d", isInCutscene());
                Str2WithCopyHandler(spacing, "isInterfaceHide", "%d", isInterfaceHidden());
                
                Str2WithCopyHandler(spacing, "GOTL", "%d", controlledPlayer()->buffs.GOTL);
                
#ifdef TARGET_OS_WIN
                Str2WithCopyHandler(spacing, "is_gw2china:", "%d", g_state.is_gw2china ? 1 : 0);
#endif
                ImGui::TreePop();
            }
            
        } ImGui::NextColumn();
        
        {
            if (ImGui::TreeNodeEx("ImGui Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                
                int display_w, display_h;
                ImGui_GetDisplaySize(&display_w, &display_h);
                
                Str2WithCopyHandler(spacing, "capKeyboard", "%d", shouldCapKeyboard(NULL));
                Str2WithCopyHandler(spacing, "captMouse", "%d", shouldCapMouse(NULL));
                Str2WithCopyHandler(spacing, "capTextInput", "%d", shouldCapInput(NULL));
                
                Str2WithCopyHandler(spacing, "displayWidth", "%d", display_w);
                Str2WithCopyHandler(spacing, "displayHeight", "%d", display_h);
                
                ImVec2 mousePos = ImGui::GetMousePos();
                Str2WithCopyHandler(spacing, "mousePosition", "(%.0f, %.0f)", mousePos.x, mousePos.y);
                
                ImGui::TreePop();
            }
        } ImGui::NextColumn();
        
		if (!pMum) {
			ImGui::NextColumn();
		}
#ifdef TARGET_OS_WIN
		else if (ImGui::TreeNodeEx("Mumble Link", ImGuiTreeNodeFlags_DefaultOpen)) {

			Str2WithCopyHandler(spacing, "build_id:", "%d", g_offsets.buildId.val);
			Str2WithCopyHandler(spacing, "p_mumble:", "%p", pMum);

			if (pMum) {
                std::string utf8 = std_utf16_to_utf8((utf16str)pMum->name);
				Str2WithCopyHandler(spacing, "name:", "%s", utf8.c_str());

				Str2WithCopyHandler(spacing, "fovy:", "%f", get_mum_fovy());

				Vec3WithCopyHandler(spacing, "cam_pos:", &pMum->cam_pos);
				Vec3WithCopyHandler(spacing, "cam_front:", &pMum->cam_front);
				Vec3WithCopyHandler(spacing, "cam_top:", &pMum->cam_top);
				Vec3WithCopyHandler(spacing, "avatar_pos:", &pMum->avatar_pos);
				Vec3WithCopyHandler(spacing, "avatar_front:", &pMum->avatar_front);
				Vec3WithCopyHandler(spacing, "avatar_top:", &pMum->avatar_top);

				Str2WithCopyHandler(spacing, "ui_version:", "%d", pMum->ui_version);
				Str2WithCopyHandler(spacing, "ui_tick:", "%d", pMum->ui_tick);

				Str2WithCopyHandler(spacing, "shard_id:", "%d", pMum->shard_id);
				Str2WithCopyHandler(spacing, "map_id:", "%d", pMum->map_id);
				Str2WithCopyHandler(spacing, "map_type:", "%d", pMum->map_type);
				Str2WithCopyHandler(spacing, "instance:", "%d", pMum->instance);
				Str2WithCopyHandler(spacing, "build_id:", "%d", pMum->build_id);

				Str2WithCopyHandler(spacing, "context_len:", "%d", pMum->context_len);
				TreeNodeTextWrapped(0.0f, "context", (char*)pMum->context);

				utf8 = std_utf16_to_utf8((utf16str)pMum->identity);
				TreeNodeTextWrapped(0.0f, "identity", utf8.c_str());

				utf8 = std_utf16_to_utf8((utf16str)pMum->description);
				TreeNodeTextWrapped(0.0f, "description", utf8.c_str());
			}

			ImGui::TreePop();
		} ImGui::NextColumn();
#endif

		ImGui::Columns(1);
		ImGui::Separator();
	}

	if (ImGui::CollapsingHeader("Character Data"))
	{
		ImGui::Columns(2, "Characters");
		ImGui::Separator();
		PrintCharacter("Player", player);
		PrintCharacter("Target", target);
		ImGui::Columns(1);
		ImGui::Separator();
	}
    
    if (ImGui::CollapsingHeader("Player Data"))
    {
        ImGui::Columns(2, "Players");
        ImGui::Separator();
        ImGui::SetColumnOffset(1, 30);
        ImGui::TextColored(white, "#"); ImGui::NextColumn();
        ImGui::TextColored(white, "ID:AGENT"); ImGui::NextColumn();
        ImGui::Separator();
        

        static int pi;
        pi = 0;
        currentContext()->closestPlayers.iter_lock([](struct CacheEntry<Player>* entry) {
            
            Character *c = &entry->value.data.c;
            StrWithCopyHandler("%d", ++pi); ImGui::NextColumn();
            std::string utf8 = toString("%d: ", c->agent_id);
            utf8 += std_utf16_to_utf8(c->decodedName ? (utf16str)c->decodedName : (utf16str)c->name);
            PrintCharacter(utf8.c_str(), c, false);
        });
        
        ImGui::Columns(1);
        ImGui::Separator();
    }

	if (ImGui::CollapsingHeader("Target Data"))
	{
		ImGui::Columns(3, "Targets");
		ImGui::Separator();
		ImGui::SetColumnOffset(1, 30);
		ImGui::TextColored(white, "#"); ImGui::NextColumn();
		ImGui::TextColored(white, "ID:AGENT"); ImGui::NextColumn();
		ImGui::TextColored(white, "TIME"); ImGui::NextColumn();
		ImGui::Separator();

        static int ti;
        ti = 0;
        currentContext()->targets.iter_lock([](struct CacheEntry<DPSTarget>* entry) {
            PrintTarget(++ti, &entry->value.data);
        });
		
		ImGui::Columns(1);
		ImGui::Separator();
	}
    
    if (ImGui::CollapsingHeader("Server Data"))
    {
        ImGui::Columns(2, "Server Players");
        ImGui::Separator();
        ImGui::SetColumnOffset(1, 30);
        ImGui::TextColored(white, "#"); ImGui::NextColumn();
        ImGui::TextColored(white, "ID:AGENT"); ImGui::NextColumn();
        ImGui::Separator();
        
        static int si;
        si = 0;
        PrintServerPlayer(++si, controlledPlayer());
        currentContext()->closestPlayers.iter_lock([](struct CacheEntry<Player>* entry) {
            Player *p = &entry->value.data;
            if (p->dps.is_connected)
                PrintServerPlayer(++si, p);
        });
        
        ImGui::Columns(1);
        ImGui::Separator();
    }

    ImGui::PopAllowKeyboardFocus();
	ImGui::PopStyleColor(1); // Text
	ImGui::End();
	ImGui::PopFont();
}
#endif // (defined _DEBUG) || (defined DEBUG)

/////////////////////////
// DEBUG FUNCTIONS END //
/////////////////////////
