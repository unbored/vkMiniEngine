#pragma once

#include "PixelBuffer.h"

class ColorBuffer : public PixelBuffer
{
public:
    ColorBuffer(Color clearColor = Color(0.0f, 0.0f, 0.0f, 0.0f)) :
        PixelBuffer(vk::ImageUsageFlagBits::eTransferDst
            | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eColorAttachment
            | vk::ImageUsageFlagBits::eStorage,
            vk::ImageAspectFlagBits::eColor)
    {
        m_ClearValue.color = clearColor.GetArray();
    }

    void CreateFromSwapchain(const std::string& Name, const Swapchain& swapchain, int index);
    void SetClearColor(Color clearColor) { m_ClearValue.color = clearColor.GetArray(); }
    vk::ClearColorValue GetClearColor() const { return m_ClearValue.color; }
//
//private:
//    Color m_ClearColor;
};
