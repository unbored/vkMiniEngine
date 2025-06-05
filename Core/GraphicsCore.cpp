//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include "BufferManager.h"
#include "Framebuffer.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
// #include "GpuTimeManager.h"
// #include "PostEffects.h"
// #include "SSAO.h"
#include "TextRenderer.h"
// #include "ColorBuffer.h"
// #include "SystemTime.h"
#include "SamplerManager.h"
// #include "DescriptorHeap.h"
#include "CommandBufferManager.h"
#include "CommandContext.h"
// #include "RootSignature.h"
// #include "CommandSignature.h"
// #include "ParticleEffectManager.h"
// #include "GraphRenderer.h"
// #include "TemporalEffects.h"
#include "Display.h"
#include "Util/CommandLineArg.h"
#include <algorithm>
#include <inttypes.h>
#include <string>
#include <vector>

// VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace GameCore
{
extern GLFWwindow *g_Window;
}

VkBool32 VKAPI_PTR debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    std::string str = "";
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        str = "verbose";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        str = "warning";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        str = "error";
        break;
    default:
        str = "default";
        break;
    }
    printf("Validation layer[%s]: %s\n", str.c_str(), pCallbackData->pMessage);

    return VK_FALSE;
}

namespace Graphics
{
bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT = false;
bool g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = false;

vk::Instance g_Instance;
vk::DebugUtilsMessengerEXT g_DebugMessenger;

vk::PhysicalDevice g_PhysicalDevice;
vk::Device g_Device;
vma::Allocator g_Allocator;

vk::SurfaceKHR g_Surface;
CommandBufferManager g_CommandManager;
ContextManager g_ContextManager;
FramebufferManager g_FramebufferManager;

// D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

// DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
//{
//     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
//     D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
//     D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
//     D3D12_DESCRIPTOR_HEAP_TYPE_DSV
// };

static const uint32_t vendorID_Nvidia = 0x10DE;
static const uint32_t vendorID_AMD = 0x1002;
static const uint32_t vendorID_Intel = 0x8086;

uint32_t GetDesiredGPUVendor()
{
    uint32_t desiredVendor = 0;

    std::string vendorVal;
    if (CommandLineArgs::GetString("vendor", vendorVal))
    {
        // Convert to lower case
        std::transform(vendorVal.begin(), vendorVal.end(), vendorVal.begin(),
                       [](const char c) { return std::tolower(c); });

        if (vendorVal.find("amd") != std::string::npos)
        {
            desiredVendor = vendorID_AMD;
        }
        else if (vendorVal.find("nvidia") != std::string::npos || vendorVal.find("nvd") != std::string::npos ||
                 vendorVal.find("nvda") != std::string::npos || vendorVal.find("nv") != std::string::npos)
        {
            desiredVendor = vendorID_Nvidia;
        }
        else if (vendorVal.find("intel") != std::string::npos || vendorVal.find("intc") != std::string::npos)
        {
            desiredVendor = vendorID_Intel;
        }
    }

    return desiredVendor;
}

const char *GPUVendorToString(uint32_t vendorID)
{
    switch (vendorID)
    {
    case vendorID_Nvidia:
        return "Nvidia";
    case vendorID_AMD:
        return "AMD";
    case vendorID_Intel:
        return "Intel";
    default:
        return "Unknown";
        break;
    }
}

} // namespace Graphics

