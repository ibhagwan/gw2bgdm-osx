//
//  imgui_osx.cpp
//  bgdm
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <sys/syslimits.h> // for PATH_MAX.
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h> // for GetCurrentWindow()->TitleBarRect9)
#include <glfw3_osx.h>
#include <GLFW/glfw3.h>
#include "core/TargetOS.h"
#include "core/time_.h"
#include "core/file.h"
#include "core/input.h"
#include "localization.h"
#include "imgui_osx.h"
#include "imgui_bgdm.h"
#include "entry_osx.h"
#include "bgdm_osx.h"
#include "meter/dps.h"
#include "meter/game.h"
#include "meter/config.h"
#include "meter/game_data.h"
#include "meter/imgui_bgdm.h"
#include "osx.common/Logging.h"
#include "offsets/offset_scan.h"
#include "imgui_extras/imgui_colorpicker.hpp"
#include "imgui_extras/imgui_memory_editor.hpp"

//#define BGDM_USE_OPENGL3
#ifdef BGDM_USE_OPENGL3
#include "imgui/examples/opengl3_example/imgui_impl_glfw_gl3.h"
#define IMGUI_INIT ImGui_ImplGlfwGL3_Init
#define IMGUI_SHUTDOWN ImGui_ImplGlfwGL3_Shutdown
#define IMGUI_INVALIDATE ImGui_ImplGlfwGL3_InvalidateDeviceObjects
#define IMGUI_CREATE ImGui_ImplGlfwGL3_CreateDeviceObjects
#define IMGUI_NEWFRAME ImGui_ImplGlfwGL3_NewFrame
#else
#include "imgui/examples/opengl2_example/imgui_impl_glfw.h"
#define IMGUI_INIT ImGui_ImplGlfw_Init
#define IMGUI_SHUTDOWN ImGui_ImplGlfw_Shutdown
#define IMGUI_INVALIDATE ImGui_ImplGlfw_InvalidateDeviceObjects
#define IMGUI_CREATE ImGui_ImplGlfw_CreateDeviceObjects
#define IMGUI_NEWFRAME ImGui_ImplGlfw_NewFrame
#endif

static const char* ProfessionImageNames[] {
    "Guardian.png",
    "Warrior.png",
    "Engineer.png",
    "Ranger.png",
    "Thief.png",
    "Elementalist.png",
    "Mesmer.png",
    "Necromancer.png",
    "Revenant.png",
    "Dragonhunter.png",
    "Berserker.png",
    "Scrapper.png",
    "Druid.png",
    "Daredevil.png",
    "Tempest.png",
    "Chronomancer.png",
    "Reaper.png",
    "Herald.png",
    "Firebrand.png",
    "Spellbreaker.png",
    "Holosmith.png",
    "Soulbeast.png",
    "Deadeye.png",
    "Weaver.png",
    "Mirage.png",
    "Scourge.png",
    "Renegade.png"
};

static const char* BuffImageNames[] {
    "Alacrity.png",
    "Aegis.png",
    "Fury.png",
    "Might.png",
    "Protection.png",
    "Quickness.png",
    "Regeneration.png",
    "Resistance.png",
    "Retaliation.png",
    "Stability.png",
    "Swiftness.png",
    "Vigor.png",
    "Seaweed.png",
};

static const char* BuffContrastImageNames[] {
    "Alacrity.png",
    "Aegis-c.png",
    "Fury-c.png",
    "Might-c.png",
    "Protection-c.png",
    "Quickness-c.png",
    "Regeneration-c.png",
    "Resistance-c.png",
    "Retaliation-c.png",
    "Stability-c.png",
    "Swiftness-c.png",
    "Vigor-c.png",
    "Seaweed-c.png"
};

static const char* BuffDarkImageNames[] {
    "Alacrity.png",
    "Aegis-d.png",
    "Fury-d.png",
    "Might-d.png",
    "Protection-d.png",
    "Quickness-d.png",
    "Regeneration-d.png",
    "Resistance-d.png",
    "Retaliation-d.png",
    "Stability-d.png",
    "Swiftness-d.png",
    "Vigor-d.png",
    "Seaweed-d.png"
};

static const char* BuffProfessionImageNames[] {
    "AssassinPresence.png",
    "BannerOfDefense.png",
    "BannerOfDiscipline.png",
    "BannerOfStrength.png",
    "BannerOfTactics.png",
    "EmpowerAllies.png",
    "FacetOfNature.png",
    "GlyphOfEmpowerment.png",
    "GraceOfTheLand.png",
    "SpiritFrost.png",
    "SpiritStone.png",
    "SpiritStorm.png",
    "SpiritSun.png",
    "Spotter.png",
    "VampiricAura.png",
    "RiteOfTheGreatDwarf.png",
    "SoothingMist.png",
    "StrengthInNumbers.png",
    "PinPointDistribution.png"
};
    
