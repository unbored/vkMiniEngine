#include "ColorBuffer.h"
#include "GraphicsCore.h"
#include "Swapchain.h"

using namespace Graphics;

void ColorBuffer::CreateFromSwapchain(const std::string &Name, const Swapchain &swapchain, int index)
{
    Destroy();

    m_Extent.width = swapchain.GetExtent().width;
    m_Extent.height = swapchain.GetExtent().height;
    m_Extent.depth = 1;
    m_Format = swapchain.GetFormat();
    m_MipLevel = 1;
    m_NumLayers = 1;

    m_Layout = vk::ImageLayout::eUndefined;

    m_Image = swapchain.GetImage(index);

    m_ImageUsage = m_ImageUsage & swapchain.GetImageUsageFlags();

    m_SubresourceRange.aspectMask = m_AspectMask;
    m_SubresourceRange.baseMipLevel = 0;
    m_SubresourceRange.levelCount = m_MipLevel;
    m_SubresourceRange.baseArrayLayer = 0;
    m_SubresourceRange.layerCount = m_NumLayers;

    vk::ImageViewCreateInfo info;
    info.setImage(swapchain.GetImage(index));
    info.setViewType(vk::ImageViewType::e2D);
    info.setFormat(swapchain.GetFormat());
    info.setComponents(vk::ComponentMapping());
    info.subresourceRange = m_SubresourceRange;
    m_ImageView = g_Device.createImageView(info);
}
