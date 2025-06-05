#include "Swapchain.h"
#include "ImageView.h"
#include "GraphicsCore.h"
#include "CommandBufferManager.h"
#include "Display.h"

//void Swapchain::Create(const vk::SwapchainCreateInfoKHR& info)
//{
//    m_CreateInfo = info;
//    m_Extent = m_CreateInfo.imageExtent;
//    m_Format = m_CreateInfo.imageFormat;
//    m_QueueIndice = std::vector<uint32_t>(info.pQueueFamilyIndices,
//        info.pQueueFamilyIndices + info.queueFamilyIndexCount);
//    m_CreateInfo.setQueueFamilyIndices(m_QueueIndice);
//
//    m_Swapchain = Graphics::g_Device.createSwapchainKHR(m_CreateInfo);
//
//    m_Images = Graphics::g_Device.getSwapchainImagesKHR(m_Swapchain);
//    //m_Semaphores.resize(m_Images.size());
//    //for (int i = 0; i < m_Images.size(); ++i)
//    //{
//    //    vk::SemaphoreCreateInfo info;
//    //    m_Semaphores[i] = Graphics::g_Device.createSemaphore(info);
//    //}
//}

void Swapchain::Create(size_t Width, size_t Height, vk::Format Format, size_t BufferCount, bool TryEnableHDR)
{
    // query for capabilities of surface
    auto capabilities = Graphics::g_PhysicalDevice.getSurfaceCapabilitiesKHR(Graphics::g_Surface);
    auto formats = Graphics::g_PhysicalDevice.getSurfaceFormatsKHR(Graphics::g_Surface);
    auto presentModes = Graphics::g_PhysicalDevice.getSurfacePresentModesKHR(Graphics::g_Surface);

    // Choose specified format, otherwise the first one
    vk::SurfaceFormatKHR format;
    auto FindFormat = [](const std::vector<vk::SurfaceFormatKHR>& formats, vk::Format type)
    {
        vk::SurfaceFormatKHR format;
        for (auto& f : formats)
        {
            if (f.format == type && f.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                format = f;
            }
        }
        return format;
    };

    format = FindFormat(formats, Format);
    if (format.format == vk::Format::eUndefined || !TryEnableHDR)
    {
        format = formats[0];
    }
    else
    {
        //Graphics::g_bEnableHDROutput = true;
    }

    // Choose mailbox mode, otherwise fifo, at least immediate
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eImmediate;
//    for (auto& p : presentModes)
//    {
//        if (p == vk::PresentModeKHR::eFifo)
//        {
//            presentMode = p;
//        }
//    }
//    for (auto& p : presentModes)
//    {
//        if (p == vk::PresentModeKHR::eMailbox)
//        {
//            presentMode = p;
//        }
//    }

    // Choose a proper swapchain size
    vk::Extent2D extent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent = vk::Extent2D(Width, Height);
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }

    // set swapchain image count
    uint32_t imageCount = BufferCount;
    if (imageCount < capabilities.minImageCount || imageCount > capabilities.maxImageCount)
    {
        printf("Swapchain does not support %d buffer!\n", BufferCount);
    }
    
    m_ImageUsage = capabilities.supportedUsageFlags;

    vk::SwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.setSurface(Graphics::g_Surface);
    swapchainInfo.setMinImageCount(imageCount);
    swapchainInfo.setImageFormat(format.format);
    swapchainInfo.setImageColorSpace(format.colorSpace);
    swapchainInfo.setImageExtent(extent);
    swapchainInfo.setImageArrayLayers(1);
    swapchainInfo.setImageUsage(m_ImageUsage);
    swapchainInfo.setPreTransform(capabilities.currentTransform);
    swapchainInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    swapchainInfo.setPresentMode(presentMode);
    swapchainInfo.setClipped(true);
    swapchainInfo.setOldSwapchain(nullptr);

    auto queueIndice = Graphics::g_CommandManager.GetIndice();
    if (queueIndice.graphicsIndex == queueIndice.presentIndex)
    {
        swapchainInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }
    else
    {
        swapchainInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
        std::vector<uint32_t> indice{ (uint32_t)queueIndice.graphicsIndex, (uint32_t)queueIndice.presentIndex };
        swapchainInfo.setQueueFamilyIndices(indice);
        swapchainInfo.setQueueFamilyIndexCount((uint32_t)indice.size());
    }

    m_CreateInfo = swapchainInfo;
    m_Extent = m_CreateInfo.imageExtent;
    m_Format = m_CreateInfo.imageFormat;
    m_QueueIndice = std::vector<uint32_t>(swapchainInfo.pQueueFamilyIndices,
        swapchainInfo.pQueueFamilyIndices + swapchainInfo.queueFamilyIndexCount);
    m_CreateInfo.setQueueFamilyIndices(m_QueueIndice);

    m_Swapchain = Graphics::g_Device.createSwapchainKHR(m_CreateInfo);

    m_Images = Graphics::g_Device.getSwapchainImagesKHR(m_Swapchain);
}

//void Swapchain::CreateFramebuffers(const vk::RenderPass& renderPass)
//{
//    m_ColorBuffers.resize(m_Images.size());
//    m_Framebuffers.resize(m_Images.size());
//    for (int i = 0; i < m_Images.size(); ++i)
//    {
//        m_ColorBuffers[i].CreateFromSwapchain(*this, i);
//        m_Framebuffers[i].Create(renderPass, m_ColorBuffers[i]);
//    }
//}

void Swapchain::Resize(unsigned int width, unsigned int height)
{
    //destroy old one
    Destroy();

    // update extent
    m_Extent.width = width;
    m_Extent.height = height;
    m_CreateInfo.imageExtent = m_Extent;
    // create new one
    m_Swapchain = Graphics::g_Device.createSwapchainKHR(m_CreateInfo);
    m_Images = Graphics::g_Device.getSwapchainImagesKHR(m_Swapchain);
    //m_Semaphores.resize(m_Images.size());
    //for (int i = 0; i < m_Images.size(); ++i)
    //{
    //    vk::SemaphoreCreateInfo info;
    //    m_Semaphores[i] = Graphics::g_Device.createSemaphore(info);
    //}
}

void Swapchain::Destroy()
{
    //for (auto& sm : m_Semaphores)
    //{
    //    Graphics::g_Device.destroySemaphore(sm);
    //}
    //for (auto& fb : m_Framebuffers)
    //{
    //    fb.Destroy();
    //}
    //for (auto& cb : m_ColorBuffers)
    //{
    //    cb.Destroy();
    //}
    if (m_Swapchain)
    {
        Graphics::g_Device.destroySwapchainKHR(m_Swapchain);
        m_Swapchain = nullptr;
    }
}
