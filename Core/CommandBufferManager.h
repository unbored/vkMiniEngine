#pragma once

#include <vulkan/vulkan.hpp>

struct CommandQueueFamilyIndice
{
    int graphicsIndex;
    int computeIndex;
    int transferIndex;
    int presentIndex;
};

class CommandQueue
{
    friend class CommandBufferManager;

public:
    CommandQueue(vk::QueueFlagBits type);
    virtual ~CommandQueue();

    void Create(const vk::Device &device, int queueFamilyIndex);
    void Shutdown();

    operator vk::Queue() const { return m_CommandQueue; }

    vk::CommandPool GetPool() const { return m_CommandPool; }

    void WaitForFence(vk::Fence &fence);

    void Submit(const vk::SubmitInfo &info, vk::Fence &fence);

    inline bool IsReady() { return m_CommandQueue != vk::Queue(); }

    void WaitForIdle() { m_CommandQueue.waitIdle(); }

protected:
    const vk::QueueFlagBits m_Type;
    vk::Queue m_CommandQueue;
    vk::CommandPool m_CommandPool;
    int m_QueueFamilyIndex;
    vk::Device m_Device;
};

class PresentQueue : public CommandQueue
{
    friend class CommandBufferManager;

public:
    PresentQueue(vk::QueueFlagBits type) : CommandQueue(type) {}
    void Present(vk::PresentInfoKHR &info);
};

class CommandBufferManager
{
public:
    CommandBufferManager();
    ~CommandBufferManager();

    void Create(const vk::Device &device, const CommandQueueFamilyIndice &indice);
    void Shutdown();

    vk::CommandBuffer CreateNewCommandBuffer(vk::QueueFlagBits type);

    CommandQueue &GetGraphicsQueue(void) { return m_GraphicsQueue; }
    CommandQueue &GetComputeQueue(void) { return m_ComputeQueue; }
    CommandQueue &GetTransferQueue(void) { return m_TransferQueue; }
    PresentQueue &GetPresentQueue(void) { return m_PresentQueue; }
    CommandQueueFamilyIndice GetIndice(void) { return m_QueueFamilyIndice; }

    CommandQueue &GetQueue(vk::QueueFlagBits Type = vk::QueueFlagBits::eGraphics)
    {
        switch (Type)
        {
        case vk::QueueFlagBits::eCompute:
            return m_ComputeQueue;
        case vk::QueueFlagBits::eTransfer:
            return m_TransferQueue;
        default:
            return m_GraphicsQueue;
        }
    }

    void IdleGPU()
    {
        m_GraphicsQueue.WaitForIdle();
        m_ComputeQueue.WaitForIdle();
        m_TransferQueue.WaitForIdle();
        m_PresentQueue.WaitForIdle();
    }

private:
    vk::Device m_Device;
    CommandQueue m_GraphicsQueue;
    CommandQueue m_ComputeQueue;
    CommandQueue m_TransferQueue;
    PresentQueue m_PresentQueue;
    CommandQueueFamilyIndice m_QueueFamilyIndice;
};
