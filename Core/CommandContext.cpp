#include "CommandContext.h"

#include "DepthBuffer.h"
#include "Framebuffer.h"
#include "GraphicsCore.h"
#include "RenderPass.h"
#include "Utility.h"
#include <algorithm>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

using namespace Graphics;

#define DS_POOL_SIZE 100

CommandContext *ContextManager::AllocateContext(vk::QueueFlagBits type)
{
    std::lock_guard<std::mutex> LockGuard(sm_ContextAllocationMutex);

    int index = 0;
    switch (type)
    {
    case vk::QueueFlagBits::eGraphics:
        index = 0;
        break;
    case vk::QueueFlagBits::eCompute:
        index = 1;
        break;
    case vk::QueueFlagBits::eTransfer:
        index = 2;
        break;
    }

    auto &AvailableContexts = sm_AvailableContexts[index];

    CommandContext *ret = nullptr;
    if (AvailableContexts.empty())
    {
        ret = new CommandContext(type);
        sm_ContextPool[index].emplace_back(ret);
        ret->Initialize();
    }
    else
    {
        // here we should also check if command buffer is completed
        // otherwise begincommandbuffer error
        auto iter = std::find_if(AvailableContexts.begin(), AvailableContexts.end(), [](CommandContext *c) {
            vk::Result result = g_Device.getFenceStatus(c->m_Fence);
            return result == vk::Result::eSuccess;
        });
        if (iter == AvailableContexts.end())
        {
            // fence not ready, create a new one
            ret = new CommandContext(type);
            sm_ContextPool[index].emplace_back(ret);
            ret->Initialize();
        }
        else
        {
            ret = *iter;
            // we clear fence state and ready for next draw
            g_Device.resetFences(ret->m_Fence);
            ret->Reset();
            AvailableContexts.erase(iter);
        }
    }
    ASSERT(ret != nullptr);

    ASSERT(ret->m_Type == type);

    // begin commandbuffer every new context created
    vk::CommandBufferBeginInfo info;
    info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
    ret->m_CommandBuffer.begin(info);

    //    printf("contexts: %zu\n", sm_ContextPool[index].size());

    return ret;
}

void ContextManager::FreeContext(CommandContext *UsedContext)
{
    ASSERT(UsedContext != nullptr);
    std::lock_guard<std::mutex> LockGuard(sm_ContextAllocationMutex);

    int index = 0;
    switch (UsedContext->m_Type)
    {
    case vk::QueueFlagBits::eGraphics:
        index = 0;
        break;
    case vk::QueueFlagBits::eCompute:
        index = 1;
        break;
    case vk::QueueFlagBits::eTransfer:
        index = 2;
        break;
    default:
        index = -1;
        break;
    }
    assert(index >= 0);

    sm_AvailableContexts[index].emplace_back(UsedContext);
}

void ContextManager::DestroyAllContexts()
{
    for (uint32_t i = 0; i < 3; ++i)
        sm_ContextPool[i].clear();
}

CommandContext::CommandContext(vk::QueueFlagBits type)
    : m_Type(type), m_CpuLinearAllocator(kCpuWritable), m_GpuLinearAllocator(kGpuExclusive), m_DsPool(DS_POOL_SIZE),
      m_DynamicImageSamplerHeap(*this, vk::DescriptorType::eCombinedImageSampler),
      m_DynamicUniformBufferHeap(*this, vk::DescriptorType::eUniformBuffer),
      m_DynamicStorageImageHeap(*this, vk::DescriptorType::eStorageImage)
{
    vk::FenceCreateInfo fenceInfo;
    // fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    m_Fence = g_Device.createFence(fenceInfo);
}

void CommandContext::Reset()
{
    m_CpuLinearAllocator.Cleanup();
    m_GpuLinearAllocator.Cleanup();
    m_DsPool.Cleanup();
}

CommandContext::~CommandContext(void)
{
    Reset();
    g_Device.destroyFence(m_Fence);
}

CommandContext &CommandContext::Begin(const std::string &ID)
{
    CommandContext *NewContext = g_ContextManager.AllocateContext(vk::QueueFlagBits::eGraphics);
    NewContext->SetID(ID);
    return *NewContext;
}

