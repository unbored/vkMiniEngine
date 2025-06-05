#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "UniformBuffers.h"
#include <Camera.h>
#include <CommandContext.h>
#include <EngineTuning.h>
#include <GpuBuffer.h>
#include <PipelineState.h>
#include <TextureManager.h>
#include <vulkan/vulkan_structs.hpp>

class GraphicsPSO;
class ShadowCamera;
class ShadowBuffer;
struct GlobalConstants;
struct Mesh;
struct Joint;

namespace Renderer
{
using namespace Math;

extern BoolVar SeparateZPass;

extern GraphicsPSO m_PSO;

extern std::vector<std::vector<vk::DescriptorImageInfo>> m_TextureHeap;
extern std::vector<vk::DescriptorImageInfo> m_CommonCubeTextures;
extern std::vector<vk::DescriptorImageInfo> m_Common2DTextures;
extern std::vector<vk::DescriptorImageInfo> m_CommonShadowTextures;
// extern std::vector<vk::DescriptorImageInfo> m_CommonTextures;

enum DescriptorBindings
{
    kMeshConstants,
    kMaterialConstants,
    kMaterialSamplers,
    kCommonCubeSamplers,
    kCommon2DSamplers,
    kCommonShadowSamplers,
    // kCommonSamplers,
    kCommonConstants,
    kSkinMatrices,

    kNumDescriptorBindings
};

void Initialize();
void Shutdown(void);

uint8_t GetPSO(uint16_t psoFlags);
void SetIBLTextures(TextureRef diffuseIBL, TextureRef specularIBL);
void SetIBLBias(float LODBias);
void DrawSkybox(GraphicsContext &gfxContext, const Camera &camera, const vk::Viewport &viewport,
                const vk::Rect2D &scissor);

void Render(GraphicsContext &gfxContext, const GlobalConstants &globals);

class MeshSorter
{
public:
    enum BatchType
    {
        kDefault,
        kShadows
    };
    enum DrawPass
    {
        kZPass,
        kOpaque,
        kTransparent,
        kNumPasses
    };

    MeshSorter(BatchType type)
    {
        m_BatchType = type;
        m_Camera = nullptr;
        m_NumColorBuffers = 0;
        m_DepthBuffer = nullptr;
        m_SortObjects.clear();
        m_SortKeys.clear();
        std::memset(m_PassCounts, 0, sizeof(m_PassCounts));
        m_CurrentPass = kZPass;
        m_CurrentDraw = 0;
    }

    void SetCamera(const BaseCamera &camera) { m_Camera = &camera; }
    void SetViewport(const vk::Viewport &viewport) { m_Viewport = viewport; }
    void SetScissor(const vk::Rect2D &scissor) { m_Scissor = scissor; }
    void AddColorBuffer(ColorBuffer &color)
    {
        ASSERT(m_NumColorBuffers < 8);
        m_ColorBuffers[m_NumColorBuffers++] = &color;
    }
    void SetDepthBuffer(DepthBuffer &depth) { m_DepthBuffer = &depth; }

    const Frustum &GetWorldFrustum() const { return m_Camera->GetWorldSpaceFrustum(); }
    const Frustum &GetViewFrustum() const { return m_Camera->GetViewSpaceFrustum(); }
    const glm::mat4 &GetViewMatrix() const { return m_Camera->GetViewMatrix(); }

    void AddMesh(const Mesh &mesh, float distance, const vk::DescriptorBufferInfo &meshUB,
                 const vk::DescriptorBufferInfo &materialUB, const GpuBuffer &bufferPtr,
                 const Joint *skeleton = nullptr);

    void Sort();

    void RenderMeshes(DrawPass pass, GraphicsContext &context, GlobalConstants &globals);

private:
    struct SortKey
    {
        union {
            uint64_t value;
            struct
            {
                uint64_t objectIdx : 16;
                uint64_t psoIdx : 12;
                uint64_t key : 32;
                uint64_t passID : 4;
            };
        };
    };
    struct SortObject
    {
        const Mesh *mesh;
        const Joint *skeleton;
        vk::DescriptorBufferInfo meshUB;
        vk::DescriptorBufferInfo materialUB;
        GpuBuffer bufferPtr;
    };

    std::vector<SortObject> m_SortObjects;
    std::vector<uint64_t> m_SortKeys;
    BatchType m_BatchType;
    uint32_t m_PassCounts[kNumPasses];
    DrawPass m_CurrentPass;
    uint32_t m_CurrentDraw;

    const BaseCamera *m_Camera;
    vk::Viewport m_Viewport;
    vk::Rect2D m_Scissor;
    uint32_t m_NumColorBuffers;
    ColorBuffer *m_ColorBuffers[8];
    DepthBuffer *m_DepthBuffer;
};
} // namespace Renderer
