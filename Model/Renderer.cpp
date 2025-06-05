#include "Renderer.h"
#include "Model.h"
#include <BufferManager.h>
#include <ColorBuffer.h>
#include <CommandContext.h>
#include <DepthBuffer.h>
#include <DescriptorSet.h>
#include <Framebuffer.h>
#include <GpuBuffer.h>
#include <GraphicsCommon.h>
#include <GraphicsCore.h>
#include <RenderPass.h>
#include <SamplerManager.h>
#include <Texture.h>
#include <TextureManager.h>
#include <Utility.h>
#include <string.h>
#include <utility>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#include <stb_image.h>

#include <CompiledShaders/CutoutDepthFrag.h>
#include <CompiledShaders/CutoutDepthSkinVert.h>
#include <CompiledShaders/CutoutDepthVert.h>
#include <CompiledShaders/DefaultFrag.h>
#include <CompiledShaders/DefaultNoTangentFrag.h>
#include <CompiledShaders/DefaultNoTangentNoUV1Frag.h>
#include <CompiledShaders/DefaultNoTangentNoUV1SkinVert.h>
#include <CompiledShaders/DefaultNoTangentNoUV1Vert.h>
#include <CompiledShaders/DefaultNoTangentSkinVert.h>
#include <CompiledShaders/DefaultNoTangentVert.h>
#include <CompiledShaders/DefaultNoUV1Frag.h>
#include <CompiledShaders/DefaultNoUV1SkinVert.h>
#include <CompiledShaders/DefaultNoUV1Vert.h>
#include <CompiledShaders/DefaultSkinVert.h>
#include <CompiledShaders/DefaultVert.h>
#include <CompiledShaders/DepthOnlySkinVert.h>
#include <CompiledShaders/DepthOnlyVert.h>
#include <CompiledShaders/SkyboxFrag.h>
#include <CompiledShaders/SkyboxVert.h>

using namespace Graphics;
using namespace Renderer;

namespace Renderer
{
BoolVar SeparateZPass("Renderer/Separate Z Pass", true);

bool s_Initialized = false;

std::vector<std::vector<vk::DescriptorImageInfo>> m_TextureHeap;

std::vector<GraphicsPSO> sm_PSOs;

TextureRef s_RadianceCubeMap;
TextureRef s_IrradianceCubeMap;
float s_SpecularIBLRange;
float s_SpecularIBLBias;
uint32_t g_SSAOFullScreenID;
uint32_t g_ShadowBufferID;

// DescriptorSet DefaultDS;
DescriptorSet m_DescriptorSet;

// VertexBuffer vertexBuffer;
// IndexBuffer indexBuffer;
// Texture DefaultTex;

// GraphicsPSO m_PSO;
GraphicsPSO m_SkyboxPSO("Renderer: Skybox PSO");
GraphicsPSO m_DefaultPSO("Renderer: Default PSO"); // Not finalized.  Used as a template.

vk::Sampler DefaultSampler;
vk::Sampler CubeMapSampler;
std::vector<vk::DescriptorImageInfo> m_CommonCubeTextures;
std::vector<vk::DescriptorImageInfo> m_Common2DTextures;
std::vector<vk::DescriptorImageInfo> m_CommonShadowTextures;
// std::vector<vk::DescriptorImageInfo> m_CommonTextures;
} // namespace Renderer

