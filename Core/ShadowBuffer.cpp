#include "ShadowBuffer.h"

void ShadowBuffer::Create(const std::string &Name, uint32_t width, uint32_t height)
{
    DepthBuffer::Create(Name, width, height, 1, vk::Format::eD16Unorm);

    m_Viewport.x = 0.0f;
    m_Viewport.y = 0.0f;
    m_Viewport.width = (float)width;
    m_Viewport.height = (float)height;
    m_Viewport.minDepth = 0.0f;
    m_Viewport.maxDepth = 1.0f;

    // Prevent drawing to the boundary pixels so that we don't have to worry about shadows stretching
    m_Scissor.offset.x = 1;
    m_Scissor.offset.y = 1;
    m_Scissor.extent.width = width - 2;
    m_Scissor.extent.height = height - 2;
}