static const char* MiscImageNames[] {
    "Time.png",
    "Class.png",
    "Scholar.png",
    "SeaweedSalad.png",
    "Downed.png",
    "Downed-Enemy.png",
    "Group.png",
    "UpArrow.png",
    "DownArrow.png",
};

static void* PROF_IMAGES[ARRAYSIZE(ProfessionImageNames)] = { 0 };
static void* STD_BUFF_ICONS[ARRAYSIZE(BuffImageNames)] = { 0 };
static void* STD_BUFF_ICONS_C[ARRAYSIZE(BuffContrastImageNames)] = { 0 };
static void* STD_BUFF_ICONS_D[ARRAYSIZE(BuffDarkImageNames)] = { 0 };
static void* PROF_BUFF_ICONS[ARRAYSIZE(BuffProfessionImageNames)] = { 0 };
static void* MISC_ICONS[ARRAYSIZE(MiscImageNames)] = { 0 };


static int64_t g_now = 0;
static GLFWwindow*  g_window = NULL;
static bool disable_input = false;
static bool cap_keyboard = false;
static bool cap_mouse = false;
static bool cap_input = false;


void setCapKeyboard(bool set)
{
    cap_keyboard = set;
}

void setCapMouse(bool set)
{
    cap_mouse = set;
}

void setCapInput(bool set)
{
    cap_input = set;
}

void setDisableCap(bool set)
{
    disable_input = set;
}

bool shouldCapKeyboard(struct GLFWwindow*)
{
    return cap_keyboard;
}

bool shouldCapMouse(struct GLFWwindow*)
{
    return cap_mouse;
}

bool shouldCapInput(struct GLFWwindow*)
{
    return cap_input;
}

bool shouldDisableCap(struct GLFWwindow*)
{
    return false;
}

static void ImGui_LoadImages(const char** imageNames, int imageCount, void** outArr)
{
    for (int i=0; i<imageCount; ++i) {
        if (!textureFromPath(imageNames[i], (GLuint *)&outArr[i]) ) {
            CLogError("Error loading image '%s'", imageNames[i]);
        }
    }
}

void ImGui_LoadImagesPlatform()
{
    ImGui_LoadImages(ProfessionImageNames,      ARRAYSIZE(ProfessionImageNames),        PROF_IMAGES);
    ImGui_LoadImages(BuffImageNames,            ARRAYSIZE(BuffImageNames),              STD_BUFF_ICONS);
    ImGui_LoadImages(BuffContrastImageNames,    ARRAYSIZE(BuffContrastImageNames),      STD_BUFF_ICONS_C);
    ImGui_LoadImages(BuffDarkImageNames,        ARRAYSIZE(BuffDarkImageNames),          STD_BUFF_ICONS_D);
    ImGui_LoadImages(BuffProfessionImageNames,  ARRAYSIZE(BuffProfessionImageNames),    PROF_BUFF_ICONS);
    ImGui_LoadImages(MiscImageNames,            ARRAYSIZE(MiscImageNames),              MISC_ICONS);
}

void* ImGui_GetProfImage(int idx, int elite)
{
    --idx;
    if (elite>0) idx += GW2::PROFESSION_REVENANT * elite;
    if (idx < ARRAYSIZE(ProfessionImageNames))
        return PROF_IMAGES[idx];
    return NULL;
}