void Renderer::Initialize()
{
    if (s_Initialized)
    {
        return;
    }

    SamplerDesc defaultSamplerInfo;
    defaultSamplerInfo.maxAnisotropy = 8;
    DefaultSampler = defaultSamplerInfo.CreateSampler();

    SamplerDesc cubeMapSamplerInfo;
    // cubeMapSamplerInfo.maxLod = 6.0f;
    CubeMapSampler = cubeMapSamplerInfo.CreateSampler();

    // kMeshConstants
    m_DescriptorSet.AddBindings(kMeshConstants, 1, vk::DescriptorType::eUniformBuffer, 1,
                                vk::ShaderStageFlagBits::eVertex);
    // kMaterialConstants
    m_DescriptorSet.AddBindings(kMaterialConstants, 1, vk::DescriptorType::eUniformBuffer, 1,
                                vk::ShaderStageFlagBits::eFragment);
    // kMaterialSamplers
    m_DescriptorSet.AddBindings(kMaterialSamplers, 1, vk::DescriptorType::eCombinedImageSampler, kNumTextures,
                                vk::ShaderStageFlagBits::eFragment);
    // kCommonSamplers
    m_DescriptorSet.AddBindings(kCommonCubeSamplers, 1, vk::DescriptorType::eCombinedImageSampler, 2,
                               vk::ShaderStageFlagBits::eFragment);
    m_DescriptorSet.AddBindings(kCommon2DSamplers, 1, vk::DescriptorType::eCombinedImageSampler, 1,
                               vk::ShaderStageFlagBits::eFragment);
    m_DescriptorSet.AddBindings(kCommonShadowSamplers, 1, vk::DescriptorType::eCombinedImageSampler, 1,
                               vk::ShaderStageFlagBits::eFragment);
    //  m_DescriptorSet.AddBindings(kCommonSamplers, 1, vk::DescriptorType::eCombinedImageSampler, 4,
    //                             vk::ShaderStageFlagBits::eFragment);
    // kCommonConstants
    m_DescriptorSet.AddBindings(kCommonConstants, 1, vk::DescriptorType::eUniformBuffer, 1,
                                vk::ShaderStageFlagBits::eAll);
    // kSkinMatrices
    m_DescriptorSet.AddBindings(kSkinMatrices, 1, vk::DescriptorType::eStorageBuffer, 1,
                                vk::ShaderStageFlagBits::eVertex);
    m_DescriptorSet.Finalize();

    vk::Format ColorFormat = g_SceneColorBuffer.GetFormat();
    vk::Format DepthFormat = g_SceneDepthBuffer.GetFormat();

    VertexInputBindingAttribute posOnly = {// binding, stride, inputRate
                                           {0, 4 * 3, vk::VertexInputRate::eVertex},
                                           {// location, binding, format, offset
                                            {0, 0, vk::Format::eR32G32B32Sfloat, 0}}}; // position

    VertexInputBindingAttribute posAndUV = {// binding, stride, inputRate
                                            {0, 4 * 3 + 4, vk::VertexInputRate::eVertex},
                                            {// location, binding, format, offset
                                             {0, 0, vk::Format::eR32G32B32Sfloat, 0},    // position
                                             {1, 0, vk::Format::eR16G16Sfloat, 4 * 3}}}; // uv0

    VertexInputBindingAttribute skinPos = {// binding, stride, inputRate
                                           {0, 4 * 3 + 4 * 2 * 2, vk::VertexInputRate::eVertex},
                                           {// location, binding, format, offset
                                            {0, 0, vk::Format::eR32G32B32Sfloat, 0},                 // position
                                            {2, 0, vk::Format::eR16G16B16A16Uint, 4 * 3},            // jointIndices
                                            {3, 0, vk::Format::eR16G16B16A16Unorm, 4 * 3 + 4 * 2}}}; // jointWeights

    VertexInputBindingAttribute skinPosAndUV = {
        // binding, stride, inputRate
        {0, 4 * 3 + 4 + 4 * 2 * 2, vk::VertexInputRate::eVertex},
        {                                                             // location, binding, format, offset
         {0, 0, vk::Format::eR32G32B32Sfloat, 0},                     // position
         {1, 0, vk::Format::eR16G16Sfloat, 4 * 3},                    // uv0
         {2, 0, vk::Format::eR16G16B16A16Uint, 4 * 3 + 4},            // jointIndices
         {3, 0, vk::Format::eR16G16B16A16Unorm, 4 * 3 + 4 + 4 * 2}}}; // jointWeights

    // Depth only PSOs
    GraphicsPSO DepthOnlyPSO("Renderer: Depth Only PSO");
    DepthOnlyPSO.SetPipelineLayout(m_DescriptorSet.GetPipelineLayout());
    DepthOnlyPSO.SetRasterizerState(RasterizerDefault);
    DepthOnlyPSO.SetBlendState(BlendDisable);
    DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
    DepthOnlyPSO.SetInputLayout(posOnly);
    DepthOnlyPSO.SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleList);
    DepthOnlyPSO.SetRenderPassFormat({}, DepthFormat);
    DepthOnlyPSO.SetVertexShader(g_DepthOnlyVert, sizeof(g_DepthOnlyVert));
    // DepthOnlyPSO.SetFragmentShader(g_DefaultFrag, sizeof(g_DefaultFrag));
    DepthOnlyPSO.Finalize();
    sm_PSOs.push_back(DepthOnlyPSO); // PSO0

    GraphicsPSO CutoutDepthPSO("Renderer: Cutout Depth PSO");
    CutoutDepthPSO = DepthOnlyPSO;
    CutoutDepthPSO.SetInputLayout(posAndUV);
    CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
    CutoutDepthPSO.SetVertexShader(g_CutoutDepthVert, sizeof(g_CutoutDepthVert));
    CutoutDepthPSO.SetFragmentShader(g_CutoutDepthFrag, sizeof(g_CutoutDepthFrag));
    CutoutDepthPSO.Finalize();
    sm_PSOs.push_back(CutoutDepthPSO); // PSO1

    GraphicsPSO SkinDepthOnlyPSO = DepthOnlyPSO;
    SkinDepthOnlyPSO.SetInputLayout(skinPos);
    SkinDepthOnlyPSO.SetVertexShader(g_DepthOnlySkinVert, sizeof(g_DepthOnlySkinVert));
    SkinDepthOnlyPSO.Finalize();
    sm_PSOs.push_back(SkinDepthOnlyPSO); // PSO2

    GraphicsPSO SkinCutoutDepthPSO = CutoutDepthPSO;
    SkinCutoutDepthPSO.SetInputLayout(skinPosAndUV);
    SkinCutoutDepthPSO.SetVertexShader(g_CutoutDepthSkinVert, sizeof(g_CutoutDepthSkinVert));
    SkinCutoutDepthPSO.Finalize();
    sm_PSOs.push_back(SkinCutoutDepthPSO); // PSO3

    ASSERT(sm_PSOs.size() == 4);

    // Shadow PSOs

    DepthOnlyPSO.SetRasterizerState(RasterizerShadow);
    DepthOnlyPSO.SetRenderPassFormat({}, g_ShadowBuffer.GetFormat());
    DepthOnlyPSO.Finalize();
    sm_PSOs.push_back(DepthOnlyPSO); // PSO5

    CutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
    CutoutDepthPSO.SetRenderPassFormat({}, g_ShadowBuffer.GetFormat());
    CutoutDepthPSO.Finalize();
    sm_PSOs.push_back(CutoutDepthPSO); // PSO6

    SkinDepthOnlyPSO.SetRasterizerState(RasterizerShadow);
    SkinDepthOnlyPSO.SetRenderPassFormat({}, g_ShadowBuffer.GetFormat());
    SkinDepthOnlyPSO.Finalize();
    sm_PSOs.push_back(SkinDepthOnlyPSO); // PSO7

    SkinCutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
    SkinCutoutDepthPSO.SetRenderPassFormat({}, g_ShadowBuffer.GetFormat());
    SkinCutoutDepthPSO.Finalize();
    sm_PSOs.push_back(SkinCutoutDepthPSO); // PSO8

    ASSERT(sm_PSOs.size() == 8);

    // Default PSO

    m_DefaultPSO.SetPipelineLayout(m_DescriptorSet.GetPipelineLayout());
    m_DefaultPSO.SetRasterizerState(RasterizerDefault);
    m_DefaultPSO.SetBlendState(BlendDisable);
    m_DefaultPSO.SetDepthStencilState(DepthStateReadWrite);
    m_DefaultPSO.SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleList);
    m_DefaultPSO.SetRenderPassFormat(ColorFormat, DepthFormat);
    m_DefaultPSO.SetVertexShader(g_DefaultVert, sizeof(g_DefaultVert));
    m_DefaultPSO.SetFragmentShader(g_DefaultFrag, sizeof(g_DefaultFrag));

    // Skybox PSO

    m_SkyboxPSO = m_DefaultPSO;
    m_SkyboxPSO.SetRasterizerState(RasterizerTwoSided);
    m_SkyboxPSO.SetDepthStencilState(DepthStateReadOnly);
    m_SkyboxPSO.SetVertexShader(g_SkyboxVert, sizeof(g_SkyboxVert));
    m_SkyboxPSO.SetFragmentShader(g_SkyboxFrag, sizeof(g_SkyboxFrag));
    m_SkyboxPSO.Finalize();

    TextureManager::Initialize("");

    m_CommonCubeTextures = std::vector<vk::DescriptorImageInfo>{
       {CubeMapSampler, GetDefaultTexture(kColorCubeMap), vk::ImageLayout::eShaderReadOnlyOptimal},
       {DefaultSampler, GetDefaultTexture(kColorCubeMap), vk::ImageLayout::eShaderReadOnlyOptimal}};
    m_Common2DTextures = std::vector<vk::DescriptorImageInfo>{
       {SamplerNearestClamp, g_SSAOFullScreen, vk::ImageLayout::eShaderReadOnlyOptimal}};
    m_CommonShadowTextures = std::vector<vk::DescriptorImageInfo>{
       {SamplerShadow, g_ShadowBuffer, vk::ImageLayout::eShaderReadOnlyOptimal}};
    // m_CommonTextures = std::vector<vk::DescriptorImageInfo>{
    //     {CubeMapSampler, GetDefaultTexture(kColorCubeMap), vk::ImageLayout::eShaderReadOnlyOptimal},
    //     {DefaultSampler, GetDefaultTexture(kColorCubeMap), vk::ImageLayout::eShaderReadOnlyOptimal},
    //     {SamplerNearestClamp, g_SSAOFullScreen, vk::ImageLayout::eShaderReadOnlyOptimal},
    //     {SamplerShadow, g_ShadowBuffer, vk::ImageLayout::eShaderReadOnlyOptimal}};


    s_Initialized = true;

    // //==================================================================
    // StagingBuffer stagingBuffer;
    // void *data;

    // //=== initialize vertex buffer ===
    // struct Vertex
    // {
    //     glm::vec3 pos;
    //     glm::vec2 tex;
    // };
    // std::vector<Vertex> vertice = {{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
    //                                {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
    //                                {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
    //                                {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}}};
    // size_t verticeSize = sizeof(Vertex) * vertice.size();
    // stagingBuffer.Create(verticeSize);
    // data = stagingBuffer.Map();
    // memcpy(data, vertice.data(), verticeSize);
    // stagingBuffer.Unmap();
    // vertexBuffer.Create(verticeSize);
    // CommandContext::InitializeBuffer(vertexBuffer, stagingBuffer, 0);
    // stagingBuffer.Destroy();

    // //=== initialize index buffer ===
    // std::vector<uint16_t> indice = {0, 1, 2, 2, 3, 0};
    // size_t indiceSize = sizeof(uint16_t) * indice.size();
    // stagingBuffer.Create(indiceSize);
    // data = stagingBuffer.Map();
    // memcpy(data, indice.data(), indiceSize);
    // stagingBuffer.Unmap();
    // indexBuffer.Create(vk::IndexType::eUint16, indice.size());
    // CommandContext::InitializeBuffer(indexBuffer, stagingBuffer, 0);
    // stagingBuffer.Destroy();

    // //=== initialize texture ===
    // int texWidth, texHeight, texChannels;
    // // stbi_set_flip_vertically_on_load(true);
    // stbi_uc *pixels = stbi_load("test.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    // if (!pixels)
    // {
    //     printf("failed to load image\n");
    //     raise(SIGABRT);
    // }

    // DefaultTex.Create2D(texWidth * 4, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, pixels);

    // stbi_image_free(pixels);

    // //=== initialize descriptor set ===
    // // DefaultDS.AddStaticSampler(0, SamplerLinearWrap);
    // // vk::DescriptorSetLayoutBinding binding =
    // //{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr };
    // // DefaultDS.AddBinding(binding);
    // // DefaultDS.AddBindings(0, 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);

    // // vk::PushConstantRange pcRange;
    // // pcRange.size = sizeof(GlobalConstants);
    // // pcRange.offset = 0;
    // // pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
    // // DefaultDS.AddPushConstant(pcRange);

    // // DefaultDS.Finalize();

    // //=== initialize render pass===
    // //    DefaultRP.SetColorAttachment(g_SceneColorBuffer.GetFormat(), RenderPass::Clear);
    // //    DefaultRP.SetDepthAttachment(g_SceneDepthBuffer.GetFormat(), RenderPass::Clear);
    // //    DefaultRP.Finalize();
    // // DefaultRP.colors = {{g_SceneColorBuffer.GetFormat(), vk::AttachmentLoadOp::eClear}};
    // // DefaultRP.depth = {g_SceneDepthBuffer.GetFormat(), vk::AttachmentLoadOp::eClear};

    // //=== initialize pipeline ===
    // m_PSO.SetInputLayout(VertexInputBindingAttribute{// binding, stride, inputRate
    //                                                  {0, sizeof(Vertex), vk::VertexInputRate::eVertex},
    //                                                  {// location, binding, format, offset
    //                                                   {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
    //                                                   {3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, tex)}}});
    // m_PSO.SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleList);
    // m_PSO.SetRasterizerState(RasterizerDefault);
    // m_PSO.SetBlendState(BlendDisable);
    // m_PSO.SetDepthStencilState(DepthStateReadWrite);
    // m_PSO.SetPipelineLayout(m_DescriptorSet.GetPipelineLayout());
    // m_PSO.SetRenderPassFormat(g_SceneColorBuffer.GetFormat(), g_SceneDepthBuffer.GetFormat());
    // m_PSO.SetVertexShader(g_DefaultVert, sizeof(g_DefaultVert));
    // m_PSO.SetFragmentShader(g_DefaultFrag, sizeof(g_DefaultFrag));
    // m_PSO.Finalize();

    // g_SceneColorBuffer.CreateFramebuffer(DefaultRP);
    //    std::vector<PixelBuffer> fbAttachments = { g_SceneColorBuffer, g_SceneDepthBuffer };
    //    DefaultFB.Create(DefaultRP, fbAttachments);
}

