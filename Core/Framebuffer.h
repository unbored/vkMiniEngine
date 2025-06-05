#pragma once

#include <vulkan/vulkan.hpp>
#include "PixelBuffer.h"
#include <vector>
#include <list>
#include <map>

class Framebuffer
{
    friend class FramebufferManager;
public:

    operator vk::Framebuffer() const { return m_Framebuffer; }

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    std::vector<vk::ClearValue> GetClearValues() const { return m_ClearValues; }

private:
    void Create(vk::RenderPass renderPass, const vk::ArrayProxy<PixelBuffer>& buffers);
    void Destroy();
    
    uint32_t m_Width;
    uint32_t m_Height;
    vk::Framebuffer m_Framebuffer;
    std::vector<vk::ClearValue> m_ClearValues;
};

// A framebuffer is created whenever a new attachment combination appears
// and recreated whenever some attachment recreates.
// Use a manager to do the work
class FramebufferManager
{
public:
    Framebuffer GetFramebuffer(vk::RenderPass renderPass, const vk::ArrayProxy<PixelBuffer>& buffers);
    void InformDestruction(const PixelBuffer& buffer);
    void DestroyAll();
    
private:
    struct FbInfo
    {
        Framebuffer fb;
        vk::RenderPass rp;
        std::vector<vk::ImageView> ivs;
    };
    std::map<size_t, FbInfo> m_FramebufferMap;
};
