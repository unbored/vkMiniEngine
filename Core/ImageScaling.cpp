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

#include "ImageScaling.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "DescriptorSet.h"
#include "EngineTuning.h"
#include "GraphicsCommon.h"
#include "GraphicsCore.h"
#include "PipelineState.h"
#include "RenderPass.h"

#include "CompiledShaders/BilinearUpsampleFrag.h"
#include "CompiledShaders/ScreenQuadPresentVert.h"

#include "CompiledShaders/BicubicHorizontalUpsampleFrag.h"
#include "CompiledShaders/BicubicUpsampleComp.h"
#include "CompiledShaders/BicubicUpsampleFast16Comp.h"
#include "CompiledShaders/BicubicUpsampleFast24Comp.h"
#include "CompiledShaders/BicubicUpsampleFast32Comp.h"
#include "CompiledShaders/BicubicVerticalUpsampleFrag.h"

#include "CompiledShaders/SharpeningUpsampleFrag.h"

#include "CompiledShaders/LanczosComp.h"
#include "CompiledShaders/LanczosFast16Comp.h"
#include "CompiledShaders/LanczosFast24Comp.h"
#include "CompiledShaders/LanczosFast32Comp.h"
#include "CompiledShaders/LanczosHorizontalFrag.h"
#include "CompiledShaders/LanczosVerticalFrag.h"

using namespace Graphics;

namespace Graphics
{
extern DescriptorSet s_PresentDS;
// extern RenderPass::RenderPassDesc s_PresentRP;
} // namespace Graphics

namespace ImageScaling
{

// RenderPass::RenderPassDesc HorizontalRP;

GraphicsPSO SharpeningUpsamplePS("Image Scaling: Sharpen Upsample PSO");
GraphicsPSO BicubicHorizontalUpsamplePS("Image Scaling: Bicubic Horizontal Upsample PSO");
GraphicsPSO BicubicVerticalUpsamplePS("Image Scaling: Bicubic Vertical Upsample PSO");
GraphicsPSO BilinearUpsamplePS("Image Scaling: Bilinear Upsample PSO");
GraphicsPSO LanczosHorizontalPS("Image Scaling: Lanczos Horizontal PSO");
GraphicsPSO LanczosVerticalPS("Image Scaling: Lanczos Vertical PSO");

enum
{
    kDefaultCS,
    kFast16CS,
    kFast24CS,
    kFast32CS,
    kNumCSModes
};

ComputePSO BicubicCS[kNumCSModes];
ComputePSO LanczosCS[kNumCSModes];

NumVar BicubicUpsampleWeight("Graphics/Display/Image Scaling/Bicubic Filter Weight", -0.5f, -1.0f, -0.25f, 0.25f);
NumVar SharpeningSpread("Graphics/Display/Image Scaling/Sharpness Sample Spread", 1.0f, 0.7f, 2.0f, 0.1f);
NumVar SharpeningRotation("Graphics/Display/Image Scaling/Sharpness Sample Rotation", 45.0f, 0.0f, 90.0f, 15.0f);
NumVar SharpeningStrength("Graphics/Display/Image Scaling/Sharpness Strength", 0.10f, 0.0f, 1.0f, 0.01f);
BoolVar ForcePixelShader("Graphics/Display/Image Scaling/Prefer Pixel Shader", false);

void BilinearScale(GraphicsContext &Context, ColorBuffer &dest, ColorBuffer &source)
{
    Context.SetDescriptorSet(s_PresentDS);
    // Context.BeginUpdateDescriptorSet();
    Context.UpdateImageSampler(0, source, SamplerLinearClamp);
    // Context.EndUpdateAndBindDescriptorSet();

    Context.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
    Context.TransitionImageLayout(dest, vk::ImageLayout::eColorAttachmentOptimal);

    Context.BeginRenderPass(dest);
    Context.BindPipeline(BilinearUpsamplePS);
    Context.SetViewportAndScissor(0, 0, dest.GetWidth(), dest.GetHeight());
    Context.Draw(3);
    Context.EndRenderPass();
}

void BicubicScale(GraphicsContext &Context, ColorBuffer &dest, ColorBuffer &source)
{
    bool destinationStorage = bool(dest.GetImageUsageFlags() & vk::ImageUsageFlagBits::eStorage);

    if (destinationStorage && !ForcePixelShader)
    {
        ComputeContext &cmpCtx = Context.GetComputeContext();
        //        ComputeContext & cmpCtx = ComputeContext::Begin("Compute");
        cmpCtx.SetDescriptorSet(s_PresentDS);

        const float scaleX = (float)source.GetWidth() / (float)dest.GetWidth();
        const float scaleY = (float)source.GetHeight() / (float)dest.GetHeight();

        cmpCtx.PushConstants(vk::ShaderStageFlagBits::eAll, 0, scaleX, scaleY, (float)BicubicUpsampleWeight);

        uint32_t tileWidth, tileHeight, shaderMode;

        if (source.GetWidth() * 16 <= dest.GetWidth() * 13 && source.GetHeight() * 16 <= dest.GetHeight() * 13)
        {
            tileWidth = 16;
            tileHeight = 16;
            shaderMode = kFast16CS;
        }
        else if (source.GetWidth() * 24 <= dest.GetWidth() * 21 && source.GetHeight() * 24 <= dest.GetHeight() * 21)
        {
            tileWidth = 32; // For some reason, occupancy drops with 24x24, reducing perf
            tileHeight = 24;
            shaderMode = kFast24CS;
        }
        else if (source.GetWidth() * 32 <= dest.GetWidth() * 29 && source.GetHeight() * 32 <= dest.GetHeight() * 29)
        {
            tileWidth = 32;
            tileHeight = 32;
            shaderMode = kFast32CS;
        }
        else
        {
            tileWidth = 16;
            tileHeight = 16;
            shaderMode = kDefaultCS;
        }

        // cmpCtx.BeginUpdateDescriptorSet();
        cmpCtx.UpdateImageSampler(0, source, SamplerNearestClamp);
        cmpCtx.UpdateStorageImage(2, dest);
        // cmpCtx.EndUpdateAndBindDescriptorSet();

        cmpCtx.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
        cmpCtx.TransitionImageLayout(dest, vk::ImageLayout::eGeneral);

        cmpCtx.BindPipeline(BicubicCS[shaderMode]);
        cmpCtx.Dispatch2D(dest.GetWidth(), dest.GetHeight(), tileWidth, tileHeight);

        //        cmpCtx.Finish();
    }
    else
    {
        // do horizontal upsampling
        Context.SetDescriptorSet(s_PresentDS);
        // Context.BeginUpdateDescriptorSet();
        Context.UpdateImageSampler(0, source, SamplerLinearClamp);
        // use source width and height
        Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, source.GetWidth(), source.GetHeight(),
                              (float)BicubicUpsampleWeight);
        // Context.EndUpdateAndBindDescriptorSet();

        Context.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(g_HorizontalBuffer, vk::ImageLayout::eColorAttachmentOptimal);

        Context.BeginRenderPass(g_HorizontalBuffer);
        Context.BindPipeline(BicubicHorizontalUpsamplePS);
        Context.SetViewportAndScissor(0, 0, dest.GetWidth(),
                                      source.GetHeight()); // dest width and source height
        Context.Draw(3);
        Context.EndRenderPass();

        // do vertical upsampling
        // Context.BeginUpdateDescriptorSet();
        Context.UpdateImageSampler(0, g_HorizontalBuffer, SamplerLinearClamp);
        // use dest width and source height
        Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, dest.GetWidth(), source.GetHeight(),
                              (float)BicubicUpsampleWeight);
        // Context.EndUpdateAndBindDescriptorSet();

