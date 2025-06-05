#include "PixelBuffer.h"
#include "CommandContext.h"
#include "GraphicsCommon.h"
#include "GraphicsCore.h"
#include "Swapchain.h"

using namespace Graphics;

void PixelBuffer::Create(const std::string &Name, uint32_t width, uint32_t height, uint32_t numMips,
                         vk::Format format)
{
    Destroy();

    m_Extent.width = width;
    m_Extent.height = height;
    m_Extent.depth = 1;
    m_Format = format;
    m_MipLevel = numMips;
    m_NumLayers = 1;

    m_Layout = vk::ImageLayout::eUndefined;

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = m_Extent;
    imageInfo.mipLevels = numMips;
    imageInfo.arrayLayers = m_NumLayers;
    imageInfo.format = format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = m_Layout;
    imageInfo.usage = m_ImageUsage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = vma::MemoryUsage::eGpuOnly;
    std::tie(m_Image, m_Allocation) = g_Allocator.createImage(imageInfo, allocInfo);
    
    m_SubresourceRange.aspectMask = m_AspectMask;
    m_SubresourceRange.baseMipLevel = 0;
    m_SubresourceRange.levelCount = m_MipLevel;
    m_SubresourceRange.baseArrayLayer = 0;
    m_SubresourceRange.layerCount = m_NumLayers;

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.setImage(m_Image);
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(format);
    viewInfo.setComponents(vk::ComponentMapping());
    viewInfo.subresourceRange = m_SubresourceRange;
    m_ImageView = g_Device.createImageView(viewInfo);
}

void PixelBuffer::Destroy()
{
    g_FramebufferManager.InformDestruction(*this);
    ImageView::Destroy();
}
