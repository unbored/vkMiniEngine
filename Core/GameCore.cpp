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

//#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "GameInput.h"
#include "BufferManager.h"
#include "CommandContext.h"
//#include "PostEffects.h"
#include "Display.h"
#include "EngineTuning.h"
#include "Util/CommandLineArg.h"
//#include <shellapi.h>
#include "Utility.h"

//#pragma comment(lib, "runtimeobject.lib") 

namespace GameCore
{
using namespace Graphics;

bool gIsSupending = false;

void InitializeApplication(IGameApp& game, int argc, char** argv)
{
    CommandLineArgs::Initialize(argc, argv);

    Graphics::Initialize(game.RequiresRaytracingSupport());
    SystemTime::Initialize();
    GameInput::Initialize();
    EngineTuning::Initialize();

    game.Startup();
}

void TerminateApplication(IGameApp& game)
{
    g_CommandManager.IdleGPU();

    game.Cleanup();

    GameInput::Shutdown();
}

bool UpdateApplication(IGameApp& game)
{
    //EngineProfiling::Update();

    float DeltaTime = Graphics::GetFrameTime();

    GameInput::Update(DeltaTime);
    EngineTuning::Update(DeltaTime);

    game.Update(DeltaTime);
    game.RenderScene();

    //PostEffects::Render();

    GraphicsContext& UiContext = GraphicsContext::Begin("Render UI");
    UiContext.ClearColor(g_OverlayBuffer);
    UiContext.TransitionImageLayout(g_OverlayBuffer, vk::ImageLayout::eColorAttachmentOptimal);
    UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
    game.RenderUI(UiContext);

    //UiContext.SetRenderTarget(g_OverlayBuffer.GetRTV());
    UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
    EngineTuning::Display(UiContext, 10.0f, 40.0f, 1900.0f, 1040.0f);

    UiContext.Finish();

    Display::Present();

    return !game.IsDone();
}

// Default implementation to be overridden by the application
bool IGameApp::IsDone(void)
{
    return GameInput::IsFirstPressed(GameInput::kKey_escape);
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    g_DisplayWidth = width;
    g_DisplayHeight = height;
    Graphics::g_WindowResized = true;
}

GLFWwindow* g_Window = nullptr;

int RunApplication(IGameApp& app, const char* className, int argc, char** argv)
{
    // init glfw
    if (GLFW_TRUE != glfwInit())
    {
        printf("GLFW init failed!\n");
        return -1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create window
    g_Window = glfwCreateWindow(g_DisplayWidth, g_DisplayHeight, className, nullptr, nullptr);

    ASSERT(g_Window != nullptr);

    glfwSetWindowSize(g_Window, g_DisplayWidth, g_DisplayHeight);

    int f_w, f_h;
    glfwGetFramebufferSize(g_Window, &f_w, &f_h);
    g_DisplayWidth = f_w;
    g_DisplayHeight = f_h;

    glfwSetFramebufferSizeCallback(g_Window, framebufferResizeCallback);

    InitializeApplication(app, argc, argv);

    do
    {
        glfwPollEvents();
    } while (UpdateApplication(app) && !glfwWindowShouldClose(g_Window)); // Returns false to quit loop

    TerminateApplication(app);
    Graphics::Shutdown();

    glfwDestroyWindow(g_Window);

    glfwTerminate();

    return 0;
}
}