        Context.TransitionImageLayout(g_HorizontalBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(dest, vk::ImageLayout::eColorAttachmentOptimal);

        Context.BeginRenderPass(dest);
        Context.BindPipeline(BicubicVerticalUpsamplePS);
        Context.SetViewportAndScissor(0, 0, dest.GetWidth(), dest.GetHeight());
        Context.Draw(3);
        Context.EndRenderPass();
    }
}

void BilinearSharpeningScale(GraphicsContext &Context, ColorBuffer &dest, ColorBuffer &source)
{
    Context.SetDescriptorSet(s_PresentDS);
    // Context.BeginUpdateDescriptorSet();
    Context.UpdateImageSampler(0, source, SamplerLinearClamp);
    float TexelWidth = 1.0f / source.GetWidth();
    float TexelHeight = 1.0f / source.GetHeight();
    float X = glm::cos(glm::radians((float)SharpeningRotation)) * (float)SharpeningSpread;
    float Y = glm::sin(glm::radians((float)SharpeningRotation)) * (float)SharpeningSpread;
    const float WA = (float)SharpeningStrength;
    const float WB = 1.0f + 4.0f * WA;
    Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, X * TexelWidth, Y * TexelHeight, Y * TexelWidth,
                          -X * TexelHeight, WA, WB);
    // Context.EndUpdateAndBindDescriptorSet();

    Context.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
    Context.TransitionImageLayout(dest, vk::ImageLayout::eColorAttachmentOptimal);

    Context.BeginRenderPass(dest);
    Context.BindPipeline(SharpeningUpsamplePS);
    Context.SetViewportAndScissor(0, 0, dest.GetWidth(), dest.GetHeight());
    Context.Draw(3);
    Context.EndRenderPass();
}

