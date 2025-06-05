#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "ShadowBuffer.h"

namespace Graphics
{
extern ColorBuffer g_SceneColorBuffer;
extern DepthBuffer g_SceneDepthBuffer;
extern ColorBuffer g_OverlayBuffer;
extern ColorBuffer g_HorizontalBuffer;

extern ShadowBuffer g_ShadowBuffer;

extern ColorBuffer g_SSAOFullScreen; // R8_UNORM

void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
void DestroyRenderingBuffers();
} // namespace Graphics
