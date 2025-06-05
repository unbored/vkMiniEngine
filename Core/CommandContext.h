#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "ColorBuffer.h"
#include "CommandBufferManager.h"
#include "DepthBuffer.h"
#include "DescriptorSet.h"
#include "DynamicDescriptorHeap.h"
#include "GpuBuffer.h"
#include "LinearAllocator.h"
#include "Math/Common.h"
#include "PipelineState.h"
#include "Utility.h"

#include <list>
#include <mutex>
#include <vector>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

class CommandContext;
class GraphicsContext;
class ComputeContext;
class ImageView;
// class RenderPass;

class ContextManager
{
public:
    ContextManager(void) {}

    CommandContext *AllocateContext(vk::QueueFlagBits type);
    void FreeContext(CommandContext *UsedContext);
    void DestroyAllContexts();

private:
    std::vector<std::unique_ptr<CommandContext>> sm_ContextPool[3];
    std::list<CommandContext *> sm_AvailableContexts[3];
    std::mutex sm_ContextAllocationMutex;
};

struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
};

// using ImageSampler = std::pair<vk::ImageView, vk::Sampler>;

class CommandContext : NonCopyable
{
    friend ContextManager;

public:
    ~CommandContext(void);

    static CommandContext &Begin(const std::string &ID = "");

    static void DestroyAllContexts(void);

    // Flush existing commands and release the current context
    // const vk::Semaphore& waitSemaphore = {}, vk::Semaphore signalSemaphore = {}
    void Finish(bool WaitForCompletion = false);

    void Initialize(void);

    GraphicsContext &GetGraphicsContext()
    {
        ASSERT(m_Type != vk::QueueFlagBits::eCompute, "Cannot convert async compute context to graphics");
        // m_CurrBindPoint = vk::PipelineBindPoint::eGraphics;
        return reinterpret_cast<GraphicsContext &>(*this);
    }
    ComputeContext &GetComputeContext()
    {
        // m_CurrBindPoint = vk::PipelineBindPoint::eCompute;
        return reinterpret_cast<ComputeContext &>(*this);
    }

    void SetDescriptorSet(const DescriptorSet &ds);
    // void BeginUpdateDescriptorSet();
    // void EndUpdateAndBindDescriptorSet();

    void UpdateUniformBuffer(uint32_t binding, const vk::DescriptorBufferInfo &buffer);
    void UpdateDynamicUniformBuffer(uint32_t Binding, size_t DataSize, const void *Data);
    void UpdateImageSampler(uint32_t binding, const vk::ImageView &imageview, const vk::Sampler &sampler);
    void UpdateImageSampler(uint32_t binding, uint32_t firstIndex,
                            const vk::ArrayProxy<vk::DescriptorImageInfo> &imageSamplers);
    void UpdateStorageImage(uint32_t binding, const vk::ImageView &image);
    void UpdateStorageImage(uint32_t binding, uint32_t firstIndex,
                            const vk::ArrayProxy<vk::DescriptorImageInfo> &image);

    void PushConstantBuffer(vk::ShaderStageFlags Stage, size_t Offset, size_t Size, const void *Data);

    void PushConstants(vk::ShaderStageFlags Stage, size_t Offset) {}
    template <typename T, typename... Rest>
    void PushConstants(vk::ShaderStageFlags Stage, size_t Offset, const T &Data, Rest... rest)
    {
        PushConstantBuffer(Stage, Offset, sizeof(Data), &Data);
        PushConstants(Stage, Offset + sizeof(T), rest...);
    }

    static void InitializeBuffer(GpuBuffer &dst, const StagingBuffer &src, size_t srcOffset);
    static void InitializeImage(ImageView &dst, const StagingBuffer &src,
                                const vk::ArrayProxy<vk::BufferImageCopy> &regions);

    void TransitionImageLayout(ImageView &img, vk::ImageLayout newLayout);

    void InsertTimestamp(vk::PipelineStageFlagBits stage, const vk::QueryPool &pool, uint32_t query);

protected:
    CommandContext(vk::QueueFlagBits type);

    void Reset();

    vk::QueueFlagBits m_Type;
    vk::CommandBuffer m_CommandBuffer;

    // fence for summitting
    vk::Fence m_Fence;

