#include "DynamicDescriptorHeap.h"

#include "CommandContext.h"
#include "GraphicsCore.h"
#include <cstddef>
#include <vulkan/vulkan_enums.hpp>

DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext &context, vk::DescriptorType type)
    : m_OwningContext(context), m_DescriptorType(type)
{
}

DynamicDescriptorHeap::~DynamicDescriptorHeap() { Cleanup(); }

void DynamicDescriptorHeap::Cleanup()
{
    m_ImageBindings.clear();
    m_BufferBindings.clear();
}

void DynamicDescriptorHeap::SetDescriptorInfo(uint32_t binding, uint32_t firstIndex,
                                              const std::vector<vk::DescriptorImageInfo> &infos)
{
    if (m_DescriptorType != vk::DescriptorType::eCombinedImageSampler &&
        m_DescriptorType != vk::DescriptorType::eStorageImage)
    {
        return;
    }
    if (infos.empty())
    {
        return;
    }
    // make sure vector contains enough elements
    auto &vec = m_ImageBindings[binding];
    size_t count = infos.size();
    if (vec.size() < firstIndex + count)
    {
        vec.resize(firstIndex + count);
    }
    for (int i = 0; i < count; ++i)
    {
        vec[firstIndex + i] = infos[i];
    }
}

void DynamicDescriptorHeap::SetDescriptorInfo(uint32_t binding, const vk::DescriptorBufferInfo &info)
{
    if (m_DescriptorType != vk::DescriptorType::eUniformBuffer &&
        m_DescriptorType != vk::DescriptorType::eStorageBuffer)
    {
        return;
    }
    for (auto &b : m_BufferBindings[binding])
    {
        if (b.offset == info.offset && b.range == info.range)
        {
            // buffer ranges coincide. just override it
            b = info;
            return;
        }
        if (info.offset + info.range > b.offset && info.offset < b.offset + b.range)
        {
            // buffers overlap, we don't deal with this case
            return;
        }
    }
    m_BufferBindings[binding].push_back(info);
}

void DynamicDescriptorHeap::CommitDescriptorSet(vk::DescriptorSet &set)
{
    std::vector<vk::WriteDescriptorSet> writes;
    for (auto &b : m_ImageBindings)
    {
        for (size_t i = 0; i < b.second.size(); ++i)
        {
            if (!b.second[i].imageView)
            {
                // skip the empty ones
                continue;
            }
            vk::WriteDescriptorSet write;
            write.dstSet = set;
            write.dstBinding = b.first;
            write.dstArrayElement = i;
            write.descriptorType = m_DescriptorType;
            write.descriptorCount = 1;
            write.setImageInfo(b.second[i]);
            writes.push_back(write);
        }
    }

    for (auto &b : m_BufferBindings)
    {
        for (size_t i = 0; i < b.second.size(); ++i)
        {
            vk::WriteDescriptorSet write;
            write.dstSet = set;
            write.dstBinding = b.first;
            write.dstArrayElement = 0;
            write.descriptorType = m_DescriptorType;
            write.descriptorCount = 1;
            write.setBufferInfo(b.second[i]);
            writes.push_back(write);
            // Graphics::g_Device.updateDescriptorSets(write, {});
        }
    }

    Graphics::g_Device.updateDescriptorSets(writes, {});
}