void LanczosScale(GraphicsContext &Context, ColorBuffer &dest, ColorBuffer &source)
{
    bool destinationStorage = bool(dest.GetImageUsageFlags() & vk::ImageUsageFlagBits::eStorage);

    if (destinationStorage && !ForcePixelShader)
    {
        ComputeContext &cmpCtx = Context.GetComputeContext();
        //        ComputeContext & cmpCtx = ComputeContext::Begin("Compute");
        cmpCtx.SetDescriptorSet(s_PresentDS);

        const float scaleX = (float)source.GetWidth() / (float)dest.GetWidth();
        const float scaleY = (float)source.GetHeight() / (float)dest.GetHeight();

        cmpCtx.PushConstants(vk::ShaderStageFlagBits::eAll, 0, scaleX, scaleY);

        uint32_t tileWidth, tileHeight, shaderMode;

        if (source.GetWidth() * 16 <= dest.GetWidth() * 13 && source.GetHeight() * 16 <= dest.GetHeight() * 13)
        {
            tileWidth = 16;
            tileHeight = 16;
            shaderMode = kFast16CS;
        }
        else if (source.GetWidth() * 24 <= dest.GetWidth() * 21 && source.GetHeight() * 24 <= dest.GetHeight() * 21)
        {
            tileWidth = 32; // For some reason, occupancy drops with 24x24, reducing perf
            tileHeight = 24;
            shaderMode = kFast24CS;
        }
        else if (source.GetWidth() * 32 <= dest.GetWidth() * 29 && source.GetHeight() * 32 <= dest.GetHeight() * 29)
        {
            tileWidth = 32;
            tileHeight = 32;
            shaderMode = kFast32CS;
        }
        else
        {
            tileWidth = 16;
            tileHeight = 16;
            shaderMode = kDefaultCS;
        }

        // cmpCtx.BeginUpdateDescriptorSet();
        cmpCtx.UpdateImageSampler(0, source, SamplerNearestClamp);
        cmpCtx.UpdateStorageImage(2, dest);
        // cmpCtx.EndUpdateAndBindDescriptorSet();

        cmpCtx.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
        cmpCtx.TransitionImageLayout(dest, vk::ImageLayout::eGeneral);

        cmpCtx.BindPipeline(LanczosCS[shaderMode]);
        cmpCtx.Dispatch2D(dest.GetWidth(), dest.GetHeight(), tileWidth, tileHeight);

        //        cmpCtx.Finish();
    }
    else
    {
        // do horizontal upsampling
        Context.SetDescriptorSet(s_PresentDS);
        // Context.BeginUpdateDescriptorSet();
        Context.UpdateImageSampler(0, source, SamplerLinearClamp);
        // use source width and height
        Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, source.GetWidth(), source.GetHeight());
        // Context.EndUpdateAndBindDescriptorSet();

        Context.TransitionImageLayout(source, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(g_HorizontalBuffer, vk::ImageLayout::eColorAttachmentOptimal);

        Context.BeginRenderPass(g_HorizontalBuffer);
        Context.BindPipeline(LanczosHorizontalPS);
        Context.SetViewportAndScissor(0, 0, dest.GetWidth(),
                                      source.GetHeight()); // dest width and source height
        Context.Draw(3);
        Context.EndRenderPass();

        // do vertical upsampling
        // Context.BeginUpdateDescriptorSet();
        Context.UpdateImageSampler(0, g_HorizontalBuffer, SamplerLinearClamp);
        // use dest width and source height
        Context.PushConstants(vk::ShaderStageFlagBits::eAll, 0, dest.GetWidth(), source.GetHeight());
        // Context.EndUpdateAndBindDescriptorSet();

        Context.TransitionImageLayout(g_HorizontalBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        Context.TransitionImageLayout(dest, vk::ImageLayout::eColorAttachmentOptimal);

        Context.BeginRenderPass(dest);
        Context.BindPipeline(LanczosVerticalPS);
        Context.SetViewportAndScissor(0, 0, dest.GetWidth(), dest.GetHeight());
        Context.Draw(3);
        Context.EndRenderPass();
    }
}

void Initialize(vk::Format DestFormat)
{
    // DestFormat is SwapchainFormat, s_PresentRP can be used

    BilinearUpsamplePS.SetRasterizerState(RasterizerTwoSided);
    BilinearUpsamplePS.SetBlendState(BlendDisable);
    BilinearUpsamplePS.SetDepthStencilState(DepthStateDisabled);
    BilinearUpsamplePS.SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    BilinearUpsamplePS.SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleList);
    BilinearUpsamplePS.SetRenderPassFormat(DestFormat);
    BilinearUpsamplePS.SetVertexShader(g_ScreenQuadPresentVert, sizeof(g_ScreenQuadPresentVert));
    BilinearUpsamplePS.SetFragmentShader(g_BilinearUpsampleFrag, sizeof(g_BilinearUpsampleFrag));
    BilinearUpsamplePS.Finalize();

    // HorizontalRP.colors = {{g_HorizontalBuffer.GetFormat(), vk::AttachmentLoadOp::eDontCare}};

    BicubicHorizontalUpsamplePS = BilinearUpsamplePS;
    BicubicHorizontalUpsamplePS.SetRenderPassFormat(g_HorizontalBuffer.GetFormat());
    BicubicHorizontalUpsamplePS.SetFragmentShader(g_BicubicHorizontalUpsampleFrag,
                                                  sizeof(g_BicubicHorizontalUpsampleFrag));
    BicubicHorizontalUpsamplePS.Finalize();

    BicubicVerticalUpsamplePS = BilinearUpsamplePS;
    BicubicVerticalUpsamplePS.SetFragmentShader(g_BicubicVerticalUpsampleFrag, sizeof(g_BicubicVerticalUpsampleFrag));
    BicubicVerticalUpsamplePS.Finalize();

    BicubicCS[kDefaultCS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    BicubicCS[kDefaultCS].SetComputeShader(g_BicubicUpsampleComp, sizeof(g_BicubicUpsampleComp));
    BicubicCS[kDefaultCS].Finalize();

    BicubicCS[kFast16CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    BicubicCS[kFast16CS].SetComputeShader(g_BicubicUpsampleFast16Comp, sizeof(g_BicubicUpsampleFast16Comp));
    BicubicCS[kFast16CS].Finalize();

    BicubicCS[kFast24CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    BicubicCS[kFast24CS].SetComputeShader(g_BicubicUpsampleFast24Comp, sizeof(g_BicubicUpsampleFast24Comp));
    BicubicCS[kFast24CS].Finalize();

    BicubicCS[kFast32CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    BicubicCS[kFast32CS].SetComputeShader(g_BicubicUpsampleFast32Comp, sizeof(g_BicubicUpsampleFast32Comp));
    BicubicCS[kFast32CS].Finalize();

    SharpeningUpsamplePS = BilinearUpsamplePS;
    SharpeningUpsamplePS.SetFragmentShader(g_SharpeningUpsampleFrag, sizeof(g_SharpeningUpsampleFrag));
    SharpeningUpsamplePS.Finalize();

    LanczosHorizontalPS = BicubicHorizontalUpsamplePS;
    LanczosHorizontalPS.SetFragmentShader(g_LanczosHorizontalFrag, sizeof(g_LanczosHorizontalFrag));
    LanczosHorizontalPS.Finalize();

    LanczosVerticalPS = BicubicVerticalUpsamplePS;
    LanczosVerticalPS.SetFragmentShader(g_LanczosVerticalFrag, sizeof(g_LanczosVerticalFrag));
    LanczosVerticalPS.Finalize();

    LanczosCS[kDefaultCS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    LanczosCS[kDefaultCS].SetComputeShader(g_LanczosComp, sizeof(g_LanczosComp));
    LanczosCS[kDefaultCS].Finalize();

    LanczosCS[kFast16CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    LanczosCS[kFast16CS].SetComputeShader(g_LanczosFast16Comp, sizeof(g_LanczosFast16Comp));
    LanczosCS[kFast16CS].Finalize();

    LanczosCS[kFast24CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    LanczosCS[kFast24CS].SetComputeShader(g_LanczosFast24Comp, sizeof(g_LanczosFast24Comp));
    LanczosCS[kFast24CS].Finalize();

    LanczosCS[kFast32CS].SetPipelineLayout(s_PresentDS.GetPipelineLayout());
    LanczosCS[kFast32CS].SetComputeShader(g_LanczosFast32Comp, sizeof(g_LanczosFast32Comp));
    LanczosCS[kFast32CS].Finalize();
}

void Upscale(GraphicsContext &Context, ColorBuffer &dest, ColorBuffer &source, eScalingFilter tech)
{
    //    ScopedTimer _prof(L"Image Upscale", Context);

    // Here Context has already begun updating DS,
    // and set source as binding 0 and with SamplerLinearClamp,
    // moreover source has been set as eShaderReadOnlyOptimal,
    // while dest has been set as eColorAttachmentOptimal

    switch (tech)
    {
    case kBicubic:
        return BicubicScale(Context, dest, source);
    case kSharpening:
        return BilinearSharpeningScale(Context, dest, source);
    case kBilinear:
        return BilinearScale(Context, dest, source);
    case kLanczos:
        return LanczosScale(Context, dest, source);
    default:
        return;
    }
}

} // namespace ImageScaling