// Initialize the vulkan resources required to run.
void Graphics::Initialize(bool RequireDXRSupport)
{
    // Enable all the extensions
    // vk::DynamicLoader dl;
    // PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
    // dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    // VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    uint32_t useDebugLayers = 0;
    CommandLineArgs::GetInteger("debug", useDebugLayers);
#ifndef NDEBUG
    // Default to true for debug builds
    useDebugLayers = 1;
#endif

    // Enable validation layers if debug mode on
    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    if (useDebugLayers)
    {
        bool layerFound = true;
        auto layers = vk::enumerateInstanceLayerProperties();
        for (auto &vl : validationLayers)
        {
            bool found = false;
            for (auto &l : layers)
            {
                if (strcmp(vl, l.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                layerFound = false;
            }
        }
        if (!layerFound)
        {
            useDebugLayers = false;
            printf("Validation layers requested but not available\n");
        }
    }

    vk::InstanceCreateInfo instanceInfo;

    // Instance Extensions
    std::vector<const char *> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_MSC_VER)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    auto CheckInstanceExtensionSupport = [](const char *extension) {
        bool found = false;
        auto properties = vk::enumerateInstanceExtensionProperties();
        for (auto &p : properties)
        {
            if (strcmp(p.extensionName, extension) == 0)
            {
                found = true;
                break;
            }
        }
        return found;
    };

    vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
    if (useDebugLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        debugInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                     vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        debugInfo.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                 vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                 vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        debugInfo.setPfnUserCallback(debugCallback);

        instanceInfo.setEnabledLayerCount(validationLayers.size());
        instanceInfo.setPEnabledLayerNames(validationLayers);
        instanceInfo.setPNext(&debugInfo);
    }

    vk::ApplicationInfo appInfo;
    // appInfo.setPApplicationName(appName);
    appInfo.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    appInfo.setPEngineName("MiniEngine");
    appInfo.setApiVersion(VK_API_VERSION_1_2);

    instanceInfo.setPApplicationInfo(&appInfo);

    bool instanceExtensionsSupport = true;
    for (auto &e : extensions)
    {
        if (!CheckInstanceExtensionSupport(e))
        {
            instanceExtensionsSupport = false;
            break;
        }
    }
    if (!instanceExtensionsSupport)
    {
        printf("Specified instance extensions not supported!\n");
        raise(SIGABRT);
    }

    instanceInfo.setEnabledExtensionCount(extensions.size());
    instanceInfo.setPEnabledExtensionNames(extensions);
#ifdef __APPLE__
    instanceInfo.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    g_Instance = vk::createInstance(instanceInfo);
    // Get extensions pointers
    // VULKAN_HPP_DEFAULT_DISPATCHER.init(g_Instance);

    if (useDebugLayers)
    {
        // create a debug callback
        auto func =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugUtilsMessengerEXT");
        // g_DebugMessenger = g_Instance.createDebugUtilsMessengerEXT(debugInfo);
        func(g_Instance, (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo, nullptr,
             (VkDebugUtilsMessengerEXT *)&g_DebugMessenger);
    }

    uint32_t desiredVendor = GetDesiredGPUVendor();
    if (desiredVendor)
    {
        printf("Looking for a %s GPU\n", GPUVendorToString(desiredVendor));
    }

    // Get a list of physical devices
    auto physicalDevices = g_Instance.enumeratePhysicalDevices();

    // Device Extensions
    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
    };

    auto CheckDeviceExtensionSupport = [](const vk::PhysicalDevice &device, const char *extension) {
        bool found = false;
        auto properties = device.enumerateDeviceExtensionProperties();
        for (auto &p : properties)
        {
            if (strcmp(p.extensionName, extension) == 0)
            {
                found = true;
                break;
            }
        }
        return found;
    };

    // Searching for a proper queue family
    auto FindQueueFamily = [](const std::vector<vk::QueueFamilyProperties> &queueFamilies, vk::QueueFlagBits bit) {
        int index = -1;
        for (int i = 0; i < queueFamilies.size(); ++i)
        {
            auto &qf = queueFamilies[i];
            if (qf.queueCount > 0 && qf.queueFlags & bit)
            {
                index = i;
                break;
            }
        }
        return index;
    };

    size_t MaxSize = 0;

    for (auto &pd : physicalDevices)
    {
        auto properties = pd.getProperties();
        auto features = pd.getFeatures();
        auto memory = pd.getMemoryProperties();

        // Is this the desired vendor desired?
        if (desiredVendor != 0 && desiredVendor != properties.vendorID)
        {
            continue;
        }

        // Does this support swapchain?
        bool support = true;
        for (auto &e : deviceExtensions)
        {
            if (!CheckDeviceExtensionSupport(pd, e))
            {
                support = false;
                break;
            }
        }
        if (!support)
        {
            continue;
        }

        // Is this support graphics/compute/transfer queue family?
        auto queueFamilies = pd.getQueueFamilyProperties();
        int queueFamilyIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eGraphics);
        if (queueFamilyIndex < 0)
        {
            continue;
        }
        queueFamilyIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eCompute);
        if (queueFamilyIndex < 0)
        {
            continue;
        }
        queueFamilyIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eTransfer);
        if (queueFamilyIndex < 0)
        {
            continue;
        }
#ifndef __APPLE__
        // Is this support presentation queue family?
        bool presentSupport = false;
        for (int i = 0; i < queueFamilies.size(); ++i)
        {
            if (pd.getWin32PresentationSupportKHR(i))
            {
                presentSupport = true;
                break;
            }
        }
        if (!presentSupport)
        {
            continue;
        }
#endif

        // is the device support anisotropy?
        if (!features.samplerAnisotropy)
        {
            continue;
        }

        // By default, search for the adapter with the most memory because that's usually the dGPU.
        if (memory.memoryHeapCount <= 0 || memory.memoryHeaps[0].size < MaxSize)
        {
            continue;
        }

        MaxSize = memory.memoryHeaps[0].size;

        g_PhysicalDevice = pd;

        printf("Selected GPU:  %s (%" PRIu64 " MB)\n", properties.deviceName.data(), memory.memoryHeaps[0].size >> 20);
    }
    if (!g_PhysicalDevice)
    {
        printf("No suitable physical device found!\n");
        raise(SIGABRT);
    }

    // Get queue family
    auto queueFamilies = g_PhysicalDevice.getQueueFamilyProperties();
    CommandQueueFamilyIndice queueFamilyIndice;
    queueFamilyIndice.graphicsIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eGraphics);
    queueFamilyIndice.computeIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eCompute);
    queueFamilyIndice.transferIndex = FindQueueFamily(queueFamilies, vk::QueueFlagBits::eTransfer);
