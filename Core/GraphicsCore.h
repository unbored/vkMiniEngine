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

#pragma once

// #define VMA_DEBUG_LOG(format, ...)                                                                                     \
//     do                                                                                                                 \
//     {                                                                                                                  \
//         printf(format, ##__VA_ARGS__);                                                                                 \
//         printf("\n");                                                                                                  \
//     } while (false)

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

//#include "PipelineState.h"
//#include "DescriptorHeap.h"
//#include "RootSignature.h"
#include "GraphicsCommon.h"
//
class CommandBufferManager;
class ContextManager;
class FramebufferManager;

namespace Graphics
{
#ifndef RELEASE
// extern const GUID WKPDID_D3DDebugObjectName;
#endif

void Initialize(bool RequireDXRSupport = false);
void Shutdown(void);

// bool IsDeviceNvidia(ID3D12Device* pDevice);
// bool IsDeviceAMD(ID3D12Device* pDevice);
// bool IsDeviceIntel(ID3D12Device* pDevice);
extern vk::Instance g_Instance;
extern vk::PhysicalDevice g_PhysicalDevice;
extern vk::Device g_Device;
extern vma::Allocator g_Allocator;
extern vk::SurfaceKHR g_Surface;
extern CommandBufferManager g_CommandManager;
extern ContextManager g_ContextManager;
extern FramebufferManager g_FramebufferManager;
// extern ID3D12Device* g_Device;
// extern CommandListManager g_CommandManager;

// extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
// extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
// extern bool g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT;

// extern DescriptorAllocator g_DescriptorAllocator[];
// inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 )
//{
//     return g_DescriptorAllocator[Type].Allocate(Count);
// }
} // namespace Graphics
