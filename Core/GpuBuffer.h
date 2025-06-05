#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

class GpuBuffer
{
    friend class CommandContext;
    friend class GraphicsContext;

public:
    GpuBuffer(vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memUsage)
        : m_BufferUsage(bufferUsage), m_MemoryUsage(memUsage)
    {
    }
    // virtual ~GpuBuffer() { Destroy(); }

    virtual void Create(size_t size); // create a custom buffer
    virtual void Destroy();

    vk::Buffer GetBuffer() const { return m_Buffer; }
    size_t GetBufferSize() const { return m_BufferSize; }
    vma::AllocationInfo GetAllocationInfo() const;

protected:
    vk::BufferUsageFlags m_BufferUsage;
    vk::Buffer m_Buffer;
    // vk::DeviceMemory m_Memory;
    vma::MemoryUsage m_MemoryUsage;
    vma::Allocation m_Allocation;
    size_t m_BufferSize;
};

class StagingBuffer : public GpuBuffer
{
public:
    StagingBuffer() : GpuBuffer(vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly) {}

    void Create(size_t size);

    void *Map();
    void Unmap();
};

class VertexBuffer : public GpuBuffer
{
public:
    VertexBuffer()
        : GpuBuffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                    vma::MemoryUsage::eGpuOnly)
    {
    }
    VertexBuffer(const GpuBuffer &other) : GpuBuffer(other) {}
};

class IndexBuffer : public GpuBuffer
{
public:
    IndexBuffer()
        : GpuBuffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                    vma::MemoryUsage::eGpuOnly)
    {
    }
    IndexBuffer(const GpuBuffer &other, vk::IndexType type) : GpuBuffer(other), m_ElementType(type) {}

    void Create(vk::IndexType type, size_t count);
    vk::IndexType GetIndexType() const;
    uint32_t GetCount() const;

private:
    vk::IndexType m_ElementType;
    uint32_t m_Count;
};

class UniformBuffer : public GpuBuffer
{
public:
    UniformBuffer()
        : GpuBuffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
                    vma::MemoryUsage::eCpuToGpu)
    {
    }
};