void CommandContext::DestroyAllContexts(void) { g_ContextManager.DestroyAllContexts(); }

void CommandContext::Finish(bool WaitForCompletion)
{
    ASSERT(m_Type == vk::QueueFlagBits::eGraphics || m_Type == vk::QueueFlagBits::eCompute);

    m_CommandBuffer.end();

    CommandQueue &Queue = g_CommandManager.GetQueue(m_Type);
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTransfer;
    vk::SubmitInfo submitInfo;
    submitInfo.setPCommandBuffers(&m_CommandBuffer);
    submitInfo.commandBufferCount = 1;
    // if (m_Sem_enabled)
    //{
    //     submitInfo.setWaitSemaphoreCount(1);
    //     submitInfo.setWaitSemaphores(m_Semaphore);
    //     submitInfo.setWaitDstStageMask(waitStage);
    // }
    // submitInfo.setSignalSemaphoreCount(1);
    // submitInfo.setSignalSemaphores(m_Semaphore);
    // m_Sem_enabled = true;
    //    if (waitSemaphore)
    //    {
    //        submitInfo.setWaitSemaphoreCount(1);
    //        submitInfo.setWaitSemaphores(waitSemaphore);
    //        submitInfo.setWaitDstStageMask(waitStage);
    //    }
    //    if (signalSemaphore)
    //    {
    //        submitInfo.setSignalSemaphoreCount(1);
    //        submitInfo.setSignalSemaphores(signalSemaphore);
    //    }

    Queue.Submit(submitInfo, m_Fence);

    if (WaitForCompletion)
    {
        Queue.WaitForFence(m_Fence);
    }

    vk::Result result = g_Device.getFenceStatus(m_Fence);

    g_ContextManager.FreeContext(this);
}

void CommandContext::Initialize(void) { m_CommandBuffer = g_CommandManager.CreateNewCommandBuffer(m_Type); }

void CommandContext::InitializeBuffer(GpuBuffer &dst, const StagingBuffer &src, size_t srcOffset)
{
    auto &initContext = CommandContext::Begin();

    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = 0;
    copyRegion.size = std::min(dst.GetBufferSize(), src.GetBufferSize());

    initContext.m_CommandBuffer.copyBuffer(src.m_Buffer, dst.m_Buffer, copyRegion);

    initContext.Finish(true);
}

void CommandContext::InitializeImage(ImageView &dst, const StagingBuffer &src,
                                     const vk::ArrayProxy<vk::BufferImageCopy> &regions)
{
    auto &initContext = CommandContext::Begin();

    initContext.TransitionImageLayout(dst, vk::ImageLayout::eTransferDstOptimal);

    initContext.m_CommandBuffer.copyBufferToImage(src.m_Buffer, dst.m_Image, vk::ImageLayout::eTransferDstOptimal,
                                                  regions);

    initContext.TransitionImageLayout(dst, vk::ImageLayout::eShaderReadOnlyOptimal);
    initContext.Finish(true);
}

void CommandContext::TransitionImageLayout(ImageView &img, vk::ImageLayout newLayout)
{
    if (img.m_Layout == newLayout)
    {
        return;
    }
    vk::PipelineStageFlags srcStage, dstStage;
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = img.m_Layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img.m_Image;
    barrier.subresourceRange = img.m_SubresourceRange;
    //    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    //    {
    //        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    //    }
    //    else
    //    {
    //        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    //    }
    //    barrier.subresourceRange.baseMipLevel = 0;
    //    barrier.subresourceRange.levelCount = 1;
    //    barrier.subresourceRange.baseArrayLayer = 0;
    //    barrier.subresourceRange.layerCount = 1;

    auto Select = [](vk::ImageLayout layout, vk::AccessFlags &mask, vk::PipelineStageFlags &stage) {
        switch (layout)
        {
        case vk::ImageLayout::eUndefined:
            [[fallthrough]];
        case vk::ImageLayout::ePreinitialized:
            mask = vk::AccessFlagBits::eNone;
            stage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            mask = vk::AccessFlagBits::eShaderRead;
            stage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            mask = vk::AccessFlagBits::eTransferWrite;
            stage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eColorAttachmentOptimal:
            [[fallthrough]];
        case vk::ImageLayout::ePresentSrcKHR:
            mask = vk::AccessFlagBits::eColorAttachmentWrite;
            stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            mask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            mask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
            stage = vk::PipelineStageFlagBits::eComputeShader;
            break;
        default:
            printf("Current image layout not suppported!\n");
            break;
        }
    };

    Select(img.m_Layout, barrier.srcAccessMask, srcStage);
    Select(newLayout, barrier.dstAccessMask, dstStage);

    m_CommandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);

    img.m_Layout = newLayout;
}

