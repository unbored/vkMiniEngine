#include "VulkanMesh.h"
#include <vulkan/vulkan_format_traits.hpp>

constexpr size_t c_MaxBinding = 32;
constexpr size_t c_MaxStride = 2048;

void ComputeInputLayout(const std::vector<vk::VertexInputAttributeDescription> &desc, uint32_t *offsets,
                        uint32_t *strides)
{
    if (offsets)
    {
        memset(offsets, 0, sizeof(uint32_t) * desc.size());
    }
    if (strides)
    {
        memset(strides, 0, sizeof(uint32_t) * c_MaxBinding);
    }

    uint32_t prevABO[c_MaxBinding] = {}; // record location offsets

    for (size_t i = 0; i < desc.size(); ++i)
    {
        uint32_t binding = desc[i].binding;
        if (binding >= c_MaxBinding)
        {
            continue;
        }
        size_t bpe = vk::blockSize(desc[i].format); // bytes per element
        if (bpe == 0)
        {
            continue;
        }
        uint32_t alignment = bpe;
        if (alignment > 2)
        {
            alignment = 4;
        }

        uint32_t alignedOffset = desc[i].offset;

        if (offsets)
        {
            offsets[i] = alignedOffset;
        }
        if (strides)
        {
            uint32_t istride = alignedOffset + bpe;
            strides[binding] = std::max(strides[binding], istride);
        }
        prevABO[binding] = alignedOffset + bpe + (bpe % alignment);
    }
}