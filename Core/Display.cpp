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
#include <cmath>
#include <limits>
#include <signal.h>

#include "BufferManager.h"
#include "ColorBuffer.h"
#include "CommandContext.h"
#include "DepthBuffer.h"
#include "DescriptorSet.h"
#include "Display.h"
#include "EngineTuning.h"
#include "GpuBuffer.h"
#include "GraphicsCore.h"
#include "RenderPass.h"
#include "SystemTime.h"
#include "Utility.h"
// #include "RootSignature.h"
#include "ImageScaling.h"
// #include "TemporalEffects.h"

// #pragma comment(lib, "dxgi.lib")

// This macro determines whether to detect if there is an HDR display and enable HDR10 output.
// Currently, with HDR display enabled, the pixel magnfication functionality is broken.
#define CONDITIONALLY_ENABLE_HDR_OUTPUT 0

namespace GameCore
{
extern GLFWwindow *g_Window;
}

#include "CompiledShaders/BufferCopyFrag.h"
#include "CompiledShaders/PresentSDRFrag.h"
#include "CompiledShaders/ScreenQuadPresentVert.h"
// #include "CompiledShaders/PresentHDRPS.h"
#include "CompiledShaders/CompositeSDRFrag.h"
#include "CompiledShaders/ScaleAndCompositeSDRFrag.h"
// #include "CompiledShaders/CompositeHDRPS.h"
// #include "CompiledShaders/BlendUIHDRPS.h"
// #include "CompiledShaders/ScaleAndCompositeHDRPS.h"
#include "CompiledShaders/MagnifyPixelsFrag.h"
// #include "CompiledShaders/GenerateMipsLinearCS.h"
// #include "CompiledShaders/GenerateMipsLinearOddCS.h"
// #include "CompiledShaders/GenerateMipsLinearOddXCS.h"
// #include "CompiledShaders/GenerateMipsLinearOddYCS.h"
// #include "CompiledShaders/GenerateMipsGammaCS.h"
// #include "CompiledShaders/GenerateMipsGammaOddCS.h"
// #include "CompiledShaders/GenerateMipsGammaOddXCS.h"
// #include "CompiledShaders/GenerateMipsGammaOddYCS.h"

#define SWAP_CHAIN_BUFFER_COUNT 3

vk::Format SwapChainFormat = vk::Format::eA2B10G10R10UnormPack32;

// using namespace Math;
using namespace ImageScaling;
using namespace Graphics;

namespace
{
float s_FrameTime = 0.0f;
uint64_t s_FrameIndex = 0;
int64_t s_FrameStartTick = 0;

BoolVar s_EnableVSync("Timing/VSync", true);
BoolVar s_LimitTo30Hz("Timing/Limit To 30Hz", false);
BoolVar s_DropRandomFrames("Timing/Drop Random Frames", false);
} // namespace

