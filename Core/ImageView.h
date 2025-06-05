#pragma once

#include "Color.h"
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

class Swapchain;
class StagingBuffer;

class ImageView
{
    friend class CommandContext;
    friend class GraphicsContext;

public:
    ImageView(vk::ImageUsageFlags imageUsage, vk::ImageAspectFlags aspectMask)
        : m_ImageUsage(imageUsage), m_AspectMask(aspectMask)
    {
    }
    operator vk::ImageView() const { return m_ImageView; }

    //    ~ImageView() { Destroy(); }

    virtual void Destroy();

    uint32_t GetWidth() const { return m_Extent.width; }
    uint32_t GetHeight() const { return m_Extent.height; }
    vk::Format GetFormat() const { return m_Format; }
    uint32_t GetMipLevels() const { return m_MipLevel; }
    vk::Image GetImage() const { return m_Image; }
    vk::ImageUsageFlags GetImageUsageFlags() const { return m_ImageUsage; }
    vk::ImageSubresourceRange GetSubresourceRange() const { return m_SubresourceRange; }

protected:
    vk::ImageUsageFlags m_ImageUsage;
    vk::ImageAspectFlags m_AspectMask;
    vk::Format m_Format;
    vk::Extent3D m_Extent;
    uint32_t m_MipLevel;
    uint32_t m_NumLayers;

    vma::Allocation m_Allocation;
    vk::Image m_Image;

    vk::ImageLayout m_Layout;

    vk::ImageView m_ImageView;
    vk::ImageSubresourceRange m_SubresourceRange;
};
