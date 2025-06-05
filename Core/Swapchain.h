#pragma once

#include <vulkan/vulkan.hpp>
#include "ColorBuffer.h"
#include "Framebuffer.h"

class Swapchain
{
public:
    //    void Create(const vk::SwapchainCreateInfoKHR& info);
    void Create(size_t Width, size_t Height, vk::Format Format, size_t BufferCount, bool TryEnableHDR);
    //void CreateFramebuffers(const vk::RenderPass& renderPass);
    void Resize(unsigned int width, unsigned int height);
    void Destroy();

    vk::Format GetFormat() const { return m_Format; }
    vk::Extent2D GetExtent() const { return m_Extent; }
    const vk::Image& GetImage(uint32_t index) const { return m_Images[index]; }
    //const Framebuffer& GetFramebuffer(unsigned int index) { return m_Framebuffers[index]; }
    const vk::SwapchainKHR& GetSwapchain() { return m_Swapchain; }
    //const vk::Semaphore& GetSemaphore(uint32_t index) { return m_Semaphores[index]; }
    vk::ImageUsageFlags GetImageUsageFlags() const { return m_ImageUsage; }

private:
    vk::SwapchainCreateInfoKHR m_CreateInfo;
    std::vector<uint32_t> m_QueueIndice;
    vk::Format m_Format;
    vk::Extent2D m_Extent;
    vk::SwapchainKHR m_Swapchain;
    std::vector<vk::Image> m_Images;
    vk::ImageUsageFlags m_ImageUsage;
    //std::vector<ColorBuffer> m_ColorBuffers;
    //std::vector<Framebuffer> m_Framebuffers;
    //std::vector<vk::Semaphore> m_Semaphores;
};