namespace Graphics
{
void PreparePresentSDR();
// void PreparePresentHDR();
void CompositeOverlays(GraphicsContext &Context);

enum eResolution
{
    k720p,
    k900p,
    k1080p,
    k1440p,
    k1800p,
    k2160p
};
enum eEQAAQuality
{
    kEQAA1x1,
    kEQAA1x8,
    kEQAA1x16
};

const uint32_t kNumPredefinedResolutions = 6;

const char *ResolutionLabels[] = {"1280x720", "1600x900", "1920x1080", "2560x1440", "3200x1800", "3840x2160"};
EnumVar NativeResolution("Graphics/Display/Native Resolution", k2160p, kNumPredefinedResolutions, ResolutionLabels);
#ifdef _GAMING_DESKTOP
// This can set the window size to common dimensions.  It's also possible for the window to take on other
// dimensions through resizing or going full-screen.
EnumVar DisplayResolution("Graphics/Display/Display Resolution", k720p, kNumPredefinedResolutions, ResolutionLabels);
#endif

bool g_bEnableHDROutput = false;
NumVar g_HDRPaperWhite("Graphics/Display/Paper White (nits)", 200.0f, 100.0f, 500.0f, 50.0f);
NumVar g_MaxDisplayLuminance("Graphics/Display/Peak Brightness (nits)", 1000.0f, 500.0f, 10000.0f, 100.0f);
const char *HDRModeLabels[] = {"HDR", "SDR", "Side-by-Side"};
EnumVar HDRDebugMode("Graphics/Display/HDR Debug Mode", 0, 3, HDRModeLabels);

uint32_t g_NativeWidth = 0;
uint32_t g_NativeHeight = 0;
uint32_t g_DisplayWidth = 1280;
uint32_t g_DisplayHeight = 720;
ColorBuffer g_PreDisplayBuffer;

void ResolutionToUINT(eResolution res, uint32_t &width, uint32_t &height)
{
    switch (res)
    {
    default:
    case k720p:
        width = 1280;
        height = 720;
        break;
    case k900p:
        width = 1600;
        height = 900;
        break;
    case k1080p:
        width = 1920;
        height = 1080;
        break;
    case k1440p:
        width = 2560;
        height = 1440;
        break;
    case k1800p:
        width = 3200;
        height = 1800;
        break;
    case k2160p:
        width = 3840;
        height = 2160;
        break;
    }
}

void SetNativeResolution(void)
{
    uint32_t NativeWidth, NativeHeight;

    ResolutionToUINT(eResolution((int)NativeResolution), NativeWidth, NativeHeight);

    if (g_NativeWidth == NativeWidth && g_NativeHeight == NativeHeight)
        return;
    DEBUGPRINT("Changing native resolution to %ux%u", NativeWidth, NativeHeight);

    g_NativeWidth = NativeWidth;
    g_NativeHeight = NativeHeight;

    g_CommandManager.IdleGPU();

    InitializeRenderingBuffers(NativeWidth, NativeHeight);
}

void SetDisplayResolution(void)
{
#ifdef _GAMING_DESKTOP
    static int SelectedDisplayRes = DisplayResolution;
    if (SelectedDisplayRes == DisplayResolution)
        return;

    SelectedDisplayRes = DisplayResolution;
    ResolutionToUINT((eResolution)SelectedDisplayRes, g_DisplayWidth, g_DisplayHeight);
    DEBUGPRINT("Changing display resolution to %ux%u", g_DisplayWidth, g_DisplayHeight);

    g_CommandManager.IdleGPU();

    glfwSetWindowSize(GameCore::g_Window, g_DisplayWidth, g_DisplayHeight);
    int f_w, f_h;
    glfwGetFramebufferSize(GameCore::g_Window, &f_w, &f_h);
    g_DisplayWidth = f_w;
    g_DisplayHeight = f_h;

    Display::Resize(g_DisplayWidth, g_DisplayHeight);

    //    SetWindowPos(GameCore::g_hWnd, 0, 0, 0, g_DisplayWidth, g_DisplayHeight, SWP_NOMOVE | SWP_NOZORDER |
    //    SWP_NOACTIVATE);
#endif
}

Swapchain s_Swapchain;
ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
// Framebuffer g_DisplayFb[SWAP_CHAIN_BUFFER_COUNT];

// vk::Semaphore g_imageSemaphores[SWAP_CHAIN_BUFFER_COUNT];
// vk::Semaphore g_presentSemaphores[SWAP_CHAIN_BUFFER_COUNT];
vk::Fence g_imageFences[SWAP_CHAIN_BUFFER_COUNT];

unsigned int g_CurrentBuffer = 0;
// unsigned int g_PresentIndex = 0;
bool g_WindowResized = false;

DescriptorSet s_PresentDS;

// RenderPass::RenderPassDesc s_BlendUIRP;
// RenderPass::RenderPassDesc s_PresentRP;

GraphicsPSO s_BlendUIPSO("Core: BlendUI");
GraphicsPSO PresentSDRPS("Core: PresentSDR");
// GraphicsPSO s_BlendUIHDRPSO(L"Core: BlendUIHDR");
// GraphicsPSO PresentHDRPS(L"Core: PresentHDR");
GraphicsPSO CompositeSDRPS("Core: CompositeSDR");
GraphicsPSO ScaleAndCompositeSDRPS("Core: ScaleAndCompositeSDR");
// GraphicsPSO CompositeHDRPS(L"Core: CompositeHDR");
// GraphicsPSO ScaleAndCompositeHDRPS(L"Core: ScaleAndCompositeHDR");
GraphicsPSO MagnifyPixelsPS("Core: MagnifyPixels");

const char *FilterLabels[] = {"Bilinear", "Sharpening", "Bicubic", "Lanczos"};
EnumVar UpsampleFilter("Graphics/Display/Scaling Filter", kSharpening, kFilterCount, FilterLabels);

enum DebugZoomLevel
{
    kDebugZoomOff,
    kDebugZoom2x,
    kDebugZoom4x,
    kDebugZoom8x,
    kDebugZoom16x,
    kDebugZoomCount
};
const char *DebugZoomLabels[] = {"Off", "2x Zoom", "4x Zoom", "8x Zoom", "16x Zoom"};
EnumVar DebugZoom("Graphics/Display/Magnify Pixels", kDebugZoomOff, kDebugZoomCount, DebugZoomLabels);
} // namespace Graphics

