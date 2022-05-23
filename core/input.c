//
//  input.c
//  bgdm
//
//  Created by Bhagwan on 7/14/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include "input.h"
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <GLFW/glfw3.h>

static keyNode* first = NULL;
static keyNode* last = NULL;

// Key-name mapping entry.
typedef struct KeyName
{
    char const* name;
    int key;
} keyName;

static keyName CONFIG_KEYS[] = {
    { "SPACE",		GLFW_KEY_SPACE },
    { "'",          GLFW_KEY_APOSTROPHE },
    { ",",          GLFW_KEY_COMMA },
    { "-",          GLFW_KEY_MINUS },
    { ".",          GLFW_KEY_PERIOD },
    { "/",          GLFW_KEY_SLASH },
    { "0",          GLFW_KEY_0 },
    { "1",          GLFW_KEY_1 },
    { "2",          GLFW_KEY_2 },
    { "3",          GLFW_KEY_3 },
    { "4",          GLFW_KEY_4 },
    { "5",          GLFW_KEY_5 },
    { "6",          GLFW_KEY_6 },
    { "7",          GLFW_KEY_7 },
    { "8",          GLFW_KEY_8 },
    { "9",          GLFW_KEY_9 },
    { ";",          GLFW_KEY_SEMICOLON },
    { "=",          GLFW_KEY_EQUAL },
    { "A",          GLFW_KEY_A },
    { "B",          GLFW_KEY_B },
    { "C",          GLFW_KEY_C },
    { "D",          GLFW_KEY_D },
    { "E",          GLFW_KEY_E },
    { "F",          GLFW_KEY_F },
    { "G",          GLFW_KEY_G },
    { "H",          GLFW_KEY_H },
    { "I",          GLFW_KEY_I },
    { "J",          GLFW_KEY_J },
    { "K",          GLFW_KEY_K },
    { "L",          GLFW_KEY_L },
    { "M",          GLFW_KEY_M },
    { "N",          GLFW_KEY_N },
    { "O",          GLFW_KEY_O },
    { "P",          GLFW_KEY_P },
    { "Q",          GLFW_KEY_Q },
    { "R",          GLFW_KEY_R },
    { "S",          GLFW_KEY_S },
    { "T",          GLFW_KEY_T },
    { "U",          GLFW_KEY_U },
    { "V",          GLFW_KEY_V },
    { "W",          GLFW_KEY_W },
    { "X",          GLFW_KEY_X },
    { "Y",          GLFW_KEY_Y },
    { "Z",          GLFW_KEY_Z },
    { "[",          GLFW_KEY_LEFT_BRACKET },
    { "\\",         GLFW_KEY_BACKSLASH },
    { "]",          GLFW_KEY_RIGHT_BRACKET },
    { "`",          GLFW_KEY_GRAVE_ACCENT },
    { "ESC",		GLFW_KEY_ESCAPE },
    { "ENTER",      GLFW_KEY_ENTER },
    { "TAB",        GLFW_KEY_TAB },
    { "BACKSPACE",	GLFW_KEY_BACKSPACE },
    { "INSERT",     GLFW_KEY_INSERT },
    { "DELETE",     GLFW_KEY_DELETE },
    { "RIGHT",		GLFW_KEY_RIGHT },
    { "LEFT",		GLFW_KEY_LEFT },
    { "DOWN",		GLFW_KEY_DOWN },
    { "UP",         GLFW_KEY_UP },
    { "PAGEUP",     GLFW_KEY_PAGE_UP },
    { "PAGEDOWN",	GLFW_KEY_PAGE_DOWN },
    { "HOME",		GLFW_KEY_HOME },
    { "END",		GLFW_KEY_END },
    { "CAPS",       GLFW_KEY_CAPS_LOCK },
    { "SCROLL",     GLFW_KEY_SCROLL_LOCK },
    { "NUM",        GLFW_KEY_NUM_LOCK },
    { "PRINT",		GLFW_KEY_PRINT_SCREEN },
    { "PAUSE",		GLFW_KEY_PAUSE },
    { "F1",         GLFW_KEY_F1 },
    { "F2",         GLFW_KEY_F2 },
    { "F3",         GLFW_KEY_F3 },
    { "F4",         GLFW_KEY_F4 },
    { "F5",         GLFW_KEY_F5 },
    { "F6",         GLFW_KEY_F6 },
    { "F7",         GLFW_KEY_F7 },
    { "F8",         GLFW_KEY_F8 },
    { "F9",         GLFW_KEY_F9 },
    { "F10",        GLFW_KEY_F10 },
    { "F11",        GLFW_KEY_F11 },
    { "F12",        GLFW_KEY_F12 },
    { "F13",        GLFW_KEY_F13 },
    { "F14",        GLFW_KEY_F14 },
    { "F15",        GLFW_KEY_F15 },
    { "NUM0",		GLFW_KEY_KP_0 },
    { "NUM1",		GLFW_KEY_KP_1 },
    { "NUM2",		GLFW_KEY_KP_2 },
    { "NUM3",		GLFW_KEY_KP_3 },
    { "NUM4",		GLFW_KEY_KP_4 },
    { "NUM5",		GLFW_KEY_KP_5 },
    { "NUM6",		GLFW_KEY_KP_6 },
    { "NUM7",		GLFW_KEY_KP_7 },
    { "NUM8",		GLFW_KEY_KP_8 },
    { "NUM9",		GLFW_KEY_KP_9 },
    { "KP.",        GLFW_KEY_KP_DECIMAL },
    { "KP/",        GLFW_KEY_KP_DIVIDE },
    { "KP*",        GLFW_KEY_KP_MULTIPLY },
    { "KP-",        GLFW_KEY_KP_SUBTRACT },
    { "KP+",        GLFW_KEY_KP_ADD },
    { "KPENT",      GLFW_KEY_KP_ENTER },
    { "KP=",        GLFW_KEY_KP_EQUAL },
    { "SHIFT",      GLFW_KEY_LEFT_SHIFT },
    { "CTRL",       GLFW_KEY_LEFT_CONTROL },
    { "ALT",        GLFW_KEY_LEFT_ALT },
    { "SHIFT",      GLFW_KEY_RIGHT_SHIFT },
    { "CTRL",       GLFW_KEY_RIGHT_CONTROL },
    { "ALT",        GLFW_KEY_RIGHT_ALT },
    { "SUPER",      GLFW_KEY_RIGHT_SUPER },
    { "MENU",       GLFW_KEY_MENU },
};

