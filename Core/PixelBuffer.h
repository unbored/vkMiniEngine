#pragma once

#include "ImageView.h"

class PixelBuffer : public ImageView
{
public:
    PixelBuffer(vk::ImageUsageFlags imageUsage, vk::ImageAspectFlags aspectMask) :
        ImageView(imageUsage, aspectMask) {}
    
    void Create(const std::string& Name, uint32_t width, uint32_t height, uint32_t numMips, vk::Format format);
    
    void Destroy();
    
    vk::ClearDepthStencilValue GetClearValue() const { return m_ClearValue.depthStencil; }
    
protected:
    vk::ClearValue m_ClearValue;
};