void* ImGui_GetBuffColImage(int idx)
{
    void **arr = STD_BUFF_ICONS;
    if (g_state.icon_pack_no == 1)
        arr = STD_BUFF_ICONS_C;
    else if (g_state.icon_pack_no == 2)
        arr = STD_BUFF_ICONS_D;
    
    switch (idx)
    {
        case(BUFF_COL_TIME):    return MISC_ICONS[0];
        case(BUFF_COL_CLS):     return MISC_ICONS[1];
        case(BUFF_COL_SCLR):    return MISC_ICONS[2];
        case(BUFF_COL_SEAW):
            { if (g_state.use_seaweed_icon) return MISC_ICONS[3]; else return arr[12]; }
        case(BUFF_COL_DOWN):
            { if (!g_state.use_downed_enemy_icon) return MISC_ICONS[4]; else return MISC_ICONS[5]; }
        case(BUFF_COL_SUB):     return MISC_ICONS[6];
            
        case(BUFF_COL_ALAC):    return arr[0];
        case(BUFF_COL_AEGIS):   return arr[1];
        case(BUFF_COL_FURY):    return arr[2];
        case(BUFF_COL_MIGHT):   return arr[3];
        case(BUFF_COL_PROT):    return arr[4];
        case(BUFF_COL_QUIK):    return arr[5];
        case(BUFF_COL_REGEN):   return arr[6];
        case(BUFF_COL_RESIST):  return arr[7];
        case(BUFF_COL_RETAL):   return arr[8];
        case(BUFF_COL_STAB):    return arr[9];
        case(BUFF_COL_SWIFT):   return arr[10];
        case(BUFF_COL_VIGOR):   return arr[11];
    
        case(BUFF_COL_REVAP):   return PROF_BUFF_ICONS[0];
        case(BUFF_COL_BAND):    return PROF_BUFF_ICONS[2];
        case(BUFF_COL_BANS):    return PROF_BUFF_ICONS[3];
        case(BUFF_COL_BANT):    return PROF_BUFF_ICONS[4];
        case(BUFF_COL_EA):      return PROF_BUFF_ICONS[5];
        case(BUFF_COL_REVNR):   return PROF_BUFF_ICONS[6];
        case(BUFF_COL_GLEM):    return PROF_BUFF_ICONS[7];
        case(BUFF_COL_GOTL):    return PROF_BUFF_ICONS[8];
        case(BUFF_COL_FROST):   return PROF_BUFF_ICONS[9];
        case(BUFF_COL_STONE):   return PROF_BUFF_ICONS[10];
        case(BUFF_COL_STORM):   return PROF_BUFF_ICONS[11];
        case(BUFF_COL_SUN):     return PROF_BUFF_ICONS[12];
        case(BUFF_COL_SPOTTER): return PROF_BUFF_ICONS[13];
        case(BUFF_COL_NECVA):   return PROF_BUFF_ICONS[14];
        case(BUFF_COL_REVRD):   return PROF_BUFF_ICONS[15];
        case(BUFF_COL_ELESM):   return PROF_BUFF_ICONS[16];
        case(BUFF_COL_GRDSN):   return PROF_BUFF_ICONS[17];
        case(BUFF_COL_ENGPP):   return PROF_BUFF_ICONS[18];
    }
    return NULL;
}

void* ImGui_GetMiscImage(int idx)
{
    switch (idx)
    {
        case(0):    return MISC_ICONS[7];
        case(1):    return MISC_ICONS[8];
    }
    return NULL;
}

static void ImGui_DrawImages(int imageCount, void** outArr)
{
    ImGui::NewLine();
    for (int i=0; i<imageCount; ++i) {
        if (outArr[i]) {
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)outArr[i], ImVec2(20, 20));
        }
    }
}

bool ImGui_Init_Win(struct GLFWwindow* window)
{
    if (g_window != NULL) return true;
    if (window) {
        LogDebug("<window=%p>", window);
        g_window = window;
        glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
        glfwSetInputCapCallbacks(window, shouldCapKeyboard, shouldCapMouse, shouldCapInput, shouldDisableCap);
        if ( !IMGUI_INIT((GLFWwindow *)window, true) )
            return false;
        if (!ImGui_InitResources()) {}
        return true;
    }
    return false;
}

struct GLFWwindow *ImGui_Init(void *ctx, void *view)
{
    if (g_window != NULL) return NULL;
    
    GLFWwindow* window = glfwCreateWindowFromCtx(ctx, view);
    if (!window) return NULL;
    
    LogDebug("<window=%p>", window);
    glfwMakeContextCurrent(window);
    
    if ( !IMGUI_INIT((GLFWwindow *)window, true) ) {
        return NULL;
    }
    g_window = window;
    //glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetInputCapCallbacks(window, shouldCapKeyboard, shouldCapMouse, shouldCapInput, shouldDisableCap);
    
    int display_w, display_h;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    LogDebug("ImGui initiated succesfully <width:%d> <height:%d>", display_w, display_h);
    
    if (!ImGui_InitResources()) {}
    
    return window;
}

void ImGui_Shutdown()
{
    __TRACE__
    if (g_window == NULL) return;
    IMGUI_SHUTDOWN();
}


void ImGui_ResetInit()
{
    __TRACE__
    if (g_window == NULL) return;
    IMGUI_INVALIDATE();
}

bool ImGui_ResetPost()
{
    __TRACE__
    if (g_window == NULL) return false;
    return IMGUI_CREATE();
}


void ImGui_NewFrame()
{
    if (g_window == NULL) return;
    IMGUI_NEWFRAME();
    g_now = time_get();
}

void ImGui_GetDisplaySize(int *outWidth, int* outHeight)
{
    glfwGetWindowSize(g_window, outWidth, outHeight);
}

