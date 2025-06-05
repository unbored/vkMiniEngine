#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
// #include "ColorBuffer.h"
// #include "DepthBuffer.h"

namespace RenderPass
{

// struct AttachmentDesc
// {
//     vk::Format format;
//     vk::AttachmentLoadOp loadOp;
// };

// struct RenderPassDesc
// {
//     std::vector<AttachmentDesc> colors;
//     AttachmentDesc depth;
// };
//    void SetColorAttachment(vk::Format format, ContentOp op);
//    void SetDepthAttachment(vk::Format format, ContentOp op);
vk::RenderPass GetRenderPass(const vk::ArrayProxy<vk::Format> &colors, vk::Format depth);
void DestroyAll();
//    void Finalize();
//    void Destroy();

//    operator vk::RenderPass() const { return m_RenderPass; }

// private:
//     vk::RenderPass m_RenderPass;

//    std::vector<vk::AttachmentDescription> m_ColorAttachments;
//    vk::AttachmentDescription m_DepthAttachment;
//
//    bool m_HasDepth = false;
//    bool m_HasStencil = false;
}; // namespace RenderPass