#ifdef __APPLE__
    queueFamilyIndice.presentIndex = queueFamilyIndice.graphicsIndex;
#else
    for (int i = 0; i < queueFamilies.size(); ++i)
    {
        if (g_PhysicalDevice.getWin32PresentationSupportKHR(i))
        {
            queueFamilyIndice.presentIndex = i;
            break;
        }
    }
#endif

    // prepare queue
    std::vector<int> queueIndice{queueFamilyIndice.graphicsIndex, queueFamilyIndice.computeIndex,
                                 queueFamilyIndice.transferIndex, queueFamilyIndice.presentIndex};
    std::sort(queueIndice.begin(), queueIndice.end());
    auto pos = std::unique(queueIndice.begin(), queueIndice.end());
    queueIndice.erase(pos, queueIndice.end());

    float queuePriorities = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    for (auto &i : queueIndice)
    {
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.setQueueFamilyIndex(i);
        queueInfo.setQueueCount(1);
        queueInfo.setPQueuePriorities(&queuePriorities);
        queueInfos.push_back(queueInfo);
    }

    // Create a surface
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(g_Instance, GameCore::g_Window, nullptr, &surface);
    g_Surface = surface;
    // query for capabilities of surface
    auto capabilities = g_PhysicalDevice.getSurfaceCapabilitiesKHR(g_Surface);
    auto formats = g_PhysicalDevice.getSurfaceFormatsKHR(g_Surface);
    auto presentModes = g_PhysicalDevice.getSurfacePresentModesKHR(g_Surface);
    if (formats.empty() || presentModes.empty())
    {
        printf("Surface not adequate for swapchain!\n");
        raise(SIGABRT);
    }

    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    // create device
    vk::DeviceCreateInfo deviceInfo;
    deviceInfo.setQueueCreateInfos(queueInfos);
    deviceInfo.setQueueCreateInfoCount(queueInfos.size());
    deviceInfo.setEnabledExtensionCount(deviceExtensions.size());
    deviceInfo.setPEnabledExtensionNames(deviceExtensions);
    deviceInfo.setPEnabledFeatures(&deviceFeatures);
    if (useDebugLayers)
    {
        deviceInfo.setEnabledLayerCount(validationLayers.size());
        deviceInfo.setPEnabledLayerNames(validationLayers);
    }
#ifdef __APPLE__
    VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures = {};
    portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
    portabilityFeatures.mutableComparisonSamplers = VK_TRUE;
    deviceInfo.pNext = &portabilityFeatures;
#endif

    g_Device = g_PhysicalDevice.createDevice(deviceInfo);
    // VULKAN_HPP_DEFAULT_DISPATCHER.init(g_Device);

    // create allocator
    vma::AllocatorCreateInfo allocInfo;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocInfo.physicalDevice = g_PhysicalDevice;
    allocInfo.device = g_Device;
    allocInfo.instance = g_Instance;
    g_Allocator = vma::createAllocator(allocInfo);

    g_CommandManager.Create(g_Device, queueFamilyIndice);

    //    // Common state was moved to GraphicsCommon.*
    InitializeCommonState();

    Display::Initialize();

    // GpuTimeManager::Initialize(4096);
    // TemporalEffects::Initialize();
    // PostEffects::Initialize();
    // SSAO::Initialize();
    TextRenderer::Initialize();
    // GraphRenderer::Initialize();
    // ParticleEffectManager::Initialize(3840, 2160);
}

void Graphics::Shutdown(void)
{
    g_CommandManager.IdleGPU();

    CommandContext::DestroyAllContexts();
    g_CommandManager.Shutdown();
    g_FramebufferManager.DestroyAll();
    // GpuTimeManager::Shutdown();
    PSO::DestroyAll();
    RenderPass::DestroyAll();
    // RootSignature::DestroyAll();
    // DescriptorAllocator::DestroyAll();

    SamplerDesc::DestroyAll();
    DestroyCommonState();
    DestroyRenderingBuffers();
    // TemporalEffects::Shutdown();
    // PostEffects::Shutdown();
    // SSAO::Shutdown();
    TextRenderer::Shutdown();
    // GraphRenderer::Shutdown();
    // ParticleEffectManager::Shutdown();
    Display::Shutdown();

    g_Allocator.destroy();

    g_Device.destroy();

    g_Instance.destroySurfaceKHR(g_Surface);

    if (g_DebugMessenger)
    {
        auto func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugUtilsMessengerEXT");
        // g_Instance.destroyDebugUtilsMessengerEXT(g_DebugMessenger);
        func(g_Instance, g_DebugMessenger, nullptr);
    }
    g_Instance.destroy();
}