void Display::Resize(uint32_t width, uint32_t height)
{
    g_CommandManager.IdleGPU();

    g_DisplayWidth = width;
    g_DisplayHeight = height;

    DEBUGPRINT("Changing display resolution to %ux%u", width, height);

    // it should be the same format as DisplayPlane
    g_PreDisplayBuffer.Create("PreDisplay Buffer", width, height, 1, SwapChainFormat);

    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        g_DisplayPlane[i].Destroy();
        //        g_DisplayFb[i].Destroy();
    }

    s_Swapchain.Resize(g_DisplayWidth, g_DisplayHeight);

    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        g_DisplayPlane[i].CreateFromSwapchain("Primary SwapChain Buffer", s_Swapchain, i);
    }

    //    g_CurrentBuffer = 0;

    g_CommandManager.IdleGPU();

    g_WindowResized = false;

    ResizeDisplayDependentBuffers(g_NativeWidth, g_NativeHeight);
}

// Initialize the DirectX resources required to run.
void Display::Initialize(void)
{
    s_Swapchain.Create(g_DisplayWidth, g_DisplayHeight, SwapChainFormat, SWAP_CHAIN_BUFFER_COUNT,
                       CONDITIONALLY_ENABLE_HDR_OUTPUT);
    SwapChainFormat = s_Swapchain.GetFormat();

    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        g_DisplayPlane[i].CreateFromSwapchain("Primary SwapChain Buffer", s_Swapchain, i);

        vk::FenceCreateInfo fenceInfo;
        g_imageFences[i] = g_Device.createFence(fenceInfo);
    }

    s_PresentDS.AddBindings(0, 2, vk::DescriptorType::eCombinedImageSampler, 1,
                            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute);
    s_PresentDS.AddBindings(2, 1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute);
    s_PresentDS.AddPushConstant(0, sizeof(uint32_t) * 6, vk::ShaderStageFlagBits::eAll);
    s_PresentDS.Finalize();

    //    s_PresentRP.SetColorAttachment(s_Swapchain.GetFormat(), RenderPass::DontCare);
    //    s_PresentRP.Finalize();
    // s_PresentRP.colors = {{s_Swapchain.GetFormat(), vk::AttachmentLoadOp::eDontCare}};
    // s_BlendUIRP.colors = {{s_Swapchain.GetFormat(), vk::AttachmentLoadOp::eLoad}};
    //
    //    s_BlendUIRP.SetColorAttachment(s_Swapchain.GetFormat(), RenderPass::Preserve);
    //    s_BlendUIRP.Finalize();

    PresentSDRPS.SetRasterizerState(RasterizerTwoSided);
    PresentSDRPS.SetBlendState(BlendDisable);
    PresentSDRPS.SetDepthStencilState(DepthStateDisabled);
    PresentSDRPS.SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    PresentSDRPS.SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleList);
    PresentSDRPS.SetRenderPassFormat(s_Swapchain.GetFormat());
    PresentSDRPS.SetVertexShader(g_ScreenQuadPresentVert, sizeof(g_ScreenQuadPresentVert));
    PresentSDRPS.SetFragmentShader(g_PresentSDRFrag, sizeof(g_PresentSDRFrag));
    PresentSDRPS.Finalize();

    s_BlendUIPSO = PresentSDRPS;
    // s_BlendUIPSO.SetRenderPassDesc(s_BlendUIRP);
    s_BlendUIPSO.SetBlendState(BlendPreMultiplied);
    s_BlendUIPSO.SetFragmentShader(g_BufferCopyFrag, sizeof(g_BufferCopyFrag));
    s_BlendUIPSO.Finalize();

