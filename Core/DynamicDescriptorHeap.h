#pragma once

#include <vulkan/vulkan.hpp>

#include "DescriptorSet.h"
#include <map>
#include <vector>
#include <vulkan/vulkan_structs.hpp>

class CommandContext;

class DynamicDescriptorHeap
{
public:
    DynamicDescriptorHeap(CommandContext &context, vk::DescriptorType type);
    ~DynamicDescriptorHeap();

    void Cleanup();

    void SetDescriptorInfo(uint32_t binding, uint32_t firstIndex, const std::vector<vk::DescriptorImageInfo> &infos);
    void SetDescriptorInfo(uint32_t binding, const vk::DescriptorBufferInfo &info);

    void CommitDescriptorSet(vk::DescriptorSet &set);

private:
    CommandContext &m_OwningContext;
    const vk::DescriptorType m_DescriptorType;
    std::map<uint32_t, std::vector<vk::DescriptorImageInfo>> m_ImageBindings;
    std::map<uint32_t, std::vector<vk::DescriptorBufferInfo>> m_BufferBindings;
};