void Renderer::Shutdown(void)
{
    // solve textureref destruct problem
    s_RadianceCubeMap = nullptr;
    s_IrradianceCubeMap = nullptr;

    TextureManager::Shutdown();

    m_DescriptorSet.Destroy();
    // DefaultDS.Destroy();
    // DefaultTex.Destroy();
    // indexBuffer.Destroy();
    // vertexBuffer.Destroy();
}

uint8_t Renderer::GetPSO(uint16_t psoFlags)
{
    using namespace PSOFlags;

    GraphicsPSO ColorPSO = m_DefaultPSO;

    uint16_t Requirements = kHasPosition | kHasNormal;
    ASSERT((psoFlags & Requirements) == Requirements);

    VertexInputBindingAttribute vertexLayout;
    uint32_t offset = 0;
    // location, binding, format, offset
    if (psoFlags & kHasPosition)
    {
        vertexLayout.second.push_back({0, 0, vk::Format::eR32G32B32Sfloat, offset});
        offset += vk::blockSize(vk::Format::eR32G32B32Sfloat);
    }
    if (psoFlags & kHasNormal)
    {
        vertexLayout.second.push_back({1, 0, vk::Format::eA2B10G10R10SnormPack32, offset});
        offset += vk::blockSize(vk::Format::eA2B10G10R10SnormPack32);
    }
    if (psoFlags & kHasTangent)
    {
        vertexLayout.second.push_back({2, 0, vk::Format::eA2B10G10R10SnormPack32, offset});
        offset += vk::blockSize(vk::Format::eA2B10G10R10SnormPack32);
    }
    if (psoFlags & kHasUV0)
    {
        vertexLayout.second.push_back({3, 0, vk::Format::eR16G16Sfloat, offset});
        offset += vk::blockSize(vk::Format::eR16G16Sfloat);
    }
    if (psoFlags & kHasUV1)
    {
        vertexLayout.second.push_back({4, 0, vk::Format::eR16G16Sfloat, offset});
        offset += vk::blockSize(vk::Format::eR16G16Sfloat);
    }
    if (psoFlags & kHasSkin)
    {
        vertexLayout.second.push_back({5, 0, vk::Format::eR16G16B16A16Uint, offset});
        offset += vk::blockSize(vk::Format::eR16G16B16A16Uint);
        vertexLayout.second.push_back({6, 0, vk::Format::eR16G16B16A16Unorm, offset});
        offset += vk::blockSize(vk::Format::eR16G16B16A16Unorm);
    }
    vertexLayout.first = {0, offset, vk::VertexInputRate::eVertex};

    std::vector<VertexInputBindingAttribute> vertexLayouts;
    vertexLayouts.push_back(vertexLayout);
    if (!(psoFlags & kHasUV0))
    {
        VertexInputBindingAttribute layout;
        layout.second.push_back({3, 1, vk::Format::eR16G16Sfloat, 0});
        layout.first = {1, vk::blockSize(vk::Format::eR16G16Sfloat), vk::VertexInputRate::eVertex};
        vertexLayouts.push_back(layout);
    }
    ColorPSO.SetInputLayout(vertexLayouts);

    if (psoFlags & kHasSkin)
    {
        if (psoFlags & kHasTangent)
        {
            if (psoFlags & kHasUV1)
            {
                ColorPSO.SetVertexShader(g_DefaultSkinVert, sizeof(g_DefaultSkinVert));
                ColorPSO.SetFragmentShader(g_DefaultFrag, sizeof(g_DefaultFrag));
            }
            else
            {
                ColorPSO.SetVertexShader(g_DefaultNoUV1SkinVert, sizeof(g_DefaultNoUV1SkinVert));
                ColorPSO.SetFragmentShader(g_DefaultNoUV1Frag, sizeof(g_DefaultNoUV1Frag));
            }
        }
        else
        {
            if (psoFlags & kHasUV1)
            {
                ColorPSO.SetVertexShader(g_DefaultNoTangentSkinVert, sizeof(g_DefaultNoTangentSkinVert));
                ColorPSO.SetFragmentShader(g_DefaultNoTangentFrag, sizeof(g_DefaultNoTangentFrag));
            }
            else
            {
                ColorPSO.SetVertexShader(g_DefaultNoTangentNoUV1SkinVert, sizeof(g_DefaultNoTangentNoUV1SkinVert));
                ColorPSO.SetFragmentShader(g_DefaultNoTangentNoUV1Frag, sizeof(g_DefaultNoTangentNoUV1Frag));
            }
        }
    }
    else
    {
        if (psoFlags & kHasTangent)
        {
            if (psoFlags & kHasUV1)
            {
                ColorPSO.SetVertexShader(g_DefaultVert, sizeof(g_DefaultVert));
                ColorPSO.SetFragmentShader(g_DefaultFrag, sizeof(g_DefaultFrag));
            }
            else
            {
                ColorPSO.SetVertexShader(g_DefaultNoUV1Vert, sizeof(g_DefaultNoUV1Vert));
                ColorPSO.SetFragmentShader(g_DefaultNoUV1Frag, sizeof(g_DefaultNoUV1Frag));
            }
        }
        else
        {
            if (psoFlags & kHasUV1)
            {
                ColorPSO.SetVertexShader(g_DefaultNoTangentVert, sizeof(g_DefaultNoTangentVert));
                ColorPSO.SetFragmentShader(g_DefaultNoTangentFrag, sizeof(g_DefaultNoTangentFrag));
            }
            else
            {
                ColorPSO.SetVertexShader(g_DefaultNoTangentNoUV1Vert, sizeof(g_DefaultNoTangentNoUV1Vert));
                ColorPSO.SetFragmentShader(g_DefaultNoTangentNoUV1Frag, sizeof(g_DefaultNoTangentNoUV1Frag));
            }
        }
    }

    if (psoFlags & kAlphaBlend)
    {
        ColorPSO.SetBlendState(BlendPreMultiplied);
        ColorPSO.SetDepthStencilState(DepthStateReadOnly);
    }
    if (psoFlags & kTwoSided)
    {
        ColorPSO.SetRasterizerState(RasterizerTwoSided);
    }
    ColorPSO.Finalize();

    // Look for an existing PSO
    for (uint32_t i = 0; i < sm_PSOs.size(); ++i)
    {
        if (ColorPSO.GetPipeline() == sm_PSOs[i].GetPipeline())
        {
            return (uint8_t)i;
        }
    }

    // If not found, keep the new one, and return its index
    sm_PSOs.push_back(ColorPSO);

    // The returned PSO index has read-write depth.  The index+1 tests for equal depth.
    ColorPSO.SetDepthStencilState(DepthStateTestEqual);
    ColorPSO.Finalize();

#ifdef DEBUG
    for (uint32_t i = 0; i < sm_PSOs.size(); ++i)
        ASSERT(ColorPSO.GetPipeline() == sm_PSOs[i].GetPipeline());
#endif
    sm_PSOs.push_back(ColorPSO);

    ASSERT(sm_PSOs.size() <= 256, "Ran out of room for unique PSOs");

    return (uint8_t)sm_PSOs.size() - 2;
}