void CommandContext::SetDescriptorSet(const DescriptorSet &ds)
{
    m_CurrLayout = ds.GetLayout();
    m_CurrPipelineLayout = ds.GetPipelineLayout();
    m_DynamicImageSamplerHeap.Cleanup();
    m_DynamicUniformBufferHeap.Cleanup();
    m_DynamicStorageImageHeap.Cleanup();
}

// void CommandContext::BeginUpdateDescriptorSet() { m_CurrSet = m_DsPool.NewDescriptorSet(m_CurrLayout); }

// void CommandContext::EndUpdateAndBindDescriptorSet()
// {
//     m_CommandBuffer.bindDescriptorSets(m_CurrBindPoint, m_CurrPipelineLayout, 0, m_CurrSet, {});
// }

void CommandContext::UpdateUniformBuffer(uint32_t binding, const vk::DescriptorBufferInfo &buffer)
{
    // vk::WriteDescriptorSet write;
    // write.dstSet = m_CurrSet;
    // write.dstBinding = binding;
    // write.dstArrayElement = 0;
    // write.descriptorType = vk::DescriptorType::eUniformBuffer;
    // write.descriptorCount = 1;
    // write.setBufferInfo(info);
    // g_Device.updateDescriptorSets(write, {});
    m_DynamicUniformBufferHeap.SetDescriptorInfo(binding, buffer);
}

void CommandContext::UpdateDynamicUniformBuffer(uint32_t Binding, size_t DataSize, const void *Data)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = DataSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Buffer buffer = g_Device.createBuffer(bufferInfo);
    auto allocInfo = m_CpuLinearAllocator.AllocAndBind(buffer);
    memcpy(allocInfo.pMappedData, Data, DataSize);
    // m_DynamicBuffers.push_back(buffer);

    vk::DescriptorBufferInfo info;
    info.buffer = buffer;
    info.offset = 0;
    info.range = DataSize;

    // vk::WriteDescriptorSet write;
    // write.dstSet = m_CurrSet;
    // write.dstBinding = Binding;
    // write.dstArrayElement = 0;
    // write.descriptorType = vk::DescriptorType::eUniformBuffer;
    // write.descriptorCount = 1;
    // write.setBufferInfo(info);
    // g_Device.updateDescriptorSets(write, {});
    m_DynamicUniformBufferHeap.SetDescriptorInfo(Binding, info);
}

void CommandContext::UpdateImageSampler(uint32_t binding, const vk::ImageView &imageview, const vk::Sampler &sampler)
{
    std::vector<vk::DescriptorImageInfo> infos{{sampler, imageview, vk::ImageLayout::eShaderReadOnlyOptimal}};

    // vk::WriteDescriptorSet write;
    // write.dstSet = m_CurrSet;
    // write.dstBinding = binding;
    // write.dstArrayElement = 0;
    // write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    // write.descriptorCount = 1;
    // write.setImageInfo(info);
    // g_Device.updateDescriptorSets(write, {});
    m_DynamicImageSamplerHeap.SetDescriptorInfo(binding, 0, infos);
}

