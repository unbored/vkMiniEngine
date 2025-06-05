#pragma once

#include "PixelBuffer.h"

class DepthBuffer : public PixelBuffer
{
public:
    DepthBuffer(float clearValue = 0.0f) :
        PixelBuffer(vk::ImageUsageFlagBits::eTransferDst
                    |vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth)
    {
        m_ClearValue.depthStencil.depth = clearValue;
        m_ClearValue.depthStencil.stencil = 0;
    }
};