#define CreatePSO(ObjName, ShaderByteCode)                                                                             \
    ObjName = PresentSDRPS;                                                                                            \
    ObjName.SetFragmentShader(ShaderByteCode, sizeof(ShaderByteCode));                                                 \
    ObjName.Finalize();

    CreatePSO(CompositeSDRPS, g_CompositeSDRFrag);
    CreatePSO(ScaleAndCompositeSDRPS, g_ScaleAndCompositeSDRFrag);
    CreatePSO(MagnifyPixelsPS, g_MagnifyPixelsFrag);

#undef CreatePSO

    SetNativeResolution();

    g_PreDisplayBuffer.Create("PreDisplay Buffer", g_DisplayWidth, g_DisplayHeight, 1, SwapChainFormat);
    ImageScaling::Initialize(g_PreDisplayBuffer.GetFormat());
}

void Display::Shutdown(void)
{
    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        g_DisplayPlane[i].Destroy();
        g_Device.destroyFence(g_imageFences[i]);
    }

    //    PresentSDRPS.Destroy();
    //    s_BlendUIPSO.Destroy();
    //    CompositeSDRPS.Destroy();
    //    ScaleAndCompositeSDRPS.Destroy();

    //    s_PresentRP.Destroy();
    //    s_BlendUIRP.Destroy();

    s_PresentDS.Destroy();

    s_Swapchain.Destroy();

    g_PreDisplayBuffer.Destroy();
}

