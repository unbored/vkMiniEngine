#include "RenderPass.h"
#include "GraphicsCore.h"
#include "Hash.h"
#include <map>

namespace RenderPass
{
static std::map<size_t, vk::RenderPass> s_RenderPassMap;

// void RenderPass::SetColorAttachment(vk::Format format, ContentOp op)
//{
//     vk::AttachmentDescription colorAttachment;
//     colorAttachment.setFormat(format);
//     colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
//     switch (op)
//     {
//         case RenderPass::DontCare:
//             colorAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
//             colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//             break;
//         case RenderPass::Clear:
//             colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//             colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//             break;
//         case RenderPass::Preserve:
//             colorAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
//             colorAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
//             break;
//         default:
//             break;
//     }
//     colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
//     colorAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
//     colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eClear);
//     colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eStore);
//
//     m_ColorAttachments.push_back(colorAttachment);
// }

// void RenderPass::SetDepthAttachment(vk::Format format, ContentOp op)
//{
//     m_HasDepth = true;
//     if (format == vk::Format::eS8Uint
//         || format == vk::Format::eD16UnormS8Uint
//         || format == vk::Format::eD24UnormS8Uint
//         || format == vk::Format::eD32SfloatS8Uint)
//         m_HasStencil = true;
//
//     vk::AttachmentDescription depthAttachment;
//     depthAttachment.setFormat(format);
//     depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
//     switch (op)
//     {
//         case RenderPass::DontCare:
//             depthAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
//             depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//             depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//             break;
//         case RenderPass::Clear:
//             depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//             depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eClear);
//             depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//             break;
//         case RenderPass::Preserve:
//             depthAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
//             depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eLoad);
//             depthAttachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
//             break;
//         default:
//             break;
//     }
//     depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
//     depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
//     depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eStore);
//     m_DepthAttachment = depthAttachment;
// }

// void RenderPass::Finalize()
//{
//     // subpass
//     vk::SubpassDescription subpass;
//     subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
//
//     std::vector<vk::AttachmentReference> colorRefs;
//     colorRefs.resize(m_ColorAttachments.size());
//     for (int i = 0; i < colorRefs.size(); ++i)
//     {
//         colorRefs[i].attachment = i;
//         colorRefs[i].layout = vk::ImageLayout::eColorAttachmentOptimal;
//     }
//     subpass.setColorAttachmentCount(colorRefs.size());
//     subpass.setColorAttachments(colorRefs);
//
//     if (m_HasDepth)
//     {
//         vk::AttachmentReference depthRef;
//         depthRef.attachment = colorRefs.size();
//         depthRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
//         subpass.setPDepthStencilAttachment(&depthRef);
//     }
//
//     // As we have only one subpass, the implicit dependencies should be enough
//     //    vk::SubpassDependency dependency;
//     //    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
//     //    dependency.dstSubpass = 0;
//     //    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
//     //    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead
//     //                            | vk::AccessFlagBits::eColorAttachmentWrite;
//     //    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
//     //    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
//     //    if (m_HasDepth)
//     //    {
//     //        dependency.srcStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
//     //        dependency.dstStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
//     //        dependency.srcAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentRead;
//     //        dependency.dstAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;
//     //    }
//
//     std::vector<vk::AttachmentDescription> attachments = m_ColorAttachments;
//     if (m_HasDepth)
//         attachments.push_back(m_DepthAttachment);
//
//     vk::RenderPassCreateInfo renderPassInfo;
//     renderPassInfo.setAttachmentCount(attachments.size());
//     renderPassInfo.setAttachments(attachments);
//     renderPassInfo.setSubpassCount(1);
//     renderPassInfo.setSubpasses(subpass);
//     //    renderPassInfo.setDependencyCount(1);
//     //    renderPassInfo.setDependencies(dependency);
//     m_RenderPass = Graphics::g_Device.createRenderPass(renderPassInfo);
// }
//
// void RenderPass::Destroy()
//{
//     if (m_RenderPass)
//     {
//         Graphics::g_Device.destroyRenderPass(m_RenderPass);
//         m_RenderPass = nullptr;
//     }
// }

vk::RenderPass GetRenderPass(const vk::ArrayProxy<vk::Format> &colors, vk::Format depth)
{
    size_t hash = 2166136261U;
    if (!colors.empty())
    {
        hash = Utility::HashState(colors.data(), colors.size(), hash);
    }
    if (depth != vk::Format::eUndefined)
    {
        hash = Utility::HashState(&depth, 1, hash);
    }
    assert(hash != 2166136261U);

    auto iter = s_RenderPassMap.find(hash);
    if (iter != s_RenderPassMap.end())
    {
        return iter->second;
    }

    std::vector<vk::AttachmentDescription> attachments;
    for (auto &c : colors)
    {
        vk::AttachmentDescription colorAttachment;
        colorAttachment.setFormat(c);
        colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
        colorAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
        colorAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
        colorAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
        colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eLoad);
        colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eStore);

        attachments.push_back(colorAttachment);
    }
    if (depth != vk::Format::eUndefined)
    {
        vk::AttachmentDescription depthAttachment;
        depthAttachment.setFormat(depth);
        depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
        depthAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
        depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        depthAttachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eLoad);
        depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eStore);

        attachments.push_back(depthAttachment);
    }

    // subpass
    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

    std::vector<vk::AttachmentReference> colorRefs;
    if (!colors.empty())
    {
        colorRefs.resize(colors.size());
        for (int i = 0; i < colorRefs.size(); ++i)
        {
            colorRefs[i].attachment = i;
            colorRefs[i].layout = vk::ImageLayout::eColorAttachmentOptimal;
        }
        subpass.setColorAttachmentCount(colorRefs.size());
        subpass.setColorAttachments(colorRefs);
    }

    if (depth != vk::Format::eUndefined)
    {
        vk::AttachmentReference depthRef;
        depthRef.attachment = colorRefs.size();
        depthRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        subpass.setPDepthStencilAttachment(&depthRef);
    }

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.setAttachmentCount(attachments.size());
    renderPassInfo.setAttachments(attachments);
    renderPassInfo.setSubpassCount(1);
    renderPassInfo.setSubpasses(subpass);
    //    renderPassInfo.setDependencyCount(1);
    //    renderPassInfo.setDependencies(dependency);
    vk::RenderPass renderPass = Graphics::g_Device.createRenderPass(renderPassInfo);

    s_RenderPassMap[hash] = renderPass;

    return renderPass;
}

void DestroyAll()
{
    for (auto &i : s_RenderPassMap)
    {
        Graphics::g_Device.destroyRenderPass(i.second);
    }
    s_RenderPassMap.clear();
}

} // namespace RenderPass
