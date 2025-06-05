#include "Framebuffer.h"
#include "GraphicsCore.h"
#include "Hash.h"
#include <algorithm>

void Framebuffer::Create(vk::RenderPass renderPass, const vk::ArrayProxy<PixelBuffer>& imageViews)
{
    if (imageViews.empty())
    {
        return;
    }
    m_Width = imageViews.front().GetWidth();
    m_Height = imageViews.front().GetHeight();

    std::vector<vk::ImageView> ivs;
    ivs.reserve(imageViews.size());
    m_ClearValues.reserve(imageViews.size());

    for (auto& iv : imageViews)
    {
        ivs.push_back(iv);
        m_ClearValues.push_back(iv.GetClearValue());
    }

    vk::FramebufferCreateInfo framebufferInfo;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = ivs.size();
    framebufferInfo.setAttachments(ivs);
    framebufferInfo.width = m_Width;
    framebufferInfo.height = m_Height;
    framebufferInfo.layers = 1;
    m_Framebuffer = Graphics::g_Device.createFramebuffer(framebufferInfo);

}

void Framebuffer::Destroy()
{
    if (m_Framebuffer)
    {
        Graphics::g_Device.destroyFramebuffer(m_Framebuffer);
        m_Framebuffer = nullptr;
    }
    m_ClearValues.clear();
}

Framebuffer FramebufferManager::GetFramebuffer(vk::RenderPass renderPass, const vk::ArrayProxy<PixelBuffer>& buffers)
{
    auto pass = VkRenderPass(renderPass);
    size_t hash = Utility::HashState(&pass);
    for (auto &i : buffers)
    {
        auto view = VkImageView(vk::ImageView(i));
        hash = Utility::HashState(&view, 1, hash);
    }
    
//    auto iter = std::find_if(m_Framebuffers.begin(), m_Framebuffers.end(),
//        [&](const std::pair<size_t, FbInfo>& pair)
//        {
//        auto &info = pair.second;
//            if (info.rp != renderPass || buffers.size() != info.ivs.size())
//            {
//                return false;
//            }
//            for (int i = 0; i < info.ivs.size(); ++i)
//            {
//                if (info.ivs[i] != vk::ImageView(buffers.data()[i]))
//                {
//                    return false;
//                }
//            }
//            return true;
//        });
//    if (iter != m_Framebuffers.end())
//    {
//        return iter->second.fb;
//    }
    
    auto iter = m_FramebufferMap.find(hash);
    if (iter != m_FramebufferMap.end())
    {
        return iter->second.fb;
    }
    
    FbInfo info;
    info.fb.Create(renderPass, buffers);
    info.rp = renderPass;
    info.ivs.reserve(buffers.size());
    for (auto& b : buffers)
    {
        info.ivs.push_back(b);
    }
    m_FramebufferMap[hash] = info;
    return info.fb;
}
void FramebufferManager::InformDestruction(const PixelBuffer& buffer)
{
    auto iter = m_FramebufferMap.begin();
    while (iter != m_FramebufferMap.end())
    {
        auto& info = *iter;
        bool found = false;
        for (auto& i : info.second.ivs)
        {
            if (vk::ImageView(buffer) == i)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            info.second.fb.Destroy();
            iter = m_FramebufferMap.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}
void FramebufferManager::DestroyAll()
{
    for (auto& f : m_FramebufferMap)
    {
        f.second.fb.Destroy();
    }
    m_FramebufferMap.clear();
}