void Renderer::SetIBLTextures(TextureRef diffuseIBL, TextureRef specularIBL)
{
    s_RadianceCubeMap = specularIBL;
    s_IrradianceCubeMap = diffuseIBL;

    s_SpecularIBLRange = 0.0f;
    if (s_RadianceCubeMap.IsValid())
    {
        auto tex = s_RadianceCubeMap.Get();
        s_SpecularIBLRange = std::max(0.0f, (float)tex->GetMipLevels() - 1);
        s_SpecularIBLBias = std::min(s_SpecularIBLBias, s_SpecularIBLRange);
    }
    m_CommonCubeTextures[0].imageView =
        specularIBL.IsValid() ? vk::ImageView(*specularIBL.Get()) : GetDefaultTexture(kColorCubeMap);
    m_CommonCubeTextures[1].imageView =
        diffuseIBL.IsValid() ? vk::ImageView(*diffuseIBL.Get()) : GetDefaultTexture(kColorCubeMap);
}

void Renderer::SetIBLBias(float LODBias) { s_SpecularIBLBias = std::min(LODBias, s_SpecularIBLRange); }

void Renderer::DrawSkybox(GraphicsContext &gfxContext, const Camera &camera, const vk::Viewport &viewport,
                          const vk::Rect2D &scissor)
{
    struct alignas(16) SkyboxVSCB
    {
        glm::mat4 ProjInverse;
        glm::mat4 ViewInverse;
    } skyVSCB;
    skyVSCB.ProjInverse = glm::inverse(camera.GetProjMatrix());
    skyVSCB.ViewInverse = glm::inverse(camera.GetViewMatrix());

    struct alignas(16) SkyboxFSCB
    {
        float TextureLevel;
    } skyFSCB;
    skyFSCB.TextureLevel = s_SpecularIBLBias;

    gfxContext.SetDescriptorSet(m_DescriptorSet);
    // gfxContext.BeginUpdateDescriptorSet();
    gfxContext.UpdateDynamicUniformBuffer(kMeshConstants, sizeof(skyVSCB), &skyVSCB);
    gfxContext.UpdateDynamicUniformBuffer(kMaterialConstants, sizeof(skyFSCB), &skyFSCB);
    gfxContext.UpdateImageSampler(kCommonCubeSamplers, 0, m_CommonCubeTextures);
    gfxContext.UpdateImageSampler(kCommon2DSamplers, 0, m_Common2DTextures);
    gfxContext.UpdateImageSampler(kCommonShadowSamplers, 0, m_CommonShadowTextures);
    // gfxContext.UpdateImageSampler(kCommonSamplers, 0, m_CommonTextures);
    // gfxContext.EndUpdateAndBindDescriptorSet();

    gfxContext.TransitionImageLayout(g_SceneDepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    gfxContext.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eColorAttachmentOptimal);

    gfxContext.BeginRenderPass(g_SceneColorBuffer, g_SceneDepthBuffer);
    gfxContext.BindPipeline(m_SkyboxPSO);
    gfxContext.SetViewportAndScissor(viewport, scissor);
    gfxContext.Draw(3);
    gfxContext.EndRenderPass();
}

