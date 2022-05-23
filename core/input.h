//
//  input.h
//  bgdm
//
//  Created by Bhagwan on 7/14/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef input_h
#define input_h

#pragma once
#include <stdbool.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*keybCallback)(int Key, bool Control, bool Alt, bool Shift);

typedef enum _keyState
{
    KEY_RELEASED,
    KEY_PRESSED
    
} keyState;

typedef struct _keyNode
{
    int key;
    bool ctrl;
    bool alt;
    bool shift;
    keyState status;
    keybCallback cb_press;
    keybCallback cb_release;
    struct _keyNode* prev;
    struct _keyNode* next;
    
} keyNode, *KeyBind;

char const* keyBindToString(keyNode* Key, char* Str, int Count);
keyNode* registerKeyStr(const char* KeyStr, keybCallback CbPress, keybCallback CbRelease);
keyNode* registerKey(int Key, bool Control, bool Alt, bool Shift, keybCallback CbPress, keybCallback CbRelease);
void deregisterKey(int Key, bool Control, bool Alt, bool Shift);
void updateKeyboard(GLFWwindow* window);
void clearKeys();
    
    
#ifdef __cplusplus
}
#endif
#endif /* input_h */
