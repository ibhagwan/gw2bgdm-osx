//
//  imgui_osx.hpp
//  bgdm
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef imgui_osx_hpp
#define imgui_osx_hpp

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void clearScreen ();
void drawTriangle ();
    
void setCapKeyboard(bool set);
void setCapMouse(bool set);
void setCapInput(bool set);
void setDisableCap(bool set);

struct GLFWwindow;
    
bool shouldCapKeyboard(struct GLFWwindow*);
bool shouldCapMouse(struct GLFWwindow*);
bool shouldCapInput(struct GLFWwindow*);
bool shouldDisableCap(struct GLFWwindow*);
    
bool ImGui_Init_Win(struct GLFWwindow* window);
    
struct GLFWwindow *ImGui_Init(void *ctx, void *view);
void ImGui_Shutdown();

void ImGui_ResetInit();
bool ImGui_ResetPost();


void ImGui_NewFrame();
void ImGui_Render(int64_t now, bool clear, bool testing);
void ImGui_GetDisplaySize(int *outWidth, int* outHeight);
void ImGui_GetCursorPos(int *outX, int *outY);
    
void ImGui_Test(bool *show_test_window, bool *show_demo_window);
    
void ImGui_LoadImagesPlatform();
void* ImGui_GetProfImage(int idx, int elite);
void* ImGui_GetBuffColImage(int idx);
void* ImGui_GetMiscImage(int idx);
    
#ifdef __cplusplus
}   // extern "C"
#endif


#endif /* imgui_osx_hpp */