// void Renderer::Render(GraphicsContext &gfxContext, const GlobalConstants &globals)
// {
//     gfxContext.SetDescriptorSet(m_DescriptorSet);
//     gfxContext.BeginUpdateDescriptorSet();
//     // gfxContext.UpdateImageSampler(kCommonSamplers, m_CommonTextures);
//     //  Set common shader constants
//     auto textures = m_TextureHeap[0];
//     textures[0] = std::make_pair(DefaultTex, SamplerLinearWrap);
//     gfxContext.UpdateImageSampler(kMaterialSamplers, textures);
//     MeshConstants meshConsts{glm::mat4(1.0), glm::mat4(1.0)};
//     gfxContext.UpdateDynamicUniformBuffer(kMeshConstants, sizeof(MeshConstants), &meshConsts);
//     gfxContext.UpdateDynamicUniformBuffer(kCommonConstants, sizeof(GlobalConstants), &globals);
//     gfxContext.EndUpdateAndBindDescriptorSet();

//     // DefaultDS.UpdateImageSampler(g_CurrentBuffer, 0, DefaultTex, SamplerLinearWrap);
//     // gfxContext.SetDescriptorSet(DefaultDS);
//     // gfxContext.BeginUpdateDescriptorSet();
//     // gfxContext.UpdateImageSampler(0, DefaultTex, SamplerLinearWrap);
//     // gfxContext.EndUpdateAndBindDescriptorSet();

