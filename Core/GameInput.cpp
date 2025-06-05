//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#include "GameInput.h"
#include "GameCore.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

#define USE_KEYBOARD_MOUSE

// #ifdef _GAMING_DESKTOP
//
//// I can't find the GameInput.h header in the GDK for Desktop yet
// #include <Xinput.h>
// #pragma comment(lib, "xinput9_1_0.lib")
//
// #define USE_KEYBOARD_MOUSE
// #define DIRECTINPUT_VERSION 0x0800
// #include <dinput.h>
// #pragma comment(lib, "dinput8.lib")
// #pragma comment(lib, "dxguid.lib")
//
// #else
//
//// This is what we should use on *all* platforms, but see previous comment
// #include <GameInput.h>
//
//// This should be handled by GameInput.h, but we'll borrow values from XINPUT.
// #define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  (7849.0f / 32768.0f)
// #define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE (8689.0f / 32768.0f)
//
// #endif

namespace GameCore
{
extern GLFWwindow *g_Window;
}

namespace
{
bool s_Buttons[2][GameInput::kNumDigitalInputs];
float s_HoldDuration[GameInput::kNumDigitalInputs] = {0.0f};
float s_Analogs[GameInput::kNumAnalogInputs];
float s_AnalogsTC[GameInput::kNumAnalogInputs];

bool isMouseCaptured;

#ifdef USE_KEYBOARD_MOUSE

// IDirectInput8A* s_DI;
// IDirectInputDevice8A* s_Keyboard;
// IDirectInputDevice8A* s_Mouse;

// DIMOUSESTATE2 s_MouseState;
struct MouseState
{
    long lX;
    long lY;
    long lZ;
    char rgbButtons[8];
} s_MouseState;
double prevMouseX;
double prevMouseY;
double prevMouseZ;
unsigned char s_Keybuffer[GLFW_KEY_LAST];
unsigned int s_glfwKeyMapping[GameInput::kNumKeys]; // map DigitalInput enum to DX key codes

#endif

inline float FilterAnalogInput(int val, int deadZone)
{
    if (val < 0)
    {
        if (val > -deadZone)
            return 0.0f;
        else
            return (val + deadZone) / (32768.0f - deadZone);
    }
    else
    {
        if (val < deadZone)
            return 0.0f;
        else
            return (val - deadZone) / (32767.0f - deadZone);
    }
}

#ifdef USE_KEYBOARD_MOUSE
void KbmBuildKeyMapping()
{
    s_glfwKeyMapping[GameInput::kKey_escape] = GLFW_KEY_ESCAPE;
    s_glfwKeyMapping[GameInput::kKey_1] = GLFW_KEY_1;
    s_glfwKeyMapping[GameInput::kKey_2] = GLFW_KEY_2;
    s_glfwKeyMapping[GameInput::kKey_3] = GLFW_KEY_3;
    s_glfwKeyMapping[GameInput::kKey_4] = GLFW_KEY_4;
    s_glfwKeyMapping[GameInput::kKey_5] = GLFW_KEY_5;
    s_glfwKeyMapping[GameInput::kKey_6] = GLFW_KEY_6;
    s_glfwKeyMapping[GameInput::kKey_7] = GLFW_KEY_7;
    s_glfwKeyMapping[GameInput::kKey_8] = GLFW_KEY_8;
    s_glfwKeyMapping[GameInput::kKey_9] = GLFW_KEY_9;
    s_glfwKeyMapping[GameInput::kKey_0] = GLFW_KEY_0;
    s_glfwKeyMapping[GameInput::kKey_minus] = GLFW_KEY_MINUS;
    s_glfwKeyMapping[GameInput::kKey_equals] = GLFW_KEY_EQUAL;
    s_glfwKeyMapping[GameInput::kKey_back] = GLFW_KEY_BACKSPACE;
    s_glfwKeyMapping[GameInput::kKey_tab] = GLFW_KEY_TAB;
    s_glfwKeyMapping[GameInput::kKey_q] = GLFW_KEY_Q;
    s_glfwKeyMapping[GameInput::kKey_w] = GLFW_KEY_W;
    s_glfwKeyMapping[GameInput::kKey_e] = GLFW_KEY_E;
    s_glfwKeyMapping[GameInput::kKey_r] = GLFW_KEY_R;
    s_glfwKeyMapping[GameInput::kKey_t] = GLFW_KEY_T;
    s_glfwKeyMapping[GameInput::kKey_y] = GLFW_KEY_Y;
    s_glfwKeyMapping[GameInput::kKey_u] = GLFW_KEY_U;
    s_glfwKeyMapping[GameInput::kKey_i] = GLFW_KEY_I;
    s_glfwKeyMapping[GameInput::kKey_o] = GLFW_KEY_O;
    s_glfwKeyMapping[GameInput::kKey_p] = GLFW_KEY_P;
    s_glfwKeyMapping[GameInput::kKey_lbracket] = GLFW_KEY_LEFT_BRACKET;
    s_glfwKeyMapping[GameInput::kKey_rbracket] = GLFW_KEY_RIGHT_BRACKET;
    s_glfwKeyMapping[GameInput::kKey_return] = GLFW_KEY_ENTER;
    s_glfwKeyMapping[GameInput::kKey_lcontrol] = GLFW_KEY_LEFT_CONTROL;
    s_glfwKeyMapping[GameInput::kKey_a] = GLFW_KEY_A;
    s_glfwKeyMapping[GameInput::kKey_s] = GLFW_KEY_S;
    s_glfwKeyMapping[GameInput::kKey_d] = GLFW_KEY_D;
    s_glfwKeyMapping[GameInput::kKey_f] = GLFW_KEY_F;
    s_glfwKeyMapping[GameInput::kKey_g] = GLFW_KEY_G;
    s_glfwKeyMapping[GameInput::kKey_h] = GLFW_KEY_H;
    s_glfwKeyMapping[GameInput::kKey_j] = GLFW_KEY_J;
    s_glfwKeyMapping[GameInput::kKey_k] = GLFW_KEY_K;
    s_glfwKeyMapping[GameInput::kKey_l] = GLFW_KEY_L;
    s_glfwKeyMapping[GameInput::kKey_semicolon] = GLFW_KEY_SEMICOLON;
    s_glfwKeyMapping[GameInput::kKey_apostrophe] = GLFW_KEY_APOSTROPHE;
    s_glfwKeyMapping[GameInput::kKey_grave] = GLFW_KEY_GRAVE_ACCENT;
    s_glfwKeyMapping[GameInput::kKey_lshift] = GLFW_KEY_LEFT_SHIFT;
    s_glfwKeyMapping[GameInput::kKey_backslash] = GLFW_KEY_BACKSLASH;
    s_glfwKeyMapping[GameInput::kKey_z] = GLFW_KEY_Z;
    s_glfwKeyMapping[GameInput::kKey_x] = GLFW_KEY_X;
    s_glfwKeyMapping[GameInput::kKey_c] = GLFW_KEY_C;
    s_glfwKeyMapping[GameInput::kKey_v] = GLFW_KEY_V;
    s_glfwKeyMapping[GameInput::kKey_b] = GLFW_KEY_B;
    s_glfwKeyMapping[GameInput::kKey_n] = GLFW_KEY_N;
    s_glfwKeyMapping[GameInput::kKey_m] = GLFW_KEY_M;
    s_glfwKeyMapping[GameInput::kKey_comma] = GLFW_KEY_COMMA;
    s_glfwKeyMapping[GameInput::kKey_period] = GLFW_KEY_PERIOD;
    s_glfwKeyMapping[GameInput::kKey_slash] = GLFW_KEY_SLASH;
    s_glfwKeyMapping[GameInput::kKey_rshift] = GLFW_KEY_RIGHT_SHIFT;
    s_glfwKeyMapping[GameInput::kKey_multiply] = GLFW_KEY_KP_MULTIPLY;
    s_glfwKeyMapping[GameInput::kKey_lalt] = GLFW_KEY_LEFT_ALT;
    s_glfwKeyMapping[GameInput::kKey_space] = GLFW_KEY_SPACE;
    s_glfwKeyMapping[GameInput::kKey_capital] = GLFW_KEY_CAPS_LOCK;
    s_glfwKeyMapping[GameInput::kKey_f1] = GLFW_KEY_F1;
    s_glfwKeyMapping[GameInput::kKey_f2] = GLFW_KEY_F2;
    s_glfwKeyMapping[GameInput::kKey_f3] = GLFW_KEY_F3;
    s_glfwKeyMapping[GameInput::kKey_f4] = GLFW_KEY_F4;
    s_glfwKeyMapping[GameInput::kKey_f5] = GLFW_KEY_F5;
    s_glfwKeyMapping[GameInput::kKey_f6] = GLFW_KEY_F6;
    s_glfwKeyMapping[GameInput::kKey_f7] = GLFW_KEY_F7;
    s_glfwKeyMapping[GameInput::kKey_f8] = GLFW_KEY_F8;
    s_glfwKeyMapping[GameInput::kKey_f9] = GLFW_KEY_F9;
    s_glfwKeyMapping[GameInput::kKey_f10] = GLFW_KEY_F10;
    s_glfwKeyMapping[GameInput::kKey_numlock] = GLFW_KEY_NUM_LOCK;
    s_glfwKeyMapping[GameInput::kKey_scroll] = GLFW_KEY_SCROLL_LOCK;
    s_glfwKeyMapping[GameInput::kKey_numpad7] = GLFW_KEY_KP_7;
    s_glfwKeyMapping[GameInput::kKey_numpad8] = GLFW_KEY_KP_8;
    s_glfwKeyMapping[GameInput::kKey_numpad9] = GLFW_KEY_KP_9;
    s_glfwKeyMapping[GameInput::kKey_subtract] = GLFW_KEY_KP_SUBTRACT;
    s_glfwKeyMapping[GameInput::kKey_numpad4] = GLFW_KEY_KP_4;
    s_glfwKeyMapping[GameInput::kKey_numpad5] = GLFW_KEY_KP_5;
    s_glfwKeyMapping[GameInput::kKey_numpad6] = GLFW_KEY_KP_6;
    s_glfwKeyMapping[GameInput::kKey_add] = GLFW_KEY_KP_ADD;
    s_glfwKeyMapping[GameInput::kKey_numpad1] = GLFW_KEY_KP_1;
    s_glfwKeyMapping[GameInput::kKey_numpad2] = GLFW_KEY_KP_2;
    s_glfwKeyMapping[GameInput::kKey_numpad3] = GLFW_KEY_KP_3;
    s_glfwKeyMapping[GameInput::kKey_numpad0] = GLFW_KEY_KP_0;
    s_glfwKeyMapping[GameInput::kKey_decimal] = GLFW_KEY_KP_DECIMAL;
    s_glfwKeyMapping[GameInput::kKey_f11] = GLFW_KEY_F11;
    s_glfwKeyMapping[GameInput::kKey_f12] = GLFW_KEY_F12;
    s_glfwKeyMapping[GameInput::kKey_numpadenter] = GLFW_KEY_KP_ENTER;
    s_glfwKeyMapping[GameInput::kKey_rcontrol] = GLFW_KEY_RIGHT_CONTROL;
    s_glfwKeyMapping[GameInput::kKey_divide] = GLFW_KEY_KP_DIVIDE;
    s_glfwKeyMapping[GameInput::kKey_sysrq] = GLFW_KEY_PRINT_SCREEN;
    s_glfwKeyMapping[GameInput::kKey_ralt] = GLFW_KEY_RIGHT_ALT;
    s_glfwKeyMapping[GameInput::kKey_pause] = GLFW_KEY_PAUSE;
    s_glfwKeyMapping[GameInput::kKey_home] = GLFW_KEY_HOME;
    s_glfwKeyMapping[GameInput::kKey_up] = GLFW_KEY_UP;
    s_glfwKeyMapping[GameInput::kKey_pgup] = GLFW_KEY_PAGE_UP;
    s_glfwKeyMapping[GameInput::kKey_left] = GLFW_KEY_LEFT;
    s_glfwKeyMapping[GameInput::kKey_right] = GLFW_KEY_RIGHT;
    s_glfwKeyMapping[GameInput::kKey_end] = GLFW_KEY_END;
    s_glfwKeyMapping[GameInput::kKey_down] = GLFW_KEY_DOWN;
    s_glfwKeyMapping[GameInput::kKey_pgdn] = GLFW_KEY_PAGE_DOWN;
    s_glfwKeyMapping[GameInput::kKey_insert] = GLFW_KEY_INSERT;
    s_glfwKeyMapping[GameInput::kKey_delete] = GLFW_KEY_DELETE;
    s_glfwKeyMapping[GameInput::kKey_lwin] = GLFW_KEY_LEFT_SUPER;
    s_glfwKeyMapping[GameInput::kKey_rwin] = GLFW_KEY_RIGHT_SUPER;
    s_glfwKeyMapping[GameInput::kKey_apps] = GLFW_KEY_MENU;
}

void KbmZeroInputs()
{
    memset(&s_MouseState, 0, sizeof(MouseState));
    memset(s_Keybuffer, 0, sizeof(s_Keybuffer));
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_UNKNOWN)
    {
        return;
    }
    if ((key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) && action == GLFW_PRESS)
    {
        if (isMouseCaptured)
        {
            // escape the capture
            glfwSetInputMode(GameCore::g_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            isMouseCaptured = false;
        }
    }
    if (action == GLFW_PRESS)
    {
        s_Keybuffer[key] = 1;
    }
    else if (action == GLFW_RELEASE)
    {
        s_Keybuffer[key] = 0;
    }
}

void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    if (isMouseCaptured)
    {
        s_MouseState.lX = xpos - prevMouseX;
        s_MouseState.lY = ypos - prevMouseY;
        prevMouseX = xpos;
        prevMouseY = ypos;
    }
}

void MousebuttonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if (!isMouseCaptured)
        {
            glfwSetInputMode(GameCore::g_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            isMouseCaptured = true;
        }
    }
    if (isMouseCaptured)
    {
        if (action == GLFW_PRESS)
        {
            s_MouseState.rgbButtons[button] = 1;
        }
        else if (action == GLFW_RELEASE)
        {
            s_MouseState.rgbButtons[button] = 0;
        }
    }
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (isMouseCaptured)
    {
        s_MouseState.lZ = yoffset;
    }
}

void KbmInitialize()
{
    KbmBuildKeyMapping();
    glfwGetCursorPos(GameCore::g_Window, &prevMouseX, &prevMouseY);
    glfwSetKeyCallback(GameCore::g_Window, KeyCallback);
    glfwSetCursorPosCallback(GameCore::g_Window, CursorPosCallback);
    glfwSetMouseButtonCallback(GameCore::g_Window, MousebuttonCallback);
    glfwSetScrollCallback(GameCore::g_Window, ScrollCallback);

    glfwSetInputMode(GameCore::g_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    isMouseCaptured = true;

    KbmZeroInputs();
}

void KbmShutdown()
{
    glfwSetInputMode(GameCore::g_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    isMouseCaptured = false;

    glfwSetKeyCallback(GameCore::g_Window, nullptr);
    glfwSetCursorPosCallback(GameCore::g_Window, nullptr);
    glfwSetMouseButtonCallback(GameCore::g_Window, nullptr);
    glfwSetScrollCallback(GameCore::g_Window, nullptr);
}

void KbmUpdate()
{
    // HWND foreground = GetForegroundWindow();
    bool focused = glfwGetWindowAttrib(GameCore::g_Window, GLFW_FOCUSED);
    // bool visible = IsWindowVisible(foreground) != 0;
    bool visible = glfwGetWindowAttrib(GameCore::g_Window, GLFW_VISIBLE);

    if (!focused || !visible)
    {
        KbmZeroInputs();
    }
    else
    {
        // use glfw callbacks to update buffers
    }
}

#endif

} // namespace

void GameInput::Initialize()
{
    memset(&s_Buttons, 0, sizeof(s_Buttons));
    memset(&s_Analogs, 0, sizeof(s_Analogs));

#ifdef USE_KEYBOARD_MOUSE
    KbmInitialize();
#endif
}

void GameInput::Shutdown()
{
#ifdef USE_KEYBOARD_MOUSE
    KbmShutdown();
#endif
}

void GameInput::Update(float frameDelta)
{
    memcpy(s_Buttons[1], s_Buttons[0], sizeof(s_Buttons[0]));
    memset(s_Buttons[0], 0, sizeof(s_Buttons[0]));
    memset(s_Analogs, 0, sizeof(s_Analogs));

    //#define SET_BUTTON_VALUE(InputEnum, GameInputMask) \
//        s_Buttons[0][InputEnum] = !!(newInputState.Gamepad.wButtons & GameInputMask);
    //
    //    XINPUT_STATE newInputState;
    //    if (ERROR_SUCCESS == XInputGetState(0, &newInputState))
    //    {
    //        SET_BUTTON_VALUE(kDPadUp, XINPUT_GAMEPAD_DPAD_UP);
    //        SET_BUTTON_VALUE(kDPadDown, XINPUT_GAMEPAD_DPAD_DOWN);
    //        SET_BUTTON_VALUE(kDPadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
    //        SET_BUTTON_VALUE(kDPadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
    //        SET_BUTTON_VALUE(kStartButton, XINPUT_GAMEPAD_START);
    //        SET_BUTTON_VALUE(kBackButton, XINPUT_GAMEPAD_BACK);
    //        SET_BUTTON_VALUE(kLThumbClick, XINPUT_GAMEPAD_LEFT_THUMB);
    //        SET_BUTTON_VALUE(kRThumbClick, XINPUT_GAMEPAD_RIGHT_THUMB);
    //        SET_BUTTON_VALUE(kLShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
    //        SET_BUTTON_VALUE(kRShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);
    //        SET_BUTTON_VALUE(kAButton, XINPUT_GAMEPAD_A);
    //        SET_BUTTON_VALUE(kBButton, XINPUT_GAMEPAD_B);
    //        SET_BUTTON_VALUE(kXButton, XINPUT_GAMEPAD_X);
    //        SET_BUTTON_VALUE(kYButton, XINPUT_GAMEPAD_Y);
    //
    //        s_Analogs[kAnalogLeftTrigger]   = newInputState.Gamepad.bLeftTrigger / 255.0f;
    //        s_Analogs[kAnalogRightTrigger]  = newInputState.Gamepad.bRightTrigger / 255.0f;
    //        s_Analogs[kAnalogLeftStickX]    = FilterAnalogInput(newInputState.Gamepad.sThumbLX,
    //        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ); s_Analogs[kAnalogLeftStickY]    =
    //        FilterAnalogInput(newInputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
    //        s_Analogs[kAnalogRightStickX]   = FilterAnalogInput(newInputState.Gamepad.sThumbRX,
    //        XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ); s_Analogs[kAnalogRightStickY]   =
    //        FilterAnalogInput(newInputState.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
    //    }

#ifdef USE_KEYBOARD_MOUSE
    KbmUpdate();

    for (uint32_t i = 0; i < kNumKeys; ++i)
    {
        if (s_Keybuffer[s_glfwKeyMapping[i]] > 0)
            s_Buttons[0][i] = true;
    }

    for (uint32_t i = 0; i < 8; ++i)
    {
        if (s_MouseState.rgbButtons[i] > 0)
            s_Buttons[0][kMouse0 + i] = true;
    }

    s_Analogs[kAnalogMouseX] = (float)s_MouseState.lX * .0018f;
    s_Analogs[kAnalogMouseY] = (float)s_MouseState.lY * -.0018f;
    s_MouseState.lX = 0;
    s_MouseState.lY = 0;

    if (s_MouseState.lZ > 0)
        s_Analogs[kAnalogMouseScroll] = 1.0f;
    else if (s_MouseState.lZ < 0)
        s_Analogs[kAnalogMouseScroll] = -1.0f;
    s_MouseState.lZ = 0;
#endif

    // Update time duration for buttons pressed
    for (uint32_t i = 0; i < kNumDigitalInputs; ++i)
    {
        if (s_Buttons[0][i])
        {
            if (!s_Buttons[1][i])
                s_HoldDuration[i] = 0.0f;
            else
                s_HoldDuration[i] += frameDelta;
        }
    }

    for (uint32_t i = 0; i < kNumAnalogInputs; ++i)
    {
        s_AnalogsTC[i] = s_Analogs[i] * frameDelta;
    }
}

bool GameInput::IsAnyPressed(void) { return s_Buttons[0] != 0; }

bool GameInput::IsPressed(DigitalInput di) { return s_Buttons[0][di]; }

bool GameInput::IsFirstPressed(DigitalInput di) { return s_Buttons[0][di] && !s_Buttons[1][di]; }

bool GameInput::IsReleased(DigitalInput di) { return !s_Buttons[0][di]; }

bool GameInput::IsFirstReleased(DigitalInput di) { return !s_Buttons[0][di] && s_Buttons[1][di]; }

float GameInput::GetDurationPressed(DigitalInput di) { return s_HoldDuration[di]; }

float GameInput::GetAnalogInput(AnalogInput ai) { return s_Analogs[ai]; }

float GameInput::GetTimeCorrectedAnalogInput(AnalogInput ai) { return s_AnalogsTC[ai]; }
