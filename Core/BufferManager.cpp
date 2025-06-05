#include "BufferManager.h"
#include "CommandContext.h"
#include "Display.h"
#include "GpuBuffer.h"
#include "TextRenderer.h"

namespace Graphics
{
ColorBuffer g_SceneColorBuffer;
DepthBuffer g_SceneDepthBuffer;
ColorBuffer g_OverlayBuffer;
ColorBuffer g_HorizontalBuffer;

ShadowBuffer g_ShadowBuffer;

ColorBuffer g_SSAOFullScreen(Color(1.0f, 1.0f, 1.0f));

vk::Format DefaultHdrColorFormat = vk::Format::eB10G11R11UfloatPack32;
} // namespace Graphics

void Graphics::InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
    g_SceneColorBuffer.Create("Main Color Buffer", NativeWidth, NativeHeight, 1, DefaultHdrColorFormat);
    g_SceneDepthBuffer.Create("Scene Depth Buffer", NativeWidth, NativeHeight, 1, vk::Format::eD32Sfloat);

    g_SSAOFullScreen.Create("SSAO Full Res", NativeWidth, NativeHeight, 1, vk::Format::eR8Unorm);

    g_OverlayBuffer.Create("UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, vk::Format::eR8G8B8A8Unorm);
    g_HorizontalBuffer.Create("Bicubic Intermediate", g_DisplayWidth, NativeHeight, 1, DefaultHdrColorFormat);

    g_ShadowBuffer.Create("Shadow Map", 2048, 2048);
}

void Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
    g_OverlayBuffer.Create("UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, vk::Format::eR8G8B8A8Unorm);
    g_HorizontalBuffer.Create("Bicubic Intermediate", g_DisplayWidth, NativeHeight, 1, DefaultHdrColorFormat);
}

void Graphics::DestroyRenderingBuffers()
{
    g_SceneColorBuffer.Destroy();
    g_SceneDepthBuffer.Destroy();

    g_OverlayBuffer.Destroy();
    g_HorizontalBuffer.Destroy();

    g_ShadowBuffer.Destroy();

    g_SSAOFullScreen.Destroy();
}
