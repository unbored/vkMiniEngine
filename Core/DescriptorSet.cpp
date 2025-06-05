#include "DescriptorSet.h"
#include "GraphicsCore.h"
#include "GpuBuffer.h"
#include "ImageView.h"

using namespace Graphics;

//void DescriptorSet::AddBinding(const vk::DescriptorSetLayoutBinding& binding)
//{
//    m_Bindings.push_back(binding);
//}

//void DescriptorSet::AddBinding(const vk::ArrayProxy<vk::DescriptorSetLayoutBinding>& bindings)
//{
//    m_Bindings.insert(m_Bindings.end(), bindings.begin(), bindings.end());
//}

void DescriptorSet::AddBindings(uint32_t BindingStart, uint32_t BindingCount, vk::DescriptorType Type, uint32_t DescriptorCount, vk::ShaderStageFlags Stage)
{
    for (uint32_t i = 0; i < BindingCount; ++i)
    {
        m_Bindings.push_back({ BindingStart + i, Type, DescriptorCount, Stage, nullptr });
    }
}

//void DescriptorSet::AddStaticSampler(uint32_t Binding, const vk::Sampler& Sampler)
//{
//    vk::DescriptorSetLayoutBinding binding;
//    binding.binding = Binding;
//    binding.descriptorCount = 1;
//    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//    binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
//    binding.setImmutableSamplers(Sampler);
//
//    m_Bindings.push_back(binding);
//}

void DescriptorSet::AddPushConstant(const vk::PushConstantRange& range)
{
    m_PcRanges.push_back(range);
}

void DescriptorSet::AddPushConstant(uint32_t Offset, uint32_t Size, vk::ShaderStageFlags Stage)
{
    vk::PushConstantRange range;
    range.offset = Offset;
    range.size = Size;
    range.stageFlags = Stage;
    m_PcRanges.push_back(range);
}

void DescriptorSet::Finalize()
{
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = m_Bindings.size();
    layoutInfo.setBindings(m_Bindings);
    m_Layout = g_Device.createDescriptorSetLayout(layoutInfo);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.setSetLayouts(m_Layout);
    if (!m_PcRanges.empty())
    {
        pipelineLayoutInfo.setPushConstantRangeCount(m_PcRanges.size());
        pipelineLayoutInfo.setPushConstantRanges(m_PcRanges);
    }
    m_PipelineLayout = g_Device.createPipelineLayout(pipelineLayoutInfo);

    //std::vector<vk::DescriptorPoolSize> poolSizes;
    //for (auto& b : m_Bindings)
    //{
    //    vk::DescriptorPoolSize poolSize;
    //    poolSize.type = b.descriptorType;
    //    poolSize.descriptorCount = dsCount;
    //    poolSizes.push_back(poolSize);
    //}
    //vk::DescriptorPoolCreateInfo poolInfo;
    //poolInfo.poolSizeCount = poolSizes.size();
    //poolInfo.setPoolSizes(poolSizes);
    //poolInfo.maxSets = dsCount;
    //m_Pool = g_Device.createDescriptorPool(poolInfo);

    //std::vector<vk::DescriptorSetLayout> layouts(dsCount, m_Layout);
    //vk::DescriptorSetAllocateInfo allocInfo;
    //allocInfo.descriptorPool = m_Pool;
    //allocInfo.descriptorSetCount = dsCount;
    //allocInfo.setSetLayouts(layouts);
    //m_DS = g_Device.allocateDescriptorSets(allocInfo);
}


void DescriptorSet::Destroy()
{
    //if (m_Pool)
    //{
    //    g_Device.destroyDescriptorPool(m_Pool);
    //    m_Pool = nullptr;
    //}
    if (m_Layout)
    {
        g_Device.destroyDescriptorSetLayout(m_Layout);
        m_Layout = nullptr;
    }
    if (m_PipelineLayout)
    {
        g_Device.destroyPipelineLayout(m_PipelineLayout);
        m_PipelineLayout = nullptr;
    }
}

//void DescriptorSet::UpdateUniformBuffer(uint32_t index, uint32_t binding, const UniformBuffer& buffer)
//{
//    vk::DescriptorBufferInfo info;
//    info.buffer = buffer.GetBuffer();
//    info.offset = 0;
//    info.range = buffer.GetBufferSize();
//
//    vk::WriteDescriptorSet write;
//    write.dstSet = m_DS[index];
//    write.dstBinding = binding;
//    write.dstArrayElement = 0;
//    write.descriptorType = vk::DescriptorType::eUniformBuffer;
//    write.descriptorCount = 1;
//    write.setBufferInfo(info);
//    g_Device.updateDescriptorSets(write, {});
//}
//
//void DescriptorSet::UpdateImageSampler(uint32_t index, uint32_t binding, const ImageView& imageview, const vk::Sampler& sampler)
//{
//    vk::DescriptorImageInfo info;
//    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
//    info.imageView = imageview;
//    info.sampler = sampler;
//
//    vk::WriteDescriptorSet write;
//    write.dstSet = m_DS[index];
//    write.dstBinding = binding;
//    write.dstArrayElement = 0;
//    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//    write.descriptorCount = 1;
//    write.setImageInfo(info);
//    g_Device.updateDescriptorSets(write, {});
//}

DescriptorPool::DescriptorPool(uint32_t size)
{
    vk::DescriptorType dsTypes[] =
    {
        vk::DescriptorType::eUniformBuffer,
        vk::DescriptorType::eCombinedImageSampler,
        vk::DescriptorType::eStorageImage,
        vk::DescriptorType::eStorageBuffer
    };
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for (auto& t : dsTypes)
    {
        vk::DescriptorPoolSize poolSize;
        poolSize.type = t;
        poolSize.descriptorCount = size * 10;
        poolSizes.push_back(poolSize);
    }
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.setPoolSizes(poolSizes);
    poolInfo.maxSets = size;
    m_Pool = g_Device.createDescriptorPool(poolInfo);
}

DescriptorPool::~DescriptorPool()
{
    g_Device.destroyDescriptorPool(m_Pool);
}

vk::DescriptorSet DescriptorPool::NewDescriptorSet(const vk::DescriptorSetLayout& layout)
{
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = m_Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.setSetLayouts(layout);
    auto set = g_Device.allocateDescriptorSets(allocInfo);
    return set[0];
}

void DescriptorPool::Cleanup()
{
    g_Device.resetDescriptorPool(m_Pool);
}