    // for dynamic buffers
    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;
    // std::vector<vk::Buffer> m_DynamicBuffers;

    // for descriptor sets
    DescriptorPool m_DsPool;
    vk::DescriptorSetLayout m_CurrLayout;
    // vk::DescriptorSet m_CurrSet;
    vk::PipelineLayout m_CurrPipelineLayout;

    DynamicDescriptorHeap m_DynamicImageSamplerHeap;
    DynamicDescriptorHeap m_DynamicUniformBufferHeap;
    DynamicDescriptorHeap m_DynamicStorageImageHeap;

    // vk::PipelineBindPoint m_CurrBindPoint;

    std::string m_ID;
    void SetID(const std::string &ID) { m_ID = ID; }
};

class GraphicsContext : public CommandContext
{
    friend ContextManager;

public:
    static GraphicsContext &Begin(const std::string &ID = "") { return CommandContext::Begin(ID).GetGraphicsContext(); }

    void BindPipeline(const PSO &pipeline);
    void BeginRenderPass(const vk::ArrayProxy<PixelBuffer> &colors);
    void BeginRenderPass(const vk::ArrayProxy<PixelBuffer> &colors, const PixelBuffer &depth);
    void EndRenderPass();

    void ClearColor(ColorBuffer &Target);
    void ClearDepth(DepthBuffer &Target);

    void SetScissor(unsigned int left, unsigned int top, unsigned int right, unsigned int bottom);
    void SetViewportAndScissor(int x, int y, unsigned int width, unsigned int height);
    void SetViewportAndScissor(const vk::Viewport &Viewport, const vk::Rect2D &Scissor);

    void SetPrimitiveTopology(vk::PrimitiveTopology topology);
    // void BindDesriptorSet(const vk::DescriptorSet& set, const vk::PipelineLayout& layout);

    void BindVertexBuffer(uint32_t Binding, const VertexBuffer &buffer, size_t offset = 0);
    void BindIndexBuffer(const IndexBuffer &buffer, size_t offset = 0);

    void BindDynamicVertexBuffer(uint32_t Binding, size_t DataSize, const void *VBData);

    inline void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t baseVertex = 0)
    {
        auto set = m_DsPool.NewDescriptorSet(m_CurrLayout);
        m_DynamicImageSamplerHeap.CommitDescriptorSet(set);
        m_DynamicUniformBufferHeap.CommitDescriptorSet(set);
        m_DynamicStorageImageHeap.CommitDescriptorSet(set);
        m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_CurrPipelineLayout, 0, set, {});

        m_CommandBuffer.drawIndexed(indexCount, 1, firstIndex, baseVertex, 0);
    }
    inline void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                              uint32_t firstInstance)
    {
        auto set = m_DsPool.NewDescriptorSet(m_CurrLayout);
        m_DynamicImageSamplerHeap.CommitDescriptorSet(set);
        m_DynamicUniformBufferHeap.CommitDescriptorSet(set);
        m_DynamicStorageImageHeap.CommitDescriptorSet(set);
        m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_CurrPipelineLayout, 0, set, {});

        m_CommandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    inline void Draw(uint32_t vertexCount, uint32_t vertexOffset = 0)
    {
        DrawInstanced(vertexCount, 1, vertexOffset, 0);
    }
};

class ComputeContext : public CommandContext
{
    friend ContextManager;

public:
    static ComputeContext &Begin(const std::string &ID = "") { return CommandContext::Begin(ID).GetComputeContext(); }

    void BindPipeline(const PSO &pipeline);

    inline void Dispatch2D(size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX, size_t GroupSizeY)
    {
        auto set = m_DsPool.NewDescriptorSet(m_CurrLayout);
        m_DynamicImageSamplerHeap.CommitDescriptorSet(set);
        m_DynamicUniformBufferHeap.CommitDescriptorSet(set);
        m_DynamicStorageImageHeap.CommitDescriptorSet(set);
        m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_CurrPipelineLayout, 0, set, {});

        m_CommandBuffer.dispatch(Math::DivideByMultiple(ThreadCountX, GroupSizeX),
                                 Math::DivideByMultiple(ThreadCountY, GroupSizeY), 1);
    }
};
