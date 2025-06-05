#pragma once

#include <vulkan/vulkan.hpp>

#include "SamplerManager.h"

using RasterizationState = vk::PipelineRasterizationStateCreateInfo;
using DepthStencilState = vk::PipelineDepthStencilStateCreateInfo;
using ColorBlendState = vk::PipelineColorBlendAttachmentState;

namespace Graphics
{
extern vk::Sampler SamplerLinearWrap;
extern vk::Sampler SamplerAnisoWrap;
extern vk::Sampler SamplerShadow;
extern vk::Sampler SamplerLinearClamp;
extern vk::Sampler SamplerNearestClamp;

enum eDefaultTexture
{
    kMagenta2D, // Useful for indicating missing textures
    kBlackOpaque2D,
    kBlackTransparent2D,
    kWhiteOpaque2D,
    kWhiteTransparent2D,
    kDefaultNormalMap,
    kBlackCubeMap,
    kColorCubeMap,

    kNumDefaultTextures
};
vk::ImageView GetDefaultTexture(eDefaultTexture texID);

extern RasterizationState RasterizerDefault; // cull back, counter-clockwise
extern RasterizationState RasterizerTwoSided;
extern RasterizationState RasterizerShadow;
extern RasterizationState RasterizerShadowTwoSided;

extern ColorBlendState BlendNoColorWrite;  // No color write
extern ColorBlendState BlendDisable;       // 1, 0
extern ColorBlendState BlendTraditional;   // SrcA, 1-SrcA
extern ColorBlendState BlendPreMultiplied; // 1, 1-SrcA

extern DepthStencilState DepthStateDisabled;
extern DepthStencilState DepthStateReadWrite;
extern DepthStencilState DepthStateReadOnly;
extern DepthStencilState DepthStateReadOnlyReversed;
extern DepthStencilState DepthStateTestEqual;

void InitializeCommonState(void);
void DestroyCommonState(void);

} // namespace Graphics