void ImGui_GetCursorPos(int *outX, int *outY)
{
    double mouse_x, mouse_y;
    glfwGetCursorPos(g_window, &mouse_x, &mouse_y);
    *outX = mouse_x;
    *outY = mouse_y;
}

void drawTriangle ()
{
    glColor3f(1.0f, 0.85f, 0.35f);
    glBegin(GL_TRIANGLES);
    {
        glVertex3f( 0.0, 0.6, 0.0);
        glVertex3f( -0.2, -0.3, 0.0);
        glVertex3f( 0.2, -0.3 ,0.0);
    }
    glEnd();
}

void clearScreen ()
{
    static ImVec4 clear_color = ImColor(114, 144, 154);
    if (g_window == NULL) return;
    int display_w, display_h;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ImGui_Render(int64_t now, bool clear, bool testing)
{
    if (g_window == NULL) return;
    
    ImGuiIO& io = ImGui::GetIO();
    cap_keyboard = io.WantCaptureKeyboard;
    cap_mouse = io.WantCaptureMouse;
    cap_input = io.WantTextInput;
    
    // Poll for keybinds
    updateKeyboard(g_window);
    
    float ms = (time_get() - g_now) / 1000.0f;
    ImGui_Main(ms, g_now);
    
#if (defined _DEBUG) || (defined DEBUG)
    ImGui_Debug(&g_state.panel_debug, currentContext(), controlledCharacter(), currentTarget());
#endif
    
    if ((bgdm_init_state() == InitStateInitialized &&
        currentCam()->valid &&
        !isMapOpen() && !isInterfaceHidden()) || testing) {
        
        if (g_state.global_on_off) {
            
            ImGui_Compass(&g_state.panel_compass, 0, 0, 0);
            ImGui_BuffUptime(&g_state.panel_buff_uptime, 0, 0);
            
            ImGui_PersonalDps(&g_state.panel_dps_self, 0, 0);
            ImGui_SkillBreakdown(&g_state.panel_skills, 0, 0);
            
            ImGui_GroupDps(&g_state.panel_dps_group, 0, 0);
        
#if !(defined BGDM_TOS_COMPLIANT)
            ImGui_HpAndDistance(&g_state.panel_hp, controlledCharacter(), currentTarget());
            ImGui_FloatBars(&g_state.panel_float, controlledPlayer());
            ImGui_CharInspect(&g_state.panel_gear_self, controlledCharacter());
            ImGui_CharInspect(&g_state.panel_gear_target, currentTarget());
#endif
        }
    }
    
    // Rendering
    if ( clear ) clearScreen();
    
    //drawTriangle();
    //glfwPollEventsMainThread();
    
    ImGui::Render();
}

void ImGui_Test(bool *show_test_window, bool *show_demo_window)
{
    static bool show_another_window = false;
    static ImVec4 clear_color = ImColor(114, 144, 154);
    
    ImGuiWindowFlags flags = 0;
    if (disable_input) flags |= ImGuiWindowFlags_NoInputs;
    
    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    ImGui::Begin("ImGui Test", show_test_window, flags);
    {
        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
        if (ImGui::Button("Demo Window")) *show_demo_window ^= 1;
        if (ImGui::Button("Another Window")) show_another_window ^= 1;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        
        ImGui::Separator();
        ImGui::Text("WantCaptureKeyboard %d", cap_keyboard);
        ImGui::Text("WantCaptureMouse %d", cap_mouse);
        ImGui::Text("WantTextInput %d", cap_input);
        ImGui::Separator();
        
        ImGui::Text("IntSize %d", uiInterfaceSize());
        ImGui::Text("Account: %s", accountName());
        ImGui::Text("ActionCam %d", isActionCam());
        
        ImGui_DrawImages(ARRAYSIZE(ProfessionImageNames),        PROF_IMAGES);
        ImGui_DrawImages(ARRAYSIZE(BuffImageNames),              STD_BUFF_ICONS);
        ImGui_DrawImages(ARRAYSIZE(BuffContrastImageNames),      STD_BUFF_ICONS_C);
        ImGui_DrawImages(ARRAYSIZE(BuffDarkImageNames),          STD_BUFF_ICONS_D);
        ImGui_DrawImages(ARRAYSIZE(BuffProfessionImageNames),    PROF_BUFF_ICONS);
        ImGui_DrawImages(ARRAYSIZE(MiscImageNames),              MISC_ICONS);
    }
    ImGui::End();

    
    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window)
    {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window, flags);
        ImGui::Text("Hello");
        ImGui::End();
    }
    
    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
    if (*show_demo_window)
    {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
        ImGui::ShowTestWindow(show_demo_window);
    }
    
}
