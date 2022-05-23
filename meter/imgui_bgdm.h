#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GFX;
struct GFXTarget;
struct Array;
struct Player;
struct Character;
struct Panel;
struct State;
struct DPSData;
struct DPSTargetEx;
struct __GameContext;
    
#if defined(_WIN32) || defined (_WIN64)
extern bool imgui_has_colorblind_mode;
extern int imgui_colorblind_mode;
    
bool ImGui_Init(void* hwnd, void* device, void *viewport, const GamePtrs* pptrs);
void ImGui_Shutdown();
    
void ImGui_ImplDX9_ResetInit();
bool ImGui_ImplDX9_ResetPost();
bool ImGui_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
void ImGui_NewFrame();
#endif

bool ImGui_InitResources();
void ImGui_Main(float ms, int64_t now);
    
void ImGui_FloatBarConfigLoad();
void ImGui_FloatBarConfigSave();

void ImGui_StyleSetDefaults();
void ImGui_StyleConfigLoad();
void ImGui_StyleConfigSave();

void ImGui_FloatBars(struct Panel* panel, struct Player *player);

void ImGui_HpAndDistance(struct Panel* panel,
                         struct Character *player,
                         struct Character *target);

void ImGui_Compass(struct Panel* panel, int x, int y, int w);

void ImGui_RemoveGraphData(int32_t id);
void ImGui_ResetGraphData(int32_t exclude_id);
    
void ImGui_PersonalDps(struct Panel *panel, int x, int y);
    
void ImGui_SkillBreakdown(struct Panel *panel, int x, int y);

void ImGui_GroupDps(struct Panel *panel, int x, int y);

void ImGui_BuffUptime(struct Panel *panel, int x, int y);

void ImGui_CharInspect(struct Panel* panel, struct Character *c);

void ImGui_Debug(struct Panel* panel,
                 struct __GameContext *chCliCtx,
                 struct Character *player,
                 struct Character *target);
    
    
void ImGui_ToggleDebug();
void ImGui_ToggleInput();
void ImGui_ToggleMinPanels();

#ifdef __cplusplus
}
#endif
