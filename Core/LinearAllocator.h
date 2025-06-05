#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#define DEFAULT_ALIGN 16

enum LinearAllocatorType
{
    kInvalidAllocator = -1,

    kGpuExclusive = 0, // DEFAULT   GPU-writeable (via UAV)
    kCpuWritable = 1,  // UPLOAD CPU-writeable (but write combined)

    kNumAllocatorTypes
};

enum
{
    kGpuAllocatorPageSize = 0x10000, // 64K
    kCpuAllocatorPageSize = 0x200000 // 2MB
};

class LinearAllocator
{
public:
    LinearAllocator(LinearAllocatorType Type);
    ~LinearAllocator();

    vma::AllocationInfo AllocAndBind(vk::Buffer &Buffer);
    void Cleanup();

private:
    vma::Pool m_Pool;
    LinearAllocatorType m_Type;

    std::vector<vma::Allocation> m_Allocations;
    std::vector<vk::Buffer> m_DynamicBuffers;
};