void CommandContext::UpdateImageSampler(uint32_t binding, uint32_t firstIndex,
                                        const vk::ArrayProxy<vk::DescriptorImageInfo> &imageSamplers)
{
    std::vector<vk::DescriptorImageInfo> infos{imageSamplers.begin(), imageSamplers.end()};

    // vk::WriteDescriptorSet write;
    // write.dstSet = m_CurrSet;
    // write.dstBinding = binding;
    // write.dstArrayElement = 0;
    // write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    // write.descriptorCount = infos.size();
    // write.setImageInfo(infos);
    // g_Device.updateDescriptorSets(write, {});
    m_DynamicImageSamplerHeap.SetDescriptorInfo(binding, firstIndex, infos);
}
void CommandContext::UpdateStorageImage(uint32_t binding, const vk::ImageView &image)
{
    std::vector<vk::DescriptorImageInfo> infos{{{}, image, vk::ImageLayout::eGeneral}};
    m_DynamicStorageImageHeap.SetDescriptorInfo(binding, 0, infos);
}
void CommandContext::UpdateStorageImage(uint32_t binding, uint32_t firstIndex,
                                        const vk::ArrayProxy<vk::DescriptorImageInfo> &image)
{
    std::vector<vk::DescriptorImageInfo> infos{image.begin(), image.end()};
    // vk::DescriptorImageInfo info;
    // info.imageLayout = vk::ImageLayout::eGeneral;
    // info.imageView = imageview;

    // vk::WriteDescriptorSet write;
    // write.dstSet = m_CurrSet;
    // write.dstBinding = binding;
    // write.dstArrayElement = 0;
    // write.descriptorType = vk::DescriptorType::eStorageImage;
    // write.descriptorCount = 1;
    // write.setImageInfo(info);
    // g_Device.updateDescriptorSets(write, {});
    m_DynamicStorageImageHeap.SetDescriptorInfo(binding, firstIndex, infos);
}

void CommandContext::PushConstantBuffer(vk::ShaderStageFlags Stage, size_t Offset, size_t Size, const void *Data)
{
    m_CommandBuffer.pushConstants(m_CurrPipelineLayout, Stage, Offset, Size, Data);
}

void CommandContext::InsertTimestamp(vk::PipelineStageFlagBits stage, const vk::QueryPool &pool, uint32_t query)
{
    m_CommandBuffer.writeTimestamp(stage, pool, query);
}

void GraphicsContext::BindPipeline(const PSO &pipeline)
{
    m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.GetPipeline());
}

void GraphicsContext::BeginRenderPass(const vk::ArrayProxy<PixelBuffer> &colors)
{
    std::vector<vk::Format> colorFormats;
    for (auto &c : colors)
    {
        colorFormats.push_back(c.GetFormat());
    }
    vk::RenderPass renderPass = RenderPass::GetRenderPass(colorFormats, vk::Format::eUndefined);
    Framebuffer framebuffer = g_FramebufferManager.GetFramebuffer(renderPass, colors);

    vk::RenderPassBeginInfo info;
    info.renderPass = renderPass;
    info.framebuffer = framebuffer;
    info.renderArea.setOffset({0, 0});
    info.renderArea.extent.width = framebuffer.GetWidth();
    info.renderArea.extent.height = framebuffer.GetHeight();

    // auto clearValues = framebuffer.GetClearValues();
    // info.setClearValues(clearValues);
    // info.clearValueCount = clearValues.size();

    m_CommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);
}
void GraphicsContext::BeginRenderPass(const vk::ArrayProxy<PixelBuffer> &colors, const PixelBuffer &depth)
{
    std::vector<vk::Format> colorFormats;
    for (auto &c : colors)
    {
        colorFormats.push_back(c.GetFormat());
    }
    vk::RenderPass renderPass = RenderPass::GetRenderPass(colorFormats, depth.GetFormat());
    std::vector<PixelBuffer> buffers;
    for (auto &i : colors)
    {
        buffers.push_back(i);
    }
    buffers.push_back(depth);
    Framebuffer framebuffer = g_FramebufferManager.GetFramebuffer(renderPass, buffers);

    vk::RenderPassBeginInfo info;
    info.renderPass = renderPass;
    info.framebuffer = framebuffer;
    info.renderArea.setOffset({0, 0});
    info.renderArea.extent.width = framebuffer.GetWidth();
    info.renderArea.extent.height = framebuffer.GetHeight();

    // auto clearValues = framebuffer.GetClearValues();
    // info.setClearValues(clearValues);
    // info.clearValueCount = clearValues.size();

    m_CommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);
}