//     gfxContext.TransitionImageLayout(DefaultTex, vk::ImageLayout::eShaderReadOnlyOptimal);
//     gfxContext.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eColorAttachmentOptimal);
//     gfxContext.TransitionImageLayout(g_SceneDepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);

//     gfxContext.BeginRenderPass(g_SceneColorBuffer, g_SceneDepthBuffer);
//     gfxContext.BindPipeline(m_PSO);
//     gfxContext.SetViewportAndScissor(0, 0, g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
//     // gfxContext.PushConstants(vk::ShaderStageFlagBits::eVertex, 0, globals);

//     gfxContext.BindVertexBuffer(0, vertexBuffer, 0);
//     gfxContext.BindIndexBuffer(indexBuffer, 0);

//     gfxContext.DrawIndexed(indexBuffer.GetCount(), 0);

//     gfxContext.EndRenderPass();
// }

void MeshSorter::AddMesh(const Mesh &mesh, float distance, const vk::DescriptorBufferInfo &meshUB,
                         const vk::DescriptorBufferInfo &materialUB, const GpuBuffer &bufferPtr, const Joint *skeleton)
{
    SortKey key;
    key.value = m_SortObjects.size(); // this is the ID of mesh

    // check mesh's flags
    bool alphaBlend = (mesh.psoFlags & PSOFlags::kAlphaBlend) == PSOFlags::kAlphaBlend;
    bool alphaTest = (mesh.psoFlags & PSOFlags::kAlphaTest) == PSOFlags::kAlphaTest;
    bool skinned = (mesh.psoFlags & PSOFlags::kHasSkin) == PSOFlags::kHasSkin;

    // 0=posOny, 1=posAndUV, 2=skinpos, 3= skinposAndUV
    // +4 for shadows
    uint64_t depthPSO = (skinned ? 2 : 0) + (alphaTest ? 1 : 0);

    // float follows IEEE754 standard SXXXXXXX XMMMMMMM MMMMMMMM MMMMMMMM,
    // Distance is always positive here. we can sort uint as we sort float.
    // Negate the uint makes distance ordered mirrored.
    // we sort to draw nearer objects first usually, but for transparent objects which are not occlusive,
    // we should draw opaque objects first, followed by further transparent ones.
    union float_or_int {
        float f;
        uint32_t u;
    } dist;
    dist.f = std::max(distance, 0.0f);

    if (m_BatchType == kShadows)
    {
        if (alphaBlend)
        {
            return;
        }

        key.passID = kZPass;
        key.psoIdx = depthPSO + 4;
        key.key = dist.u;
        m_SortKeys.push_back(key.value);
        m_PassCounts[kZPass]++;
    }
    else if (mesh.psoFlags & PSOFlags::kAlphaBlend)
    {
        key.passID = kTransparent;
        key.psoIdx = mesh.pso;
        key.key = ~dist.u; // mirror the distance to meet the transparent order requirement
        m_SortKeys.push_back(key.value);
        m_PassCounts[kTransparent]++;
    }
    else if (SeparateZPass || alphaTest)
    {
        key.passID = kZPass;
        key.psoIdx = depthPSO;
        key.key = dist.u;
        m_SortKeys.push_back(key.value);
        m_PassCounts[kZPass]++;

        key.passID = kOpaque;
        key.psoIdx = mesh.pso + 1;
        key.key = dist.u;
        m_SortKeys.push_back(key.value);
        m_PassCounts[kOpaque]++;
    }
    else
    {
        key.passID = kOpaque;
        key.psoIdx = mesh.pso;
        key.key = dist.u;
        m_SortKeys.push_back(key.value);
        m_PassCounts[kOpaque]++;
    }

    m_SortObjects.emplace_back(SortObject{&mesh, skeleton, meshUB, materialUB, bufferPtr});
}