void Graphics::CompositeOverlays(GraphicsContext &Context)
{
    Context.GetGraphicsContext();
    // s_PresentDS.UpdateImageSampler(g_CurrentBuffer, 0, g_OverlayBuffer, SamplerLinearWrap);
    Context.SetDescriptorSet(s_PresentDS);
    // Context.BeginUpdateDescriptorSet();
    Context.UpdateImageSampler(0, g_OverlayBuffer, SamplerLinearClamp);
    // Context.EndUpdateAndBindDescriptorSet();

    Context.TransitionImageLayout(g_OverlayBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    Context.TransitionImageLayout(g_DisplayPlane[g_CurrentBuffer], vk::ImageLayout::eColorAttachmentOptimal);

    Context.BeginRenderPass(g_DisplayPlane[g_CurrentBuffer]);
    Context.BindPipeline(s_BlendUIPSO);
    Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
    Context.Draw(3, 0);
    Context.EndRenderPass();
}

void Graphics::PreparePresentSDR(void)
{
    GraphicsContext &Context = GraphicsContext::Begin("Present");

    bool NeedsScaling = g_NativeWidth != g_DisplayWidth || g_NativeHeight != g_DisplayHeight;

    // prefer scaling and compositing in one step via fragment shader
    if (DebugZoom == kDebugZoomOff && (UpsampleFilter == kSharpening || !NeedsScaling))
    {
        Context.SetDescriptorSet(s_PresentDS);
        // Context.BeginUpdateDescriptorSet();
        Context.UpdateImageSampler(0, g_SceneColorBuffer, SamplerLinearClamp);
        Context.UpdateImageSampler(1, g_OverlayBuffer, SamplerNearestClamp);
        glm::vec2 buffer{0.7071f / g_NativeWidth, 0.7071f / g_NativeHeight};
        Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, buffer);
        // Context.EndUpdateAndBindDescriptorSet();

        Context.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(g_OverlayBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(g_DisplayPlane[g_CurrentBuffer], vk::ImageLayout::eColorAttachmentOptimal);

        Context.BeginRenderPass(g_DisplayPlane[g_CurrentBuffer]);
        Context.BindPipeline(NeedsScaling ? ScaleAndCompositeSDRPS : CompositeSDRPS);
        Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
        Context.Draw(3);
        Context.EndRenderPass();
    }
    else
    {
        ColorBuffer &Dest = DebugZoom == kDebugZoomOff ? g_DisplayPlane[g_CurrentBuffer] : g_PreDisplayBuffer;

        // Scale or Copy
        if (NeedsScaling)
        {
            ImageScaling::Upscale(Context, Dest, g_SceneColorBuffer, eScalingFilter((int)UpsampleFilter));
        }
        else
        {
            Context.SetDescriptorSet(s_PresentDS);
            // Context.BeginUpdateDescriptorSet();
            Context.UpdateImageSampler(0, g_SceneColorBuffer, SamplerLinearClamp);
            // Context.EndUpdateAndBindDescriptorSet();

            Context.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            Context.TransitionImageLayout(Dest, vk::ImageLayout::eColorAttachmentOptimal);

            Context.BeginRenderPass(Dest);
            Context.BindPipeline(PresentSDRPS);
            Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
            Context.Draw(3);
            Context.EndRenderPass();
        }

        // Magnify without stretching
        if (DebugZoom != kDebugZoomOff)
        {
            Context.SetDescriptorSet(s_PresentDS);
            // Context.BeginUpdateDescriptorSet();
            Context.UpdateImageSampler(0, g_PreDisplayBuffer, SamplerLinearClamp);
            Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, 1.0f / ((int)DebugZoom + 1.0f));
            // Context.EndUpdateAndBindDescriptorSet();

            Context.TransitionImageLayout(g_PreDisplayBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            Context.TransitionImageLayout(g_DisplayPlane[g_CurrentBuffer], vk::ImageLayout::eColorAttachmentOptimal);

            Context.BeginRenderPass(g_DisplayPlane[g_CurrentBuffer]);
            Context.BindPipeline(MagnifyPixelsPS);
            Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
            Context.Draw(3);
            Context.EndRenderPass();
        }

        CompositeOverlays(Context);
    }

    Context.TransitionImageLayout(g_DisplayPlane[g_CurrentBuffer], vk::ImageLayout::ePresentSrcKHR);

    Context.Finish();
}

void Display::Present(void)
{
    auto result =
        g_Device.acquireNextImageKHR(s_Swapchain.GetSwapchain(), UINT64_MAX, {}, g_imageFences[g_CurrentBuffer]);
    g_Device.waitForFences(g_imageFences[g_CurrentBuffer], VK_TRUE, UINT64_MAX);
    g_Device.resetFences(g_imageFences[g_CurrentBuffer]);

    if (result.result == vk::Result::eErrorOutOfDateKHR || g_WindowResized)
    {
        Resize(g_DisplayWidth, g_DisplayHeight);
        return;
    }
    else if (result.result != vk::Result::eSuccess)
    {
        printf("Something wrong with acquireNextImageKHR\n");
        return;
    }
    else
    {
        g_CurrentBuffer = result.value;
    }

    PreparePresentSDR();

    vk::PresentInfoKHR presentInfo;
    presentInfo.setSwapchainCount(1);
    presentInfo.setSwapchains(s_Swapchain.GetSwapchain());
    presentInfo.setImageIndices(g_CurrentBuffer);
    // presentInfo.setWaitSemaphoreCount(1);
    // presentInfo.setWaitSemaphores(g_imageSemaphores[g_CurrentBuffer]);
    g_CommandManager.GetPresentQueue().Present(presentInfo);

    //    g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

    int64_t CurrentTick = SystemTime::GetCurrentTick();

    s_FrameTime = (float)SystemTime::TimeBetweenTicks(s_FrameStartTick, CurrentTick);
    printf("Frame time: %f\n", s_FrameTime);

    s_FrameStartTick = CurrentTick;

    ++s_FrameIndex;

    SetNativeResolution();
    SetDisplayResolution();
}

uint64_t Graphics::GetFrameCount(void) { return s_FrameIndex; }

float Graphics::GetFrameTime(void) { return s_FrameTime; }

float Graphics::GetFrameRate(void) { return s_FrameTime == 0.0f ? 0.0f : 1.0f / s_FrameTime; }