void GraphicsContext::EndRenderPass() { m_CommandBuffer.endRenderPass(); }

void GraphicsContext::ClearColor(ColorBuffer &Target)
{
    TransitionImageLayout(Target, vk::ImageLayout::eGeneral);

    vk::ImageSubresourceRange range;
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    m_CommandBuffer.clearColorImage(Target.GetImage(), vk::ImageLayout::eGeneral, Target.GetClearColor(), range);
}

void GraphicsContext::ClearDepth(DepthBuffer &Target)
{
    TransitionImageLayout(Target, vk::ImageLayout::eGeneral);

    vk::ImageSubresourceRange range;
    range.aspectMask = vk::ImageAspectFlagBits::eDepth;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    m_CommandBuffer.clearDepthStencilImage(Target.GetImage(), vk::ImageLayout::eGeneral, Target.GetClearValue(), range);
}

void GraphicsContext::SetScissor(unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
{
    vk::Rect2D scissor;
    scissor.offset.x = left;
    scissor.offset.y = top;
    scissor.extent.width = right - left;
    scissor.extent.height = bottom - top;
    m_CommandBuffer.setScissor(0, scissor);
}

void GraphicsContext::SetViewportAndScissor(int x, int y, unsigned int width, unsigned int height)
{
    // Caution: flip vulkan's NDC y coordinate by setting
    // y = y + height, height = -height
    vk::Viewport viewport;
    viewport.x = (float)x;
    viewport.y = (float)(y + height);
    viewport.width = (float)width;
    viewport.height = -(float)height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vk::Rect2D scissor;
    scissor.setOffset({x, y});
    scissor.setExtent({width, height});

    m_CommandBuffer.setViewport(0, viewport);
    m_CommandBuffer.setScissor(0, scissor);
}

void GraphicsContext::SetViewportAndScissor(const vk::Viewport &Viewport, const vk::Rect2D &Scissor)
{
    // Caution: flip vulkan's NDC y coordinate by setting
    // y = y + height, height = -height
    vk::Viewport viewport = Viewport;
    viewport.y = viewport.y + viewport.height;
    viewport.height = -viewport.height;
    m_CommandBuffer.setViewport(0, viewport);
    m_CommandBuffer.setScissor(0, Scissor);
}

void GraphicsContext::SetPrimitiveTopology(vk::PrimitiveTopology topology)
{
    m_CommandBuffer.setPrimitiveTopology(topology);
}

// void GraphicsContext::BindDesriptorSet(const vk::DescriptorSet& set, const vk::PipelineLayout& layout)
//{
//     m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, set, {});
// }

void GraphicsContext::BindVertexBuffer(uint32_t Binding, const VertexBuffer &buffer, size_t offset)
{
    m_CommandBuffer.bindVertexBuffers(Binding, buffer.GetBuffer(), offset);
}

void GraphicsContext::BindIndexBuffer(const IndexBuffer &buffer, size_t offset)
{
    m_CommandBuffer.bindIndexBuffer(buffer.GetBuffer(), offset, buffer.GetIndexType());
}

void GraphicsContext::BindDynamicVertexBuffer(uint32_t Binding, size_t DataSize, const void *VBData)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = DataSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    //| vk::BufferUsageFlagBits::eIndexBuffer
    //| vk::BufferUsageFlagBits::eTransferDst;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Buffer buffer = g_Device.createBuffer(bufferInfo);
    auto allocInfo = m_CpuLinearAllocator.AllocAndBind(buffer);
    memcpy(allocInfo.pMappedData, VBData, DataSize);
    // m_DynamicBuffers.push_back(buffer);

    m_CommandBuffer.bindVertexBuffers(Binding, buffer, {0});
}

void ComputeContext::BindPipeline(const PSO &pipeline)
{
    m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.GetPipeline());
}