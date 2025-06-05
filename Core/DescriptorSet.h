#pragma once

#include "DynamicDescriptorHeap.h"
#include <vulkan/vulkan.hpp>

class UniformBuffer;
class ImageView;

class DescriptorSet
{
    friend class DynamicDescriptorHeap;

public:
    // void AddBinding(const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings);
    void AddBindings(uint32_t BindingStart, uint32_t BindingCount, vk::DescriptorType Type, uint32_t DescriptorCount,
                     vk::ShaderStageFlags Stage);
    // void AddStaticSampler(uint32_t Binding, const vk::Sampler& Sampler);
    void AddPushConstant(const vk::PushConstantRange &range);
    void AddPushConstant(uint32_t Offset, uint32_t Size, vk::ShaderStageFlags Stage);

    void Finalize();

    void Destroy();

    vk::DescriptorSetLayout GetLayout() const { return m_Layout; }

    vk::PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

    // vk::DescriptorSet operator[](size_t i) { return m_DS[i]; }

    // void UpdateUniformBuffer(uint32_t index, uint32_t binding,
    //     const UniformBuffer& buffer);
    // void UpdateImageSampler(uint32_t index, uint32_t binding,
    //     const ImageView& imageview, const vk::Sampler& sampler);

private:
    std::vector<vk::DescriptorSetLayoutBinding> m_Bindings;
    std::vector<vk::PushConstantRange> m_PcRanges;
    vk::DescriptorSetLayout m_Layout;
    vk::PipelineLayout m_PipelineLayout;
    // vk::DescriptorPool m_Pool;
    // std::vector<vk::DescriptorSet> m_DS;
};

class DescriptorPool
{
public:
    DescriptorPool(uint32_t size);
    ~DescriptorPool();

    vk::DescriptorSet NewDescriptorSet(const vk::DescriptorSetLayout &layout);
    void Cleanup();

private:
    vk::DescriptorPool m_Pool;
};
