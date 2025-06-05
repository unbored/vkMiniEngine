#include "GraphicsCommon.h"
#include "Display.h"
#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "Texture.h"
#include "Utility.h"

namespace Graphics
{
vk::Sampler SamplerLinearWrap;
vk::Sampler SamplerAnisoWrap;
vk::Sampler SamplerShadow;
vk::Sampler SamplerLinearClamp;
vk::Sampler SamplerNearestClamp;

Texture DefaultTextures[kNumDefaultTextures];
vk::ImageView GetDefaultTexture(eDefaultTexture texID)
{
    ASSERT(texID < kNumDefaultTextures);
    return DefaultTextures[texID];
}

RasterizationState RasterizerDefault; // cull back, counter-clockwise
RasterizationState RasterizerTwoSided;
RasterizationState RasterizerShadow;
RasterizationState RasterizerShadowTwoSided;

ColorBlendState BlendNoColorWrite;
ColorBlendState BlendDisable;
ColorBlendState BlendTraditional;
ColorBlendState BlendPreMultiplied;

DepthStencilState DepthStateDisabled;
DepthStencilState DepthStateReadWrite;
DepthStencilState DepthStateReadOnly;
DepthStencilState DepthStateReadOnlyReversed;
DepthStencilState DepthStateTestEqual;

void InitializeCommonState(void)
{
    SamplerDesc linearWrapInfo;
    linearWrapInfo.anisotropyEnable = VK_FALSE;
    SamplerLinearWrap = linearWrapInfo.CreateSampler();

    SamplerDesc anisoWrapInfo;
    anisoWrapInfo.maxAnisotropy = 4;
    SamplerAnisoWrap = anisoWrapInfo.CreateSampler();

    SamplerDesc shadowInfo;
    shadowInfo.anisotropyEnable = VK_FALSE;
    shadowInfo.magFilter = vk::Filter::eNearest;
    shadowInfo.minFilter = vk::Filter::eNearest;
    shadowInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    shadowInfo.compareEnable = VK_TRUE;
    shadowInfo.compareOp = vk::CompareOp::eGreaterOrEqual;
    shadowInfo.SetAddressMode(vk::SamplerAddressMode::eClampToEdge);
    SamplerShadow = shadowInfo.CreateSampler();

    SamplerDesc linearClampInfo;
    linearClampInfo.anisotropyEnable = VK_FALSE;
    linearClampInfo.SetAddressMode(vk::SamplerAddressMode::eClampToEdge);
    SamplerLinearClamp = linearClampInfo.CreateSampler();

    SamplerDesc nearestClampInfo;
    nearestClampInfo.magFilter = vk::Filter::eNearest;
    nearestClampInfo.minFilter = vk::Filter::eNearest;
    nearestClampInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    nearestClampInfo.SetAddressMode(vk::SamplerAddressMode::eClampToEdge);
    SamplerNearestClamp = nearestClampInfo.CreateSampler();

    uint32_t MagentaPixel = 0xFFFF00FF;
    DefaultTextures[kMagenta2D].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &MagentaPixel);
    uint32_t BlackOpaqueTexel = 0xFF000000;
    DefaultTextures[kBlackOpaque2D].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &BlackOpaqueTexel);
    uint32_t BlackTransparentTexel = 0x00000000;
    DefaultTextures[kBlackTransparent2D].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &BlackTransparentTexel);
    uint32_t WhiteOpaqueTexel = 0xFFFFFFFF;
    DefaultTextures[kWhiteOpaque2D].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &WhiteOpaqueTexel);
    uint32_t WhiteTransparentTexel = 0x00FFFFFF;
    DefaultTextures[kWhiteTransparent2D].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &WhiteTransparentTexel);
    uint32_t FlatNormalTexel = 0x00FF8080;
    DefaultTextures[kDefaultNormalMap].Create2D(4, 1, 1, vk::Format::eR8G8B8A8Unorm, &FlatNormalTexel);
    uint32_t BlackCubeTexels[6] = {};
    DefaultTextures[kBlackCubeMap].CreateCube(4, 1, 1, vk::Format::eR8G8B8A8Unorm, BlackCubeTexels);
    uint32_t ColorCubeTexels[6] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF, 0xFFFFFF00};
    DefaultTextures[kColorCubeMap].CreateCube(4, 1, 1, vk::Format::eR8G8B8A8Unorm, ColorCubeTexels);

    RasterizerDefault.polygonMode = vk::PolygonMode::eFill;
    RasterizerDefault.cullMode = vk::CullModeFlagBits::eBack;
    RasterizerDefault.frontFace = vk::FrontFace::eCounterClockwise;
    RasterizerDefault.depthBiasEnable = VK_FALSE;
    RasterizerDefault.depthBiasClamp = 0.0f;
    RasterizerDefault.depthBiasSlopeFactor = 0.0f;

    RasterizerDefault.depthClampEnable = VK_FALSE; // means clip enable
    RasterizerDefault.rasterizerDiscardEnable = VK_FALSE;
    RasterizerDefault.lineWidth = 1.0f;

    RasterizerTwoSided = RasterizerDefault;
    RasterizerTwoSided.cullMode = vk::CullModeFlagBits::eNone;

    // Shadows need their own rasterizer state so we can reverse the winding of faces
    RasterizerShadow = RasterizerDefault;
    // RasterizerShadow.cullMode = vk::CullModeFlagBits::eFront; // Hacked here rather than fixing the content
    RasterizerShadow.depthBiasEnable = VK_TRUE;
    RasterizerShadow.depthBiasConstantFactor = -100;
    RasterizerShadow.depthBiasSlopeFactor = -1.5f;
    // RasterizerShadow.depthBiasClamp = -100;

    RasterizerShadowTwoSided = RasterizerShadow;
    RasterizerShadowTwoSided.cullMode = vk::CullModeFlagBits::eNone;

    DepthStateDisabled.depthTestEnable = VK_FALSE;
    DepthStateDisabled.depthWriteEnable = VK_FALSE;
    DepthStateDisabled.depthCompareOp = vk::CompareOp::eAlways;
    DepthStateDisabled.stencilTestEnable = VK_FALSE;
    DepthStateDisabled.depthBoundsTestEnable = VK_FALSE;

    DepthStateReadWrite = DepthStateDisabled;
    DepthStateReadWrite.depthTestEnable = VK_TRUE;
    DepthStateReadWrite.depthWriteEnable = VK_TRUE;
    DepthStateReadWrite.depthCompareOp = vk::CompareOp::eGreaterOrEqual;

    DepthStateReadOnly = DepthStateReadWrite;
    DepthStateReadOnly.depthWriteEnable = VK_FALSE;

    DepthStateReadOnlyReversed = DepthStateReadOnly;
    DepthStateReadOnlyReversed.depthCompareOp = vk::CompareOp::eLess;

    DepthStateTestEqual = DepthStateReadOnly;
    DepthStateTestEqual.depthCompareOp = vk::CompareOp::eEqual;

    vk::ColorComponentFlags allFlags = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                       vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    ColorBlendState alphaBlend;
    alphaBlend.blendEnable = VK_FALSE;
    alphaBlend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    alphaBlend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    alphaBlend.colorBlendOp = vk::BlendOp::eAdd;
    alphaBlend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    alphaBlend.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    alphaBlend.alphaBlendOp = vk::BlendOp::eAdd;
    alphaBlend.colorWriteMask = {};
    BlendNoColorWrite = alphaBlend;

    alphaBlend.colorWriteMask = allFlags;
    BlendDisable = alphaBlend;

    alphaBlend.blendEnable = VK_TRUE;
    BlendTraditional = alphaBlend;

    alphaBlend.srcColorBlendFactor = vk::BlendFactor::eOne;
    BlendPreMultiplied = alphaBlend;
}

void DestroyCommonState(void)
{
    for (int i = 0; i < kNumDefaultTextures; ++i)
    {
        DefaultTextures[i].Destroy();
    }
}
} // namespace Graphics
