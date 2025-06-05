#include "CommandBufferManager.h"

CommandQueue::CommandQueue(vk::QueueFlagBits type) :
    m_QueueFamilyIndex(-1),
    m_Type(type)
{
}

CommandQueue::~CommandQueue()
{
    Shutdown();
}

void CommandQueue::Create(const vk::Device& device, int queueFamilyIndex)
{
    m_Device = device;
    m_QueueFamilyIndex = queueFamilyIndex;
    m_CommandQueue = device.getQueue(queueFamilyIndex, 0);

    vk::CommandPoolCreateInfo createInfo;
    createInfo.queueFamilyIndex = m_QueueFamilyIndex;
    createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    m_CommandPool = device.createCommandPool(createInfo);
}

void CommandQueue::Shutdown()
{
    if (m_CommandPool)
    {
        m_Device.destroyCommandPool(m_CommandPool);
        m_CommandPool = nullptr;
    }
}

void CommandQueue::WaitForFence(vk::Fence& fence)
{
    m_Device.waitForFences(fence, VK_TRUE, UINT64_MAX);
    //m_Device.resetFences(fence);
}

void CommandQueue::Submit(const vk::SubmitInfo& info, vk::Fence& fence)
{
    m_CommandQueue.submit(info, fence);
}


CommandBufferManager::CommandBufferManager() :
    m_QueueFamilyIndice{},
    m_GraphicsQueue(vk::QueueFlagBits::eGraphics),
    m_ComputeQueue(vk::QueueFlagBits::eCompute),
    m_TransferQueue(vk::QueueFlagBits::eTransfer),
    m_PresentQueue(vk::QueueFlagBits::eGraphics)
{
}

CommandBufferManager::~CommandBufferManager()
{
    Shutdown();
}

void CommandBufferManager::Create(const vk::Device& device, const CommandQueueFamilyIndice& indice)
{
    m_Device = device;
    m_GraphicsQueue.Create(m_Device, indice.graphicsIndex);
    m_ComputeQueue.Create(m_Device, indice.computeIndex);
    m_TransferQueue.Create(m_Device, indice.transferIndex);
    m_PresentQueue.Create(m_Device, indice.presentIndex);
    m_QueueFamilyIndice = indice;
}

void CommandBufferManager::Shutdown()
{
    m_GraphicsQueue.Shutdown();
    m_ComputeQueue.Shutdown();
    m_TransferQueue.Shutdown();
    m_PresentQueue.Shutdown();
}

vk::CommandBuffer CommandBufferManager::CreateNewCommandBuffer(vk::QueueFlagBits type)
{
    vk::CommandBufferAllocateInfo info;
    switch (type)
    {
    case vk::QueueFlagBits::eGraphics:
        info.commandPool = m_GraphicsQueue.m_CommandPool;
        break;
    case vk::QueueFlagBits::eCompute:
        info.commandPool = m_ComputeQueue.m_CommandPool;
        break;
    case vk::QueueFlagBits::eTransfer:
        info.commandPool = m_TransferQueue.m_CommandPool;
        break;
    default:
        break;
    }
    info.level = vk::CommandBufferLevel::ePrimary;
    info.commandBufferCount = 1;
    auto buffers = m_Device.allocateCommandBuffers(info);

    return buffers[0];
}

void PresentQueue::Present(vk::PresentInfoKHR& info)
{
    m_CommandQueue.presentKHR(info);
}
