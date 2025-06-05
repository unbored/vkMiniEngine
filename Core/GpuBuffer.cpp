#include "GpuBuffer.h"
#include "CommandContext.h"
#include "GraphicsCore.h"

using namespace Graphics;

void GpuBuffer::Create(size_t size)
{
    Destroy();

    m_BufferSize = size;
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = m_BufferSize;
    bufferInfo.usage = m_BufferUsage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = m_MemoryUsage;

    std::tie(m_Buffer, m_Allocation) = g_Allocator.createBuffer(bufferInfo, allocInfo);
}

void GpuBuffer::Destroy()
{
    if (m_Allocation)
    {
        g_Allocator.destroyBuffer(m_Buffer, m_Allocation);
        m_Buffer = nullptr;
        m_Allocation = nullptr;
    }
    // if (m_Buffer)
    //{
    //   g_Device.destroyBuffer(m_Buffer);
    //   m_Buffer = nullptr;
    // }
    // if (m_Memory)
    //{
    //   g_Device.freeMemory(m_Memory);
    //   m_Memory = nullptr;
    // }
}

vma::AllocationInfo GpuBuffer::GetAllocationInfo() const { return g_Allocator.getAllocationInfo(m_Allocation); }

void StagingBuffer::Create(size_t size) { GpuBuffer::Create(size); }

void *StagingBuffer::Map()
{
    // return g_Device.mapMemory(m_Memory, 0, m_BufferSize);
    return g_Allocator.mapMemory(m_Allocation);
}

void StagingBuffer::Unmap()
{
    // g_Device.unmapMemory(m_Memory);
    g_Allocator.unmapMemory(m_Allocation);
}

void IndexBuffer::Create(vk::IndexType type, size_t count)
{
    uint32_t elementSize = 0;
    switch (type)
    {
    case vk::IndexType::eUint16:
        elementSize = sizeof(uint16_t);
        break;
    case vk::IndexType::eUint32:
        elementSize = sizeof(uint32_t);
        break;
    case vk::IndexType::eUint8EXT:
        elementSize = sizeof(uint8_t);
        break;
    default:
        return;
    }
    GpuBuffer::Create(elementSize * count);
    m_ElementType = type;
    m_Count = count;
}

vk::IndexType IndexBuffer::GetIndexType() const { return m_ElementType; }

uint32_t IndexBuffer::GetCount() const { return m_Count; }
