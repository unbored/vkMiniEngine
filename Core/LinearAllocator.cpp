#include "LinearAllocator.h"
#include "GraphicsCore.h"

using namespace Graphics;

LinearAllocator::LinearAllocator(LinearAllocatorType Type) : m_Type(Type)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = 1024;
    bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer |
                       vk::BufferUsageFlagBits::eUniformBuffer;
    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = vma::MemoryUsage::eCpuToGpu;
    uint32_t index = g_Allocator.findMemoryTypeIndexForBufferInfo(bufferInfo, allocInfo);

    vma::PoolCreateInfo poolInfo;
    poolInfo.flags = vma::PoolCreateFlagBits::eLinearAlgorithm; // linear for better performance
    poolInfo.memoryTypeIndex = index;
    poolInfo.blockSize = m_Type == kGpuExclusive ? kGpuAllocatorPageSize : kCpuAllocatorPageSize;
    m_Pool = g_Allocator.createPool(poolInfo);
}

LinearAllocator::~LinearAllocator() { g_Allocator.destroyPool(m_Pool); }

vma::AllocationInfo LinearAllocator::AllocAndBind(vk::Buffer &Buffer)
{
    vma::AllocationCreateInfo createInfo;
    createInfo.usage = vma::MemoryUsage::eCpuToGpu;
    createInfo.pool = m_Pool;
    createInfo.flags = vma::AllocationCreateFlagBits::eMapped;

    vma::Allocation allocation = g_Allocator.allocateMemoryForBuffer(Buffer, createInfo);
    g_Allocator.bindBufferMemory(allocation, Buffer);
    m_Allocations.push_back(allocation);
    m_DynamicBuffers.push_back(Buffer);

    vma::AllocationInfo allocInfo = g_Allocator.getAllocationInfo(allocation);
    return allocInfo;
}

void LinearAllocator::Cleanup()
{
    for (auto &b : m_DynamicBuffers)
    {
        g_Device.destroyBuffer(b);
    }
    m_DynamicBuffers.clear();

    for (auto &a : m_Allocations)
    {
        g_Allocator.freeMemory(a);
    }
    m_Allocations.clear();
}