void MeshSorter::Sort()
{
    // uint64_t can be directly sorted. why write a cmp function?
    std::sort(m_SortKeys.begin(), m_SortKeys.end());
}

void MeshSorter::RenderMeshes(DrawPass pass, GraphicsContext &context, GlobalConstants &globals)
{
    ASSERT(m_DepthBuffer != nullptr);

    // Update common textures, in case they are changed
    m_Common2DTextures[0].imageView = g_SSAOFullScreen;
    m_CommonShadowTextures[0].imageView = g_ShadowBuffer;

    context.SetDescriptorSet(m_DescriptorSet);
    // context.BeginUpdateDescriptorSet();
    context.UpdateImageSampler(kCommonCubeSamplers, 0, m_CommonCubeTextures);
    context.UpdateImageSampler(kCommon2DSamplers, 0, m_Common2DTextures);
    context.UpdateImageSampler(kCommonShadowSamplers, 0, m_CommonShadowTextures);
    // context.UpdateImageSampler(kCommonSamplers, 0, m_CommonTextures);
    // Set common shader constants
    globals.ViewProjMatrix = m_Camera->GetViewProjMatrix();
    globals.CameraPos = glm::vec4(m_Camera->GetPosition(), 0.0);
    globals.IBLRange = s_SpecularIBLRange - s_SpecularIBLBias;
    globals.IBLBias = s_SpecularIBLBias;
    context.UpdateDynamicUniformBuffer(kCommonConstants, sizeof(GlobalConstants), &globals);

    if (m_BatchType == kShadows)
    {
        context.ClearDepth(*m_DepthBuffer);

        if (m_Viewport.width == 0)
        {
            m_Viewport.x = 0.0f;
            m_Viewport.y = 0.0f;
            m_Viewport.width = (float)m_DepthBuffer->GetWidth();
            m_Viewport.height = (float)m_DepthBuffer->GetHeight();
            m_Viewport.maxDepth = 1.0f;
            m_Viewport.minDepth = 0.0f;

            m_Scissor.offset.x = 1;
            m_Scissor.offset.y = 1;
            m_Scissor.extent.width = m_DepthBuffer->GetWidth() - 2;
            m_Scissor.extent.height = m_DepthBuffer->GetHeight() - 2;
        }
    }
    else
    {
        // check for buffer sizes
        for (uint32_t i = 0; i < m_NumColorBuffers; ++i)
        {
            ASSERT(m_ColorBuffers[i] != nullptr);
            ASSERT(m_DepthBuffer->GetWidth() == m_ColorBuffers[i]->GetWidth());
            ASSERT(m_DepthBuffer->GetHeight() == m_ColorBuffers[i]->GetHeight());
        }

        if (m_Viewport.width == 0)
        {
            m_Viewport.x = 0.0f;
            m_Viewport.y = 0.0f;
            m_Viewport.width = (float)m_DepthBuffer->GetWidth();
            m_Viewport.height = (float)m_DepthBuffer->GetHeight();
            m_Viewport.maxDepth = 1.0f;
            m_Viewport.minDepth = 0.0f;

            m_Scissor.offset.x = 0;
            m_Scissor.offset.y = 0;
            m_Scissor.extent.width = m_DepthBuffer->GetWidth();
            m_Scissor.extent.height = m_DepthBuffer->GetHeight();
        }
    }

    for (; m_CurrentPass <= pass; m_CurrentPass = (DrawPass)(m_CurrentPass + 1))
    {
        const uint32_t passCount = m_PassCounts[m_CurrentPass];
        if (passCount == 0)
        {
            continue;
        }

        if (m_BatchType == kDefault)
        {
            switch (m_CurrentPass)
            {
            case kZPass:
                context.TransitionImageLayout(*m_DepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
                context.BeginRenderPass(*m_DepthBuffer);
                break;
            case kOpaque:
                // if (SeparateZPass)
                //{
                //     context.TransitionImageLayout(*m_DepthBuffer, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
                // }
                // else
                //{
                // }
                context.TransitionImageLayout(*m_DepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
                context.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eColorAttachmentOptimal);
                context.BeginRenderPass(g_SceneColorBuffer, *m_DepthBuffer);
                break;
            case kTransparent:
                context.TransitionImageLayout(*m_DepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
                context.TransitionImageLayout(g_SceneColorBuffer, vk::ImageLayout::eColorAttachmentOptimal);
                context.BeginRenderPass(g_SceneColorBuffer, *m_DepthBuffer);
                break;
            default:
                break;
            }
        }

        context.SetViewportAndScissor(m_Viewport, m_Scissor);

        const uint32_t lastDraw = m_CurrentDraw + passCount;

        while (m_CurrentDraw < lastDraw)
        {
            SortKey key;
            key.value = m_SortKeys[m_CurrentDraw];
            const SortObject &object = m_SortObjects[key.objectIdx];
            const Mesh &mesh = *object.mesh;

            context.UpdateUniformBuffer(kMeshConstants, object.meshUB);
            context.UpdateUniformBuffer(kMaterialConstants, object.materialUB);
            context.UpdateImageSampler(kMaterialSamplers, 0, m_TextureHeap[mesh.imageTable]);

            if (mesh.numJoints > 0)
            {
                // TODO: update storage buffer
            }
            // context.EndUpdateAndBindDescriptorSet();

            context.BindPipeline(sm_PSOs[key.psoIdx]);

            // bufferPtr consists of three parts one bye one: VB, VBDepth, IB
            if (m_CurrentPass == kZPass)
            {
                // alpha test needs uv coords, which needs 4 bytes
                // TODO: Didn't see skin matrices strides. doesn't need to process?
                //
                // seems vertex buffer don't care about the strides. GPU will read data according to input layout
                // bool alphaTest = (mesh.psoFlags & PSOFlags::kAlphaTest) == PSOFlags::kAlphaTest;
                // uint32_t stride = alphaTest ? 16u : 12u;
                // if (mesh.numJoints > 0)
                //    stride += 16;
                VertexBuffer buffer(object.bufferPtr);
                context.BindVertexBuffer(0, buffer, mesh.vbDepthOffset);
            }
            else
            {
                VertexBuffer buffer(object.bufferPtr);
                context.BindVertexBuffer(0, buffer, mesh.vbOffset);
            }

            IndexBuffer ib(object.bufferPtr, (vk::IndexType)mesh.ibFormat);
            context.BindIndexBuffer(ib, mesh.ibOffset);

            for (uint32_t i = 0; i < mesh.numDraws; ++i)
            {
                context.DrawIndexed(mesh.draw[i].primCount, mesh.draw[i].startIndex, mesh.draw[i].baseVertex);
            }

            ++m_CurrentDraw;
        }
        context.EndRenderPass();
    }

    // transition depth buffer to depth attachment? necessary?
}