char const* keyBindToString(keyNode* Key, char* Str, int Count)
{
    if (!Key || !Str || !Count) return NULL;
    
    const char* KeyStr = NULL;
    
    for (int i = 0; i < (sizeof(CONFIG_KEYS)/sizeof(CONFIG_KEYS[0])); ++i)
    {
        if (CONFIG_KEYS[i].key == Key->key)
        {
            KeyStr = CONFIG_KEYS[i].name;
            break;
        }
    }
    
    memset(Str, 0, Count);
    snprintf(Str, Count, "%s%s%s%s",
             Key->alt ? "ALT + " : "",
             Key->ctrl ? "CTRL + " : "",
             Key->shift ? "SHIFT + " : "",
             KeyStr ? KeyStr : "");
    
    return Str;
}

keyNode* registerKeyStr(const char* KeyStr, keybCallback CbPress, keybCallback CbRelease)
{
    static char buff[1024];
    static char const delimiters[] = " + ";
    int Key = 0;
    bool Control = false;
    bool Alt = false;
    bool Shift = false;
    
    memset(buff, 0, sizeof(buff));
    strncpy(buff, KeyStr, sizeof(buff));
    
    char *pch = strtok (buff, delimiters);
    while (pch != NULL)
    {
        if (strcasecmp(pch, "ALT") == 0)
        {
            Alt = true;
        }
        else if (strcasecmp(pch, "CTRL") == 0)
        {
            Control = true;
        }
        else if (strcasecmp(pch, "SHIFT") == 0)
        {
            Shift = true;
        }
        else
        {
            for (int i = 0; i < (sizeof(CONFIG_KEYS)/sizeof(CONFIG_KEYS[0])); ++i)
            {
                if (strcasecmp(pch, CONFIG_KEYS[i].name) == 0)
                {
                    Key = CONFIG_KEYS[i].key;
                    break;
                }
            }
        }
        pch = strtok (NULL, delimiters);
    }
    
    return registerKey(Key, Control, Alt, Shift, CbPress, CbRelease);
}

keyNode* registerKey(int Key, bool Control, bool Alt, bool Shift, keybCallback CbPress, keybCallback CbRelease)
{
    keyNode *ret = NULL;
    if (first == NULL)
    {
        first = (keyNode*)malloc(sizeof(keyNode));
        ret = first;
        last = first;
        first->key = Key;
        first->ctrl = Control;
        first->alt = Alt;
        first->shift = Shift;
        first->cb_press = CbPress;
        first->cb_release = CbRelease;
        first->status = KEY_RELEASED;
        first->next = NULL;
        first->prev = NULL;
        
    }
    else
    {
        last->next = (keyNode*)malloc(sizeof(keyNode));
        ret = last->next;
        last->next->prev = last;
        last = last->next;
        last->key = Key;
        last->ctrl = Control;
        last->alt = Alt;
        last->shift = Shift;
        last->cb_press = CbPress;
        last->cb_release = CbRelease;
        last->status = KEY_RELEASED;
        last->next = NULL;
    }
    return ret;
}

void deregisterKey(int Key, bool Control, bool Alt, bool Shift)
{
    if (first == NULL)
        return;
    
    if (first->key == Key && first->ctrl == Control && first->alt == Alt && first->shift == Shift)
    {
        keyNode* del = first;
        
        first = first->next;
        
        if (last == del)
            last = NULL;
        
        free(del);
    }
    else if (last->key == Key && last->ctrl == Control && last->alt == Alt && last->shift == Shift)
    {
        keyNode* del = last;
        last = last->prev;
        free(del);
    }
    else
    {
        keyNode* item = first->next;
        
        while (item != NULL)
        {
            if (item->key == Key && item->ctrl == Control && item->alt == Alt && item->shift == Shift)
            {
                item->next->prev = item->prev;
                item->prev->next = item->next;
                free(item);
                return;
            }
            else
                item = item->next;
        }
    }
}

void updateKeyboard(GLFWwindow* window)
{
    keyNode* item = first;
    
    while (item != NULL)
    {
        keyState status = (keyState)glfwGetKey(window, item->key);
        if (!item->key) {
            item = item->next;
            continue;
        }
        
        if (status != item->status)
        {
            // Since we are using sticky keys the GLFW_PRESS will be cleared after the first glfwGetKey cal
            // so we must only check for modifiers if our key was pressed
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            bool alt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
            bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            
            if (status == KEY_RELEASED)
            {
                item->status = status;
                if (item->cb_release) item->cb_release(item->key, ctrl, alt, shift);
            }
            else if (item->ctrl == ctrl && item->alt == alt && item->shift == shift)
            {
                item->status = status;
                if (item->cb_press) item->cb_press(item->key, ctrl, alt, shift);
            }
        }
        
        item = item->next;
    }
}

void clearKeys()
{
    keyNode* item = first->next;
    
    while (item != NULL)
    {
        keyNode* temp = item;
        item = item->next;
        free(temp);
        
    }
    
    first = NULL;
    last = NULL;
}
